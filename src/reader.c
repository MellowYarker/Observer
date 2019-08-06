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

        int bufsize = 1000; // assume tx = 1kB
        char *transaction = malloc(sizeof(char) * bufsize);

        if (transaction == NULL) {
            perror("malloc");
            exit(1);
        }

        char **outputs; // array of pointers to output addresses

        while (read(fd[0], transaction, bufsize) != -1) {
            char **outputs; // array of pointers to output addresses
            int ntxOut = 0; // the number of output addresses
            // TODO: set ntxOut after parsing JSON response
            outputs = malloc(sizeof(char *) * ntxOut);

            if (outputs == NULL) {
                perror("malloc");
                exit(1);
            }

            char *batch; // all the queries combined in a string
            char *query; // a single query
            char query_size = ntxOut * 48 + (40 * ntxOut); // estimating size
            int address_type;
            char format[] = "SELECT %q FROM keys WHERE %q='%q'";

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
                // TODO: here we check if we need to reallocate 'batch'
                // then we append query to batch
                sqlite3_free(query);
            }

            // Maybe we can use the linked list from /init_download for response
            // TODO: write any returned records to the linked list
            rc = sqlite3_exec(db, batch, callback, 0, &zErrMsg);
            if (rc != SQLITE_OK) {
                fprintf(stderr, "SQL error: %s\n", zErrMsg);
                sqlite3_free(zErrMsg);
            }


            free(batch);

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

        // Step 2: Set up WebSocket connection with blockchain (LATER PROBLEM)


        // Step 3: begin (blocking) loop of reading from socket
        //          - in this loop, we check the bloom filter!
        // infinite loop goes here
        // read from socket, parse for outputs

        size_t sizeout = 128;
        char output_address[sizeout];
        strncpy(output_address, "1ExampLe_Address", 34);

        // this linked list consists of addresses we must check, nodes are
        // "free'd" as they're written to the child process
        struct node *positive_address_head = NULL;
        int list_size = 0;

        // TODO: here we loop over the Tx outputs

        // check if we own the output address
        if (bloom_check(&address_bloom, &output_address, strlen(output_address))
            == 1) {
            struct node *cur = create_node(output_address);
            add_to_head(cur, positive_address_head); // add the node to the head
            list_size++; // increment number of elements in the linked list
        }

        // after checking all outputs in this transaction..
        if (positive_address_head != NULL) {
            // we write the transaction, then the number of addresses in the LL,
            // then the size of the incoming address, then the address itself

            // Note, we don't actually have access to transactions yet.
            // We will eventually just make a large buffer that we can write to
            // that can fit all transactions.
            // *************************************************
            write(fd[1], transaction, strlen(transaction)); // write transaction
            // *************************************************
            write(fd[1], &list_size, sizeof(list_size)); // write number of addrs

            struct node *cur = positive_address_head;
            // write all the addresses we must check to the pipe!
            while (cur->next != NULL) {
                write(fd[1], &cur->size, sizeof(cur->size)); // # bytes in cur::data
                write(fd[1], cur->data, cur->size); // the data itself

                // free the current node
                free(cur->data);
                struct node *temp = cur;
                cur = cur->next;
                free(temp);
                list_size--; // will eventually be 0
            }
        }
    }
    // otherwise keep going
    
    // end of loop via signal?
    bloom_free(&address_bloom);
    btc_ecc_stop();

    return 0;
}