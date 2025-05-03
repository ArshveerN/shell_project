#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Linked list node structure
typedef struct Node {
    char name[128];
    char value[128];
    struct Node *next;
} Node;

// Function prototypes
Node *create_node(char *name, char *value);
Node *find_name(char *name, Node *first);
void set_variable(char *name, char *value, Node **first, Node **last);
void print_all(Node *first);
void free_list(Node *first);
void free_all(Node *first);
int expand_arr(char *arr, Node *first);
#endif // VARIABLES_H

