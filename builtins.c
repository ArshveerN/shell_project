#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>

#include "builtins.h"
#include "io_helpers.h"
int active_children = 0;
int current_children = 0;
int pidarr[1000] = {};
int amount = 0;
size_t tokenize_path(char *in_ptr, char **tokens) {
    char *curr_ptr = strtok (in_ptr, "/");
    size_t token_count = 0;

    while (curr_ptr != NULL) {
        tokens[token_count] = curr_ptr;
        curr_ptr = strtok(NULL, "/");
        token_count++;
    }
    tokens[token_count] = NULL;
    return token_count;
}

int is_valid_signal(int sig) {
    struct sigaction act;
    return (sigaction(sig, NULL, &act) == 0);
}

// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo.
 */
ssize_t bn_echo(char **tokens) {
    ssize_t index = 1;

    if (tokens[index] != NULL) {

        // Implement the echo command
        display_message(tokens[index]);
        index++;
    }
    while (tokens[index] != NULL) {
           // Implement the echo command
        display_message(" ");
        display_message(tokens[index]);
        index += 1;
    }
    display_message("\n");

    return 0;
}

int is_all_digits(const char *input) {
    for (int i = 0; input[i] != '\0'; i++) {
        if (!isdigit((unsigned char)input[i])) {
            return -1;
    }
    }
    return 0;
}


int recursive_method(int remaining_depth, char *path, char *sub_name) {
    if (remaining_depth == 0) {
            return 0;
    } else if (strcmp(path, "")!=0 && access(path, F_OK) != 0) {return 0;}

    if (strcmp(path, "") == 0) {getcwd(path, sizeof(path));}

    if ((access(path, F_OK) != 0)) {
        display_error("ERROR: Invalid path", "");
        return -2;
    }

    if (remaining_depth < -1) {return 0;}

    if (remaining_depth == -1) {
        DIR *pointer = opendir(path);
        if (pointer == NULL) {
                display_message(path);
                display_message("\n");
                return 0;
        }
        if (strcmp(sub_name, "") == 0) {
            display_message(".\n");
                if (strcmp(path, "/") != 0) {
                    display_message("..\n");
            }
        }
        struct dirent *line;
        while ((line = readdir(pointer)) != NULL) {
            if (strcmp(line->d_name, ".") == 0 || strcmp(line->d_name, "..") == 0) {continue;}
        if ((line->d_name)[0] == '.'){continue;}
            if (strcmp(sub_name, "") == 0 || strstr(line->d_name, sub_name)!=NULL) {
                display_message(line->d_name);
                display_message("\n");
            }
        }
        closedir(pointer);
    }

    if (remaining_depth >0) {
        DIR *pointer = opendir(path);
        if (pointer == NULL) {
            return -1;
        }
        struct dirent *line;

        while ((line = readdir(pointer)) != NULL) {
            if (strcmp(line->d_name, ".") == 0 || strcmp(line->d_name, "..") == 0) {
                continue;
            }
            if ((line->d_name)[0] == '.'){continue;}
            if (strcmp(sub_name, "") == 0 || strstr(line->d_name, sub_name)!=NULL) {
                display_message(line->d_name);
                display_message("\n");
            }
        }

        if (strcmp(sub_name, "") == 0) {
            display_message(".\n");
            if (strcmp(path, "/") != 0) {
                display_message("..\n");
            }
        }
        rewinddir(pointer);
        while ((line = readdir(pointer)) != NULL) {
            if ((line->d_name)[0] == '.'){continue;}
            if (strcmp(line->d_name, ".") == 0 || strcmp(line->d_name, "..") == 0) {
                continue;
            }
            char temp_file_name[PATH_MAX]="";
            snprintf(temp_file_name, sizeof(temp_file_name), "%s/%s", path, line->d_name);
            recursive_method(remaining_depth-1, temp_file_name, sub_name);

        }
        closedir(pointer);
    }
    return 0;
}

