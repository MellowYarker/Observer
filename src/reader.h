#define p2pkh 0
#define p2sh 1
#define p2wpkh 2

/* A node in a linked list. Free'd when written to child process. */
struct node {
    char *data; // pointer to data
    int size; // size of data in bytes
    char *private; // pointer to private key if this is a spendable address
    struct node *next; // pointer to next node
};

/* Creates a node struct and assigns the string data to node::data. */
struct node* create_node(char *data);

/* Update the linked list such that Node is the new head. */
void add_to_head(struct node *Node, struct node *head);

/* Add a private key to this node if the address is spendable. */
int add_private(struct node *Node, char *private);

/* TODO: Puts all the resulting addresses in a list  */
int callback(void *arr, int argc, char **argv, char **columns);