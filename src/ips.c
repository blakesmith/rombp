#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "ips.h"

// Copy the input file to the output file, it is assumed
// that both files will be at position 0 before this function
// is called.
#define BUF_SIZE 255

static int copy_file(FILE* input_file, FILE* output_file) {
    char buf[BUF_SIZE];
    int rc;

    int infd = fileno(input_file);
    if (infd == -1) {
        fprintf(stderr, "Bad input file, is the stream closed?\n");
        return infd;
    }
    struct stat input_file_stat;
    rc = fstat(infd, &input_file_stat);
    if (rc == -1) {
        fprintf(stderr, "Failed to stat input file, errno: %d\n", errno);
        return rc;
    }

    off_t input_file_size = input_file_stat.st_size;
    size_t total_read = 0;
    while (1) {
        size_t nread = fread(&buf, 1, BUF_SIZE, input_file);
        total_read += nread;
        if (nread < BUF_SIZE) {
            if (total_read < input_file_size) {
                fprintf(stderr, "Failed to read the entire input file, read: %ld bytes, input file size: %ld\n", (long int)total_read, (long int)input_file_size);
                return -1;
            } else {
                return 0;
            }
        }
        size_t nwritten = fwrite(&buf, 1, nread, output_file);
        if (nwritten < nread) {
            fprintf(stderr, "Tried to copy %ld bytes to the output file, but only copied: %ld\n", (long int)nread, (long int)nwritten);
            return -1;
        }
    }

    return 0;
}

int ips_patch(FILE* input_file, FILE* output_file, FILE* ips_file) {
    int rc;

    // First, copy the input to output
    rc = copy_file(input_file, output_file);
    if (rc != 0) {
        fprintf(stderr, "Failed to seek to copy input file to output file: %d\n", rc);
        return rc;
    }
    return 0;
}
