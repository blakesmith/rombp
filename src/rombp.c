#include <stdio.h>
#include <errno.h>

#include "ips.h"
#include "ui.h"

static void close_files(FILE* input_file, FILE* output_file, FILE* ips_file) {
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

static int patch_file(rombp_patch_command* command) {
    fprintf(stdout, "Patching file\n");

    FILE* input_file = fopen(command->input_file, "r");
    if (input_file == NULL) {
        fprintf(stderr, "Failed to open input file: %d\n", errno);
        return errno;
    }

    FILE* output_file = fopen(command->output_file, "w");
    if (output_file == NULL) {
        fprintf(stderr, "Failed to open output file: %d\n", errno);
        close_files(input_file, NULL, NULL);
        return errno;
    }

    FILE* ips_file = fopen(command->ips_file, "r");
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

int main() {
    rombp_patch_command command;

    command.input_file = "fixtures/Super Metroid (JU) [!].smc";
    command.output_file = "Ascent.Super Metroid (JU) [!].smc";
    command.ips_file = "fixtures/Ascent1.12.IPS";

    return patch_file(&command);
}
