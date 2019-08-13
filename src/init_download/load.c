#include "load.h"

#include <bloom.h>
#include <sqlite3.h>
#include <time.h>


// Addresses can range from 28-42 characters, we will add them to a linked list.
int callback(void *head, int argc, char **argv, char **columns) {
    struct node *Node = create_node(argv[0]);
    if (Node == NULL) {
        exit(1);
    }
    add_to_head(Node, head);
    return 0;
}

struct node* create_node(char *data) {
    struct node *Node = malloc(sizeof(struct node));

    if (Node == NULL) {
        perror("malloc");
        return NULL;
    }
    Node->data = malloc(strlen(data) * sizeof(char) + 1);

    if (Node->data == NULL) {
        perror("malloc");
        return NULL;
    }
    // fill with data
    strncpy(Node->data, data, strlen(data));
    Node->next = NULL;

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

void batch_insert_filter(struct node *head, struct bloom *filter) {
    struct node *cur = head;
    while (cur != NULL) {
        bloom_add(filter, cur->data, strlen(cur->data));
        
        // free this node and iterate to the next one
        struct node *next = cur->next;
        free(cur->data);
        free(cur);
        cur = next; // cur is now the next node
    }
    // cur (the head) is NULL
    bloom_save(filter, "../used_address_filter.b");
}

int main(int argc, char **argv) {
    // set up bloom filter
    struct bloom filter;
    // assume we will insert about 550 million records 
    // Note: my db has 530M, I don't plan on increasing it
    int records = 550000000;
    bloom_init2(&filter, records, 0.01);

    // set up linked list
    struct node *head = NULL;

    // sql query and handling
    int limit = 27500000; // only store 27.5 million addresses (~1GB) at a time.
    int offset = 0; // start from 0, this indicates where to start reading in db
    char *query;
    clock_t start, end; // times the execution

    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open("../../db/observer.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(1);
    } else {
        printf("Opened database connection!\n");
    }
    // Note: we use 20 here because my database has a little over 530 million
    // records. ceil(530,029,220/27,500,000) = ceil(19.27) = 20
    for (int i = 0; i < 20; i++) {
        printf("Adding addresses %d to %d.\n", offset, offset + limit);
        query = sqlite3_mprintf("SELECT * FROM usedAddresses LIMIT %d OFFSET %d;", limit, offset);
        if (query == NULL) {
            fprintf(stderr, "Could not allocate memory for query.");
            exit(1);
        }

        start = clock();

        rc = sqlite3_exec(db, query, callback, head, &zErrMsg);
        if (rc != SQLITE_OK ) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        }
        end = clock();
        printf("Took %f seconds to get batch %d records and add "\
               "them to the linked list.\n", 
               ((double) end - start)/CLOCKS_PER_SEC, i);
        offset += limit;

        start = clock();
        // fill filter and save result
        batch_insert_filter(head, &filter);
        end = clock();
        printf("Took %f seconds to add batch %d to the bloom filter.\n", 
               ((double) end - start)/CLOCKS_PER_SEC, i);
    }
    printf("Process complete.\n");

    bloom_free(&filter);
    return 0;
}
