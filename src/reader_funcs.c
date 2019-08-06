#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reader.h"

struct node* create_node(char *data) {
    struct node *Node = malloc(sizeof(struct node));

    if (Node == NULL) {
        perror("malloc");
        return NULL;
    }
    // fill with data
    Node->data = malloc(strlen(data) * sizeof(char) + 1);

    if (Node->data == NULL) {
        perror("malloc");
        return NULL;
    }
    Node->next = NULL;

    // set properties of the struct
    Node->size = strlen(data) + 1; // size includes null terminator
    strncpy(Node->data, data, Node->size); // this also copies null terminator

    return Node;
}

/* Update the linked list such that Node is the new head. */
void add_to_head(struct node *Node, struct node *head) {    
    // empty linked list
    if (head == NULL) {
        head = Node;
    } else {
        Node->next = head;
        head = Node;
    }
}


int callback(void *arr, int argc, char **argv, char **columns) {
    // struct key_set *keys = malloc(sizeof(struct key_set));
    // fill_key_set(keys, argv[0], argv[1], argv[2], argv[3], argv[4]);
    // push_Array(arr, keys);
    return 0;
}
