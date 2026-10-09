#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "history.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"


int main(void)
{
    /* ShellForge welcome message */
    printf("=====================================\n");
    printf("           ShellForge\n");
    printf("     A Unix Style Shell in C\n");
    printf("=====================================\n");

    /* Start background process handling */
    setup_background_handler();

    /* Enable Readline history */
    using_history();

    token_list_t tokens;
    pipeline_t pipeline;
    char *input;

    while (1)
    {
        /* Read command from user */
        input = readline("shellforge$ ");

        /* Ctrl+D */
        if (input == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        /* Ignore empty input */
        if (strlen(input) == 0)
        {
            free(input);
            continue;
        }


        /* Built-in history command */
        if (strcmp(input, "history") == 0)
        {
            print_history();
            free(input);
            continue;
        }


        /* Store command in history */
        add_history(input);


        /* Convert input into tokens */
        lexer(input, &tokens);


        /* Parse tokens into commands */
        if (parser(&tokens, &pipeline))
        {
            /* Expand variables such as $HOME and $USER */
            expand_variables(&pipeline);
        }
        else
        {
            printf("ShellForge: invalid command\n");
            free(input);
            continue;
        }


        /* Handle exit command */
        if (pipeline.command_count == 1 &&
            pipeline.commands[0].argc > 0 &&
            strcmp(pipeline.commands[0].argv[0], "exit") == 0)
        {
            free(input);
            break;
        }


        /* Execute command or pipeline */
        if (pipeline.command_count > 0)
        {
            execute_pipeline(&pipeline);
        }


        /* Free input memory */
        free(input);
    }

    return 0;
}
