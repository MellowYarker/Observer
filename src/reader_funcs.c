#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reader.h"
#include "cjson/cJSON.h"


struct node* create_node(char *data) {
    struct node *Node = malloc(sizeof(struct node));

    if (Node == NULL) {
        perror("malloc");
        return NULL;
    }
    Node->size = strlen(data) + 1; // size includes null terminator
    // fill with data
    Node->data = malloc(Node->size * sizeof(char));

    if (Node->data == NULL) {
        perror("malloc");
        return NULL;
    }
    Node->next = NULL;

    strncpy(Node->data, data, Node->size); // this also copies null terminator

    return Node;
}

/* Update the linked list such that Node is the new head. */
void add_to_head(struct node *Node, struct node **head) {
    // empty linked list
    if (*head == NULL) {
        *head = Node;
    } else {
        Node->next = *head;
        *head = Node;
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

struct output* create_output(char *address, unsigned int value, char *script) {
    int addr_size = strlen(address);
    int script_size = strlen(script);

    struct output *new = malloc(sizeof(struct output));
    if (new == NULL) {
        perror("malloc");
        return NULL;
    }

    new->address = malloc((addr_size + 1) * sizeof(char));
    if (new->address == NULL) {
        perror("malloc");
        return NULL;
    }
    strncpy(new->address, address, addr_size);
    new->address[addr_size] = '\0';

    new->value = value;

    new->script = malloc((script_size + 1) * sizeof(char));
    if (new->script == NULL) {
        perror("malloc");
        return NULL;
    }
    strncpy(new->script, script, script_size);
    new->script[script_size] = '\0';
    new->positive = 0;

    return new;
}

struct transaction* create_transaction(char *tx, int size) {
    struct transaction *new = malloc(sizeof(struct transaction));
    if (new == NULL) {
        perror("malloc");
        return NULL;
    }

    new->tx = malloc(sizeof(char) * size + 1);
    if (new->tx == NULL) {
        perror("malloc");
        free(new);
        return NULL;
    }

    strcpy(new->tx, tx);
    new->size = size + 1;
    new->tx[size] = '\0';
    new->nOutputs = 0;

    // Parse the json for the output addresses
    const cJSON *x = NULL;
    const cJSON *outputs = NULL;
    const cJSON *output = NULL;

    cJSON *tx_structure = cJSON_Parse(new->tx);
    if (tx_structure == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, "Error, not valid json.\n");
            free(new);
            free(new->tx);
            cJSON_Delete(tx_structure);
            return NULL;
        }
    }

    x = cJSON_GetObjectItemCaseSensitive(tx_structure, "x");
    if (!cJSON_IsObject(x)) {
        fprintf(stderr, "Couldn't parse transaction.\n");
        free(new);
        free(new->tx);
        cJSON_Delete(tx_structure);
        return NULL;
    }

    outputs = cJSON_GetObjectItemCaseSensitive(x, "out");

    // first check to see how many outputs there are
    cJSON_ArrayForEach(output, outputs) {
        const cJSON *address = NULL; // an outputs address
        address = cJSON_GetObjectItemCaseSensitive(output, "addr");

        if (cJSON_IsString(address) && (address->valuestring != NULL)) {
            new->nOutputs++;
        }
    }
    // each element is a pointer to an output struct
    new->outputs = malloc(new->nOutputs * sizeof(struct output*));
    if (new->outputs == NULL) {
        free(new);
        free(new->tx);
        perror("malloc");
        return NULL;
    }

    // now actually store each output
    int i = 0;
    cJSON_ArrayForEach(output, outputs) {
        const cJSON *address = NULL; // output address
        const cJSON *value = NULL; // value in satoshi
        const cJSON *script = NULL; // the locking script

        address = cJSON_GetObjectItemCaseSensitive(output, "addr");
        value = cJSON_GetObjectItemCaseSensitive(output, "value");
        script = cJSON_GetObjectItemCaseSensitive(output, "script");

        // if we find no errors, create the output
        if (cJSON_IsString(address) && (address->valuestring != NULL) &&
            cJSON_IsString(script) && (script->valuestring != NULL) &&
            cJSON_IsNumber(value)) {

            new->outputs[i] = create_output(address->valuestring,
                                            value->valueint,
                                            script->valuestring);
            if (new->outputs[i] == NULL) {
                perror("malloc");
                free(new);
                free(new->tx);
                cJSON_Delete(tx_structure);
                return NULL;
            }
            i++;
        }
    }
    cJSON_Delete(tx_structure);

    return new;
}

void free_transaction(struct transaction *tx) {
    for (int i = 0; i < tx->nOutputs; i++) {
        free(tx->outputs[i]->address);
        free(tx->outputs[i]->script);
        free(tx->outputs[i]); // pointer to output struct
    }
    free(tx->outputs);
    free(tx->tx);
    free(tx);
}

int callback(void *arr, int argc, char **argv, char **columns) {
    // arg 0 = private key
    // arg 1 = address
    if (strlen(argv[1]) > 0) {
        struct node *record = create_node(argv[1]);
        if (record == NULL) {
            fprintf(stderr, "Couldn't allocate space for returned record.\n");
            return 1;
        }
        if (add_private(record, argv[0]) == 1) {
            fprintf(stderr, "Couldn't allocate space for private key.\n");
            return 1;
        }
        add_to_head(record, arr);
    }

    return 0;
}
