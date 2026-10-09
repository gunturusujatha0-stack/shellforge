
#ifndef SHELLFORGE_EXECUTOR_H
#define SHELLFORGE_EXECUTOR_H

#include "parser.h"

/* Execute one parsed command */
int execute_command(command_t *command);

/* Execute a complete pipeline */
int execute_pipeline(pipeline_t *pipeline);

/* Initialize background process handling */
void setup_background_handler(void);
void process_completed_jobs(void);
#endif