ssize_t bn_ls(char **tokens) {
    int d = -1;
    int rec = -1;
    char sub_name[PATH_MAX] = "";
    char path[PATH_MAX] = ".";

    for (int i = 1; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "--rec") == 0) {
            rec = 1;
        }
        else if (strcmp(tokens[i], "--d") == 0) {
            if (tokens[i + 1] == NULL) {
                display_error("ERROR: Missing value", "");
            return -1;
            }
            if (is_all_digits(tokens[i+1]) == -1){
                display_error("ERROR: Invalid depth ", tokens[i+1]);
                return -1;
            }

            if (d != -1 && d != atoi(tokens[i+1])) {
                display_error("ERROR: Multiple values passed", "");
                return -1;
            }
            d = atoi(tokens[i + 1]);

            if (d<0) {
                display_error("ERROR: Invalid depth ", tokens[i+1]);
                return -1;
            }
            i++;
        }

        else if (strcmp(tokens[i], "--f") == 0) {
            if (tokens[i + 1] == NULL) {
                display_error("ERROR: Invalid substring", "");
                return -1;
            }
            if (strcmp(sub_name, "") != 0) {
                display_error("ERROR: Multiple substrings passed", "");
                return -1;
            }
            if (strncmp(tokens[i+1], "--", 2) == 0) {
                display_error("ERROR: Invalid substring ", tokens[i+1]);
                return -1;
            }
            strncpy(sub_name, tokens[i + 1], sizeof(sub_name) - 1);
            sub_name[sizeof(sub_name) - 1] = '\0';
            i++;
    } else if (access(tokens[i], F_OK) == 0) {
        if (strcmp(path, ".") == 0) {
            strncpy(path, tokens[i], sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        } else if (strcmp(tokens[i], path) != 0) {
            continue;
        }
    } else {
        display_error("ERROR: Invalid path", "");
        return -1;
        }
    }

    if (d != -1 && rec == -1) {
        display_error("ERROR: --d requires --rec", "");
        return -1;
    }

    if (d == -1 && rec == 1) {
        d = 99999;
    }
    if (strcmp(path, ".")!=0 && d == 0) {
        display_message(".");
        display_message("\n");
        return 0;
    } else if (d == 0) {
        display_message(".");
        display_message("\n");
        return 0;
    }

    struct stat path_stat;
    if (stat(path, &path_stat) == 0) {
        if (S_ISREG(path_stat.st_mode)) {
            d = -1;
        }
    }

    int i = recursive_method(d, path, sub_name);
    if (i == -2) {return -1;}
    return 0;
}

ssize_t bn_ps( __attribute__((unused))char **tokens) {
    for (int i = 0; i < amount; i++) {
        if (kill(pidarr[i], 0) == 0) {
            kill(pidarr[i], SIGUSR2);
        }
        usleep(100);
    }
    return 0;
}


ssize_t bn_kill(char **tokens) {
    if (tokens[1] == NULL) {
        display_error("ERROR: No process passed", "");
        return -1;
    }
    pid_t pid = (pid_t)atoi(tokens[1]);
    if (pid == 0) {
        display_error("ERROR: Invalid process ID ", tokens[1]);
        return -1;
    }
    if (kill(pid, 0) != 0) {
        display_error("ERROR: The process does not exist ", tokens[1]);
        return -1;
    }
    kill(pid, SIGUSR1);
    usleep(400);
    if (tokens[2]) {
        int signal = atoi(tokens[2]);
        if (!(signal > 0 && signal <= SIGRTMAX)) {
            display_error("ERROR: Invalid signal specified ", tokens[2]);
            return -1;
        }
        kill(pid, signal);
    } else {
        kill(pid, SIGTERM);
    }
    return 0;
}


