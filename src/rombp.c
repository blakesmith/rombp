#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "bps.h"
#include "ips.h"
#include "log.h"
#include "ui.h"

static const char* PATCH_NEXT_MESSAGE = "Patching. Wrote %d hunks";
static const char* PATCH_SUCCESS_MESSAGE = "Success! Wrote %d hunks";
static const char* PATCH_FAIL_INVALID_OUTPUT_SIZE_MESSAGE = "ERR: Invalid output size!";
static const char* PATCH_FAIL_INVALID_OUTPUT_CHECKSUM_MESSAGE = "ERR: Invalid output checksum!";
static const char* PATCH_UNKNOWN_ERROR_MESSAGE = "ERR: Unknown end error!";

static const int DEFAULT_SLEEP = 16;
static const int HUNKS_PER_UI_LOOP = 10;

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

static rombp_patch_type detect_patch_type(FILE* patch_file) {
    rombp_log_info("Trying to detect patch type\n");
    int rc = ips_verify_marker(patch_file);

    if (rc == 0) {
        rombp_log_info("Detected patch type: IPS\n");
        return PATCH_TYPE_IPS;
    }

    rc = fseek(patch_file, 0, SEEK_SET);
    if (rc == -1) {
        rombp_log_err("Error seeking patch file to beginning\n");
        return PATCH_TYPE_UNKNOWN;
    }

    rombp_log_info("Trying to detect BPS patch type\n");
    rc = bps_verify_marker(patch_file);
    if (rc == 0) {
        rombp_log_info("Detected patch type: BPS\n");
        return PATCH_TYPE_BPS;
    }

    return PATCH_TYPE_UNKNOWN;
}

// Used for any patch type specific data types that
// need to be passed into our start function.
typedef union {
    bps_file_header bps_file_header;
} rombp_patch_context;

static int start_patch(rombp_patch_type patch_type, rombp_patch_context* ctx, FILE* input_file, FILE* patch_file, FILE* output_file) {
    int rc;

    rombp_log_info("Start patching\n");

    switch (patch_type) {
        case PATCH_TYPE_IPS:
            rombp_log_info("Patch type started with IPS!\n");
            rc = ips_start(input_file, output_file);
            if (rc != PATCH_OK) {
                rombp_log_err("Failed to start patching IPS file: %d\n", rc);
                return -1;
            }
            return 0;
        case PATCH_TYPE_BPS:
            rc = bps_start(patch_file, &ctx->bps_file_header);
            if (rc != PATCH_OK) {
                rombp_log_err("Failed to start patching BPS file: %d\n", rc);
                return -1;
            }
            return 0;
        default:
            rombp_log_err("Cannot start unknown patch type\n");
            return -1;
    }
}

static rombp_patch_err end_patch(rombp_patch_type patch_type, rombp_patch_context* ctx, FILE* patch_file) {
    rombp_log_info("End patching\n");
    switch (patch_type) {
        case PATCH_TYPE_BPS: return bps_end(&ctx->bps_file_header, patch_file);
        case PATCH_TYPE_IPS:
        default:
            return PATCH_OK; // No cleanup work for IPS patches, by default nothing left to do.
    }
}

static rombp_hunk_iter_status next_hunk(rombp_patch_type patch_type, rombp_patch_context* patch_ctx, FILE* input_file, FILE* output_file, FILE* patch_file) {
    switch (patch_type) {
        case PATCH_TYPE_IPS: return ips_next(input_file, output_file, patch_file);
        case PATCH_TYPE_BPS: return bps_next(&patch_ctx->bps_file_header, input_file, output_file, patch_file);
        default: return HUNK_NONE;
    }
}

