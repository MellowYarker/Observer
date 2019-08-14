#include "load.h"

#include <bloom.h>
#include <sqlite3.h>
#include <time.h>


struct node* create_node(char *data) {
    struct node *Node = malloc(sizeof(struct node));

    if (Node == NULL) {
        perror("malloc");
        return NULL;
    }
    Node->size = strlen(data);
    Node->data = malloc(Node->size * sizeof(char) + 1);

    if (Node->data == NULL) {
        perror("malloc");
        return NULL;
    }
    // fill with data
    strncpy(Node->data, data, Node->size + 1);
    Node->next = NULL;

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

void batch_insert_filter(struct node **head, struct bloom *filter) {
    struct node *cur = *head;
    while (cur != NULL) {
        bloom_add(filter, cur->data, cur->size);
        
        // free this node and iterate to the next one
        struct node *temp = cur;
        free(cur->data);
        cur = cur->next;
        free(temp);
    }
    *head = NULL;
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
        sqlite3_stmt *stmt;
        printf("Adding addresses %d to %d.\n", offset, offset + limit);
        query = sqlite3_mprintf("SELECT * FROM usedAddresses LIMIT %d OFFSET %d;", limit, offset);
        if (query == NULL) {
            fprintf(stderr, "Could not allocate memory for query.");
            exit(1);
        }

        start = clock();
        rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);

        if (rc != SQLITE_OK ) {
            printf("error: %s", sqlite3_errmsg(db));
            exit(1);
        }
        sqlite3_free(query);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            struct node *Node = create_node((char *)
                                            sqlite3_column_text(stmt, 0));

            if (Node == NULL) {
                exit(1);
            }

            add_to_head(Node, &head);
        }
        if (rc != SQLITE_DONE) {
            printf("error: %s", sqlite3_errmsg(db));
            return -1;
        }
        sqlite3_finalize(stmt);

        end = clock();
        printf("Took %f seconds to get batch %d records and add "\
               "them to the linked list.\n", 
               ((double) end - start)/CLOCKS_PER_SEC, i);
        offset += limit;

        start = clock();
        // fill filter and save result
        batch_insert_filter(&head, &filter);
        end = clock();
        printf("Took %f seconds to add batch %d to the bloom filter.\n", 
               ((double) end - start)/CLOCKS_PER_SEC, i);
    }
    printf("Process complete.\n");

    bloom_free(&filter);
    return 0;
}
