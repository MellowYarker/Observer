#include <btc.h>
#include <ecc_key.h>
#include <sha2.h>
#include <utils.h>


#include <bloom.h>

#include <sqlite3.h>

#define SIZEOUT 128
#define MAX_BUF BTC_ECKEY_PKEY_LENGTH + 1 // add a byte for the null terminator
#define PRIVATE_KEY_TYPES 3 // # of private keys we generate from a given seed
#define UPDATE 0
#define CHECK 1

/*  A helpful struct that stores information about the collection of data we
    gain from a seed.
*/
struct key_set {
    char private[MAX_BUF];
    char *seed;
    char p2pkh[SIZEOUT];
    char p2sh_p2wpkh[SIZEOUT];
    char p2wpkh[SIZEOUT];
} keys;

/*  A slightly modified array that stores the size of the array and how much
    space has been used. Useful for keeping track of when to reallocate.
*/
struct Array {
    struct key_set **array;
    size_t used;
    size_t size;
} array;

// func pointer typedef
typedef void (*priv_func_ptr) (char *, char *, int);

/** This is an array of function pointers that take seeds and set private keys.
 *  This array makes it really easy to add or remove ways to turn seeds into
 *  private keys.
*/
extern const priv_func_ptr priv_gen_functions[PRIVATE_KEY_TYPES];


/*  Sorts the input seed file and make sures there are no duplicates.
    This helps us avoid poorly constructed database transactions, for example,
    a transaction where we try to insert the same record twice.

    Returns 0 on success, 1 on failure.
*/
int sort_seeds(char *orig, char *sorted);

/*  Stores the number of lines in the input seed file into count.
    Returns 0 on success, 1 on failure.
*/
int seed_count(char *file, unsigned long *count);

/*  On success, this returns the number of records in the database. If it fails,
    it returns -1.
*/
size_t get_record_count(sqlite3 *db);


/*  Reset the bloom filters, resize them, and refill them with the records
    from the database. Returns 0 on success, 1 on failure.
*/
int resize_bloom_filters(struct bloom *private_filter, struct bloom *addr_filter,
                         sqlite3 *db, unsigned long count);


/*  Fill the key_set set with the provided string arguements.
    Returns 0 if it succeeds and 1 if it fails.
*/
int fill_key_set(struct key_set *set, char *private, char *seed, char *p2pkh,
                  char *p2sh_p2wpkh, char *p2wpkh);


/* Compares the private keys of two key_set structs. */
int compare_key_sets_privkey(const void *p1, const void *p2);


/***  Array struct functions. ***/

/*  Initializes an Array structure that will store key_set structs.
    Returns 0 if it succeeds and 1 if it fails.
*/
int init_Array(struct Array *key_array, size_t size);


/*  Add key_set set to the key_array. 
    Updates the key_array struct and reallocates if necessary.
*/
void push_Array(struct Array *key_array, struct key_set *set);


/*  Push all elements of b - a to dest.*/
void push_Difference(struct Array *a, struct Array *b, struct Array *dest);


/* Frees the key_array and all key_sets within it. */
void free_Array(struct Array *key_array);

/*  Populates dest with all elements from src, but makes sure elements are
    unique. Returns 0 on success, 1 on failure.
*/
int remove_duplicates(struct Array *src, struct Array *dest);


/*  Start of a database transaction. */
void start_tx(char **query,  size_t *current_len);


/*  End of a database transaction. */
void end_tx(char **query, size_t *current_len);


/*  Checks if query needs to be reallocated, if yes it reallocates.
    If resize_check fails for any reason it returns 1, otherwise it returns 0.
*/
int resize_check(char *values, char **query, size_t *current_len, int *q_size);


/*  Allocates space and calls the appropriate query builder function.
    Returns 1 if the build failed, 0 if it succeeded.
*/
int prepare_query(struct Array *arr, char **query, int query_size, int type);


/*  Builds the query for adding to the database.
    Returns 1 if it fails, 0 if it succeeds.
*/
int build_update_query(struct Array *update, char **query, int query_size);


/*  Builds the query for checking the database for certain records.
    Returns 1 if it fails, 0 if it succeeds.
*/
int build_check_query(struct Array *check, char **query, int query_size);


/*  sqlite3 callback function for checking if a private key is in the db */
int callback(void *arr, int argc, char **argv, char **columns);


void remove_newline(char *s);


/*  Takes a seed and returns a pointer to an array of pointers to private keys.
    Returns NULL pointer on failure.
*/
char **seed_to_priv(char *seed, int len);


/*  Puts the seed into the string buf then front pads it with 0's until
    there are 33 characters (including a null terminator).
*/
void front_pad_pkey(char *seed, char *buf, int len);


/*  Puts the seed into the string buf then back pads it with 0's until
    there are 33 characters (including a null terminator).
*/
void back_pad_pkey(char *seed, char *buf, int len);


/*  Puts the seed through sha256, then stores the first 32 characters of the
    resulting hex string in buf.
*/
void sha256_pkey(char *seed, char *buf, int len);


/*  Takes a buffer (private key string), an empty btc_key and empty btc_pubkey 
    and fills the btc_key and btc_pubkey. 
*/
void create_pubkey(char *buffer, btc_key *key, btc_pubkey *pubkey);