#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bloom.h>

struct node {
    char *data; // pointer to data
    struct node *next; // pointer to next node
};

/* Creates a node struct and assigns the string data to node::data. */
struct node* create_node(char *data);

/* Update the linked list such that Node is the new head. */
void add_to_head(struct node *Node, struct node *head);

/*  Inserts all elements of the linked list into the bloom filter.
    Nodes are free'd as they're inserted. Returns 0 on success, 1 on error.
*/
void batch_insert_filter(struct node *head, struct bloom *filter);



