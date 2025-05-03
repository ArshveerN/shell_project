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

struct listen_sock s;
struct client_sock *clients = NULL;
int server_pid = 0;
int sigint_received = 0;
int num_clients = 0;
int total_clients = 0;

void client_ctrl_d_handler(__attribute__((unused)) int signal) {
    sigint_received = -1;
}

void server_ctrlc_handler(__attribute__((unused)) int signal) {
    sigint_received = -1;
}

void reduce_child_handler(__attribute__((unused)) int signal) {
    num_clients--;
}

cm_ptr check_command(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < COMMANDS_COUNT &&
           strncmp(COMMANDS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return COMMANDS_FN[cmd_num];
}



void exit_handler(__attribute__((unused)) int signo) {
    clean_exit(s, clients);
}

ssize_t cm_start(char **tokens) {
    total_clients = 0;
    num_clients = 0;
    if (server_pid != 0) {
        display_error("ERROR: Server in progress", "");
        return -1;
    }

    int pipefd[2];
    if (pipe((pipefd)) == -1) {
        display_error("ERROR: Pipe Failed", "");
        exit(-1);
    }

    signal(SIGQUIT, reduce_child_handler);


    char comm[2];
    int n = fork();
    if (n  == 0) {
        close(pipefd[0]);
        if (tokens[1] == NULL) {
            display_error("ERROR: No port provided", "");
            comm[0] = 'e';
            comm[1] = '\0';
            write(pipefd[1], comm, 2);
            close(pipefd[1]);
            return -1;
        }
        else if (tokens[2] != NULL) {
            display_error("ERROR: Too many arguments", tokens[2]);
            comm[0] = 'e';
            comm[1] = '\0';
            write(pipefd[1], comm, 2);
            close(pipefd[1]);
            return -1;
        }

        signal(SIGTERM, exit_handler);

        char *endptr;

        long port_number = strtol(tokens[1], &endptr, 10);
        if (*endptr != '\0' || port_number <= 0 || port_number > 65535) {
            display_error("ERROR: Invalid port number", tokens[1]);
            comm[0] = 'e';
            comm[1] = '\0';
            write(pipefd[1], comm, 2);
            close(pipefd[1]);
            return -1;
        }


        // start the server at given port
        setup_server_socket(&s, (int)port_number, pipefd);

        comm[0] = 'f';
        comm[1] = '\0';
        write(pipefd[1], comm, 2);
        close(pipefd[1]);

        int max_fc = s.sock_fd;
        fd_set all_fds;
        fd_set listen_fds;

        FD_ZERO(&all_fds);
        FD_SET(s.sock_fd, &all_fds);

        while (1){
            listen_fds = all_fds;
            select(max_fc + 1, &listen_fds, NULL, NULL, NULL);

            if (FD_ISSET(s.sock_fd, &listen_fds)) {
                int client_fd = accept_connection(s.sock_fd, &clients, &num_clients, &total_clients);
                if (client_fd < 0) {
                    continue;
                }
                if (client_fd > max_fc) {
                    max_fc = client_fd;
                }
                FD_SET(client_fd, &all_fds);
            }

            struct client_sock *curr = clients;

            while (curr) {
                if (!FD_ISSET(curr->sock_fd, &listen_fds)) {
                    curr = curr->next;
                    continue;
                }
                int client_closed = read_from_client(curr);

                if (client_closed == -1) {
                    client_closed = 1;
                }

                char *msg;
                char *msg1;

                while (client_closed == 0 && !get_message(&msg, curr->buf, &(curr->inbuf))) {
                    char write_buf[BUF_SIZE];
                    write_buf[0] = '\0';
                    char name[MAX_NAME] = "";
                        sprintf(name, "client%d:",curr->username);
                        strncat(write_buf, name, MAX_NAME);
                        strncat(write_buf, " ", MAX_NAME);

                    if (msg[0] == '\x1F'){
                        num_clients--;
                    }

                    msg1 = malloc(strlen(msg) + 50);
                    size_t msg_len = strlen(msg);
                    if (msg_len >= 2 && strcmp(msg + msg_len - 2, "\r\n") == 0) {
                        msg[msg_len - 2] = '\0';
                    }

                    if (strcmp(msg, "\\connected") == 0) {
                        snprintf(msg1, strlen(msg) + 50, "SYSTEM: # of clients: %d\r\n", num_clients);
                    } else {
                        snprintf(msg1, strlen(msg) + 50, "%s\r\n", msg);
                    }


                    strncat(write_buf, msg1, MAX_USER_MSG);
                    free(msg);
                    free(msg1);
                    int data_len = strlen(write_buf);

                    struct client_sock *dest_c = clients;
                    while (dest_c) {

                        int ret = write_buf_to_client(dest_c, write_buf, data_len);

                        if (ret != 0) {
                            if (ret == 2) {
                                close(dest_c->sock_fd);
                                FD_CLR(dest_c->sock_fd, &all_fds);
                                continue;
                            }
                        }
                        dest_c = dest_c->next;
                    }
                    display_message(write_buf);
                }
                if (client_closed == 1) { // Client disconnected
                // Note: Never reduces max_fd when client disconnects
                FD_CLR(curr->sock_fd, &all_fds);
                close(curr->sock_fd);

                // Remove client from the linked list
                struct client_sock *temp = curr;
                curr = curr->next;
                remove_client(&temp, &clients);
                num_clients--;
                }
                else {
                    curr = curr->next;
                }
            }
        }
        return 0;
    }
    if (n > 0) {
        close(pipefd[1]);
        read(pipefd[0], comm, 2);
        close(pipefd[0]);
        if (comm[0] == 'e') {
            wait(NULL);
        }

        server_pid = n;
        return 0;
    }
    return 0;
}

ssize_t cm_close(__attribute__((unused)) char **tokens) {
    if (server_pid <= 0) {
        display_error("ERROR: No server is running", "");
        return -1;
    }
    else {
        kill(server_pid, SIGALRM);
        server_pid = 0;
    }
    return 0;
}

ssize_t internal_close() {
    if (server_pid <= 0) {
        return 0;
    }
    else {
        kill(server_pid, SIGALRM);
        server_pid = 0;
    }
    return 0;
}

ssize_t cm_send(char **tokens) {

    if (tokens[1] == NULL){
        display_error("ERROR: No port provided", "");
        return -1;
    }

    else if (tokens[2] == NULL){
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    // Convert port number
    long port_number = strtol(tokens[1], NULL, 10);

    if (port_number <= 0 || port_number > 65535) {
        display_error("ERROR: Invalid port number", tokens[1]);
        return -1;
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        display_error("ERROR: Socket creation failed", "");
        return -1;
    }

    // Set up server address structure
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number);

    // Convert hostname to binary format  from pt
    if (inet_pton(AF_INET, tokens[2], &server_addr.sin_addr) <= 0) {
        display_error("ERROR: Invalid host", "");
        close(sock_fd);
        return -1;
    }

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        display_error("ERROR: Connection failed", "");
        close(sock_fd);
        return -1;
    }

    // Prepare the message
    char write_buf[MAX_USER_MSG + 3]; // +2 for \r\n
    char other_buf[MAX_USER_MSG+3];
    write_buf[0] = '\x1F';
    write_buf[1] = '\0';

    // Combine all message parts into one string
    for (int i = 3; tokens[i] != NULL; i++) {
        strncat(write_buf, tokens[i], MAX_USER_MSG - strlen(write_buf) - 1);
        if (tokens[i + 1] != NULL) {
            strncat(write_buf, " ", MAX_USER_MSG - strlen(write_buf) - 1);
        }
    }
    int counter = 0;
    for (int place=0; place < (int)strlen(write_buf); place++) {
        if (write_buf[place] != ' ') {
            other_buf[counter++] = write_buf[place];
        }
        else if (write_buf[place] == ' ' && write_buf[place+1] == ' '){
            continue;
        }
        else{
            other_buf[counter++] = write_buf[place];
        }
    }

    strncat(other_buf, "\r\n", MAX_USER_MSG - strlen(other_buf) - 1);

    ssize_t sent_bytes = write(sock_fd, other_buf, strlen(other_buf));
    if (sent_bytes < 0) {
        display_error("ERROR: Sending message failed", "");
    }

    // Close the socket
    close(sock_fd);
    return 0;
}

