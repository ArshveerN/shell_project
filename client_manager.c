#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"
#include "client_manager.h"


// start a server at given socket at given port
void setup_server_socket(struct listen_sock *s, int port, int *pipefd) {
    if (!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
        perror("malloc");
        exit(1);
    }
    s->addr->sin_family = AF_INET;
    s->addr->sin_port = htons(port);
    memset(&(s->addr->sin_zero), 0, 8);
    s->addr->sin_addr.s_addr = INADDR_ANY;

    s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sock_fd < 0) {
        perror("server socket");
        char comm[2];
        comm[0] = 'f';
        comm[1] = '\0';
        write(pipefd[1], comm, 2);
        close(pipefd[1]);
        exit(1);
    }
    int on = 1;
    int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        char comm[2];
        comm[0] = 'f';
        comm[1] = '\0';
        write(pipefd[1], comm, 2);
        close(pipefd[1]);
        perror("setsockopt");
        exit(1);
    }

    if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
        char comm[2];
        comm[0] = 'f';
        comm[1] = '\0';
        write(pipefd[1], comm, 2);
        close(pipefd[1]);

        display_error("ERROR: socket already in use", "");
        display_error("ERROR: Builtin failed: ", "start-server");
        close(s->sock_fd);
        exit(1);
    }

    if (listen(s->sock_fd, 5) < 0) {
        char comm[2];
        comm[0] = 'f';
        comm[1] = '\0';
        write(pipefd[1], comm, 2);
        close(pipefd[1]);
        display_error("ERROR: Piping failed", "");
        close(s->sock_fd);
        exit(1);
    }
}

int write_to_socket(int sock_fd, char *buf, int len) {
    int total_written = 0;
    while (total_written < len) {
        int written_bytes = write(sock_fd, buf + total_written, len - total_written);
        total_written += written_bytes;
    }
    return 0;
}

/*
 * Search the first `inbuf` characters of `buf` for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, size_t inbuf) {
    for (size_t i = 1; i < inbuf; i++) {
        if (buf[i] == '\n' && buf[i - 1] == '\r') {
            return (int)(i + 1);
        }
    }
    return -1;
}


/*
 * Reads from socket sock_fd into buffer *buf containing *inbuf bytes
 * of data. Updates *inbuf after reading from socket.
 *
 * Return -1 if read error or maximum message size is exceeded.
 * Return 0 upon receipt of CRLF-terminated message.
 * Return 1 if socket has been closed.
 * Return 2 upon receipt of partial (non-CRLF-terminated) message.
 */
int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    ssize_t bytes_read = read(sock_fd, buf + *inbuf, BUF_SIZE - *inbuf);


    if (bytes_read == 0) {
        return 1;
    }

    *inbuf += bytes_read;
    int newline_index = find_network_newline(buf, *inbuf);

    if (newline_index != -1) {
        return 0;
    } else {
        return 2;
    }
}

/*
 * Search src for a network newline, and copy complete message
 * into a newly-allocated NULL-terminated string **dst.
 * Remove the complete message from the *src buffer by moving
 * the remaining content of the buffer to the front.
 *
 * Return 0 on success, 1 on error.
 */
int get_message(char **dst, char *src, int *inbuf) {
    // Find the position of the network newline (\r\n)
    int newline_index = find_network_newline(src, *inbuf);

    if (newline_index == -1) {
        return 1;
    }
    *dst = malloc(newline_index + 1);

    memcpy(*dst, src, newline_index);
    (*dst)[newline_index] = '\0';

    int remaining_data = *inbuf - newline_index;
    if (remaining_data > 0) {
        memmove(src, src + newline_index, remaining_data);
    }

    *inbuf -= newline_index;

    return 0;
}

int accept_connection(int server_fd, struct client_sock **client_list, int *active_clients, int *client_counter) {
    struct sockaddr_in client_addr;
    unsigned int addr_len = sizeof(client_addr);
    client_addr.sin_family = AF_INET;


    int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

    // Increment client counters
    (*client_counter)++;
    (*active_clients)++;

    struct client_sock *new_client = malloc(sizeof(struct client_sock));

    new_client->sock_fd = new_fd;
    new_client->inbuf = 0;
    new_client->state = 0;
    new_client->username = *client_counter;
    new_client->next = NULL;
    memset(new_client->buf, 0, BUF_SIZE);

    if (*client_list != NULL) {
        struct client_sock *current = *client_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_client;

    }
    else {
        *client_list = new_client;
        *active_clients = 1;
    }

    return new_fd;
}


int write_buf_to_client(struct client_sock *c, char *buf, int len) {
    int result = write_to_socket(c->sock_fd, buf, len);
    if (result == -1) {
        return 1;
    } else if (result != 0) {
        return 2;
    }
    return 0;
}

int read_from_client(struct client_sock *curr) {
    return read_from_socket(curr->sock_fd, curr->buf, &(curr->inbuf));
}

int remove_client(struct client_sock **target_client, struct client_sock **client_list) {
    if (*target_client == NULL || *client_list == NULL) {
        return 1;
    }

    struct client_sock *current = *client_list;
    struct client_sock *previous = NULL;

    while (current != NULL && current != *target_client) {
        previous = current;
        current = current->next;
    }

    if (current == NULL) {
        return 1;
    }

    if (previous != NULL) {
        previous->next = current->next;
    } else {
        *client_list = current->next;
    }
    free(current);
    *target_client = NULL;

    return 0;
}

void clean_exit(struct listen_sock server, struct client_sock *client_list) {
    struct client_sock *current_client;
    while (client_list != NULL) {
        current_client = client_list;
        close(current_client->sock_fd);
        client_list = client_list->next;
        free(current_client);
    }
    close(server.sock_fd);
    free(server.addr);
}
