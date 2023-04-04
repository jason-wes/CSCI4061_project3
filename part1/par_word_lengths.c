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
        perror("Error opening file");
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
                counts[word_length]++;
                word_length = 0;
            }
        }
    }
    // ADD EH
    if(ferror(fp) != 0) {
        fclose(fp);
        
    }

    // ADD EH
    fclose(fp);

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
    //int counts[MAX_WORD_LEN];
    int *counts = malloc(MAX_WORD_LEN * sizeof(int));
    count_word_lengths(file_name, counts);

    if(write(out_fd, &counts, sizeof(counts)) == -1) {
        perror("write");
        free(counts);
        return -1;
    }
    free(counts);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        // No files to consume, return immediately
        return 0;
    }

    // TODO Create a pipe for child processes to write their results
    int pipefds[2];
    pipe(pipefds); // 0 for read, 1 for write

    // TODO Fork a child to analyze each specified file (names are argv[1], argv[2], ...)
    for(int i = 1; i < argc; i++) {
        pid_t child_pid = fork();

        if(child_pid == -1) {
            // error
        } else if(child_pid == 0) { // child
            close(pipefds[0]);
            process_file(argv[i], pipefds[1]);
            close(pipefds[1]);
        }
    }

    // TODO Aggregate all the results together by reading from the pipe in the parent
    close(pipefds[1]);
    int *counts = malloc(MAX_WORD_LEN * sizeof(int));
    int *tempCounts = malloc(MAX_WORD_LEN * sizeof(int));
    for(int i = 0; i < argc-1; i++) { // loop for each file processed
        // NEED EH
        read(pipefds[0], tempCounts, sizeof(tempCounts)); // read in child's processed file results written
        for(int len = 0; len < MAX_WORD_LEN; len++) {
            counts[len] += tempCounts[len];
        }
    }
    // TODO Change this code to print out the total count of words of each length
    for (int i = 1; i <= MAX_WORD_LEN; i++) {
        printf("%d-Character Words: %d\n", i, counts[i]);
    }
    return 0;
}
