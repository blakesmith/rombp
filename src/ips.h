#ifndef ROMBP_IPS_H_
#define ROMBP_IPS_H_

#include <stdio.h>
#include <stdint.h>

enum ips_err {
    IPS_INVALID_HEADER = 1,
};

enum ips_hunk_iter_status {
    HUNK_ERR_IPS = -1,
    HUNK_DONE = 1,
    HUNK_NEXT = 2,
};

typedef struct ips_hunk_header {
    uint32_t offset;
    uint16_t length;
} ips_hunk_header;

int ips_verify_header(FILE* ips_file);
int ips_patch(FILE* input_file, FILE* outut_file, FILE* ips_file);

#endif
