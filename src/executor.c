#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "parser.h"
#include "executor.h"
#include "builtin.h"
#include "jobs.h"
static volatile sig_atomic_t child_exit_pending = 0;


/* =========================================================
   HANDLE COMPLETED BACKGROUND PROCESSES
   ========================================================= */

static void handle_child_exit(int signal_number)
{
    int old_errno = errno;
    (void)signal_number;
    child_exit_pending = 1;
    errno = old_errno;
}

/* =========================================================
   SETUP BACKGROUND PROCESS SUPPORT
   ========================================================= */

void setup_background_handler(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));

    action.sa_handler = handle_child_exit;
    sigemptyset(&action.sa_mask);

    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &action, NULL) == -1)
    {
        perror("SIGCHLD");
    }
}


void process_completed_jobs(void)
{
    int old_errno = errno;
    int status;
    pid_t child_pid;

    child_exit_pending = 0;
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        job_t *job = job_find_by_pgid(child_pid);
        if (job != NULL)
            job_done(job->pgid);
    }
    errno = old_errno;
}


/* =========================================================
   REDIRECTION HANDLING
   ========================================================= */

static int redirect_io(command_t *command)
{
    int file_descriptor;

    /* ---------- INPUT: < ---------- */

    if (command->input[0] != '\0')
    {
        file_descriptor = open(command->input, O_RDONLY);

        if (file_descriptor == -1)
        {
            perror(command->input);
            return -1;
        }

        if (dup2(file_descriptor, STDIN_FILENO) == -1)
        {
            perror("input redirection");

            close(file_descriptor);
            return -1;
        }

        close(file_descriptor);
    }


    /* ---------- OUTPUT: > or >> ---------- */

    if (command->output[0] != '\0')
    {
        if (command->append)
        {
            /* Append to existing file */
            file_descriptor = open(
                command->output,
                O_WRONLY | O_CREAT | O_APPEND,
                0644
            );
        }
        else
        {
            /* Create or overwrite file */
            file_descriptor = open(
                command->output,
                O_WRONLY | O_CREAT | O_TRUNC,
                0644
            );
        }

        if (file_descriptor == -1)
        {
            perror(command->output);
            return -1;
        }

        if (dup2(file_descriptor, STDOUT_FILENO) == -1)
        {
            perror("output redirection");

            close(file_descriptor);
            return -1;
        }

        close(file_descriptor);
    }

    return 0;
}


/* =========================================================
   EXECUTE ONE COMMAND
   ========================================================= */

int execute_command(command_t *command)
{
    pid_t child_pid;
    int child_status;

    if (command == NULL)
        return -1;

    if (command->argc == 0)
        return 0;


    /* ---------- FOREGROUND BUILTIN ---------- */

    if (is_builtin(command) && !command->background)
    {
        return execute_builtin(command);
    }


    /* ---------- CREATE CHILD ---------- */

    child_pid = fork();

    if (child_pid == -1)
    {
        perror("fork");
        return -1;
    }


    /* ---------- CHILD ---------- */

    if (child_pid == 0)
    {
        char *arguments[MAX_ARGS + 1];

        /* Setup input/output files */
        if (redirect_io(command) < 0)
        {
            _exit(EXIT_FAILURE);
        }

        /* Copy command arguments */
        for (int index = 0; index < command->argc; index++)
        {
            arguments[index] = command->argv[index];
        }

        arguments[command->argc] = NULL;


        /* Execute builtin inside child
           when it is a background command */
        if (is_builtin(command))
        {
            int result = execute_builtin(command);
            _exit(result);
        }


        /* Execute normal Linux command */
        execvp(arguments[0], arguments);

        /* Reached only if execvp failed */
        perror(arguments[0]);
        _exit(127);
    }


    /* ---------- PARENT ---------- */

    if (command->background)
    {
        printf("[Background PID: %d]\n", child_pid);
        job_add(child_pid, command->argv[0], JOB_RUNNING);

        /* Do not wait for background process */
        return 0;
    }


    /* Wait for foreground process */
    if (waitpid(child_pid, &child_status, 0) == -1)
    {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(child_status))
    {
        return WEXITSTATUS(child_status);
    }

    return -1;
}


