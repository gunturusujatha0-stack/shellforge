#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

/* Check whether a command is a built-in command */
int is_builtin(const command_t *cmd);

/* Execute a built-in command */
int execute_builtin(command_t *cmd);

/* New built-in commands */
int builtin_clear(command_t *cmd);
int builtin_help(command_t *cmd);
int builtin_whoami(command_t *cmd);

#endif
