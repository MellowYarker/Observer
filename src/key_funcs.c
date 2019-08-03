#include "keys.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
    #include <sys/types.h>
    #include <sys/wait.h>
#endif


const priv_func_ptr priv_gen_functions[PRIVATE_KEY_TYPES] = { &front_pad_pkey,
                                                              &back_pad_pkey,
                                                              &sha256_pkey/*,
                                                              &your_method */};

int sort_seeds(char *orig, char *sorted) {
    int r = fork();

    if (r < 0) {
        perror("fork");
        return 1;
    } else if (r == 0) {
        int f = open(sorted, O_CREAT | O_WRONLY);

        dup2(f, STDOUT_FILENO); // sort's output will write to original file
        execl("/usr/bin/sort", "sort", "-u", orig, NULL);
        exit(1);
    } else {
        int status;

        wait(&status); // wait for sorting to finish
        if (WEXITSTATUS(status) == 1) {
            printf("Failed to sort seeds. Exiting.\n");
            return 1;
        } else {
            printf("Sorted seed set stored in %s.\n", sorted);
        }
        return 0;
    }
}


size_t get_record_count(sqlite3 *db) {
    char *query = "SELECT count() FROM keys;";
    sqlite3_stmt *stmt;
    int records = 0;

    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("error: %s", sqlite3_errmsg(db));
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        records = sqlite3_column_int64 (stmt, 0);
    }
    if (rc != SQLITE_DONE) {
        printf("error: %s", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_finalize(stmt);
    return records;
}


int resize_private_bloom(struct bloom *filter, sqlite3 *db, unsigned long count)
{
    /*  1. Reset this bloom filter.
        2. Read every record from db and write all priv keys to new BF.
    */
    size_t old = filter->entries;
    bloom_reset(filter);

    // TODO: I don't like depending on count, but we need to right now
    bloom_init2(filter, (old * 2) + count, 0.01);
    sqlite3_stmt *stmt;

    char *query = "SELECT privkey FROM keys;";
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        printf("error: %s", sqlite3_errmsg(db));
        return -1;
    }

    char private[MAX_BUF];
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        strcpy(private, (char *) sqlite3_column_text (stmt, 0));

        if (bloom_add(filter, private, strlen(private)) < 0) {
            fprintf(stderr, "Bloom filter not initialized\n");
            return 1;
        }
    }
    sqlite3_finalize(stmt);
    return 0;
}

int resize_address_bloom(struct bloom *filter, sqlite3 *db, unsigned long count)
{
    /*  1. Reset this bloom filter.
        2. Read every record from db an dwrite all addresses to new BF.
    */
    size_t old = filter->entries;
    bloom_reset(filter);

    // TODO: I don't like depending on count, but we need to right now.
    bloom_init2(filter, (old * 2) + count, 0.01);
    sqlite3_stmt *stmt;

    char *query = "SELECT P2PKH, P2SH, P2WPKH FROM keys;";
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        printf("error: %s", sqlite3_errmsg(db));
        return -1;
    }

    char p2pkh[SIZEOUT];
    char p2sh_p2wpkh[SIZEOUT];
    char p2wpkh[SIZEOUT];

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        strcpy(p2pkh, (char *) sqlite3_column_text (stmt, 0));
        strcpy(p2sh_p2wpkh, (char *) sqlite3_column_text (stmt, 1));
        strcpy(p2wpkh, (char *) sqlite3_column_text (stmt, 2));

        if (bloom_add(filter, p2pkh, strlen(p2pkh)) < 0 ||
            bloom_add(filter, p2sh_p2wpkh, strlen(p2sh_p2wpkh)) < 0 ||
            bloom_add(filter, p2wpkh, strlen(p2wpkh)) < 0) {
            fprintf(stderr, "bloom filter not initialized\n");
            return 1;
        }
    }
    sqlite3_finalize(stmt);
    return 0;
}


