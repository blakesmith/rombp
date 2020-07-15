#include <stdio.h>
#include <errno.h>

#include "ips.h"

void close_files(FILE* input_file, FILE* output_file, FILE* ips_file) {
    if (input_file != NULL) {
        fclose(input_file);
    }
    if (output_file != NULL) {
        fclose(output_file);
    }
    if (ips_file != NULL) {
        fclose(ips_file);
    }
}

int main() {
    fprintf(stdout, "Patching file\n");

    FILE* input_file = fopen("fixtures/Super Metroid (JU) [!].smc", "r");
    if (input_file == NULL) {
        fprintf(stderr, "Failed to open input file: %d\n", errno);
        return errno;
    }

    FILE* output_file = fopen("Ascent.Super Metroid (JU) [!].smc", "w");
    if (output_file == NULL) {
        fprintf(stderr, "Failed to open output file: %d\n", errno);
        close_files(input_file, NULL, NULL);
        return errno;
    }

    FILE* ips_file = fopen("fixtures/Ascent1.12.IPS", "r");
    if (ips_file == NULL) {
        fprintf(stderr, "Failed to open IPS file: %d\n", errno);
        close_files(input_file, output_file, NULL);
        return errno;
    }

    int hunk_count = ips_patch(input_file, output_file, ips_file);
    if (hunk_count < 0) {
        fprintf(stderr, "Failed to copy file: %d\n", hunk_count);
        close_files(input_file, output_file, ips_file);
        return hunk_count;
    }

    close_files(input_file, output_file, ips_file);

    fprintf(stdout, "Done patching file, hunk count: %d\n", hunk_count);
    return 0;
}
