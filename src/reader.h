#define p2pkh 0
#define p2sh 1
#define p2wpkh 2

/* A node in a linked list. Free'd when written to child process. */
struct node {
    char *data; // pointer to data
    int size; // size of data in bytes
    struct node *next; // pointer to next node
};

/* Creates a node struct and assigns the string data to node::data. */
struct node* create_node(char *data);

/* Update the linked list such that Node is the new head. */
void add_to_head(struct node *Node, struct node *head);

/* TODO: Puts all the resulting addresses in a list  */
int callback(void *arr, int argc, char **argv, char **columns);