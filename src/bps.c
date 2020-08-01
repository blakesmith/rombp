#include "bps.h"

rombp_patch_err bps_verify_header(FILE* bps_file) {
    return PATCH_INVALID_HEADER;
}

rombp_patch_err bps_start(FILE* bps_file, bps_file_header* file_header) {
    return PATCH_OK;
}

rombp_hunk_iter_status bps_next(bps_file_header* file_header, FILE* input_file, FILE* output_file, FILE* bps_file) {
    return HUNK_DONE;
}
