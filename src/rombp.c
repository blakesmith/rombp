#include <stdio.h>
#include <errno.h>

#include "ips.h"

int main() {
    fprintf(stdout, "Patching file\n");

    FILE* input_file = fopen("tetris.gb", "r");
    if (input_file == NULL) {
        fprintf(stderr, "Failed to open input file: %d\n", errno);
        return errno;
    }

    FILE* output_file = fopen("tetris.patched.gb", "w");
    if (output_file == NULL) {
        fprintf(stderr, "Failed to open output file: %d\n", errno);
        return errno;
    }

    int rc = ips_patch(input_file, output_file, NULL);
    if (rc != 0) {
        fprintf(stderr, "Failed to copy file: %d\n", rc);
        return rc;
    }

    fprintf(stdout, "Done patching file\n");
    return 0;
}
