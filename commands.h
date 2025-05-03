#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <unistd.h>

extern struct listen_sock s;
extern struct client_sock *clients;
extern int server_pid;

typedef ssize_t (*cm_ptr)(char **);
// the supported builtins
ssize_t cm_start(char **tokens);
ssize_t cm_close(char **tokens);
ssize_t cm_send(char **tokens);
ssize_t cm_start_client(char **tokens);
ssize_t internal_close();

cm_ptr check_command(const char *cmd);

static const char * const COMMANDS[] = {"start-server", "close-server", "send", "start-client"};
static const cm_ptr COMMANDS_FN[] = {cm_start, cm_close, cm_send, cm_start_client, NULL};    // Extra null element for 'non-builtin'
static const ssize_t COMMANDS_COUNT = sizeof(COMMANDS) / sizeof(char *);

#endif
