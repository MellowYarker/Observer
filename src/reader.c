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

#include <libwebsockets.h>

#ifdef __linux__
    #include <sys/types.h>
    #include <sys/wait.h>
#endif

#include "reader.h"

int main() {
    btc_ecc_start(); // load libbtc
    
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
        int response; // TODO: (scan-15) use this to see if the parent closed the pipe

        char *child_transaction;
        int child_tx_size;

        // begin (blocking) loop of reading from the pipe
        // Note: Check the pipe protocol in the parent!
        while ((response = read(fd[0], &child_tx_size, sizeof(int))) != 0) {
            if (response == -1) {
                perror("read");
                exit(1);
            }
            child_transaction = malloc(child_tx_size * sizeof(char));
            if (child_transaction == NULL) {
                perror("malloc");
                exit(1);
            }

            if (read(fd[0], child_transaction, child_tx_size) == -1) {
                perror("read");
                exit(1);
            }

            if (read(fd[0], &ntxOut, sizeof(ntxOut)) == -1) {
                perror("read");
                exit(1);
            }

            printf("Will check %d records.\n");
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
            int batch_buf_size = ntxOut * 51 + total_addr_size_sum + 1;
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
            memset(child_transaction, '\0', child_tx_size);
        }

        // parent process has closed the pipe, begin shutdown
        sqlite3_close(db);
        if (close(fd[0]) == -1) {
            perror("close");
            exit(1);
        }
        free(child_transaction);

        exit(0);
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
        struct lws_context_creation_info info;
        const char *p;
        int n = 0, logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

        lws_set_log_level(logs, NULL);
        lwsl_user("Initializing WebSocket connection...\n");

        memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
        info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        info.port = CONTEXT_PORT_NO_LISTEN; /* we do not run any server */
        info.protocols = protocols;

        /*
        * since we know this lws context is only ever going to be used with
        * one client wsis / fds / sockets at a time, let lws know it doesn't
        * have to use the default allocations for fd tables up to ulimit -n.
        * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
        * will use.
        */
        info.fd_limit_per_thread = 1 + 1 + 1;

        context = lws_create_context(&info);
        if (!context) {
            lwsl_err("lws init failed\n");
            return 1;
        }
        struct transaction *cur_tx; // store transaction details here
        char *buffer = NULL; // a buffer that stores partial writes
        int buffer_size = 0;
        while (n >= 0) {
            n = lws_service(context, 1000); // read from the server
            // tranasction_buf and transaction_size are declared in socket.c
            if (transaction_buf != NULL) {
                /* 4 cases:
                    1. Partial write & empty buffer
                    2. Partial write & non-empty buffer (prev msg was partial)
                    3. Complete write & non-empty buffer (...)
                    4. Complete write & empty buffer
                */
                if (buffer == NULL) {
                    buffer = malloc(sizeof(char) * (transaction_size + 1));
                    if (buffer == NULL) {
                        perror("malloc");
                        exit(1);
                    }
                    strcpy(buffer, transaction_buf);
                    buffer[transaction_size] = '\0';
                    buffer_size = transaction_size + 1;

                    // we will add the next message to the buffer
                    if (partial_write) {
                        printf("Received partial transaction. Waiting for"\
                               " remaining message.\n");
                        continue;
                    }
                } else if (buffer != NULL) {
                    buffer = realloc(buffer, (buffer_size + transaction_size)
                                     * sizeof(char));
                    if (buffer == NULL) {
                        perror("realloc");
                        exit(1);
                    }
                    strcat(buffer, transaction_buf);
                    buffer_size = buffer_size + transaction_size; // TODO: null terminate?

                    // we will add the next message to the buffer
                    if (partial_write) {
                        printf("Received partial transaction. Waiting for"\
                               " remaining message.\n");
                        continue;
                    } else {
                        printf("MESSAGE COMPLETED\n");
                    }
                }

                cur_tx = create_transaction(buffer, buffer_size);

                // reset our buffers now that we saved the data in cur_tx
                memset(transaction_buf, '\0', transaction_size);
                transaction_buf = NULL; // otherwise we will enter this if block
                free(transaction_buf);
                transaction_size = 0;

                memset(buffer, '\0', buffer_size - 1);
                buffer = NULL;
                free(buffer);
                buffer_size = 0;

                if (cur_tx == NULL) {
                    fprintf(stderr, "Had to discard previous partial "\
                            "transactions.\nThis usually occurs when the order"\
                            " of partial reads is lost.\n");
                    continue;
                }

                // linked list of addresses that came back as "positive" from BF
                struct node *positive_address_head = NULL;
                int list_size = 0;

                // loop over outputs
                for (int i = 0; i < cur_tx->nOutputs; i++) {
                    // check if we own the output address
                    if (bloom_check(&address_bloom, cur_tx->outputs[i],
                                    strlen(cur_tx->outputs[i])) == 1) {
                        printf("\n************Positive hit***********\n");
                        // store positive addresses in linked list
                        struct node *positive = create_node(cur_tx->outputs[i]);
                        if (positive == NULL) {
                            fprintf(stderr, "Couldn't allocate space for Node"\
                            "\n");
                            exit(1);
                        }
                        add_to_head(positive, &positive_address_head);
                        list_size++; // increment number of elements in the LL
                    }
                }
                // write to pipe if we have found potentially spendable addrs
                if (positive_address_head != NULL) {
                    printf("May have found spendable outputs. Checking "\
                            "database.\n");
                    /** Pipe Protocol
                     * 1. Transaction size: sizeof(int)
                     * 2. Transaction: size described in previous message
                     * 3. # of output addresses that will be sent: sizeof(int)
                     * 4. # of bytes of incoming address (+ '\0'): sizeof(int)
                     * 5. Address (null terminated): size described in 4.
                    **/

                    // step 1
                    if (write(fd[1], &cur_tx->size, sizeof(cur_tx->size)) == -1)
                    {
                        perror("write");
                        fprintf(stderr, "Failed to write transaction size to"\
                                        " pipe.\n");
                        exit(1);
                    }
                    // step 2
                    if (write(fd[1], cur_tx->tx, cur_tx->size) == -1) {
                        perror("write");
                        fprintf(stderr, "Failed to write transaction to the"\
                                        " pipe.\n");
                        exit(1);
                    }
                    // step 3
                    if (write(fd[1], &list_size, sizeof(list_size)) == -1) {
                        perror("write");
                        fprintf(stderr, "Failed to write the number of "\
                                        "addresses to the pipe.\n");
                        exit(1);
                    }

                    struct node *cur = positive_address_head;
                    // write all the positive addresses to the pipe
                    while (cur != NULL) {
                        // step 4
                        if (write(fd[1], &cur->size, sizeof(cur->size)) == -1) {
                            perror("write");
                            fprintf(stderr, "Failed to write the address "\
                                            "length to the pipe.\n");
                            exit(1);
                        }
                        // step 5
                        if (write(fd[1], cur->data, cur->size) == -1) {
                            perror("write");
                            fprintf(stderr, "Failed to write the address to "\
                                            "the pipe.\n");
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
                free_transaction(cur_tx);
            }
        }

        lws_context_destroy(context);
        lwsl_user("Completed\n");

        // We also want to get here if user sends a signal
        // Close the pipe to shutdown the child process.
        if (close(fd[1]) == -1) {
            perror("close");
            exit(1);
        }
        wait(NULL);
    }
    
    // end of loop via signal?

    bloom_free(&address_bloom);
    btc_ecc_stop();

    return 0;
}