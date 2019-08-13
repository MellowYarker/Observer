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

int add_private(struct node *Node, char *private) {
    Node->private = malloc(strlen(private) * sizeof(char) + 1);
    if (Node->private == NULL) {
        perror("malloc");
        return 1;
    }
    strcpy(Node->private, private);
    return 0;
}

int callback(void *arr, int argc, char **argv, char **columns) {
    // arg 0 = private key
    // arg 1 = address
    if (strlen(argv[1]) > 0) {
        struct node *record = create_node(argv[1]);
        if (record == NULL) {
            fprintf(stderr, "Couldn't allocate space for returned record.");
            return 1;
        }
        if (add_private(record, argv[0]) == 1) {
            fprintf(stderr, "Couldn't allocate space for private key.");
            return 1;
        }
        add_to_head(record, arr);
    }

    return 0;
}
