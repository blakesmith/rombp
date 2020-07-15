#ifndef ROMBP_IPS_H_
#define ROMBP_IPS_H_

#include <stdio.h>
#include <stdint.h>

enum ips_err {
    IPS_INVALID_HEADER = 1,
};

enum ips_hunk_iter_status {
    HUNK_DONE = 1,
};

typedef struct ips_hunk_header {
    uint32_t offset;
    uint16_t length;
} ips_hunk_header;

int ips_patch(FILE* input_file, FILE* outut_file, FILE* ips_file);

#endif
