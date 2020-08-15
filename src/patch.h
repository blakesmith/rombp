#ifndef ROMBP_PATCH_H_
#define ROMBP_PATCH_H_

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

typedef enum rombp_patch_type {
    PATCH_TYPE_UNKNOWN = -1,
    PATCH_TYPE_IPS = 0,
    PATCH_TYPE_BPS = 1,
} rombp_patch_type;

// Status code used outside of hunk iteration.
typedef enum rombp_patch_err {
    PATCH_OK = 0,
    PATCH_INVALID_HEADER = -1,
    PATCH_ERR_IO = -2,
    PATCH_INVALID_INPUT_SIZE = -3,
    PATCH_INVALID_OUTPUT_SIZE = -4,
    PATCH_INVALID_INPUT_CHECKSUM = -5,
    PATCH_INVALID_OUTPUT_CHECKSUM = -6,
} rombp_patch_err;

// Status code used during hunk iteration.
typedef enum rombp_hunk_iter_status {
    HUNK_ERR_IO = -2,
    HUNK_ERR_IPS = -1,
    HUNK_NONE = 0,
    HUNK_DONE = 1,
    HUNK_NEXT = 2,
} rombp_hunk_iter_status;

// Status that can be shared betwen threads during multi-threaded patching
typedef struct rombp_patch_status {
    pthread_mutex_t lock;
    rombp_hunk_iter_status iter_status;
    rombp_patch_err err;
    int hunk_count;
} rombp_patch_status;

rombp_patch_err patch_verify_marker(FILE* patch_file, const uint8_t* expected_header, const size_t header_size);
void patch_status_init(rombp_patch_status* status);
void patch_status_destroy(rombp_patch_status* status);

#endif
