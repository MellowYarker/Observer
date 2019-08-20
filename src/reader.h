#include <libwebsockets.h>

extern char *transaction_buf;
extern int transaction_size;
extern int partial_write; // represents if the current transaction is complete

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

/* A mempool transaction. */
struct transaction {
    char *tx; // pointer to the transaction string
    int size; // size of the transaction string
    char **outputs; // a list of this transaction's output addresses
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

/*  Creates a new tranasction struct with the given tx string.
    Returns NULL on failure.
*/
struct transaction* create_transaction(char *tx, int size);

/*  Free's a transaction struct and all it's members. */
void free_transaction(struct transaction *tx);

/* Store all addresses that are returned from the database into a linked list.*/
int callback(void *arr, int argc, char **argv, char **columns);