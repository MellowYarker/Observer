#include "keys.h"
#include <string.h>

void fill_key_set(struct key_set *set, char *private, char *seed, char *p2pkh, 
                  char *p2sh_p2wpkh, char *p2wpkh) {
    strcpy(set->private, private);
    strcpy(set->seed, seed);
    strcpy(set->p2pkh, p2pkh);
    strcpy(set->p2sh_p2wpkh, p2sh_p2wpkh);
    strcpy(set->p2wpkh, p2wpkh);
}

void init_Array(struct Array *key_array, size_t size) {
    key_array->array = malloc(sizeof(key_array->array) * size);
    if (key_array->array == NULL) {
        perror("malloc");
        exit(1);
    }
    key_array->used = 0;
    key_array->size = size;
}

void push_Array(struct Array *key_array, struct key_set *set) {
    // optimal reallocation window
    if (key_array->used >= (key_array->size * 0.7)) {
        key_array->array = realloc(key_array->array, 
                                   sizeof(struct key_set *) * key_array->size * 
                                   2);
        if (key_array == NULL) {
            perror("malloc");
            exit(1);
        }
        key_array->size *= 2;
    }
    key_array->array[key_array->used++] = set; // increment used and add to the array
}

void free_Array(struct Array *key_array) {
    for (int i = 0; i < key_array->used; i++) {
        free(key_array->array[i]);
    }
    free(key_array->array);
}


int build_update_query(struct Array *update, char **query, int query_size) {
    size_t current_len = 0; // keep track of length of query string.
    for (int i = 0; i < update->used; i++) {
        char *values = sqlite3_mprintf("INSERT INTO keys VALUES ('%q', '%q', "\
                                       "'%q', '%q', '%q'); ",
                                       update->array[i]->private,
                                       update->array[i]->seed,
                                       update->array[i]->p2pkh,
                                       update->array[i]->p2sh_p2wpkh,
                                       update->array[i]->p2wpkh);
        if (values == NULL) {
            fprintf(stderr, "Could not allocate memory for insert query.");
            return 1;
        }
        // check if we need to reallocate the query
        int val_len = strlen(values);
        if (val_len + current_len >= query_size) {
            *query = realloc(*query, 2 * query_size * sizeof(char));
            if (*query == NULL) {
                perror("realloc");
                return 1;
            }
            query_size *= 2;
        }
        memcpy(*query + current_len, values, val_len + 1);
        current_len += val_len;
        sqlite3_free(values);
    }
    (*query)[current_len] = '\0';
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

    // fill the btc_key privkeys with the ascii values from buffer.
    for (int i = 0; i < BTC_ECKEY_PKEY_LENGTH; i++) {
        key->privkey[i] = (int) buffer[i];
    }
    btc_pubkey_from_key(key, pubkey);
}