cd ~/shellforge && cat > src/job_control.c <<'EOF'
#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "job_control.h"

static pid_t shell_pgid;

/* Set up the shell process group */
void job_control_init(void)
{
    if (!isatty(STDIN_FILENO))
        return;

    shell_pgid = getpid();

    if (setpgid(shell_pgid, shell_pgid) < 0)
        perror("setpgid");

    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0)
        perror("tcsetpgrp");

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}

/* Give terminal control to a job */
void give_terminal_to(pid_t pgid)
{
    if (isatty(STDIN_FILENO))
        tcsetpgrp(STDIN_FILENO, pgid);
}

/* Return terminal control to the shell */
void take_terminal_back(void)
{
    if (isatty(STDIN_FILENO))
        tcsetpgrp(STDIN_FILENO, shell_pgid);
}

/* Return shell process group */
pid_t get_shell_pgid(void)
{
    return shell_pgid;
}
EOF
