#include <stdio.h>
#include <errno.h>

#include "ips.h"
#include "log.h"
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

typedef enum rombp_patch_err {
    ERR_FILE_IO = -1,
    ERR_BAD_PATCH_TYPE = -2,
} rombp_patch_err;

typedef enum rombp_patch_type {
    PATCH_TYPE_UNKNOWN = -1,
    PATCH_TYPE_IPS = 0,
} rombp_patch_type;

rombp_patch_type detect_patch_type(FILE* patch_file) {
    int rc = ips_verify_header(patch_file);

    if (rc == 0) {
        return PATCH_TYPE_IPS;
    }

    return PATCH_TYPE_UNKNOWN;
}

static rombp_patch_err patch_file(rombp_patch_command* command) {
    rombp_log_info("Patching file\n");

    FILE* input_file = fopen(command->input_file, "r");
    if (input_file == NULL) {
        rombp_log_err("Failed to open input file: %s, errno: %d\n", command->input_file, errno);
        return ERR_FILE_IO;
    }

    FILE* output_file = fopen(command->output_file, "w");
    if (output_file == NULL) {
        rombp_log_err("Failed to open output file: %d\n", errno);
        close_files(input_file, NULL, NULL);
        return ERR_FILE_IO;
    }

    FILE* ips_file = fopen(command->ips_file, "r");
    if (ips_file == NULL) {
        rombp_log_err("Failed to open IPS file: %d\n", errno);
        close_files(input_file, output_file, NULL);
        return ERR_FILE_IO;
    }

    rombp_patch_type patch_type = detect_patch_type(ips_file);
    if (patch_type == PATCH_TYPE_UNKNOWN) {
        rombp_log_err("Bad patch file type: %d\n", patch_type);
        ui_free_command(command);
        close_files(input_file, output_file, ips_file);
        return ERR_BAD_PATCH_TYPE;
    }

    int hunk_count = ips_patch(input_file, output_file, ips_file);
    if (hunk_count < 0) {
        rombp_log_err("Failed to copy file: %d\n", hunk_count);
        close_files(input_file, output_file, ips_file);
        return hunk_count;
    }

    close_files(input_file, output_file, ips_file);
    ui_free_command(command);

    rombp_log_info("Done patching file, hunk count: %d\n", hunk_count);
    return hunk_count;
}

int ui_loop(rombp_patch_command* command) {
    rombp_ui ui;

    int rc = ui_start(&ui);
    if (rc != 0) {
        rombp_log_err("Failed to start UI, error code: %d\n", rc);
        return 1;
    }

    while (1) {
        rombp_ui_event event = ui_handle_event(&ui, command);
        rombp_patch_err err;

        switch (event) {
            case EV_QUIT:
                ui_stop(&ui);
                return 0;
            case EV_PATCH_COMMAND:
                err = patch_file(command);
                if (err == ERR_BAD_PATCH_TYPE) {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: Not a valid patch type");
                    rombp_log_err("Invalid patch type\n");
                } else if (err == ERR_FILE_IO) {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: Cannot write ROM");
                    rombp_log_err("Failed to patch file, io error: %d\n", rc);
                } else {
                    static const char *success_message = "Success! Wrote %d hunks";
                    char tmp_buf[255];
                    int hunk_count = err;

                    sprintf(tmp_buf, success_message, hunk_count);
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, tmp_buf);
                }
                break;
            default:
                break;
        }

        rc = ui_draw(&ui);
        if (rc != 0) {
            rombp_log_err("Failed to draw: %d\n", rc);
            return rc;
        }

        SDL_Delay(16);
    }
}

int main(int argc, char** argv) {
    rombp_patch_command command;

    command.input_file = NULL;
    command.ips_file = NULL;
    command.output_file = NULL;

    int rc = ui_loop(&command);
    if (rc != 0) {
        rombp_log_err("Failed to initiate UI loop: %d\n", rc);
        return -1;
    }

    return 0;
}
