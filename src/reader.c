#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>

#include <bloom.h>

#include <btc.h>
#include <chainparams.h>
#include <ecc.h>
#include <ecc_key.h>

#include "reader.h"


#define p2pkh 0
#define p2sh 1
#define p2wpkh 2

int main() {
    btc_ecc_start(); // load libbtc
    const btc_chainparams* chain = &btc_chainparams_main; // mainnet
    
    struct bloom address_bloom; // filter of all generated addresses.
    const char address_filter_file[] = "generated_addresses_filter.b";
    
    if (access((char *) &address_filter_file, F_OK) != -1) {
        if (bloom_load(&address_bloom, (char *) &address_filter_file) == 0) {
            printf("Loaded Address filter.\n");
        } else {
            printf("Failed to load bloom filter.\n");
        }
    } else {
        printf("Could not find filter: %s\n", address_filter_file);
    }
    // set up the pipe, data flows from parent to child only.
    int fd[2];
    pipe(fd);

    int r = fork();
    if (r < 0) {
        perror("fork");
        exit(1);
    } else if (r == 0) {

        // child process

        /*  The purpose of this process is to take a transaction from the
            pipe and check if it's outputs are in our database.

            The parent process only writes to the child if an output came back
            positive from the address bloom filter, so we only check
            addresses that may actually be present.
        */

        // close the write end of the pipe
        if (close(fd[1]) == -1) {
            perror("close");
            exit(1);
        }
        // set up db connection, we do not modify db, so concurrency is safe
        sqlite3 *db;
        char *zErrMsg = 0;
        int rc;

        rc = sqlite3_open("../db/observer.db", &db);
        if (rc) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            exit(1);
        }

        // Step 3: begin (blocking) loop of reading from the pipe
        //           - in this loop we check the database!

        // TODO: we need to see the size of a transaction to create a large
        // enough buffer for all instances

        int bufsize = 1000; // should be same in parent, TODO: set this before fork
        char *transaction = malloc(sizeof(char) * bufsize);

        if (transaction == NULL) {
            perror("malloc");
            exit(1);
        }

        char **outputs; // array of pointers to output addresses
        int ntxOut = 0; // the number of output addresses
        int txResponse;
        // TODO: error check every read call
        // Doing this many reads is a little dangerous, there's a lot of
        // blocking behaviour, but then again we need all the data too come through

        while ((txResponse = read(fd[0], transaction, bufsize)) != 0) {
            if (txResponse == -1) {
                perror("read");
                exit(1);
            }
            // set # of incoming addrs
            if (read(fd[0], &ntxOut, sizeof(ntxOut)) == -1) {
                perror("read");
                exit(1);
            }
            outputs = malloc(sizeof(char *) * ntxOut);

            if (outputs == NULL) {
                perror("malloc");
                exit(1);
            }
            int addr_size; // # of bytes of incoming address
            int total_addr_size_sum = 0; // sum of length of addresses read

            // read each output address into the outputs array
            for (int j = 0; j < ntxOut; j++) {
                if (read(fd[0], &addr_size, sizeof(addr_size)) == -1) {
                    perror("read");
                    exit(1);
                }

                total_addr_size_sum = total_addr_size_sum + addr_size - 1;
                // allocate space for the incoming address
                outputs[j] = malloc(addr_size * sizeof(char));

                if (outputs[j] == NULL) {
                    perror("malloc");
                    exit(1);
                }

                // store the address directly in the array
                if (read(fd[0], outputs[j], addr_size) == -1) {
                    perror("read");
                    exit(1);
                }
            }

            char *batch; // all the queries combined in a string
            char *query; // a single query
            // Note, 51 is the size of "format" with q's replaced with P2WPKH
            // and the final %q being empty. This should allocate more than
            // enough for the final query.
            int query_buf_size = ntxOut * 51 + total_addr_size_sum;
            int address_type;
            char format[] = "SELECT privkey, %q FROM keys WHERE %q='%q'; ";

            // Check every output against our database
            for (int i = 0; i < ntxOut; i++) {
                // determine the type of address - helps speed up sql.
                if (strncmp(outputs[i], "1", 1) == 0) {
                    address_type = p2pkh;
                } else if (strncmp(outputs[i], "3", 1) == 0) {
                    address_type = p2sh;
                } else {
                    address_type = p2wpkh;
                }

                // build the query
                switch(address_type) {
                    case p2pkh:
                        query = sqlite3_mprintf(format, "P2PKH", "P2PKH",
                                                outputs[i]);
                        break;
                    case p2sh:
                        query = sqlite3_mprintf(format, "P2SH", "P2SH",
                                                outputs[i]);
                        break;
                    case p2wpkh:
                        query = sqlite3_mprintf(format, "P2WPKH", "P2WPKH",
                                                outputs[i]);
                        break;
                    default:
                        query = NULL;
                }

                if (query == NULL) {
                    fprintf(stderr, "Failed to build query.");
                    exit(1);
                }
                // append the query to batch
                strcat(batch, query);
                sqlite3_free(query);
            }

            // write any returned records to the linked list
            struct node *exists = NULL;

            rc = sqlite3_exec(db, batch, callback, exists, &zErrMsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }

            free(batch);

            // TODO: at this point, we need to handle any records that are
            // returned from the database
            if (exists != NULL) {
                // this transaction contains output addresses that we control
                // TODO: we can create a new transaction that spends them.
                printf("We have found a spendable output!\n");
            }

            // after handling all records, clean up and get ready to read the
            // next transaction!

            // free all the addresses we saved
            for (int j = 0; j < ntxOut; j++) {
                free(outputs[j]);
            }

            free(outputs);
            free(transaction);
        }
        // parent process closed the pipe, begin shutdown
        sqlite3_close(db);
        if (close(fd[0]) == -1) {
            perror("close");
            exit(1);
        }

        exit(0); // process terminated normally
    } else {
        // parent process

        /*  The parent process's job is to get new transactions and check them
            against the bloom filter. If we get any positive responses, write
            the tx to the pipe.

            This allows the parent process to avoid performing disk IO, allowing
            it to quickly read the next transaction.*/

        // Step 1: close the read end of the pipe
        if (close(fd[0]) == -1) {
            perror("close");
            exit(1);
        }

        // Step 2: Set up WebSocket connection with blockchain

        // This will be completed later

        // Step 3: begin (blocking) loop of reading from socket

        // infinite loop goes here
        // read from socket, parse for outputs

        size_t sizeout = 128;
        char output_address[sizeout]; // temporary address buffer
        strncpy(output_address, "1ExampLe_Address", 34);

        // linked list of addresses that came back as "positive" from BF
        // we will write these to the child for further investigation

        // note, nodes are free'd after they're written to the child
        struct node *positive_address_head = NULL;
        int list_size = 0;

        // TODO: here we loop over the Tx outputs
        //      -> Store each output in output_address as we get to it

        // check if we own the output address
        if (bloom_check(&address_bloom, &output_address, strlen(output_address))
            == 1) {
            struct node *cur = create_node(output_address);
            add_to_head(cur, positive_address_head); // add the node to the head
            list_size++; // increment number of elements in the linked list
        }
        // end of the loop

        // after checking all outputs in this transaction..
        if (positive_address_head != NULL) {
            /** Data is written to child like so
             * 1. Transaction: x byte buffer
             * 2. # of output addresses that will be sent: sizeof(int)
             * 3. # of bytes of incoming address: sizeof(int)
             * 4. Address (null terminated): size described in previous msg
            **/

            // TODO: Note, we don't actually have access to transactions yet.
            // We will eventually just make a large buffer that we can write to
            // that can fit all transactions.
            // *************************************************
            char *transaction;
            // step 1
            if (write(fd[1], transaction, strlen(transaction)) == -1) {
                perror("write");
                fprintf(stderr, "Failed to write to child.");
                exit(1);
            }
            // *************************************************
            // step 2
            if (write(fd[1], &list_size, sizeof(list_size)) == -1) {
                perror("write");
                fprintf(stderr, "Failed to write to child.");
                exit(1);
            }

            struct node *cur = positive_address_head;
            // write all the positive addresses to the pipe
            while (cur->next != NULL) {
                // step 3
                if (write(fd[1], &cur->size, sizeof(cur->size)) == -1) {
                    perror("write");
                    fprintf(stderr, "Failed to write to child.");
                    exit(1);
                }
                // step 4
                if (write(fd[1], cur->data, cur->size) == -1) {
                    perror("write");
                    fprintf(stderr, "Failed to write to child.");
                    exit(1);
                }

                // free the current node
                free(cur->data);
                struct node *temp = cur;
                cur = cur->next;
                free(temp);

                list_size--;
            }
            if (list_size != 0) {
                printf("ERROR: we failed to write all elements to the pipe.\n");
            } else {
                printf("Successfully wrote positive addresses to the pipe.\n");
            }
        }
    }
    // otherwise keep going
    
    // end of loop via signal?
    bloom_free(&address_bloom);
    btc_ecc_stop();

    return 0;
}