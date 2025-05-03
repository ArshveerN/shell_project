#ifndef __CLIENT_MANAGER_H__
#define __CLIENT_MANAGER_H__
#include <netinet/in.h>


#ifndef MAX_NAME
    #define MAX_NAME 10
#endif

#ifndef MAX_USER_MSG
    #define MAX_USER_MSG 129
#endif

/*
 * Under our chat protocol, the maximum size of a message sent by
 * a server is MAX(username) + space + MAX(user message) + CRLF
 */
#ifndef MAX_PROTO_MSG
    #define MAX_PROTO_MSG MAX_NAME+1+MAX_USER_MSG+2
#endif

/* Working with string functions to parse/manipulate the contents of
 * the buffer can be convenient. Since we are using a text-based
 * protocol (i.e., message contents will consist only of valid ASCII
 * characters) let's leave 1 extra byte to add a NULL terminator
 * so that we can more easily use string functions. We will never
 * actually send a NULL terminator over the socket though.
 */
#ifndef BUF_SIZE
    #define BUF_SIZE MAX_PROTO_MSG+1
#endif

struct listen_sock {
    int sock_fd;
    struct sockaddr_in *addr;
};

struct client_sock {
    int sock_fd;
    int state;
    int username;
    char buf[BUF_SIZE];
    int inbuf;
    struct client_sock *next;
};

struct server_sock {
    int sock_fd;
    char buf[BUF_SIZE];
    int inbuf;
};

void setup_server_socket(struct listen_sock *s, int port, int *piepfd);
int write_to_socket(int sock_fd, char *buf, int len);
int find_network_newline(const char *buf, size_t inbuf);
int read_from_socket(int sock_fd, char *buf, int *inbuf);
int get_message(char **dst, char *src, int *inbuf);

int accept_connection(int fd, struct client_sock **clients, int *num_clients, int *total_clients);
int write_buf_to_client(struct client_sock *c, char *buf, int len);

int remove_client(struct client_sock **curr, struct client_sock **clients);

int read_from_client(struct client_sock *curr);

void clean_exit(struct listen_sock s, struct client_sock *clients);
#endif