ssize_t bn_cd(char **tokens) {
    if (tokens == NULL || tokens[1] == NULL) {
        display_error("ERROR: No path provided", "");
        return -1;
    }
    if (tokens[2] != NULL) {
        display_error("ERROR: Too many arguments: cd takes a single path", "");
        return -1;
    }

    char path_copy[512];
    strncpy(path_copy, tokens[1], sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *path_tokens[256];
    size_t num_tokens = tokenize_path(path_copy, path_tokens);

    char final_path[512] = "";

    if (tokens[1][0] == '/') {
        strcpy(final_path, "/");
    } else {
        getcwd(final_path, sizeof(final_path));
    }

    for (size_t i = 0; i < num_tokens; i++) {
        int dot_count = 0;
        while (path_tokens[i][dot_count] == '.') {
            dot_count++;
        }

        if (dot_count == 0) {
            if (strlen(final_path) > 1) {
                strncat(final_path, "/", sizeof(final_path) - strlen(final_path) - 1);
            }
            strncat(final_path, path_tokens[i], sizeof(final_path) - strlen(final_path) - 1);

        } else if (dot_count == 1) {
            continue;

        } else {
            for (int j = 0; j < dot_count - 1; j++) {
                char *last_slash = strrchr(final_path, '/');
                if (last_slash != NULL && last_slash != final_path) {
                    *last_slash = '\0';
                } else {
                    strcpy(final_path, "/");
                    break;
                }
            }
        }
    }

    if (final_path[0] == '\0') {
        strcpy(final_path, "/");
    }


    struct stat statbuf;
    if (stat(final_path, &statbuf) != 0) {
        display_error("ERROR: Invalid path", "");
        return -1;
    }
    if (chdir(final_path) != 0) {
        display_error("ERROR: Invalid path", "");
        return -1;
    }

    return 0;
}



ssize_t bn_cat(char **tokens){
    if (tokens[2] != NULL) {
        display_error("ERROR: Too many arguments: cat takes a single file", "");
        return -1;
    }

    FILE *file;
    int is_stdin = 0;

    if (tokens[1] == NULL) {
        file = stdin;
        is_stdin = 1;
    } else {
        char *file_name = tokens[1];
        file = fopen(file_name, "r");

        if (file == NULL) {
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        display_message(line);
    }

    if (is_stdin) {
        clearerr(stdin);
    } else {
        fclose(file);
    }

    return 0;
}

ssize_t bn_wc(char **tokens) {

    if (tokens[2] != NULL) {
        display_error("ERROR: Too many arguments: wc takes a single file", "");
        return -1;
    }

    FILE *file;
    int is_stdin = 0;

    if (tokens[1] == NULL) {
        file = stdin;
        is_stdin = 1;
    } else {
        char *file_name = tokens[1];
        file = fopen(file_name, "r");

        if (file == NULL) {
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
    }

    int num_words = 0, num_chars = 0, num_lines = 0;
    int t_num_words = 0, t_num_chars = 0;
    int seen = 0;
    int a;

    while ((a = fgetc(file)) != EOF) {

        if (a != ' ' && a != '\t' && a != '\r' && a != '\n') {
            t_num_chars++;
            seen = 1;
        } else if (seen == 1 && (a == ' ' || a == '\t')) {
            t_num_words++;
            t_num_chars++;
            seen = 0;

        } else if (a==' ' || a=='\t') {
            t_num_chars++;

        } else if (a == '\n') {
            num_lines++;

            if (seen == 1) {
                num_words += (t_num_words + 1);
            } else {
                num_words += t_num_words;
            }
            num_chars += t_num_chars;
            num_chars++;

            t_num_words = 0;
            t_num_chars = 0;
            seen = 0;
        } else if (a == '\r'){
            t_num_chars++;
            if (seen == 1){
                t_num_words++;
                seen=0;
            }
        }
    }

    if (seen == 1) {
        num_words++;
    }

    if (t_num_chars > 0) {
        num_chars += t_num_chars;
    }
    if (t_num_words > 0) {
        num_words += t_num_words;
    }

    char outw[128];
    char outl[128];
    char outc[128];

    sprintf(outw, "word count %d\n", num_words);
    sprintf(outc, "character count %d\n", num_chars);
    sprintf(outl, "newline count %d\n", num_lines);

    display_message(outw);
    display_message(outc);
    display_message(outl);

    if (is_stdin) {
        clearerr(stdin);
    } else {
        fclose(file);
    }

    return 0;
}