#ifndef ROMBP_IPS_H_
#define ROMBP_IPS_H_

#include <stdio.h>
#include <stdint.h>

#include "patch.h"

typedef struct ips_hunk_header {
    uint32_t offset;
    uint16_t length;
} ips_hunk_header;

rombp_patch_err ips_verify_marker(FILE* ips_file);
rombp_patch_err ips_start(FILE* input_file, FILE* output_file);
rombp_hunk_iter_status ips_next(FILE* input_file, FILE* output_file, FILE* ips_file);

#endif
