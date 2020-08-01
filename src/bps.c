#include "bps.h"

static const uint8_t BPS_EXPECTED_MARKER[] = {
    0x42, 0x50, 0x53, 0x31 // BPS1
};
static const size_t BPS_MARKER_SIZE = sizeof(BPS_EXPECTED_MARKER) / sizeof(uint8_t);

rombp_patch_err bps_verify_header(FILE* bps_file) {
    return patch_verify_marker(bps_file, BPS_EXPECTED_MARKER, BPS_MARKER_SIZE);
}

rombp_patch_err bps_start(FILE* bps_file, bps_file_header* file_header) {
    return PATCH_OK;
}

rombp_hunk_iter_status bps_next(bps_file_header* file_header, FILE* input_file, FILE* output_file, FILE* bps_file) {
    return HUNK_DONE;
}
