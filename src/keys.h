
#include <btc.h>
#include <ecc_key.h>

#define SIZEOUT 128
#define MAX_BUF BTC_ECKEY_PKEY_LENGTH + 1 // add a byte for the null terminator
#define PRIVATE_KEY_TYPES 2 // # of private keys we generate from a given seed

struct key_set {
    char private[33];
    char seed[33];
    char p2pkh[SIZEOUT];
    char p2sh_p2wpkh[SIZEOUT];
    char p2wpkh[SIZEOUT];
} keys;

struct Array {
    struct key_set **array;
    size_t used;
    size_t size;
} array;

/* Fill the key_set set with the provided string arguements. */
void fill_key_set(struct key_set *set, char *private, char *seed, char *p2pkh, 
                  char *p2sh_p2wpkh, char *p2wpkh);

/* Initializes an Array structure that will store key_set structs. */
void init_Array(struct Array *key_array, size_t size);

/*  Add key_set set to the key_array. 
    Updates the key_array struct and reallocates if necessary.
*/
void push_Array(struct Array *key_array, struct key_set *set);

void remove_newline(char *s);

/*  Puts the seed into the string front_pad then front pads it with 0's until
    there are 33 characters (including a null terminator).
*/
void front_pad_pkey(char *seed, char *front_pad, int len);

/*  Puts the seed into the string front_pad then back pads it with 0's until
    there are 33 characters (including a null terminator).
*/
void back_pad_pkey(char *seed, char *back_pad, char *front_pad, int len);

/*  Takes a buffer (private key string), an empty btc_key and empty btc_pubkey 
    and fills the btc_key and btc_pubkey. 
*/
void create_pubkey(char *buffer, btc_key *key, btc_pubkey *pubkey);