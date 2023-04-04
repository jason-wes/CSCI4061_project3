#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_WORD_LEN 25

/*
 * Counts the number of occurrences of words of different lengths in a text
 * file and stores the results in an array.
 * file_name: The name of the text file from which to read words
 * counts: An array of integers storing the number of words of each possible
 *     length.  counts[0] is the number of 1-character words, counts [1] is the
 *     number of 2-character words, and so on.
 * Returns 0 on success or -1 on error.
 */
int count_word_lengths(const char *file_name, int *counts) {
    // open and EH file
    FILE *fp = fopen(file_name, "r");
    if(fp == NULL) {
        perror("fopen");
        return -1;
    }

    int c;
    int word_length = 0;

    // initialize counts array
    for(int i = 0; i < MAX_WORD_LEN; i++) {
        counts[i] = 0;
    }

    // read each character in file
    while((c = fgetc(fp)) != EOF) {
        // check if character is a leter
        if(isalpha(c)) {
            word_length++;
        } else {
            if(word_length > 0) {
                // increment counts array for the word's length
                counts[word_length-1]++;
                word_length = 0;
            }
        }
    }

    // error check fgetc
    if(ferror(fp) != 0) {
        perror("fgetc");
        if(fclose(fp) != 0) {
            perror("error closing file");
        }
        return -1;
    }

    if(fclose(fp) != 0) {
        perror("error closing file");
        return -1;
    }

    return 0;
}

/*
 * Processes a particular file (counting the number of words of each length)
 * and writes the results to a file descriptor.
 * This function should be called in child processes.
 * file_name: The name of the file to analyze.
 * out_fd: The file descriptor to which results are written
 * Returns 0 on success or -1 on error
 */
int process_file(const char *file_name, int out_fd) {
    int counts[MAX_WORD_LEN];
    // error check count_word_lengths
    if (count_word_lengths(file_name, counts) == -1) {
        return -1;
    }

    // write to pipe and error check
    if(write(out_fd, &counts, sizeof(counts)) == -1) {
        perror("write");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }

    // Create a pipe for child processes to write their results
    int pipefds[2];
    // 0 for read, 1 for write
    if(pipe(pipefds) == -1) {
        perror("pipe");
        return 1;
    } 

    // Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    for(int i = 1; i < argc; i++) {
        pid_t child_pid = fork();

        if(child_pid == -1) { // error check fork
            perror("fork");
            if (close(pipefds[0])) {
                perror("close");
            }
            if (close(pipefds[0])) {
                perror("close");
            }
            return 1;
        } else if(child_pid == 0) { // child
            int exit_code = 0;
            if (close(pipefds[0])) {
                perror("close");
                exit_code = -1;
            }
            if (process_file(argv[i], pipefds[1])) {
                exit_code = -1;
            }
            if (close(pipefds[1])) {
                perror("close");
                exit_code = -1;
            }
            exit(exit_code);
        }
    }

    // close write end and error check
    if (close(pipefds[1])) {
        perror("close");
        if (close(pipefds[0])) {
            perror("close");
        }
        return -1;
    }

    // Wait for all children to finish and make sure that they all exited correctly
    for(int i = 0; i<argc-1; i++) {
        int status;
        if (wait(&status) == -1) {
            perror("wait");
            return 1;
        }

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            if (exit_status == -1) {
                return 1;
            }
        }
    }

    // Initialze int arrays
    int counts[MAX_WORD_LEN];
    int tempCounts[MAX_WORD_LEN];
    for (int i = 0; i<MAX_WORD_LEN; i++) {
        counts[i] = 0;
        tempCounts[i] = 0;
    }

    // Aggregate all the results together by reading from the pipe in the parent

    for(int i = 0; i < argc-1; i++) { // loop for each file processed

        // read in child's processed file results written
        if (read(pipefds[0], tempCounts, sizeof(tempCounts)) == -1) {
            perror("read");
            return 1;
        } 

        for(int len = 0; len < MAX_WORD_LEN; len++) {
            counts[len] += tempCounts[len];
        }
    }

    // close read end and error check
    if (close(pipefds[0])) {
        perror("close");
        return 1;
    }

    // Print out the total count of words of each length
    for (int i = 0; i < MAX_WORD_LEN; i++) {
        printf("%d-Character Words: %d\n", i+1, counts[i]);
    }

    return 0;
}
