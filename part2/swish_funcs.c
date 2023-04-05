#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
// may need to remove
#include <string.h>
#include "string_vector.h"
#include "swish_funcs.h"

#define MAX_ARGS 10

/*
 * Helper function to run a single command within a pipeline. You should make
 * make use of the provided 'run_command' function here.
 * tokens: String vector containing the tokens representing the command to be
 * executed, possible redirection, and the command's arguments.
 * pipes: An array of pipe file descriptors.
 * n_pipes: Length of the 'pipes' array
 * in_idx: Index of the file descriptor in the array from which the program
 *         should read its input, or -1 if input should not be read from a pipe.
 * out_idx: Index of the file descriptor in the array to which the program
 *          should write its output, or -1 if output should not be written to
 *          a pipe.
 * Returns 0 on success or -1 on error.
 */
int run_piped_command(strvec_t *tokens, int *pipes, int n_pipes, int in_idx, int out_idx) {
    // TODO Complete this function's implementation
    if (in_idx != -1) {
        // need to EH dup2 and close
        dup2(pipes[in_idx], STDIN_FILENO);
        close(pipes[in_idx]);
    }
    if (out_idx != -1) {
        // need to EH dup2 and close
        dup2(pipes[out_idx], STDOUT_FILENO);
        close(pipes[out_idx]);
    }

    // Need EH here
    run_command(tokens);

    return 0;
}

int run_pipelined_commands(strvec_t *tokens) {
    // TODO Complete this function's implementation
    // parse tokens for piped commands, put number of piped commands in variable num_children
    
    int n = 0;
    n = strvec_num_occurrences(tokens, "|");

    int *pipe_fds = malloc(2*(n) * sizeof(int));
    if (pipe_fds == NULL) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    // Set up all pipes
    for (int i = 0; i < n; i++) {
        if (pipe(pipe_fds + 2*i) == -1) {
            perror("pipe");
            for (int j = 0; j < i; j++) {
                close(pipe_fds[2*j]);
                close(pipe_fds[2*j + 1]);
            }
            free(pipe_fds);
            return -1;
        }
    }

    // Create child process for each file
    for (int i = 0; i < n+1; i++) {
        pid_t child_pid = fork();

        if (child_pid == -1) {
            perror("fork");
            for (int j = 0; j < n; j++) {
                close(pipe_fds[2*j]);
                close(pipe_fds[2*j + 1]);
            }
            free(pipe_fds);
            return -1;
        } else if (child_pid == 0) {

            // Close unused pipes
            int failure = 0;
            for (int j = 0; j < n; j++) {

                // close necessary read ends
                if (j != i-1 && close(pipe_fds[2*j]) == -1) {
                    perror("close");
                    failure = 1;
                }

                // close necessary write ends
                if (j != i && close(pipe_fds[2*j + 1]) == -1) {
                    perror("close");
                    failure = 1;
                }
            }

            // create temp strvec obj
            strvec_t temp_tokens;
            if(strvec_init(&temp_tokens) == -1) {
                printf("strvec_init error\n");
                strvec_clear(&temp_tokens);
                failure = 1;
            }

            // find the correct slice in the string vector for the current child
            int prev_pipe_index = 0;
            int next_pipe_index = 0;
            int num_occurrences = 0;
            for (int j = 0; j < tokens->length; j++) {

                if (strcmp(tokens->data[j], "|") == 0) {
                    prev_pipe_index = next_pipe_index;
                    next_pipe_index = j;
                    num_occurrences++;
                }

                if (num_occurrences == i+1) {
                    break;
                }

                if (j == tokens->length-1) {
                    prev_pipe_index = next_pipe_index;
                    next_pipe_index = tokens->length;
                }
            }

            // This will exclude the pipe for string vector slicing
            if (prev_pipe_index != 0) {
                prev_pipe_index++;
            }

            // put the desired slice into temp_tokens
            if (strvec_slice(tokens, &temp_tokens, prev_pipe_index, next_pipe_index)) {
                printf("strvec_slice error\n");
                strvec_clear(&temp_tokens);
                failure = 1;
            }

            // error handling
            if (failure) {
                // close cur child read end
                if (i != 0) {
                    close(pipe_fds[2*(i-1)]);
                }
                // close cur child write end
                if (i != n) {
                    close(pipe_fds[2*i + 1]);
                }
                free(pipe_fds);
                exit(-1);
            }

            // for (int j = 0; j<temp_tokens.length; j++) {
            //     printf("%s", temp_tokens.data[j]);
            // }
            // printf("\n");
            // printf("number of tokens: %d\n", temp_tokens.length);

            if (i == 0) {
                if (run_piped_command(&temp_tokens, pipe_fds, n, -1, 2*i + 1)) {
                    free(pipe_fds);
                    strvec_clear(&temp_tokens);
                    exit(-1);
                } else {
                    free(pipe_fds);
                    strvec_clear(&temp_tokens);
                    exit(1);
                }
            } else if (i == n) {
                if (run_piped_command(&temp_tokens, pipe_fds, n, 2*(i-1), -1)) {
                    free(pipe_fds);
                    strvec_clear(&temp_tokens);
                    exit(-1);
                } else {
                    free(pipe_fds);
                    strvec_clear(&temp_tokens);
                    exit(1);
                }
            } else {
                if (run_piped_command(&temp_tokens, pipe_fds, n, 2*(i-1), 2*i + 1)) {
                    free(pipe_fds);
                    strvec_clear(&temp_tokens);
                    exit(-1);
                } else {
                    free(pipe_fds);
                    strvec_clear(&temp_tokens);
                    exit(1);
                }
            }
            
        } 
    }

    free(pipe_fds);

    // some wait logic with checking exit codes
    // TODO figure out how to wait for all children to finish without causing timeout on automated testing
    // for (int i = 0; i<n+1; i++) {
    //     wait(NULL);
    // }


    return 0;
}
