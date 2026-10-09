
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include "builtin.h"
#include "jobs.h"
#include "job_control.h"

extern char **environ;

/* Change directory */
static int builtin_cd(command_t *cmd)
{
    const char *path;

    if (cmd->argc == 1)
        path = getenv("HOME");
    else if (cmd->argc == 2)
        path = cmd->argv[1];
    else {
        fprintf(stderr, "cd: too many arguments\n");
        return -1;
    }

    if (path == NULL || chdir(path) != 0) {
        perror("cd");
        return -1;
    }

    return 0;
}

/* Print current directory */
int builtin_pwd(command_t *cmd)
{
    char path[4096];

    (void)cmd;

    if (getcwd(path, sizeof(path)) == NULL) {
        perror("pwd");
        return -1;
    }

    printf("%s\n", path);
    return 0;
}

/* Print arguments */
static int builtin_echo(command_t *cmd)
{
    for (int i = 1; i < cmd->argc; i++) {
        printf("%s", cmd->argv[i]);

        if (i < cmd->argc - 1)
            printf(" ");
    }

    printf("\n");
    return 0;
}

/* Exit shell */
static int builtin_exit(command_t *cmd)
{
    if (cmd->argc > 1) {
        fprintf(stderr, "exit: too many arguments\n");
        return -1;
    }

    return 1;
}

/* Show available commands */
int builtin_help(command_t *cmd)
{
    (void)cmd;

    printf("ShellForge Commands\n");
    printf("-------------------\n");
    printf("cd       - Change directory\n");
    printf("pwd      - Show current directory\n");
    printf("echo     - Print text\n");
    printf("exit     - Exit ShellForge\n");
    printf("jobs     - List tracked jobs\n");
    printf("fg       - Resume a job in foreground\n");
    printf("bg       - Resume a job in background\n");
    printf("status   - Show job status summary\n");
    printf("jobinfo  - Show details of a job\n");
    printf("clear    - Clear the terminal\n");
    printf("whoami   - Show username\n");
    printf("version  - Show ShellForge version\n");
    printf("env      - Show environment variables\n");
    printf("date     - Show current date and time\n");
    printf("uptime   - Show system uptime\n");
    printf("help     - Show this help\n");

    return 0;
}

/* Clear terminal */
int builtin_clear(command_t *cmd)
{
    (void)cmd;
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}

/* Show current user */
int builtin_whoami(command_t *cmd)
{
    const char *user = getenv("USER");

    (void)cmd;

    if (user == NULL)
        user = "unknown";

    printf("%s\n", user);
    return 0;
}

/* Show shell version */
int builtin_version(command_t *cmd)
{
    (void)cmd;
    printf("ShellForge version 1.1\n");
    printf("A Unix-style shell written in C\n");
    return 0;
}

/* Print environment variables */
int builtin_env(command_t *cmd)
{
    (void)cmd;

    for (char **item = environ; *item != NULL; item++)
        printf("%s\n", *item);

    return 0;
}

/* Display date and time */
int builtin_date(command_t *cmd)
{
    time_t now = time(NULL);
    struct tm *current = localtime(&now);
    char output[100];

    (void)cmd;

    if (current == NULL ||
        strftime(output, sizeof(output),
                 "%a %b %d %H:%M:%S %Z %Y", current) == 0) {
        fprintf(stderr, "date: unable to get date\n");
        return -1;
    }

    printf("%s\n", output);
    return 0;
}

/* Display system uptime */
int builtin_uptime(command_t *cmd)
{
    FILE *file;
    double seconds;

    (void)cmd;

    file = fopen("/proc/uptime", "r");
    if (file == NULL) {
        perror("uptime");
        return -1;
    }

    if (fscanf(file, "%lf", &seconds) != 1) {
        fclose(file);
        fprintf(stderr, "uptime: cannot read system uptime\n");
        return -1;
    }

    fclose(file);

    printf("System uptime: %.0f seconds\n", seconds);
    return 0;
}

/* List jobs */
int builtin_jobs(command_t *cmd)
{
    (void)cmd;
    jobs_print();
    return 0;
}

/* Show job status summary */
int builtin_status(command_t *cmd)
{
    int running = 0, stopped = 0, done = 0;

    (void)cmd;

    for (int i = 1; i <= MAX_JOBS; i++) {
        job_t *job = job_find(i);

        if (job == NULL)
            continue;

        if (job->state == JOB_RUNNING)
            running++;
        else if (job->state == JOB_STOPPED)
            stopped++;
        else if (job->state == JOB_DONE)
            done++;
    }

    printf("Job status summary\n");
    printf("Running: %d\n", running);
    printf("Stopped: %d\n", stopped);
    printf("Done:    %d\n", done);

    return 0;
}