int fill_key_set(struct key_set *set, char *private, char *seed, char *p2pkh,
                  char *p2sh_p2wpkh, char *p2wpkh) {
    set->seed = malloc(sizeof(char) * strlen(seed) + 1);
    if (set->seed == NULL) {
        perror("malloc");
        return 1;
    }
    strcpy(set->private, private);
    strcpy(set->seed, seed);
    strcpy(set->p2pkh, p2pkh);
    strcpy(set->p2sh_p2wpkh, p2sh_p2wpkh);
    strcpy(set->p2wpkh, p2wpkh);
    return 0;
}


int compare_key_sets_privkey(const void *p1, const void *p2){
    struct key_set *a = *(struct key_set **) p1;
    struct key_set *b = *(struct key_set **) p2;
    return (strcmp(a->private, b->private));
}


int init_Array(struct Array *key_array, size_t size) {
    key_array->array = malloc(sizeof(key_array->array) * size);
    if (key_array->array == NULL) {
        perror("malloc");
        return 1;
    }
    key_array->used = 0;
    key_array->size = size;
    return 0;
}


void push_Array(struct Array *key_array, struct key_set *set) {
    // optimal reallocation window
    if (key_array->used >= (key_array->size * 0.7)) {
        key_array->array = realloc(key_array->array, 
                                   sizeof(struct key_set *) * key_array->size * 
                                   2);
        if (key_array == NULL) {
            perror("realloc");
            exit(1);
        }
        key_array->size *= 2;
    }
    key_array->array[key_array->used++] = set; // increment used, add to array
}


void push_Difference(struct Array *a, struct Array *b, struct Array *dest) {
    size_t current_index = 0; // last index we searched to
    size_t count = 0;

    // sort both Arrays
    qsort(a->array, a->used, sizeof(struct key_set *), compare_key_sets_privkey);
    qsort(b->array, b->used, sizeof(struct key_set *), compare_key_sets_privkey);

    for (int i = 0; i < b->used; i++) {
        // no more elements to search for
        if (count == b->used - a->used) {
            break;
        } else if (current_index == a->used - 1 || a->used == 0) {
            // add all remaining because they're all greater than max in Array a
            push_Array(dest, b->array[i]);
            continue;
        }

        int comp;
        for (int j = current_index; j < a->used; j++) {
            // exists in db, we don't want it
            if ((comp = compare_key_sets_privkey(&(b->array[i]),
                                                 &(a->array[j]))) == 0) {
                current_index = j;
                break;
            } else if (comp < 0) {
                // not in database, save it
                current_index = j;
                push_Array(dest, b->array[i]);
                count++;
                break;
            }
        }
    }
}


void free_Array(struct Array *key_array) {
    for (int i = 0; i < key_array->used; i++) {
        free(key_array->array[i]->seed);
        free(key_array->array[i]);
    }
    free(key_array->array);
}


int remove_duplicates(struct Array *src, struct Array *dest) {
    if (init_Array(dest, (size_t) src->used * 0.5) == 2) {
        return 1;
    }

    for (int i = 0; i < src->used; i++) {
        // last element
        if (i == src->used - 1) {
            push_Array(dest, src->array[i]);
        } else if (strcmp(src->array[i]->private, src->array[i + 1]->private)
                   != 0) {
            // add to dest if this element is unique
            push_Array(dest, src->array[i]);
        }
    }
    return 0;
}


void start_tx(char **query,  size_t *current_len) {
    char *begin = "BEGIN; ";
    strcpy(*query, begin);
    *current_len += strlen(begin);
}

void end_tx(char **query, size_t *current_len) {
    char *commit = "COMMIT;";
    strcat(*query + *current_len, commit);
    *current_len += strlen(commit);
    (*query)[*current_len] = '\0';
}


int resize_check(char *value, char **query, size_t *current_len, int *q_size) {
    int val_len = strlen(value);
    if (val_len + *current_len >= *q_size) {
        *query = realloc(*query, 2 * (*q_size) * sizeof(char));
        if (*query == NULL) {
            perror("realloc");
            return 1;
        }
        (*q_size) *= 2;
    }
    memcpy(*query + (*current_len), value, val_len + 1);
    *current_len += val_len;

    return 0;
}


