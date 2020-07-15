#ifndef ROMBP_IPS_H_
#define ROMBP_IPS_H_

#include <stdio.h>

enum ips_err {
    IPS_INVALID_HEADER = 1,
};

int ips_patch(FILE* input_file, FILE* outut_file, FILE* ips_file);

#endif
