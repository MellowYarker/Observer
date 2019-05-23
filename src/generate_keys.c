// libbtc
#include <base58.h>
#include <btc.h>
#include <chainparams.h>
#include <ecc.h>
#include <ecc_key.h>
#include <tool.h>

// standard C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

// libbloom
#include <bloom.h>

//sqlite3
#include <sqlite3.h>

#include "keys.h"

static int callback(void *NotUsed, int argc, char **argv, char **azColName);

int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stdout, "Usage: %s <# keys to generate> <file>\n", argv[0]);
        exit(1);
    }


    clock_t start, end; // times the execution (for fun)
    start = clock();
    btc_ecc_start();

    const btc_chainparams* chain = &btc_chainparams_main; // mainnet
    const long count = strtol(argv[1], NULL, 10); // # of seeds to use

    // array of keys to add to DB, default size is 20% of count
    struct Array update;
    init_Array(&update, ceil(2 * count * 0.2));

    // array of keys that may or may not be in DB, must check.
    // default size is 1% of count, since bloom filter has error rate of 1%
    struct Array check;
    init_Array(&check, ceil(2 * count * 0.01));

    /** Private Key Bloom Filter
     * 
     *   This bloom filter is used to check if a generated private key is in 
     *   our database yet. If it is not, we create the bitcoin addresses and 
     *   store them in the DB. Otherwise, check the DB.
     * 
     *   Note: While it would be more efficient to just check the SEED, if a new
     *         method of mutating the seed into a private key is written, then
     *         the filter will not let these new private keys be entered into
     *         the database. Therefore, we pass the private keys to the filter.
    **/
    struct bloom priv_bloom;
    int false_positive_count = 0;

    // check if the bloom filter exists
    if (access("private_key_filter.b", F_OK) != -1) {
        bloom_load(&priv_bloom, "private_key_filter.b");
    } else {
        // TODO: should consider # of elements required. Leave at 1M for now.
        bloom_init2(&priv_bloom, 1000000, 0.01);
    }

    FILE *fname = fopen(argv[2], "r");

    if (fname == NULL) {
        perror("fopen");
        exit(1);
    }

    for (int i = 0; i < count; i++) {
        char seed[MAX_BUF];

        // array of priv keys
        char **keys = malloc(PRIVATE_KEY_TYPES * sizeof(char *));

        if (keys == NULL) {
            perror("malloc");
            exit(1);
        }
        
        if (fgets(seed, MAX_BUF, fname) == NULL)
            break;
        remove_newline(seed);
        int len = strlen(seed);

        #ifdef DEBUG
            printf("\n\nPrivate Seed: %s\n", seed);
        #endif

        char front_pad[MAX_BUF];
        char back_pad[MAX_BUF];

        // private key types
        front_pad_pkey(seed, front_pad, len);
        keys[0] = front_pad; // TODO: func that generates and adds keys to array
        back_pad_pkey(seed, back_pad, front_pad, len);
        keys[1] = back_pad;

        // add private keys to bloom filter
        for (int i = 0; i < PRIVATE_KEY_TYPES; i++) {
            int exists = bloom_add(&priv_bloom, keys[i], MAX_BUF - 1);
            if (exists < 0) {
                fprintf(stderr, "Bloom filter not initialized\n");
                exit(1);
            }
            size_t sizeout = 128;

            btc_key key; // private key
            btc_pubkey pubkey;

            // address types
            char address_p2pkh[sizeout];
            char address_p2sh_p2wpkh[sizeout];
            char address_p2wpkh[sizeout];

            btc_privkey_init(&key);
            btc_pubkey_init(&pubkey);
            create_pubkey(keys[i], &key, &pubkey); // fills priv & pub keys

            btc_pubkey_getaddr_p2pkh(&pubkey, chain, address_p2pkh);
            btc_pubkey_getaddr_p2sh_p2wpkh(&pubkey, chain, address_p2sh_p2wpkh);
            btc_pubkey_getaddr_p2wpkh(&pubkey, chain, address_p2wpkh);

            // TODO: likely wasting a lot of space with seed, try to
            // dynamically change the size later.

            struct key_set *set = malloc(sizeof(struct key_set));
            if (set == NULL) {
                perror("malloc");
                exit(1);
            }
            fill_key_set(set, keys[i], seed, address_p2pkh,
                            address_p2sh_p2wpkh, address_p2wpkh);

            #ifdef DEBUG
                char pubkey_hex[sizeout];
                btc_pubkey_get_hex(&pubkey, pubkey_hex, &sizeout);

                printf("\nPrivate key: %s\n", keys[i]);
                printf("Public Key: %s\n", pubkey_hex);
                printf("P2PKH: %s\n", address_p2pkh);
                printf("P2SH: %s\n", address_p2sh_p2wpkh);
                printf("P2WPKH: %s\n", address_p2wpkh);
            #endif

            if (exists == 0) {
                #ifdef DEBUG
                    printf("New private key. Adding to update set.\n");
                #endif
                push_Array(&update, set); // add to update set
            } else if (exists == 1) {
                #ifdef DEBUG
                    printf("This key might exist. Adding to check set.\n");
                #endif
                false_positive_count++;
                push_Array(&check, set); // add to check set
            }
        }
    }
    end = clock();

    printf("\nTook %f seconds to generate %ld keys\n", ((double) end - start)/CLOCKS_PER_SEC, count * 2);

    fclose(fname);
    bloom_save(&priv_bloom, "private_key_filter.b");
    printf("False Positives: %d\nFalse Positive Rate: %f\n", false_positive_count, ((double) false_positive_count / (2 * count)));

    // create the sql queries
    // TODO: do the check query first so we can add them to update list before
    int query_len = 500; // assume one statement is ~500 bytes (can realloc)

    // size of a query * number of queries needed
    char *update_sql_query = malloc(sizeof(char) * query_len * update.used);
    if (update_sql_query == NULL) {
        perror("malloc");
        exit(1);
    }

    if (build_update_query(&update, &update_sql_query, query_len) == 1) {
        exit(1);
    }
    end = clock();

    printf("Took %f seconds to build the query!\n", ((double) end - start)/CLOCKS_PER_SEC);


    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open("../db/Observer.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    rc = sqlite3_exec(db, update_sql_query, callback, 0, &zErrMsg);
    free(update_sql_query);

    if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }


    // TODO put the check query first so we can write everything once.
    // char *check_sql_query = malloc(sizeof(char) * query_len); // 500 bytes to start
    // // check query
    // for (int i = 0; i < check.used; i++) {
    //     char *values = sqlite3_mprintf("SELECT privkey FROM keys WHERE "\
    //                                    "privkey='%q'; ",
    //                                    check.array[i]->private);
    //     if (values == NULL) {
    //         fprintf(stderr, "Could not allocate memory for insert query.");
    //         exit(1);
    //     }
    //     // check if we need to reallocate the query
    //     if (strlen(values) + strlen(check_sql_query) >= query_len - 1) {
    //         check_sql_query = realloc(check_sql_query,
    //                                    2 * query_len * sizeof(char));
    //         query_len *= 2;
    //     }
    //     strcat(check_sql_query, values);
    //     sqlite3_free(values);
    // }
    sqlite3_close(db);

    end = clock();
    printf("Took %f seconds to complete!\n", ((double) end - start)/CLOCKS_PER_SEC);

    free_Array(&update);
    free_Array(&check);

    return 0;
}

// TODO rewrite CALLBACK function for our purposes
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
   int i;
   for(i = 0; i<argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
   }
   printf("\n");
   return 0;
}