/* =========================================================
   EXECUTE COMMAND PIPELINE
   ========================================================= */

int execute_pipeline(pipeline_t *pipeline)
{
    int total_commands;
    int input_pipe = -1;

    pid_t process_ids[MAX_COMMANDS];

    int status;
    int result = 0;

    if (pipeline == NULL)
        return -1;

    total_commands = pipeline->command_count;

    if (total_commands <= 0)
        return 0;


    /* Background status comes from the final command */
    int run_in_background =
        pipeline->commands[total_commands - 1].background;


    /* ---------- ONLY ONE COMMAND ---------- */

    if (total_commands == 1)
    {
        return execute_command(&pipeline->commands[0]);
    }


    /* ---------- MULTIPLE COMMANDS ---------- */

    for (int current = 0; current < total_commands; current++)
    {
        int new_pipe[2];

        /* Create pipe for every command except last */
        if (current < total_commands - 1)
        {
            if (pipe(new_pipe) == -1)
            {
                perror("pipe");
                return -1;
            }
        }


        /* Create process */
        process_ids[current] = fork();

        if (process_ids[current] == -1)
        {
            perror("fork");
            return -1;
        }


        /* =================================================
           CHILD PROCESS
           ================================================= */

        if (process_ids[current] == 0)
        {
            command_t *command =
                &pipeline->commands[current];

            char *arguments[MAX_ARGS + 1];


            /* Connect previous command to stdin */
            if (input_pipe != -1)
            {
                if (dup2(input_pipe, STDIN_FILENO) == -1)
                {
                    perror("previous pipe");
                    _exit(EXIT_FAILURE);
                }
            }


            /* Connect stdout to next command */
            if (current < total_commands - 1)
            {
                if (dup2(new_pipe[1], STDOUT_FILENO) == -1)
                {
                    perror("next pipe");
                    _exit(EXIT_FAILURE);
                }
            }


            /* Close old pipe */
            if (input_pipe != -1)
            {
                close(input_pipe);
            }


            /* Close current pipe descriptors */
            if (current < total_commands - 1)
            {
                close(new_pipe[0]);
                close(new_pipe[1]);
            }


            /* Apply file redirection */
            if (redirect_io(command) < 0)
            {
                _exit(EXIT_FAILURE);
            }


            /* Prepare argument list */
            for (int index = 0;
                 index < command->argc;
                 index++)
            {
                arguments[index] =
                    command->argv[index];
            }

            arguments[command->argc] = NULL;


            /* Builtin inside pipeline */
            if (is_builtin(command))
            {
                int builtin_result =
                    execute_builtin(command);

                _exit(builtin_result);
            }


            /* Run external command */
            execvp(arguments[0], arguments);

            perror(arguments[0]);
            _exit(127);
        }


        /* =================================================
           PARENT PROCESS
           ================================================= */

        /* Previous pipe is no longer required */
        if (input_pipe != -1)
        {
            close(input_pipe);
        }


        /* Save pipe for next command */
        if (current < total_commands - 1)
        {
            close(new_pipe[1]);
            input_pipe = new_pipe[0];
        }
        else
        {
            input_pipe = -1;
        }
    }


    /* =====================================================
       BACKGROUND PIPELINE
       ===================================================== */

    if (run_in_background)
    {
        printf(
            "[Background Pipeline PID: %d]\n",
            process_ids[0]
        );

        /* Parent does not wait */
        return 0;
    }


    /* =====================================================
       FOREGROUND PIPELINE
       ===================================================== */

    for (int current = 0;
         current < total_commands;
         current++)
    {
        status = 0;

        if (waitpid(process_ids[current], &status, 0) == -1)
        {
            perror("waitpid");
            continue;
        }

        /* Return status of final command */
        if (current == total_commands - 1)
        {
            if (WIFEXITED(status))
            {
                result = WEXITSTATUS(status);
            }
            else
            {
                result = -1;
            }
        }
    }

    return result;
}
