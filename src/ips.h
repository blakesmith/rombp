#ifndef ROMBP_IPS_H_
#define ROMBP_IPS_H_

#include <stdio.h>
#include <stdint.h>

typedef enum ips_err {
    IPS_OK = 0,
    IPS_INVALID_HEADER = 1,
    IPS_ERR_IO = 2,
} ips_err;

// TODO: Move up a level
typedef enum ips_hunk_iter_status {
    HUNK_ERR_IPS = -1,
    HUNK_NONE = 0,
    HUNK_DONE = 1,
    HUNK_NEXT = 2,
} ips_hunk_iter_status;

typedef struct ips_hunk_header {
    uint32_t offset;
    uint16_t length;
} ips_hunk_header;

ips_err ips_verify_header(FILE* ips_file);
ips_err ips_start(FILE* input_file, FILE* output_file);
ips_hunk_iter_status ips_next(FILE* input_file, FILE* output_file, FILE* ips_file);

#endif
