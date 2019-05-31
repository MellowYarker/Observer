// libbtc
#include <btc.h>
#include <chainparams.h>
#include <ecc.h>
#include <ecc_key.h>

// standard C
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// libbloom
#include <bloom.h>

//sqlite3
#include <sqlite3.h>

#include "keys.h"


int main(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stdout, "Usage: %s <# keys to generate> <file>\n", argv[0]);
        exit(1);
    }


    clock_t start, end; // times the execution
    start = clock();
    btc_ecc_start();

    // new sorted filename
    char *temp = "sorted_";
    char sorted[strlen(temp) + strlen(argv[2]) + 1];
    strcpy(sorted, temp);
    strcat(sorted, argv[2]);

    if (sort_seeds(argv[2], sorted) == 1) {
        exit(1);
    }

    const btc_chainparams* chain = &btc_chainparams_main; // mainnet
    const unsigned long count = strtol(argv[1], NULL, 10); // # of seeds to use
    const unsigned long generated = count * PRIVATE_KEY_TYPES;

    // array of keys to add to DB, default size is 20% of generated priv keys
    struct Array update;

    if (init_Array(&update, ceil(generated * 0.2)) == 1) {
        exit(1);
    }

    // array of keys that may or may not be in DB, must check. Default size is
    // 1% of generated priv keys, since the bloom filter has error rate of 1%
    struct Array check;
    if (init_Array(&check, ceil(generated * 0.01)) == 1) {
        exit(1);
    }

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

        // check if the bloom filter needs to be resized
        sqlite3 *db;
        int rc = sqlite3_open("../db/Observer.db", &db);
        if (rc) {
            fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
            exit(1);
        }

        size_t records = get_record_count(db);
        if (records == -1) {
            exit(1);
        }

        // resize if we're at 80% of the expected entries
        if (records >= priv_bloom.entries * 0.8) {
            printf("\nResizing bloom filter!\n");
            if (resize_private_bloom(&priv_bloom, db, count) == 1) {
                exit(1);
            }
            printf("Finished resizing bloom filter.\n");
        }

        sqlite3_close(db);

    } else {
        // TODO: should consider # of elements required. Leave at 1M for now.
        bloom_init2(&priv_bloom, 1000000, 0.01);
    }

    FILE *fname = fopen(sorted, "r");

    if (fname == NULL) {
        perror("fopen");
        exit(1);
    }

    printf("Generating %d private keys per seed...\n", PRIVATE_KEY_TYPES);
    for (int i = 0; i < count; i++) {
        char seed[MAX_BUF];
        
        if (fgets(seed, MAX_BUF, fname) == NULL)
            break;
        remove_newline(seed);
        int len = strlen(seed);

        #ifdef DEBUG
            printf("\n\nPrivate Seed: %s\n", seed);
        #endif

        char **keys = seed_to_priv(seed, len); // array of private keys

        // add private keys to bloom filter
        for (int j = 0; j < PRIVATE_KEY_TYPES; j++) {
            int exists = bloom_add(&priv_bloom, keys[j], MAX_BUF - 1);
            if (exists < 0) {
                fprintf(stderr, "Bloom filter not initialized\n");
                exit(1);
            }
            size_t sizeout = 128;

            btc_key key; // private key struct (for libbtc)
            btc_pubkey pubkey;

            // address types
            char address_p2pkh[sizeout];
            char address_p2sh_p2wpkh[sizeout];
            char address_p2wpkh[sizeout];

            btc_privkey_init(&key);
            btc_pubkey_init(&pubkey);
            create_pubkey(keys[j], &key, &pubkey); // fills priv & pub keys

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
            fill_key_set(set, keys[j], seed, address_p2pkh,
                            address_p2sh_p2wpkh, address_p2wpkh);

            #ifdef DEBUG
                char pubkey_hex[sizeout];
                btc_pubkey_get_hex(&pubkey, pubkey_hex, &sizeout);

                printf("\nPrivate key: %s\n", keys[j]);
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
            free(keys[j]);
        }
        free(keys);
    }
    end = clock();

    printf("\nTook %f seconds to generate %ld key sets.\n",
           ((double) end - start)/CLOCKS_PER_SEC, generated);

    fclose(fname);
    bloom_save(&priv_bloom, "private_key_filter.b");
    bloom_free(&priv_bloom);
    printf("Bloom filter caught %d records.\n", false_positive_count);

    // create the sql queries
    int check_len = 75; // check statement ~75 bytes
    int update_len = 240; // update statement ~240 bytes

    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open("../db/Observer.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    }

    // update database
    char *update_sql_query;
    if (update.used > 0) {
        if (prepare_query(&update, &update_sql_query, update_len, UPDATE) == 1){
            fprintf(stderr, "Failed to build query\n");
            exit(1);
        }

        rc = sqlite3_exec(db, update_sql_query, callback, 0, &zErrMsg);
        free(update_sql_query);

        if (rc != SQLITE_OK ) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            printf("Wrote %zu records to the keys table.\n", update.used);
        }
    }

    free_Array(&update); // no longer need these records
    // array of records caught by bloom filter but not in db
    struct Array candidates;
    if (init_Array(&candidates, ceil(0.5 * check.used) == 1) {
        exit(1);
    }

    // check database for records
    char *check_sql_query;
    if (check.used > 0) {
        if (prepare_query(&check, &check_sql_query, check_len, CHECK) == 1) {
            fprintf(stderr, "Failed to build query\n");
            exit(1);
        }

        struct Array exists; // array of elements that were found in the db
        if (init_Array(&exists, ceil(2 * check.used * 0.5)) == 1) {
            exit(1);
        }

        rc = sqlite3_exec(db, check_sql_query, callback, &exists, &zErrMsg);
        if (rc != SQLITE_OK ) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        printf("\n%zu of the %d records caught by the bloom filter were already"\
        " stored in the database.\n", exists.used, false_positive_count);
        free(check_sql_query);
        push_Difference(&exists, &check, &candidates);
        // see the wiki for details on freeing Array structs.
        free_Array(&exists); // <- has references to new key_sets, a valid free
    }

    // add records that had to be checked (if there are any)
    if (candidates.used > 0) {
        // Sort the candidates array then remove duplicates
        qsort(candidates.array, candidates.used, sizeof(struct key_set *),
              compare_key_sets_privkey);

        if (remove_duplicates(&candidates, &update) == 1) {
            fprintf(stderr, "Something went wrong while removing duplicate "\
            "records from the candidate array.\n");
            exit(1);
        }

        if (prepare_query(&update, &update_sql_query, update_len, UPDATE) == 1){
            fprintf(stderr, "Failed to build query\n");
            exit(1);
        }
        // This will free check, candidates, and update.
        // This is because both candidates and update contain the key sets that
        // are stored in check, free_Array frees all elements inside the Array.
        free_Array(&check);
        free(candidates.array);
        free(update.array);

        rc = sqlite3_exec(db, update_sql_query, callback, 0, &zErrMsg);
        free(update_sql_query);

        if (rc != SQLITE_OK ) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            printf("Wrote an additional %zu records to the keys table.\n",
                   update.used);
        }
    }

    sqlite3_close(db);
    end = clock();
    printf("\nTook %f seconds.\n", ((double) end - start)/CLOCKS_PER_SEC);

    return 0;
}