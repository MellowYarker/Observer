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

// libbloom
#include <bloom.h>


#define MAX_BUF BTC_ECKEY_PKEY_LENGTH + 1 // add a byte for the null terminator
#define PRIVATE_KEY_TYPES 2 // # of private keys we generate from a given seed

void remove_newline(char *s); // for reading from textfile of passwords
void front_pad_pkey(char *seed, char *front_pad, int len);
void back_pad_pkey(char *seed, char *back_pad, char *front_pad, int len);
void create_pubkey(char *buffer, btc_key *key, btc_pubkey *pubkey);

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stdout, "Usage: %s <# keys to generate>\n", argv[0]);
        exit(1);
    }

    clock_t start, end; // times the execution (for fun)
    start = clock();
    btc_ecc_start();

    // const btc_chainparams* chain = &btc_chainparams_main; // mainnet

    const long count = strtol(argv[1], NULL, 10); // # of seeds to use

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

    FILE *fname = fopen("common_keys.txt", "r");

    if (fname == NULL) {
        perror("fopen");
        exit(1);
    }

    for (int i = 0; i < count; i++) {
        char seed[MAX_BUF];
        char **keys = malloc(PRIVATE_KEY_TYPES * MAX_BUF); // array of priv keys
        
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
            if (exists >= 0) {
                btc_key key;
                btc_privkey_init(&key);
                btc_pubkey pubkey;
                btc_pubkey_init(&pubkey);
                create_pubkey(keys[i], &key, &pubkey);

                #ifdef DEBUG
                    size_t sizeout = 128;
                    char pubkey_hex[sizeout];
                    btc_pubkey_get_hex(&pubkey, pubkey_hex, &sizeout);

                    printf("Private key: %s\n", keys[i]);
                    printf("Public Key: %s\n", pubkey_hex);
                #endif
            } else {
                fprintf(stderr, "Bloom filter not initialized\n");
                exit(1);
            }
            if (exists == 0) {
                #ifdef DEBUG
                    printf("This is a new private key!\n");
                #endif
                // add to DB
            } else if (exists == 1) {
                #ifdef DEBUG
                    printf("This key might exist.\n");
                #endif
                false_positive_count++;
                // CHECK DB
                // if not there, add.
            }
        }
    }
    end = clock();

    printf("\nTook %f seconds to generate %ld keys\n", ((double) end - start)/CLOCKS_PER_SEC, count * 2);

    fclose(fname);
    bloom_save(&priv_bloom, "private_key_filter.b");
    printf("False Positives: %d\nFalse Positive Rate: %f\n", false_positive_count, ((double) false_positive_count / (2 * count)));

    return 0;
}

void remove_newline(char *s) {
    for (int i = 0; i < strlen(s); i++) {
        if (s[i] == '\n') {
            s[i] = '\0';
            return;
        }
    }
}

/* Fills the string front_pad with 0's then the seed like so: 0000...{seed}\0 */
void front_pad_pkey(char *seed, char *front_pad, int len) {
    // fill the buffers with 0's and the seed
    memset(front_pad, '0', (MAX_BUF - 1) - len);
    front_pad[MAX_BUF - 1 - len] = '\0';
    strncat(front_pad, seed, len); // looks like 0000...{seed}\0
}

/* Fills the string back_pad with the seed then 0's. Ex: {seed}00...000\0 */
void back_pad_pkey(char *seed, char *back_pad, char *front_pad, int len) {
    strncpy(back_pad, seed, len);
    back_pad[len] = '\0';
    strncat(back_pad, front_pad, (MAX_BUF - 1) - len);
}

void create_pubkey(char *buffer, btc_key *key, btc_pubkey *pubkey) {

    // fill the btc_key privkeys with the ascii values from our key strings.
    for (int i = 0; i < BTC_ECKEY_PKEY_LENGTH; i++) {
        key->privkey[i] = (int) buffer[i];
    }
    btc_pubkey_from_key(key, pubkey);
}
