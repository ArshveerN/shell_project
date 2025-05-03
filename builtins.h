#ifndef __BUILTINS_H__
#define __BUILTINS_H__

#include <unistd.h>

extern int active_children;
extern int current_children;
extern int pidarr[1000];
extern int amount;
/* Type for builtin handling functions
 * Input: Array of tokens
 * Return: >=0 on success and -1 on error
 */
typedef ssize_t (*bn_ptr)(char **);
ssize_t bn_echo(char **tokens);
ssize_t bn_cat(char **tokens);
ssize_t bn_ls(char **tokens);
ssize_t bn_wc(char **tokens);
ssize_t bn_cd(char **tokens);
ssize_t bn_kill(char **tokens);
ssize_t bn_ps( __attribute__((unused))char **tokens);

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd);


/* BUILTINS and BUILTINS_FN are parallel arrays of length BUILTINS_COUNT
 */
static const char * const BUILTINS[] = {"echo", "cat", "ls", "wc", "cd", "kill", "ps"};
static const bn_ptr BUILTINS_FN[] = {bn_echo, bn_cat, bn_ls, bn_wc, bn_cd, bn_kill, bn_ps, NULL};    // Extra null element for 'non-builtin'
static const ssize_t BUILTINS_COUNT = sizeof(BUILTINS) / sizeof(char *);

#endif