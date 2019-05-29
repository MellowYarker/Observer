#include "keys.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>


const priv_func_ptr priv_gen_functions[PRIVATE_KEY_TYPES] = { &front_pad_pkey,
                                                              &back_pad_pkey/*,
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
    key_array->array[key_array->used++] = set; // increment used, add to array
}


void push_Difference(struct Array *a, struct Array *b, struct Array *dest) {
    size_t count = 0;
    for (int i = 0; i < b->used; i++) {
        // no more records need to be checked
        if (count == b->used - a->used) {
            break;
        }
        int found = 0;
        for (int j = 0; j < a->used; j++) {
            // compare private keys
            if (strcmp(b->array[i]->private, a->array[j]->private) == 0) {
                found = 1;
                break;
            }
        }
        if (found == 0) {
            push_Array(dest, b->array[i]);
            count++;
        }
    }
}


void free_Array(struct Array *key_array) {
    for (int i = 0; i < key_array->used; i++) {
        free(key_array->array[i]);
    }
    free(key_array->array);
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
    sqlite3_free(value);
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


void front_pad_pkey(char *seed, char *front_pad, int len) {
    memset(front_pad, '0', (MAX_BUF - 1) - len);
    front_pad[MAX_BUF - 1 - len] = '\0';
    strncat(front_pad, seed, len);
}


void back_pad_pkey(char *seed, char *back_pad, int len) {
    strncpy(back_pad, seed, len);
    memset(back_pad + len, '0', (MAX_BUF - 1) - len);
    back_pad[MAX_BUF - 1] = '\0';
}


void create_pubkey(char *buffer, btc_key *key, btc_pubkey *pubkey) {
    // fill the btc_key privkeys with the ascii values from buffer.
    for (int i = 0; i < BTC_ECKEY_PKEY_LENGTH; i++) {
        key->privkey[i] = (int) buffer[i];
    }
    btc_pubkey_from_key(key, pubkey);
}