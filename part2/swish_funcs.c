#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

    pid_t child_pid = fork();

        if (child_pid == -1) {
            // fork failure
            perror("fork");
            return -1;
        } else if (child_pid == 0) {
            // child process
            int dup2_fail = 0;
            if (in_idx != -1) {
                // need to EH dup2 and close
                if (dup2(pipes[in_idx], STDIN_FILENO) == -1) {
                    dup2_fail = 1;
                    perror("dup2");
                }
            }
            if (out_idx != -1) {
                // need to EH dup2 and close
                if (dup2(pipes[out_idx], STDOUT_FILENO) == -1) {
                    dup2_fail = 1;
                    perror("dup2");
                }
            }

            int close_failure = 0;
            for (int i = 0; i < n_pipes; i++) {
                if (close(pipes[2*i])) {
                    perror("close");
                    close_failure = -1;
                }
                if (close(pipes[2*i + 1])) {
                    perror("close");
                    close_failure = 1;
                }
            }
            
            if (close_failure || dup2_fail) {
                free(pipes);
                exit(-1);
            }
            run_command(tokens);
        } 

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
    // create temp strvec obj
    strvec_t temp_tokens;

    // loop through, taking the last command of the piped command out each time, until we have no more pipes
    int last_pipe_index = 0;
    for (int i = n; i >= 0; i--) { 
        last_pipe_index = strvec_find_last(tokens, "|");

        // put the desired slice into temp_tokens
        if (strvec_slice(tokens, &temp_tokens, last_pipe_index+1, tokens->length) == -1) {
            printf("strvec_slice error\n");
            strvec_clear(&temp_tokens);
            for (int j = 0; j < i; j++) {
                close(pipe_fds[2*j]);
                close(pipe_fds[2*j + 1]);
            }
            free(pipe_fds);
            return -1;
        }
        
        strvec_take(tokens, last_pipe_index);
        if (i == n) {
            // last piped command
            if (run_piped_command(&temp_tokens, pipe_fds, n, 2*(i-1), -1)) {
                free(pipe_fds);
                strvec_clear(&temp_tokens);

                // need to close all pipes for failure
                for (int j = 0; j < n; j++) {
                    if (close(pipe_fds[2*j])) {
                        perror("close");
                    }
                    if (close(pipe_fds[2*j + 1])) {
                        perror("close");
                    }
                }

                return -1;
            }
            strvec_clear(&temp_tokens);
        } else if (i == 0) {
            // first piped command
            if (run_piped_command(&temp_tokens, pipe_fds, n, -1, 2*i + 1)) {
                free(pipe_fds);
                strvec_clear(&temp_tokens);

                // need to close all pipes for failure
                for (int j = 0; j < n; j++) {
                    if (close(pipe_fds[2*j])) {
                        perror("close");
                    }
                    if (close(pipe_fds[2*j + 1])) {
                        perror("close");
                    }
                }

                return -1;
            }
            strvec_clear(&temp_tokens);
        } else {
            // interior piped commands
            if (run_piped_command(&temp_tokens, pipe_fds, n, 2*(i-1), 2*i + 1)) {
                free(pipe_fds);
                strvec_clear(&temp_tokens);

                // need to close all pipes for failure
                for (int j = 0; j < n; j++) {
                    if (close(pipe_fds[2*j])) {
                        perror("close");
                    }
                    if (close(pipe_fds[2*j + 1])) {
                        perror("close");
                    }
                }
                return -1;

            }
            strvec_clear(&temp_tokens);
        }
    }
    strvec_clear(&temp_tokens);
    // all pipes close except parent process at the end
    for (int j = 0; j < n; j++) {
        if (close(pipe_fds[2*j])) {
            perror("close");
        }
        if (close(pipe_fds[2*j + 1])) {
            perror("close");
        }
    }
    free(pipe_fds);

    // some wait logic with checking exit codes
    for (int i = 0; i<n+1; i++) {
        wait(NULL);
    }

    return 0;
}
