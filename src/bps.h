#ifndef ROMPB_BPS_H_
#define ROMPB_BPS_H_

#include <stdio.h>
#include <stdint.h>

#include "patch.h"

typedef struct bps_file_header {
    uint64_t source_size;
    uint64_t target_size;
    uint64_t metadata_size;

    uint64_t patch_size;
} bps_file_header;

rombp_patch_err bps_verify_marker(FILE* bps_file);
rombp_patch_err bps_start(FILE* bps_file, bps_file_header* file_header);
rombp_hunk_iter_status bps_next(bps_file_header* file_header, FILE* input_file, FILE* output_file, FILE* bps_file);

#endif