static rombp_patch_err open_patch_files(FILE** input_file, FILE** output_file, FILE** ips_file, rombp_patch_command* command) {
    *input_file = fopen(command->input_file, "r");
    if (*input_file == NULL) {
        rombp_log_err("Failed to open input file: %s, errno: %d\n", command->input_file, errno);
        return PATCH_ERR_IO;
    }

    *output_file = fopen(command->output_file, "w+");
    if (*output_file == NULL) {
        rombp_log_err("Failed to open output file: %d\n", errno);
        close_files(*input_file, NULL, NULL);
        return PATCH_ERR_IO;
    }

    *ips_file = fopen(command->ips_file, "r");
    if (*ips_file == NULL) {
        rombp_log_err("Failed to open IPS file: %d\n", errno);
        close_files(*input_file, *output_file, NULL);
        return PATCH_ERR_IO;
    }

    return PATCH_OK;
}

int ui_loop(rombp_patch_command* command) {
    rombp_ui ui;
    char tmp_buf[255];

    FILE* input_file;
    FILE* output_file;
    FILE* patch_file;

    rombp_patch_type patch_type = PATCH_TYPE_UNKNOWN;
    rombp_hunk_iter_status hunk_status = HUNK_NONE;
    int hunk_count = 0;
    int sleep_delay = DEFAULT_SLEEP;
    rombp_patch_context patch_ctx;
    
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
                err = open_patch_files(&input_file, &output_file, &patch_file, command);
                if (err == PATCH_ERR_IO) {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: Cannot open files for patching");
                    rombp_log_err("Failed to open files for patching: %d\n", err);
                    break;
                }
                patch_type = detect_patch_type(patch_file);
                if (patch_type == PATCH_TYPE_UNKNOWN) {
                    rombp_log_err("Bad patch file type: %d\n", patch_type);
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: Not a valid patch type");
                    ui_free_command(command);
                    close_files(input_file, output_file, patch_file);
                    break;
                }
                rc = start_patch(patch_type, &patch_ctx, input_file, patch_file, output_file);
                if (rc < 0) {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: Failed to start patching");
                    rombp_log_err("Failed to start patching, type: %d\n", patch_type);
                    ui_free_command(command);
                    close_files(input_file, output_file, patch_file);
                    break;
                }
                hunk_count = 0;
                hunk_status = HUNK_NEXT;
                sleep_delay = 0;
                break;
            default:
                break;
        }

        switch (hunk_status) {
            case HUNK_NEXT: {
                for (int i = 0; i < HUNKS_PER_UI_LOOP; i++) {
                    hunk_status = next_hunk(patch_type, &patch_ctx, input_file, output_file, patch_file);
                    if (hunk_status == HUNK_NEXT) {
                        hunk_count++;
                        rombp_log_info("Got next hunk, hunk count: %d\n", hunk_count);

                        sprintf(tmp_buf, PATCH_NEXT_MESSAGE, hunk_count);
                        ui_status_bar_reset_text(&ui, &ui.bottom_bar, tmp_buf);
                    } else {
                        break;
                    }
                }

                break;
            }
            case HUNK_DONE: {
                rombp_patch_err end_err = end_patch(patch_type, &patch_ctx, patch_file);
                if (end_err == PATCH_OK) {
                    sprintf(tmp_buf, PATCH_SUCCESS_MESSAGE, hunk_count);
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, tmp_buf);
                    rombp_log_info("Done patching file, hunk count: %d\n", hunk_count);
                } else if (end_err == PATCH_INVALID_OUTPUT_SIZE) {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, PATCH_FAIL_INVALID_OUTPUT_SIZE_MESSAGE);
                    rombp_log_err("Invalid output size\n");
                } else if (end_err == PATCH_INVALID_OUTPUT_CHECKSUM) {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, PATCH_FAIL_INVALID_OUTPUT_CHECKSUM_MESSAGE);
                    rombp_log_err("Invalid output checksum\n");
                } else {
                    ui_status_bar_reset_text(&ui, &ui.bottom_bar, PATCH_UNKNOWN_ERROR_MESSAGE);
                    rombp_log_err("Unknown end error: %d\n", end_err);
                }

                close_files(input_file, output_file, patch_file);
                ui_free_command(command);

                // reset hunk state
                hunk_status = HUNK_NONE;
                sleep_delay = DEFAULT_SLEEP;
                break;
            }
            case HUNK_ERR_IPS:
                ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: Cannot write ROM");
                rombp_log_err("Failed to patch file, io error: %d\n", rc);

                close_files(input_file, output_file, patch_file);
                ui_free_command(command);
                
                hunk_status = HUNK_NONE;
                sleep_delay = DEFAULT_SLEEP;
                break;
            case HUNK_ERR_IO:
                ui_status_bar_reset_text(&ui, &ui.bottom_bar, "ERROR: IO error decoding next patch hunk");
                rombp_log_err("I/O error during hunk iteration\n");

                close_files(input_file, output_file, patch_file);
                ui_free_command(command);
                
                hunk_status = HUNK_NONE;
                sleep_delay = DEFAULT_SLEEP;
                break;
            case HUNK_NONE:
                break;
        }

        rc = ui_draw(&ui);
        if (rc != 0) {
            rombp_log_err("Failed to draw: %d\n", rc);
            return rc;
        }

        if (sleep_delay > 0) {
            SDL_Delay(sleep_delay);
        }
    }
}