int prepare_query(struct Array *arr, char **query, int query_size, int type) {
    *query = malloc(sizeof(char) * query_size * arr->used);
    query_size *= arr->used; // current size of the query

    if (*query == NULL) {
        perror("malloc");
        return 1;
    }

    if (type == CHECK) {
        if (build_check_query(arr, query, query_size) == 1) {
            return 1;
        }
    } else if (type == UPDATE) {
        if (build_update_query(arr, query, query_size) == 1) {
            return 1;
        }
    }
    return 0;
}


int build_update_query(struct Array *update, char **query, int query_size) {
    size_t current_len = 0; // keep track of length of query string.
    start_tx(query, &current_len);

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
        // reallocate query if necessary
        if (resize_check(values, query, &current_len, &query_size) == 1) {
            return 1;
        }
        sqlite3_free(values);
    }

    end_tx(query, &current_len);
    return 0;
}


int build_check_query(struct Array *check, char **query, int query_size) {
    size_t current_len = 0;
    start_tx(query, &current_len);

    for (int i = 0; i < check->used; i++) {
        char *values = sqlite3_mprintf("SELECT * FROM keys WHERE privkey='%q'; ",
                                       check->array[i]->private);

        if (values == NULL) {
                fprintf(stderr, "Could not allocate memory for check query.");
                return 1;
        }
        // reallocate query if necessary
        if (resize_check(values, query, &current_len, &query_size) == 1) {
            return 1;
        }
        sqlite3_free(values);
    }
    end_tx(query, &current_len);
    return 0;
}


int callback(void *arr, int argc, char **argv, char **columns) {
    // TODO: we could just store the private key, not the whole key set.
    // add this key_set to our in_db array
    struct key_set *keys = malloc(sizeof(struct key_set));
    fill_key_set(keys, argv[0], argv[1], argv[2], argv[3], argv[4]);
    push_Array(arr, keys);
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


/* To add or remove methods for turning a seed into a private key:
    1. Increment/decrement PRIVATE_KEY_TYPES
    2. Write a function that does the conversion (ex. front_pad_pkey)
        i. note, prototype must adhere to priv_func_ptr
    3. Add it to priv_gen_functions (seed_to_priv will use it automatically)
*/
char **seed_to_priv(char *seed, int len) {
    char **arr = malloc(sizeof(char *) * PRIVATE_KEY_TYPES);
    if (arr == NULL) {
        perror("malloc");
        return NULL;
    }

    for (int i = 0; i < PRIVATE_KEY_TYPES; i++) {
        arr[i] = malloc(sizeof(char) * MAX_BUF);
        if (arr[i] == NULL) {
            perror("malloc");
            return NULL;
        }
        priv_gen_functions[i] (seed, arr[i], len);
    }
    return arr;
}


void front_pad_pkey(char *seed, char *buf, int len) {
    memset(buf, '0', (MAX_BUF - 1) - len);
    buf[MAX_BUF - 1 - len] = '\0';
    strncat(buf, seed, len);
}


void back_pad_pkey(char *seed, char *buf, int len) {
    strncpy(buf, seed, len);
    memset(buf + len, '0', (MAX_BUF - 1) - len);
    buf[MAX_BUF - 1] = '\0';
}


void sha256_pkey(char *seed, char *buf, int len) {
    uint256 bin;
    // populates bin with 256 bit hash of seed
    sha256_Raw((const unsigned char *) seed, len, bin);

    // translate 256 bit bin array to 64 char hex and save the first 32 chars
    strncpy(buf, utils_uint8_to_hex((const uint8_t*) bin,
            BTC_ECKEY_PKEY_LENGTH), BTC_ECKEY_PKEY_LENGTH);
    buf[MAX_BUF - 1] = '\0';
}


void create_pubkey(char *buffer, btc_key *key, btc_pubkey *pubkey) {
    // fill the btc_key privkeys with the ascii values from buffer.
    for (int i = 0; i < BTC_ECKEY_PKEY_LENGTH; i++) {
        key->privkey[i] = (int) buffer[i];
    }
    btc_pubkey_from_key(key, pubkey);
}