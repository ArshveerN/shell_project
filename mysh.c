#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"
#include "client_manager.h"

char *token_arr[MAX_STR_LEN] = {NULL};
size_t token_count = 0;
int saved_stdout;
int child;

int find_equal(char *content){
    char *index = strchr(content, '=');
    if (index){return index - content;}
    return -1;
}

int find_index(char *input) {
    char *pos = strchr(input, '=');
    if (pos) {
        return (int)(pos - input);
    }
    return -1;
}

void free_tokens(char **tokens, int token_count) {
    if (tokens == NULL) return;
    int i = 0;
    while (i<(int)token_count) {
        free(tokens[i]);
	    i++;
    }
}

void close_server_handler(__attribute__((unused)) int signal) {
    usleep(100);
    clean_exit(s, clients);
    exit(0);
}


void sigint_handler(int signal){
    if (signal == SIGINT){
        display_message("\nmysh$ ");
        fflush(stdout);
    }
    if (signal == SIGTERM || signal == SIGHUP) {
        display_message("\n");
        free_tokens(token_arr, token_count);
        if (child > 0) {
            internal_close();
        }
        exit(0);
    }
}

void sigchld_handler(__attribute__((unused)) int signal) {
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {
            active_children--;
            if (active_children == 0) {
                current_children = 0;
            }
        }
}

void ps_handler(__attribute__((unused)) int signo) {
    int saved_stdout2 = dup(STDOUT_FILENO);
    dup2(saved_stdout, STDOUT_FILENO);
    char output[128];
    sprintf(output, "%s %d\n", token_arr[0], getpid());
    display_message(output);
    dup2(saved_stdout2, STDOUT_FILENO);
    return;
}

void kill_handler(__attribute__((unused)) int signo) {
    char output[128];
    sprintf(output, "[%d]+  Done ", current_children);
    display_message(output);
    for (int idc=0; idc<(int)token_count; idc++) {
        display_message(token_arr[idc]);
        if (idc == (int)token_count - 1) {display_message("\n");continue;}
        display_message(" ");
    }
}

void handle_command(char **token_arr, int token_count, int ret, Node **first_node, Node **last_node) {
    if (token_count != 0) {
        if (ret != -1 && (token_count == 0 ||
            (strncmp("exit", token_arr[0], 4) == 0 && token_arr[0][4] == '\0'))) {
            free_tokens(token_arr, token_count);
            if (child > 0) {
                internal_close();
            }
            exit(0);
        }
    }
    if (token_count >= 1) {
        int index = find_index(token_arr[0]);
        bn_ptr builtin_fn = check_builtin(token_arr[0]);
        cm_ptr command_fn = check_command(token_arr[0]);

        if (builtin_fn != NULL) {
            ssize_t err = builtin_fn(token_arr);
            if (err == -1) {
                display_error("ERROR: Builtin failed: ", token_arr[0]);
            }
        }
        else if (command_fn != NULL) {
            ssize_t err = command_fn(token_arr);
            if (err == -1) {
                display_error("ERROR: Builtin failed ", token_arr[0]);
            }
        }
        else if (index > 0) {
            char name[128];
            char value[128];

            if (token_count == 1) {
                strncpy(name, token_arr[0], index);
                name[index] = '\0';
                strcpy(value, token_arr[0] + index + 1);

                if (strlen(name) > 0) {
                    set_variable(name, value, first_node, last_node);
                }
            }
            else {
                display_error("ERROR: Extra arguments: ", token_arr[1]);
            }
        }
        else if (builtin_fn == NULL && command_fn == NULL) {
            char path1[256];
            char path2[256];
            snprintf(path1, sizeof(path1), "/usr/bin/%s", token_arr[0]);
            snprintf(path2, sizeof(path2), "/bin/%s", token_arr[0]);
            if (!(access(path1, X_OK) == 0 && access(path2, X_OK) == 0)) {
                display_error("ERROR: Unknown command: ", token_arr[0]);
                return;
            }
            int n = fork();
            if (n == 0) {
                execvp(token_arr[0], token_arr);
                internal_close();
                exit(1);
            }
            wait(NULL);
        }
    }
}

int check_if_pipe(char **token_arr, int token_count) {
    if (token_arr == NULL || token_count <= 0) {
        return 0;
    }
    for (int i = 0; i < token_count; i++) {
        if (token_arr[i] != NULL && strcmp(token_arr[i], "|") == 0) {
            return 1;
        }
        for (int a = 0; a < (int) strlen(token_arr[i]); a++) {
            if (token_arr[i][a] == '|'){
                return 1;
            }
        }
    }
    return 0;
}

