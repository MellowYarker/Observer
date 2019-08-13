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

int main() {
    btc_ecc_start(); // load libbtc
    // const btc_chainparams* chain = &btc_chainparams_main; // mainnet
    
    struct bloom address_bloom; // filter of all generated addresses.
    const char address_filter_file[] = "generated_addresses_filter.b";
    
    // load the bloom filter
    if (access((char *) &address_filter_file, F_OK) != -1) {
        if (bloom_load(&address_bloom, (char *) &address_filter_file) == 0) {
            printf("Loaded address filter.\n");
        } else {
            printf("Failed to load bloom filter.\n");
            exit(1);
        }
    } else {
        printf("Could not find filter: %s\n", address_filter_file);
        printf("You have not generated any addresses.\n");
        exit(1);
    }

    // set up the pipe, data flows from parent to child.
    int fd[2];
    pipe(fd);

    // TODO: we need to see the size of a transaction to create a large
    // enough buffer for all instances
    char *transaction; // transaction string. txs are stored here after reading
    int bufsize = 1000; // max buffer size of a transaction. Right pad with '/0'

    // same memory space in parent and child, but not shared between them
    transaction = malloc(sizeof(char) * bufsize);

    if (transaction == NULL) {
        perror("malloc");
        exit(1);
    }

    int r = fork();
    if (r < 0) {
        perror("fork");
        exit(1);
    } else if (r == 0) {

        /*  Child process reads transactions from the pipe, and checks if their
            positive outputs are in our database.

            A positive output is an addresses that came back as "positive" from
            the bloom filter. I.e, it might be in our database.
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

        char **outputs; // array of pointers to output addresses
        int ntxOut = 0; // the number of output addresses
        int txResponse;

        // begin (blocking) loop of reading from the pipe
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
                // increment but don't count null terminator
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
            char *query;
            // Note: 51 = size of "format" when first 2 "%q"'s are replaced by
            // "P2WPKH" and the final "%q" is empty
            // Will allocate slightly more than enough for the final query.
            int batch_buf_size = ntxOut * 51 + total_addr_size_sum;
            batch = malloc(batch_buf_size * sizeof(char));

            if (batch == NULL) {
                perror("malloc");
                exit(1);
            }

            int address_type;
            char format[] = "SELECT privkey, %q FROM keys WHERE %q='%q'; ";

            // Check every output against our database
            for (int i = 0; i < ntxOut; i++) {
                // determine the type of address and build the query
                if (strncmp(outputs[i], "1", 1) == 0) {
                    query = sqlite3_mprintf(format, "P2PKH", "P2PKH",
                                                outputs[i]);
                } else if (strncmp(outputs[i], "3", 1) == 0) {
                    query = sqlite3_mprintf(format, "P2SH", "P2SH", outputs[i]);
                } else {
                    query = sqlite3_mprintf(format, "P2WPKH", "P2WPKH",
                                                outputs[i]);
                }

                if (query == NULL) {
                    fprintf(stderr, "Failed to build query.");
                    exit(1);
                }
                // append the query to batch
                strcat(batch, query);
                sqlite3_free(query);
            }

            // write any returned records to this linked list
            struct node *exists = NULL;

            rc = sqlite3_exec(db, batch, callback, &exists, &zErrMsg);
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
                struct node *cur = exists;
                while (cur != NULL) {
                    printf("\nSpendable output discovered!\n");
                    printf("Address: %s\nPrivate Key: %s\n", cur->data,
                            cur->private);
                    // TODO: do something
                    free(cur->data);
                    free(cur->private);
                    struct node *temp = cur;
                    cur = cur->next;
                    free(temp);
                }
            }

            // after handling all records, clean up and get ready to read the
            // next transaction!

            // free all the addresses we saved
            for (int j = 0; j < ntxOut; j++) {
                free(outputs[j]);
            }

            free(outputs);
            memset(transaction, '\0', bufsize);
        }

        // parent process has closed the pipe, begin shutdown
        sqlite3_close(db);
        if (close(fd[0]) == -1) {
            perror("close");
            exit(1);
        }
        free(transaction);

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

        // Read from socket in loop, gather outputs from JSON
        int loop = 1;
        while (loop) { // loop will change to be "while we get a valid response"
            size_t sizeout = 128;
            char output_address[sizeout]; // temporary address buffer

            // delete from here **
            // an array of pointers to addresses. Some of which are in our db
            char **test_list = malloc(4 * sizeof(char *));

            // test values
            test_list[0] = malloc((strlen("13sTVUMCUyNjz2b2cfgrBcXj6FMjMA5CNE")
                                   + 1) * sizeof(char)); // privkey maddie05000000000000000000000000 - p2pkh
            strcpy(test_list[0], "13sTVUMCUyNjz2b2cfgrBcXj6FMjMA5CNE");
            test_list[1] = malloc((strlen("bc1qmz2ghff8xhd5f34cl46pa38uafmwn2tsuppur4")
                                   + 1) * sizeof(char)); // privkey 000000000000000000000000#1stunna - p2sh
            strcpy(test_list[1], "bc1qmz2ghff8xhd5f34cl46pa38uafmwn2tsuppur4");
            test_list[2] = malloc((strlen("3LSczTmoYNYAi5LeeKR88JKGEREnLdDDjn")
                                   + 1) * sizeof(char)); // privkey 707a2cdf7931c60188ab936c7fa29df7 - p2wpkh
            strcpy(test_list[2], "3LSczTmoYNYAi5LeeKR88JKGEREnLdDDjn");
            test_list[3] = malloc((strlen("1MbccsqwFwkc7RLfkqXKxMLUGj9tyq9GT9")
                                   + 1) * sizeof(char)); // Not in database
            strcpy(test_list[3], "1MbccsqwFwkc7RLfkqXKxMLUGj9tyq9GT9");
            // ** to here
            // linked list of addresses that came back as "positive" from BF
            struct node *positive_address_head = NULL;
            int list_size = 0;

            // TODO: here we loop over the Tx outputs
            //      -> Store each output in output_address as we get to it
            int outputCount = 4; // temporary until we get the actual tx
            for (int i = 0; i < outputCount; i++) {
                // parse the tx for each output (using jansson)
                // strcpy(output_address, transacation[index we want]);
                // delete from here **
                strncpy(output_address, test_list[i], sizeout);
                free(test_list[i]);
                // ** to here

                // check if we own the output address
                if (bloom_check(&address_bloom, &output_address,
                                strlen(output_address)) == 1) {
                    // store positive addresses in linked list
                    struct node *positive = create_node(output_address);
                    if (positive == NULL) {
                        fprintf(stderr, "Couldn't allocate space for Node.");
                        exit(1);
                    }
                    add_to_head(positive, &positive_address_head);
                    list_size++; // increment number of elements in the LL
                }
            }
            free(test_list); // delete

            if (positive_address_head != NULL) {
                /** Data is written to child like so
                 * 1. Transaction: x byte buffer
                 * 2. # of output addresses that will be sent: sizeof(int)
                 * 3. # of bytes of incoming address (including \0): sizeof(int)
                 * 4. Address (null terminated): size described in previous msg
                **/

                // TODO: Note, we don't have access to transactions yet
                // Eventually, we will make a large buffer that we can write
                // to that can fit all transactions.
                // *************************************************

                memset(transaction, '\0', bufsize); // delete
                strcpy(transaction, "No data"); // delete
                // right pad transaction with null terminators // delete
                // step 1
                if (write(fd[1], transaction, bufsize) == -1) {
                    perror("write");
                    fprintf(stderr, "Failed to write transaction to child.");
                    exit(1);
                }
                // *************************************************
                // step 2
                if (write(fd[1], &list_size, sizeof(list_size)) == -1) {
                    perror("write");
                    fprintf(stderr, "Failed to write quantity of addresses.");
                    exit(1);
                }

                struct node *cur = positive_address_head;
                // write all the positive addresses to the pipe
                while (cur != NULL) {
                    // step 3
                    if (write(fd[1], &cur->size, sizeof(cur->size)) == -1) {
                        perror("write");
                        fprintf(stderr, "Failed to write address size to child.");
                        exit(1);
                    }
                    // step 4
                    if (write(fd[1], cur->data, cur->size) == -1) {
                        perror("write");
                        fprintf(stderr, "Failed to write address to child.");
                        exit(1);
                    }

                    // free the current node
                    free(cur->data);
                    struct node *temp = cur;
                    cur = cur->next;
                    free(temp);

                    list_size--;
                }
            }
            memset(transaction, '\0', bufsize);
            loop = 0; // delete
        }
        // We also want to get here if user sends a signal
        // Close the pipe to shutdown the child process.
        if (close(fd[1]) == -1) {
            perror("close");
            exit(1);
        }
        free(transaction);
        wait(NULL);
    }
    
    // end of loop via signal?

    bloom_free(&address_bloom);
    btc_ecc_stop();

    return 0;
}