ssize_t cm_start_client(char **args) {
    signal(SIGINT, server_ctrlc_handler);
    signal(SIGINT, client_ctrl_d_handler);

    if (args[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    } else if (args[2] == NULL) {
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    struct server_sock conn;
    conn.inbuf = 0;

    conn.sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn.sock_fd < 0) {
        display_error("ERROR: Command failed", "");
        return -1;
    }

    long port_val = strtol(args[1], NULL, 10);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_val);
    if (inet_pton(AF_INET, args[2], &addr.sin_addr) < 1) {
        display_error("ERROR: Command failed", "");
        close(conn.sock_fd);
        return -1;
    }

    if (connect(conn.sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        display_error("ERROR: Invalid port number", args[1]);
        close(conn.sock_fd);
        return -1;
    }

    fd_set master_fds, active_fds;
    FD_ZERO(&master_fds);
    FD_SET(STDIN_FILENO, &master_fds);
    FD_SET(conn.sock_fd, &master_fds);
    int highest_fd = conn.sock_fd;

    char input_buf[MAX_USER_MSG + 2];
    int check=0;
    while (1) {
        active_fds = master_fds;
        if (select(highest_fd + 1, &active_fds, NULL, NULL, NULL) == -1) {
            if (sigint_received == -1) {
                return 0;
            }
            display_error("ERROR: Select failed", "");
            return 0;
        }

        if (FD_ISSET(STDIN_FILENO, &active_fds)) {
            if (fgets(input_buf, MAX_USER_MSG + 2, stdin) == NULL) {
                return 0;
            }

            int input_len = strlen(input_buf);
            if (input_len == MAX_USER_MSG + 1 && input_buf[MAX_USER_MSG] != '\n') {
                while ((getchar()) != '\n' && getchar() != EOF);
            }

            input_buf[strcspn(input_buf, "\n")] = '\0';

            if (strlen(input_buf) <= 128){
                strcat(input_buf, "\r\n");
                check = 0;
            }
            else {
                display_error("ERROR: Message too long", "");
                check = 1;
                input_buf[0] = '\0';
            }
            if (write_to_socket(conn.sock_fd, input_buf, strlen(input_buf)) && !check) {
                if (sigint_received == -1) {
                    return 0;
                }
                break;
            }
        }

        if (FD_ISSET(conn.sock_fd, &active_fds)) {
            int read_status = read_from_socket(conn.sock_fd, conn.buf, &(conn.inbuf));
            if (read_status == -1 || read_status == 1) {
                if (sigint_received == -1) {
                    return 0;
                }
            }

            char *received_data;
            while (get_message(&received_data, conn.buf, &(conn.inbuf)) == 0) {
                char *split_pos = strchr(received_data, ' ');
                if (!split_pos) {
                    free(received_data);
                    if (sigint_received == -1) {
                        return 0;
                    }
                }
                *split_pos = '\0';
                char processed_data[MAX_USER_MSG];
                sprintf(processed_data, "%s %s", received_data, split_pos + 1);
                display_message(processed_data);
                free(received_data);
            }
        }
    }

    close(conn.sock_fd);
    return 0;
}