int split_by_pipe(char **token_arr, int token_count, char **commands) {
    int count = 0;
    char buff[MAX_STR_LEN] = {0};
    int total_count = 0;
    for (int i = 0; i < token_count; i++) {
        for (int a = 0; a < (int)strlen(token_arr[i]); a++) {
            if (token_arr[i][a] == '|') {
                if (count > 0) {
                    buff[count] = '\0';
                    commands[total_count] = malloc((count + 1) * sizeof(char));
                    strncpy(commands[total_count], buff, count);
                    commands[total_count][count] = '\0';
                    total_count++;
                    count = 0;
                    memset(buff, 0, sizeof(buff));
                }
            } else {
                if (count < MAX_STR_LEN - 1) {
                    buff[count] = token_arr[i][a];
                    count++;
                }
            }
        }
        if (count > 0 && i < token_count - 1) {
            if (count < MAX_STR_LEN - 1) {
                buff[count] = ' ';
                count++;
            }
        }
    }
    if (count > 0) {
        buff[count] = '\0';
        commands[total_count] = malloc((count + 1) * sizeof(char));
        strncpy(commands[total_count], buff, count);
        commands[total_count][count] = '\0';
        total_count++;
    }
    commands[total_count] = NULL;
    return total_count;
}


int validation(char **token_arr, int token_count) {
    if (token_count == 0) return 0;
    if (strcmp(token_arr[0], "|") == 0 || strcmp(token_arr[token_count - 1], "|") == 0) {
        return 0;
    }
    int flag = 0;
    for (int i = 0; i < token_count; i++) {
        for (int a = 0; a < (int)strlen(token_arr[i]); a++) {
            if (flag == 1 && token_arr[i][a] == '|') {
                return 0;
            }
            if (token_arr[i][a] == '|') {
                flag = 1;
            }
            else if (token_arr[i][a] == ' ') {
                a++;
            }
            else {
                flag = 0;
            }
        }
    }
    return 1;
}

int count_pipes(const char *str) {
    int count = 0;
    const char *ptr = str;
    while ((ptr = strchr(ptr, '|')) != NULL) {
        count++;
        ptr++;
    }
    return count;
}

// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc,
         __attribute__((unused)) char* argv[]) {
    // save original output
    saved_stdout = dup(STDOUT_FILENO);
    // set up the signals

    // ctrl c
    signal(SIGINT, sigint_handler);

    // child ended update active children
    signal(SIGCHLD, sigchld_handler);

    // receive the kill signal
    signal(SIGUSR1, kill_handler);

    // print for ps
    signal(SIGUSR2, ps_handler);

    // close the server signal
    signal(SIGALRM, close_server_handler);
    char *prompt = "mysh$ ";

    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';

    // copy of the original since original is altered
    char temp_input_buf[MAX_STR_LEN + 1];
    temp_input_buf[MAX_STR_LEN] = '\0';

    // another list to tokenize based on |
    char *commands[MAX_STR_LEN] = {NULL};

    Node *first_node = NULL;
    Node *last_node = NULL;

    // flag for if in a child, 9 is irrelevant, just smth other than 0
    child = 9;

    while (1) {
        // ctrl c
        signal(SIGINT, sigint_handler);

        // child ended update active children
        signal(SIGCHLD, sigchld_handler);

        // receive the kill signal
        signal(SIGUSR1, kill_handler);

        // print for ps
        signal(SIGUSR2, ps_handler);

        // close the server signal
        signal(SIGALRM, close_server_handler);

        display_message(prompt);

        int ret = get_input(input_buf);
        strncpy(temp_input_buf, input_buf, MAX_STR_LEN);
        temp_input_buf[MAX_STR_LEN] = '\0';
        token_count = tokenize_input(input_buf, token_arr);

        // ctrl d case
         if (ret == 0) {
             display_message("\n");
             free_tokens(token_arr, token_count);
             free_all(first_node);
             if (child > 0) {
                internal_close();
             }
             exit(0);
         }

        // enter case
        if (ret == -1) {
            free_tokens(token_arr, token_count);
            continue;
        }

        int size = 0;
        int max = MAX_STR_LEN;

        // flag for expand
        int check = 0;
        int count = 0;


        int back_pid_pipe[2];
        if (pipe(back_pid_pipe) == -1) {
            display_error("ERROR: Pipe failed", "");
            continue;
        }

        // expand case
        for (size_t i = 0; i < token_count; i++) {
            if (check == 1) {
                if (strlen(token_arr[i]) > 0) {
                    token_arr[i][0] = '\0';
                }
            } else {
                expand_arr(token_arr[i], first_node);
                int expanded_len = strlen(token_arr[i]);

                if (size + expanded_len < max) {
                    size += expanded_len + 1;
                } else {
                    int remain = max - size;
                    token_arr[i][remain] = '\0';
                    check = 1;
                }
            }
        }

        // background case
        if (token_count > 0 && token_arr[token_count - 1] != NULL &&
            strcmp(token_arr[token_count - 1], "&") == 0) {
            // new processes will spawn, increase count
            count++;
            active_children++;
            current_children++;
            child = fork();
            char output[128];

            // store the child pid for the ps call unless there is a pipe
            if (child > 0 && (check_if_pipe(token_arr, token_count) == 0)) {
                if (server_pid == -1){
                    server_pid = child;
                    printf("%d\n", server_pid);
                }
                pidarr[amount] = child;
                amount++;
            }

            // remove the & in the child case
            if (child == 0) {
                token_count--;
                // free and remove the &
                if (token_arr[token_count] != NULL) {
                    free(token_arr[token_count]);
                    token_arr[token_count] = NULL;
                }
            }
            else if (child > 0) {
                if (server_pid == -1){
                    server_pid = child;
                    printf("%d\n", server_pid);
                }
                // queuing the child message
                sprintf(output, "[%d] %d\n", current_children, child);
                display_message(output);
                int received_pid = 0;
                int num_pipes = count_pipes(temp_input_buf);
                if (num_pipes > 0) {
                    for (int i = 0; i < num_pipes + 1; i++) {
                        if (read(back_pid_pipe[0], &received_pid, sizeof(received_pid)) <= 0) {
                            perror("ERROR: Failed to read PID");
                            break;
                        }
                        pidarr[amount++] = received_pid;
                    }
                }
                close(back_pid_pipe[0]);
                free_tokens(token_arr, token_count);
                continue;
            } else {
                display_error("ERROR: Fork failed", "");
            }
        }
        // pipe case
        if (check_if_pipe(token_arr, token_count)) {
            // check if the syntax is proper
            if (validation(token_arr, token_count) == 0) {
                display_error("ERROR: Invalid pipe syntax", "");
                free_tokens(token_arr, token_count);
                memset(token_arr, 0, sizeof(token_arr));
                continue;
            }

            int num_cmds = split_by_pipe(token_arr, token_count, commands);

            // if there are not enough tokens
            if (num_cmds -1 <= 0) {
                display_error("ERROR: Invalid command", "");
                free_tokens(token_arr, token_count);
                free_tokens(commands, num_cmds);
                continue;
            }

            int pipefd[2 * (num_cmds - 1)];

            // setup pipes for the commands
            for (int i = 0; i < num_cmds - 1; i++) {
                if (pipe(pipefd + (i * 2)) == -1) {
                    perror("pipe");
                    exit(1);
                }
            }

            // fork and setup std in and out for forks
            for (int i = 0; i < num_cmds; i++) {
                int pid = fork();

                if (pid == 0) {
                    close(back_pid_pipe[1]);
                    if (i > 0) {
                        dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
                    }
                    if (i < num_cmds - 1) {
                        dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
                    }

                    for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                        close(pipefd[j]);
                    }

                    free_tokens(token_arr, token_count);
                    memset(token_arr, 0, sizeof(token_arr));
                    token_count = tokenize_input(commands[i], token_arr);

                    handle_command(token_arr, token_count, ret, &first_node, &last_node);

                    free_tokens(token_arr, token_count);
                    free_tokens(commands, num_cmds);
                    exit(0);
                }
                // child of background parent of pipe
                else if (pid > 0 && child == 0) {
                    close(back_pid_pipe[0]);
                    if (write(back_pid_pipe[1], &pid, sizeof(pid)) <= 0) {
                        perror("ERROR: Failed to write PID");
                    }
                }
            }

            // close all pipes
            for (int i = 0; i < 2 * (num_cmds - 1); i++) {
                close(pipefd[i]);
            }
            // wait for all forks
            for (int i = 0; i < num_cmds; i++) {
                wait(NULL);
            }
            // in parent free everything
            if (child != 0) {
                free_tokens(token_arr, token_count);
                free_tokens(commands, num_cmds);
            }
        } else {
            // if its a simple command, call the handler
            handle_command(token_arr, token_count, ret, &first_node, &last_node);
            if (child != 0) {
                free_tokens(token_arr, token_count);
            }
        }

        // printing completion for child
        if (child == 0) {
            char output[128];
            sprintf(output, "[%d]+  Done ", current_children);
            display_message(output);
            // print the tokens
            for (int idc=0; idc<(int)token_count; idc++) {
                display_message(token_arr[idc]);
                if (idc == (int)token_count - 1) {continue;}
                display_message(" ");
            }
            display_message("\n");
            display_message("mysh$ ");
            free_tokens(token_arr, token_count);
            exit(0);
        }
    }
    free_all(first_node);
    if (child > 0){
        internal_close();
    }
    return 0;
}