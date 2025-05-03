#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "io_helpers.h"

typedef struct Node {
    char name[128];
    char value[128];
    struct Node *next;
} Node;

Node *create_node(char *name, char *value) {
    Node *node = malloc(sizeof(Node));

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->name[sizeof(node->name) - 1] = '\0';

    strncpy(node->value, value, sizeof(node->value) - 1);
    node->value[sizeof(node->value) - 1] = '\0';

    node->next = NULL;

    return node;
}

Node *find_name(char *name, Node *first) {
    Node *curr = first;

    while (curr != NULL) {
        if (strcmp(name, curr->name) == 0) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}

void set_variable(char *name, char *value, Node **first, Node **last) {
    if (*first != NULL) {
        Node *node = find_name(name, *first);
        if (node != NULL) {
            strncpy(node->value, value, sizeof(node->value) - 1);
            node->value[sizeof(node->value) - 1] = '\0';
        } else {
            Node *new_node = create_node(name, value);
            (*last)->next = new_node;
            *last = new_node;
        }
    } else {
        *first = create_node(name, value);
        *last = *first;
    }
}

void free_all(Node *first) {
    Node *current = first;
    while (current != NULL) {
        Node *temp = current;
        current = current->next;
        free(temp);
    }
}


void print_all(Node *first) {
    Node *curr = first;
    while (curr != NULL) {
        display_message(curr->name);
        display_message("=");
        display_message(curr->value);
        display_message("\n");
        curr = curr->next;
    }
}

//int count_sign(char *arr){
//      int count = 0
//      for (int i = 0; i < strlen(arr); i++) {
//      if (arr[i] == '$') {
//      count++;
//      }
//      return count;
//}


int expand_arr(char *arr, Node *first) {
    char output[128];
    int out_idx = 0;
    int len = strlen(arr);
    int only_variable = (arr[0] == '$');
    int seen = 0;

    for (int i = 0; i < len; i++) {
        if (arr[i] == '$') {
            int start = i;
            i++;

            char var_name[128] = {0};
            int var_idx = 0;

            while (i < len && arr[i] != ' ' && arr[i] != '$') {
                if (var_idx < 127) {
                    var_name[var_idx++] = arr[i];
                }
                i++;
            }
            var_name[var_idx] = '\0';

            if (var_idx == 0) {
                if (out_idx < 127) { // Ensure room for '$'
                    output[out_idx++] = '$';
                }
                continue;
            }

            Node *node = find_name(var_name, first);

            if (node != NULL) {
                int var_len = strlen(node->value);
                int remaining_space = 127 - out_idx; // Leave room for '\0'

                if (var_len > remaining_space) {
                    // Truncate variable value if it exceeds available space
                    strncpy(output + out_idx, node->value, remaining_space);
                    out_idx += remaining_space;
                } else {
                    strncpy(output + out_idx, node->value, var_len);
                    out_idx += var_len;
                }
		output[out_idx] = '\0';

                only_variable = 0;
                seen = 1;
            } else {
                if (only_variable && i == len) {
                    arr[0] = '\0';
                    return 0;
                } else if (seen != 1) {
                    int remaining_space = 127 - out_idx; // Ensure space limit

                    int copy_len = (i - start < remaining_space) ? (i - start) : remaining_space;
                    strncpy(output + out_idx, arr + start, copy_len);
                    out_idx += copy_len;
                }
            }
            i--;
        } else {
            if (out_idx < 127) { // Ensure space is available
                output[out_idx++] = arr[i];
            }
            only_variable = 0;
        }
    }

    output[out_idx] = '\0'; // Null-terminate output
    strncpy(arr, output, 128); // Copy safely back to arr
    return 1;
}