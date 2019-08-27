#include <libwebsockets.h>

extern char *transaction_buf;
extern int transaction_size;
extern int partial_write; // represents if the current transaction is complete
extern int discard; // if 1, clear partial read buffer

extern const struct lws_protocols protocols[];
extern struct lws_context *context;
extern struct lws *client_wsi;


/* A node in a linked list. Free'd when written to child process. */
struct node {
    char *data; // pointer to data
    int size; // size of data in bytes
    char *private; // pointer to private key if this is a spendable address
    struct node *next; // pointer to next node
};

/* An output from a transaction. */
struct output {
    char *address; // bitcoin address
    unsigned int value; // value in satoshi's
    char *script; // the "locking" script
    int positive; // 1 if positive after bloom filter check, 0 if negative
};

/* A mempool transaction. */
struct transaction {
    struct output **outputs; // a list of this transaction's outputs
    int nOutputs; // the number of output addresses in this transaction
};

/* Creates a node struct and assigns the string data to node::data. */
struct node* create_node(char *data);

/* Update the linked list such that Node is the new head. */
void add_to_head(struct node *Node, struct node **head);

/*  Add a private key to this node if the address is spendable.
    Returns 0 on success, 1 on failure.
*/
int add_private(struct node *Node, char *private);

/*  Creates a new transaction struct consisting of the adddr, val, and script
    arguments. Returns NULL on failure. */
struct output* create_output(char *address, unsigned int value, char *script);

/*  Creates a new tranasction struct with the given tx string.
    Returns NULL on failure.
*/
struct transaction* create_transaction(char *tx, int size);

/*  Free's a transaction struct and all it's members. */
void free_transaction(struct transaction *tx);

/* Store all addresses that are returned from the database into a linked list.*/
int callback(void *arr, int argc, char **argv, char **columns);