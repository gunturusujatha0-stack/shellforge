
#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

/* Built-in command management */
int is_builtin(const command_t *cmd);
int execute_builtin(command_t *cmd);

/* Job control */
int builtin_jobs(command_t *cmd);
int builtin_fg(command_t *cmd);
int builtin_bg(command_t *cmd);
int builtin_status(command_t *cmd);
int builtin_jobinfo(command_t *cmd);

/* Shell information */
int builtin_help(command_t *cmd);
int builtin_clear(command_t *cmd);
int builtin_whoami(command_t *cmd);
int builtin_version(command_t *cmd);

/* Environment and navigation */
int builtin_env(command_t *cmd);
int builtin_pwd(command_t *cmd);

/* Extra utilities */
int builtin_date(command_t *cmd);
int builtin_uptime(command_t *cmd);

#endif