static void display_help() {
    fprintf(stderr, "rombp: IPS and BPS patcher\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "rombp [options]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t-i [FILE], Input ROM file\n");
    fprintf(stderr, "\t-p [FILE], IPS or BPS patch file\n");
    fprintf(stderr, "\t-o [FILE], Patched output file\n\n");
    fprintf(stderr, "Running rombp with no option arguments launches the SDL UI\n");
}

static int parse_command_line(int argc, char** argv, rombp_patch_command* command) {
    int c;

    while ((c = getopt(argc, argv, "i:p:o:")) != -1) {
        switch (c) {
            case 'i':
                command->input_file = optarg;
                break;
            case 'p':
                command->ips_file = optarg;
                break;
            case 'o':
                command->output_file = optarg;
                break;
            case '?':
                display_help();
                return -1;
            default:
                display_help();
                return -1;
        }
    }

    rombp_log_info("rombp arguments. input: %s, patch: %s, output: %s\n",
                   command->input_file, command->ips_file, command->output_file);

    return 0;
}

static int execute_patch(rombp_patch_command* command, rombp_patch_status* status) {
    int rc;
    rombp_patch_type patch_type = PATCH_TYPE_UNKNOWN;
    rombp_patch_context patch_ctx;
    rombp_patch_status local_status;

    FILE* input_file;
    FILE* output_file;
    FILE* patch_file;

    local_status.err = open_patch_files(&input_file, &output_file, &patch_file, command);
    if (local_status.err == PATCH_ERR_IO) {
        rombp_log_err("Failed to open files for patching: %d\n", local_status.err);
        return local_status.err;
    }
    patch_type = detect_patch_type(patch_file);
    if (patch_type == PATCH_TYPE_UNKNOWN) {
        rombp_log_err("Bad patch file type: %d\n", patch_type);
        close_files(input_file, output_file, patch_file);
        return -1;
    }
    rc = start_patch(patch_type, &patch_ctx, input_file, patch_file, output_file);
    if (rc < 0) {
        rombp_log_err("Failed to start patching, type: %d\n", patch_type);
        close_files(input_file, output_file, patch_file);
        return -1;
    }
    local_status.iter_status = HUNK_NEXT;

    while (1) {
        switch (local_status.iter_status) {
            case HUNK_NEXT: {
                local_status.iter_status = next_hunk(patch_type, &patch_ctx, input_file, output_file, patch_file);
                if (local_status.iter_status == HUNK_NEXT) {
                    local_status.hunk_count++;
                    rombp_log_info("Got next hunk, hunk count: %d\n", local_status.hunk_count);
                } else {
                    break;
                }

                break;
            }
            case HUNK_DONE: {
                local_status.err = end_patch(patch_type, &patch_ctx, patch_file);
                if (local_status.err == PATCH_OK) {
                    rombp_log_info("Done patching file, hunk count: %d\n", local_status.hunk_count);
                } else if (local_status.err == PATCH_INVALID_OUTPUT_SIZE) {
                    rombp_log_err("Invalid output size\n");
                } else if (local_status.err == PATCH_INVALID_OUTPUT_CHECKSUM) {
                    rombp_log_err("Invalid output checksum\n");
                } else {
                    rombp_log_err("Unknown end error: %d\n", local_status.err);
                }

                close_files(input_file, output_file, patch_file);
                return 0;
            }
            case HUNK_ERR_IPS:
                rombp_log_err("Failed to patch file, io error: %d\n", rc);
                close_files(input_file, output_file, patch_file);
                return -1;
            case HUNK_ERR_IO:
                rombp_log_err("I/O error during hunk iteration\n");
                close_files(input_file, output_file, patch_file);
                return -1;
            case HUNK_NONE:
                break;
        }
    }
}

typedef struct rombp_patch_thread_args {
    rombp_patch_command* command;
    rombp_patch_status status;
    int rc;
} rombp_patch_thread_args;

static void* execute_patch_threaded(void* args) {
    rombp_patch_thread_args* patch_args = (rombp_patch_thread_args *)args;
    int rc = execute_patch(patch_args->command, NULL);
    if (rc != 0) {
        rombp_log_err("Threaded patch failed: %d\n", rc);
        patch_args->rc = rc;
    }

    patch_args->rc = 0;
    return NULL;
}

// Start patching on a separate thread
static int rombp_start_patch_thread(pthread_t* patch_thread, rombp_patch_thread_args* thread_args) {
    int rc = pthread_create(patch_thread, NULL, &execute_patch_threaded, thread_args);
    if (rc != 0) {
        rombp_log_err("Failed to create patching thread: %d\n", rc);
        return rc;
    }
    return 0;
}

static int rombp_wait_patch_thread(pthread_t* patch_thread) {
    int rc = pthread_join(*patch_thread, NULL);
    if (rc != 0) {
        rombp_log_err("Failed to join patch thread: %d\n", rc);
        return rc;
    }
    return 0;
}

static int execute_command_line(int argc, char** argv, pthread_t* patch_thread, rombp_patch_command* command) {
    int rc;

    rc = parse_command_line(argc, argv, command);
    if (rc != 0) {
        return rc;
    }

    rombp_patch_thread_args thread_args;
    thread_args.command = command;
    thread_args.rc = 0;
    patch_status_init(&thread_args.status);

    rc = rombp_start_patch_thread(patch_thread, &thread_args);
    if (rc != 0) {
        rombp_log_err("Could not start patch thread: %d\n", rc);
        return rc;
    }
    rc = rombp_wait_patch_thread(patch_thread);
    if (rc != 0) {
        rombp_log_err("Could not wait for patch thread to stop: %d\n", rc);
        return rc;
    }

    if (thread_args.rc != 0) {
        rombp_log_err("Patch thread returned non-zero error code: %d\n", thread_args.rc);
        return thread_args.rc;
    }

    return 0;
}

int main(int argc, char** argv) {
    rombp_patch_command command;
    pthread_t patch_thread;

    command.input_file = NULL;
    command.ips_file = NULL;
    command.output_file = NULL;

    if (argc > 1) {
        // If the user passed command line arguments, assume they don't want to launch
        // the SDL UI.
        return execute_command_line(argc, argv, &patch_thread, &command);
    } else {
        int rc = ui_loop(&command);
        if (rc != 0) {
            rombp_log_err("Failed to initiate UI loop: %d\n", rc);
            return -1;
        }
    }

    return 0;
}