/* Show details for one job */
int builtin_jobinfo(command_t *cmd)
{
    int id;
    job_t *job;

    if (cmd->argc != 2) {
        fprintf(stderr, "Usage: jobinfo <job_id>\n");
        return -1;
    }

    id = atoi(cmd->argv[1]);
    if (id <= 0) {
        fprintf(stderr, "jobinfo: invalid job ID\n");
        return -1;
    }

    job = job_find(id);
    if (job == NULL) {
        fprintf(stderr, "jobinfo: job not found\n");
        return -1;
    }

    printf("Job ID:   %d\n", job->job_id);
    printf("PID/PGID: %d\n", (int)job->pgid);
    printf("Command:  %s\n", job->command);
    printf("State:    ");

    if (job->state == JOB_RUNNING)
        printf("Running\n");
    else if (job->state == JOB_STOPPED)
        printf("Stopped\n");
    else
        printf("Done\n");

    return 0;
}

/* Bring a job to the foreground */
int builtin_fg(command_t *cmd)
{
    int id, status;
    job_t *job;

    if (cmd->argc != 2) {
        fprintf(stderr, "Usage: fg <job_id>\n");
        return -1;
    }

    id = atoi(cmd->argv[1]);
    job = job_find(id);

    if (id <= 0 || job == NULL) {
        fprintf(stderr, "fg: job not found\n");
        return -1;
    }

    pid_t pgid = job->pgid;

    give_terminal_to(pgid);

    if (job->state == JOB_STOPPED) {
        if (kill(-pgid, SIGCONT) == -1)
            perror("fg: SIGCONT");

        job_continue(pgid);
    }

    while (1) {
        pid_t result = waitpid(-pgid, &status, WUNTRACED);

        if (result == -1) {
            perror("fg: waitpid");
            break;
        }

        if (WIFSTOPPED(status)) {
            job_stop(pgid);
            break;
        }

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_done(pgid);
            break;
        }
    }

    take_terminal_back();

    if (job->state == JOB_DONE)
        job_remove(id);

    return 0;
}

/* Continue a job in the background */
int builtin_bg(command_t *cmd)
{
    int id;
    job_t *job;

    if (cmd->argc != 2) {
        fprintf(stderr, "Usage: bg <job_id>\n");
        return -1;
    }

    id = atoi(cmd->argv[1]);
    job = job_find(id);

    if (id <= 0 || job == NULL) {
        fprintf(stderr, "bg: job not found\n");
        return -1;
    }

    if (kill(-job->pgid, SIGCONT) == -1) {
        perror("bg: SIGCONT");
        return -1;
    }

    job_continue(job->pgid);
    printf("[%d] %s &\n", job->job_id, job->command);

    return 0;
}

/* Check whether a command is built in */
int is_builtin(const command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return 0;

    const char *name = cmd->argv[0];

    return strcmp(name, "cd") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "echo") == 0 ||
           strcmp(name, "exit") == 0 ||
           strcmp(name, "jobs") == 0 ||
           strcmp(name, "fg") == 0 ||
           strcmp(name, "bg") == 0 ||
           strcmp(name, "status") == 0 ||
           strcmp(name, "jobinfo") == 0 ||
           strcmp(name, "help") == 0 ||
           strcmp(name, "clear") == 0 ||
           strcmp(name, "whoami") == 0 ||
           strcmp(name, "version") == 0 ||
           strcmp(name, "env") == 0 ||
           strcmp(name, "date") == 0 ||
           strcmp(name, "uptime") == 0;
}

/* Run the selected built-in */
int execute_builtin(command_t *cmd)
{
    if (cmd == NULL || cmd->argc == 0)
        return -1;

    const char *name = cmd->argv[0];

    if (strcmp(name, "cd") == 0)      return builtin_cd(cmd);
    if (strcmp(name, "pwd") == 0)     return builtin_pwd(cmd);
    if (strcmp(name, "echo") == 0)    return builtin_echo(cmd);
    if (strcmp(name, "exit") == 0)    return builtin_exit(cmd);
    if (strcmp(name, "jobs") == 0)    return builtin_jobs(cmd);
    if (strcmp(name, "fg") == 0)      return builtin_fg(cmd);
    if (strcmp(name, "bg") == 0)      return builtin_bg(cmd);
    if (strcmp(name, "status") == 0)  return builtin_status(cmd);
    if (strcmp(name, "jobinfo") == 0) return builtin_jobinfo(cmd);
    if (strcmp(name, "help") == 0)    return builtin_help(cmd);
    if (strcmp(name, "clear") == 0)   return builtin_clear(cmd);
    if (strcmp(name, "whoami") == 0)  return builtin_whoami(cmd);
    if (strcmp(name, "version") == 0) return builtin_version(cmd);
    if (strcmp(name, "env") == 0)     return builtin_env(cmd);
    if (strcmp(name, "date") == 0)    return builtin_date(cmd);
    if (strcmp(name, "uptime") == 0)  return builtin_uptime(cmd);

    return -1;
}

