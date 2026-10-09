
#include <stdio.h>
#include <string.h>
#include "jobs.h"

static job_t jobs[MAX_JOBS];
static int next_id = 1;

/* Initialize jobs */
void jobs_init(void)
{
    memset(jobs, 0, sizeof(jobs));
    next_id = 1;
}

/* Add a background job */
int job_add(pid_t pgid, const char *command, job_state_t state)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (jobs[i].job_id == 0)
        {
            jobs[i].job_id = next_id++;
            jobs[i].pgid = pgid;
            jobs[i].state = state;

            snprintf(jobs[i].command,
                     sizeof(jobs[i].command),
                     "%s", command);

            return jobs[i].job_id;
        }
    }

    printf("Maximum jobs reached\n");
    return -1;
}

/* Find job using ID */
job_t *job_find(int id)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (jobs[i].job_id == id)
            return &jobs[i];
    }

    return NULL;
}

/* Find job using process group */
job_t *job_find_by_pgid(pid_t pgid)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (jobs[i].job_id != 0 && jobs[i].pgid == pgid)
            return &jobs[i];
    }

    return NULL;
}

/* Remove completed job */
void job_remove(int id)
{
    job_t *job = job_find(id);

    if (job != NULL)
        memset(job, 0, sizeof(job_t));
}

/* Update job state */
void job_stop(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job != NULL)
        job->state = JOB_STOPPED;
}

void job_continue(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job != NULL)
        job->state = JOB_RUNNING;
}

void job_done(pid_t pgid)
{
    job_t *job = job_find_by_pgid(pgid);

    if (job != NULL)
        job->state = JOB_DONE;
}

/* Display job list */
void jobs_print(void)
{
    for (int i = 0; i < MAX_JOBS; i++)
    {
        if (jobs[i].job_id == 0)
            continue;

        const char *state = "Unknown";

        if (jobs[i].state == JOB_RUNNING)
            state = "Running";
        else if (jobs[i].state == JOB_STOPPED)
            state = "Stopped";
        else if (jobs[i].state == JOB_DONE)
            state = "Done";

        printf("[%d] %-10s %s\n",
               jobs[i].job_id,
               state,
               jobs[i].command);
    }
}
