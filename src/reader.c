#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>

#include <bloom.h>

#include <libwebsockets.h>

#ifdef __linux__
    #include <sys/types.h>
    #include <sys/wait.h>
#endif

#include "reader.h"

int interrupted = 0; // becomes 1 if we receive SIGINT

void signal_handler(int signum) {
    interrupted = 1;
}

int main() {
    // set up the pipe, data flows from parent to child.
    int fd[2];
    pipe(fd);

    int r = fork();
    if (r < 0) {
        perror("fork");
        exit(1);
    } else if (r == 0) {

        /*  Child process reads positive outputs from the pipe and checks if
            the address's corresponding private key is in our database.
        */

        if (close(fd[1]) == -1) {
            perror("close");
            exit(1);
        }

        signal(SIGINT, SIG_IGN); //ignore sigint, parent will close pipe instead

        // set up db connection, we do not modify db, so concurrency is safe
        sqlite3 *db;
        char *zErrMsg = 0;
        int rc;

        rc = sqlite3_open("../db/observer.db", &db);
        if (rc) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            exit(1);
        }
        struct output **outputs; // array of pointers to output structs

        int ntxOut = 0; // the number of output addresses
        int response;

        // begin (blocking) loop of reading from the pipe
        // Note: Check the pipe protocol in the parent!
        while ((response = read(fd[0], &ntxOut, sizeof(int))) != 0) {
            if (response == -1) {
                perror("read");
                exit(1);
            }

            outputs = malloc(sizeof(struct output *) * ntxOut);

            if (outputs == NULL) {
                perror("malloc");
                exit(1);
            }

            int addr_size; // # of bytes of incoming address
            int total_addr_size_sum = 0; // sum of length of addresses read

            for (int i = 0; i < ntxOut; i++) {
                int addr_size = 0;
                int script_size = 0;

                struct output *out = malloc(sizeof(struct output));

                if (out == NULL) {
                    fprintf(stderr, "Couldn't allocate space for output.\n");
                    exit(1);
                }
                printf("Will check %d record(s).\n", ntxOut);

                outputs[i] = out; // assign this output to the array

                // 2. address size
                if ((response = read(fd[0], &addr_size, sizeof(int))) == -1) {
                    perror("read");
                    exit(1);
                } else if (response == 0) {
                    fprintf(stdout, "Parent closed the pipe.\n");
                    for (int addr = 0; addr < i; addr++) {
                        free(outputs[addr]->address);
                        free(outputs[addr]->script);
                        free(outputs[addr]);
                    }
                    free(outputs);
                    break;
                }
                // increment but don't count null terminator
                total_addr_size_sum = total_addr_size_sum + addr_size - 1;

                out->address = malloc(sizeof(char) * addr_size);
                if (out->address == NULL) {
                    perror("malloc");
                    exit(1);
                }

                // 3. address
                if ((response = read(fd[0], out->address, addr_size)) == -1) {
                    perror("read");
                    exit(1);
                } else if (response == 0) {
                    fprintf(stdout, "Parent closed the pipe.\n");
                    if (i == 0) {
                        free(outputs[i]->address);
                        free(outputs[i]);
                    } else if (i > 0) {
                        for (int addr = 0; addr < i; addr++) {
                            free(outputs[addr]->address);
                            free(outputs[addr]->script);
                            free(outputs[addr]);
                        }
                    }
                    free(outputs);
                    break;
                }
                printf("Received %s from parent.\n", outputs[i]->address);

                // 4. value
                if ((response = read(fd[0], &(out->value), sizeof(unsigned int)))
                    == -1) {
                    perror("read");
                    exit(1);
                } else if (response == 0) {
                    if (i == 0) {
                        free(outputs[i]->address);
                        free(outputs[i]);
                    } else if (i > 0) {
                        for (int addr = 0; addr < i; addr++) {
                            free(outputs[addr]->address);
                            free(outputs[addr]->script);
                            free(outputs[addr]);
                        }
                    }
                    free(outputs);
                    break;
                }
                printf("Value: %lu sats.\n", outputs[i]->value);

                // 5. script length
                if ((response = read(fd[0], &script_size, sizeof(int)))== -1) {
                    perror("read");
                    exit(1);
                } else if (response == 0) {
                    if (i == 0) {
                        free(outputs[i]->address);
                        free(outputs[i]);
                    } else if (i > 0) {
                        for (int addr = 0; addr < i; addr++) {
                            free(outputs[addr]->address);
                            free(outputs[addr]->script);
                            free(outputs[addr]);
                        }
                    }
                    free(outputs);
                    break;
                }
                out->script = malloc(sizeof(char) * script_size);
                if (out->script == NULL) {
                    perror("malloc");
                    exit(1);
                }
                // 6. script
                if ((response = read(fd[0], out->script, script_size)) == -1) {
                    perror("read");
                    exit(1);
                } else if (response == 0) {
                    for (int addr = 0; addr < i; addr++) {
                        free(outputs[addr]->address);
                        free(outputs[addr]->script);
                        free(outputs[addr]);
                    }
                    free(outputs);
                    break;
                }
                printf("With script: %s.\n", outputs[i]->script);
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
                if (strncmp(outputs[i]->address, "1", 1) == 0) {
                    query = sqlite3_mprintf(format, "P2PKH", "P2PKH",
                                                outputs[i]->address);
                } else if (strncmp(outputs[i]->address, "3", 1) == 0) {
                    query = sqlite3_mprintf(format, "P2SH", "P2SH",
                                            outputs[i]->address);
                } else {
                    query = sqlite3_mprintf(format, "P2WPKH", "P2WPKH",
                                                outputs[i]->address);
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
            } else {
                printf("This transaction contained no spendable outputs.\n");
            }

            // clean up and get ready to read the next transaction

            // free all the addresses we saved
            for (int j = 0; j < ntxOut; j++) {
                free(outputs[j]->address);
                free(outputs[j]->script);
                free(outputs[j]);
            }

            free(outputs);
        }

        // parent process has closed the pipe, begin shutdown
        sqlite3_close(db);
        if (close(fd[0]) == -1) {
            perror("close");
            exit(1);
        }

        exit(0);
    } else {
        // parent process

        /*  The parent process's job is to get new transactions and check them
            against the bloom filter. If we get any positive responses, write
            the tx to the pipe.

            This allows the parent process to avoid performing disk IO, allowing
            it to quickly read the next transaction.*/

        if (close(fd[0]) == -1) {
            perror("close");
            exit(1);
        }

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

        // some final counts to show the user
        int total_transactions_checked = 0;
        int total_addresses_checked = 0;
        int positive_hit_count = 0;

        // Create WebSocket connection with Blockchain.com
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

        // start handling sigint here
        struct sigaction sa;
        sa.sa_handler = signal_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            fprintf(stderr, "Something went wrong setting up signal handler\n");
            exit(1);
        }

        // loop where we handle messages from the server
        while (n >= 0 && !interrupted) {
            n = lws_service(context, 1000); // read from the server
            // tranasction_buf and transaction_size are declared in socket.c
            if (transaction_buf != NULL) {
                /* 4 cases:
                    1. Partial write & empty buffer
                    3. Complete write & non-empty buffer (...)
                    2. Partial write & non-empty buffer (prev msg was partial)
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
                    // buffer[buffer_size] = '\0';

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
                free(transaction_buf);
                transaction_buf = NULL; // otherwise we will enter this if block
                transaction_size = 0;

                memset(buffer, '\0', buffer_size - 1);
                free(buffer);
                buffer = NULL;
                buffer_size = 0;

                if (cur_tx == NULL) {
                    fprintf(stderr, "Had to discard some partial transactions."\
                                    "\nThis usually occurs when the order"\
                                    " of partial reads is lost.\n");
                    continue;
                }

                // linked list of addresses that came back as "positive" from BF
                struct node *positive_address_head = NULL;
                int list_size = 0;

                // loop over outputs
                for (int i = 0; i < cur_tx->nOutputs; i++) {
                    // check if we own the output address
                    if (bloom_check(&address_bloom, cur_tx->outputs[i]->address,
                                    strlen(cur_tx->outputs[i]->address)) == 1) {
                        printf("\n********************Positive hit************"\
                               "********\n");
                        positive_hit_count++;
                        cur_tx->outputs[i]->positive = 1; // Will send to child
                        list_size++; // increment number of elements in the LL
                    }
                }
                total_transactions_checked++;
                total_addresses_checked += cur_tx->nOutputs;
                // write to pipe if we have found potentially spendable addrs
                if (list_size > 0) {
                    printf("May have found spendable outputs. Checking "\
                            "database.\n");
                    /* Pipe Protocol:
                        1. Send the number of outputs: (int)
                        2. Send the size of the current output address: (int)
                        3. Send the output address: ^size^
                        4. Send the value: sizeof(unsigned int)
                        5. Send the size of the script: sizeof(int)
                        6. Send the script: ^size^
                    */
                    // step 1. (new)
                    if (write(fd[1], &list_size, sizeof(list_size)) == -1) {
                        perror("write");
                        fprintf(stderr, "Failed to write the number of outputs"\
                                        "to the pipe.\n");
                        exit(1);
                    }

                    // (new)
                    for (int i = 0; i < cur_tx->nOutputs; i++) {
                        // write the positive output
                        if (cur_tx->outputs[i]->positive) {
                            int addr_size =
                                strlen(cur_tx->outputs[i]->address) + 1;

                            // step 2 (new)
                            if (write(fd[1], &addr_size, sizeof(int)) == -1) {
                                perror("write");
                                fprintf(stderr, "Failed to write the address "\
                                                "length to the pipe.\n");
                                exit(1);
                            }

                            // step 3 (new)
                            if (write(fd[1], cur_tx->outputs[i]->address,
                                      addr_size) == -1) {
                                perror("write");
                                fprintf(stderr, "Failed to write the address "\
                                                "to the pipe.\n");
                                exit(1);
                            }

                            // step 4 (new)
                            if (write(fd[1], &(cur_tx->outputs[i]->value),
                                      sizeof(unsigned int)) == -1) {
                                perror("write");
                                fprintf(stderr, "Failed to write the output's"\
                                                " value to the pipe.\n");
                                exit(1);
                            }

                            int script_size =
                                strlen(cur_tx->outputs[i]->script) + 1;

                            // step 5 (new)
                            if (write(fd[1], &script_size, sizeof(int)) == -1) {
                                perror("write");
                                fprintf(stderr, "Failed to write the script "\
                                                "length to the pipe.\n");
                                exit(1);
                            }

                            // step 6 (new)
                            if (write(fd[1], cur_tx->outputs[i]->script,
                                      script_size) == -1) {
                                perror("write");
                                fprintf(stderr, "Failed to write the script to"\
                                                " the pipe.\n");
                                exit(1);
                            }
                            list_size--;
                        }
                    }
                }
                free_transaction(cur_tx);
            }
        }

        lws_context_destroy(context);
        lwsl_user("Connection closed.\n");

        // Close the pipe to shutdown the child process.
        if (close(fd[1]) == -1) {
            perror("close");
            exit(1);
        }

        int status;
        wait(&status);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0){
            printf("Received and checked:\n\t%d Transactions\n\t%d Addresses\n",
                    total_transactions_checked, total_addresses_checked);
            printf("Positive hit count: %d\n", positive_hit_count);
        } else {
            printf("Something went wrong in the child process. Exiting.\n");
        }
        bloom_free(&address_bloom);
    }

    printf("Finished cleaning up. Exiting.\n");

    return 0;
}
