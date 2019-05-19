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


#define MAX_BUF BTC_ECKEY_PKEY_LENGTH + 1

void remove_newline(char *s);
void create_pubkey(char *buffer, btc_key key, btc_pubkey pubkey);

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
        
        if (fgets(seed, MAX_BUF, fname) == NULL)
            break;
        remove_newline(seed);
        int len = strlen(seed);

        #ifdef DEBUG
            printf("\n\nPrivate Seed: %s\n", seed);
        #endif

        char front_pad[MAX_BUF];
        char back_pad[MAX_BUF];

        // fill the buffers with 0's and the seed
        memset(front_pad, '0', (MAX_BUF - 1) - len);
        front_pad[MAX_BUF - 1 - len] = '\0';
        strncat(front_pad, seed, len); // looks like 0000...{seed}\0

        strncpy(back_pad, seed, len);
        back_pad[len] = '\0';
        strncat(back_pad, front_pad, (MAX_BUF - 1) - len); // {seed}...000\0

        // add private keys to bloom filter
        // TODO: put these private keys in a mutable array on the heap
        //       so we can do this in a loop.
        int exists = bloom_add(&priv_bloom, front_pad, MAX_BUF - 1);
        if (exists >= 0) {
            btc_key key;
            btc_privkey_init(&key);
            btc_pubkey pubkey;
            btc_pubkey_init(&pubkey);
            create_pubkey(front_pad, key, pubkey);
        } else {
            fprintf(stderr, "filter not initialized\n");
            continue;
        }
        if (exists == 0) {
            printf("This is a new private key!\n");
            // add to DB
        } else if (exists == 1) {
            printf("This key might exist.\n");
            false_positive_count++;
            // CHECK DB
            // if not there, add.
        }
        exists = bloom_add(&priv_bloom, back_pad, MAX_BUF - 1);
        if (exists >= 0) {
            btc_key key;
            btc_privkey_init(&key);
            btc_pubkey pubkey;
            btc_pubkey_init(&pubkey);
            create_pubkey(back_pad, key, pubkey);
        } else {
            fprintf(stderr, "filter not initialized\n");
            continue;
        }
        if (exists == 0) {
            printf("This is a new private key!\n");
            // add to DB
        } else if (exists == 1) {
            printf("This key might exist.\n");
            false_positive_count++;
            // CHECK DB
            // if not there, add.
        }

        #ifdef DEBUG
            printf("\nfront padded: %s\nback padded: %s\n", front_pad, back_pad);
        #endif

        // size_t sizeout = 128;
        // // char WIF_FRONT[sizeout];
        // // char WIF_BACK[sizeout];

        // btc_key front_key, back_key;
        // btc_privkey_init(&front_key);
        // btc_privkey_init(&back_key);

        // // fill the btc_key privkeys with the ascii values from our key strings.
        // for (int i = 0; i < BTC_ECKEY_PKEY_LENGTH; i++) {
        //     front_key.privkey[i] = (int) front_pad[i];
        //     back_key.privkey[i] = (int) back_pad[i];
        // }

        // test private key
        // int temp[] = {12,40,252,163,134,199,162,39,96,11,47,229,11,124,174,17,236,134,211,191,31,190,71,27,232,152,39,225,157,114,170,29};
        // for (int i = 0; i < 32; i++) {
        //     front_key.privkey[i] = temp[i];
        //     printf("%d ", front_key.privkey[i]);
        // }

        // // if we want WIF keys
        // btc_privkey_encode_wif(&front_key, chain, WIF_FRONT, &sizeout);
        // btc_privkey_encode_wif(&back_key, chain, WIF_BACK, &sizeout);

        // #ifdef DEBUG
        //     printf("\nWIF front: %s\n", WIF_FRONT);
        //     printf("WIF back: %s\n\n", WIF_BACK);
        // #endif

        // // WIF takes longer (have to decode to 32 byte priv key anyways)
        // // save a lot of operations by skipping it.

        // char front_pubkey_hex[sizeout];
        // char back_pubkey_hex[sizeout];
        // // pubkey_from_privatekey(chain, WIF_FRONT, front_pubkey_hex, &sizeout);
        // // pubkey_from_privatekey(chain, WIF_BACK, back_pubkey_hex, &sizeout);
        // btc_pubkey front_pubkey, back_pubkey;
        // btc_pubkey_init(&front_pubkey);
        // btc_pubkey_init(&back_pubkey);

        // btc_pubkey_from_key(&front_key, &front_pubkey);
        // btc_pubkey_from_key(&back_key, &back_pubkey);

        // btc_pubkey_get_hex(&front_pubkey, front_pubkey_hex, &sizeout);
        // btc_pubkey_get_hex(&back_pubkey, back_pubkey_hex, &sizeout);

        // #ifdef DEBUG
        //     printf("\nfront Public Key: %s\nback Public Key: %s\n\n###############################################################", front_pubkey_hex, back_pubkey_hex);
        // #endif

    }
    end = clock();

    // printf("Achived a rate of: %f keys per second\n", (count * 2)/(((double) end - start)/CLOCKS_PER_SEC));
    printf("\nTook %f seconds to generate %ld keys\n", ((double) end - start)/CLOCKS_PER_SEC, count * 2);

    fclose(fname);
    bloom_save(&priv_bloom, "private_key_filter.b");
    printf("False Positives: %d\nFalse Positive Rate: %f\n", false_positive_count, ((double) false_positive_count / count) / 100);

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

void create_pubkey(char *buffer, btc_key key, btc_pubkey pubkey) {
    size_t sizeout = 128;
    btc_privkey_init(&key);
    // fill the btc_key privkeys with the ascii values from our key strings.
    for (int i = 0; i < BTC_ECKEY_PKEY_LENGTH; i++) {
        key.privkey[i] = (int) buffer[i];
    }
    char pubkey_hex[sizeout];
    btc_pubkey_init(&pubkey);
    btc_pubkey_from_key(&key, &pubkey);
    btc_pubkey_get_hex(&pubkey, pubkey_hex, &sizeout);
}
