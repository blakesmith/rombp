#include <stdlib.h>

#include "log.h"
#include "patch.h"

rombp_patch_err patch_verify_marker(FILE* patch_file, const uint8_t* expected_header, const size_t header_size) {
    uint8_t buf[header_size];

    size_t nread = fread(&buf, 1, header_size, patch_file);
    if (nread < header_size) {
        rombp_log_err("Header malformed, expected to get at least %ld bytes in the patch file, read: %ld\n", (long int)header_size, (long int)nread);
        return PATCH_INVALID_HEADER;
    }

    for (int i = 0; i < header_size; i++) {
        if (expected_header[i] != buf[i]) {
            rombp_log_err("Marker at byte %d doesn't match. Value: %d\n", i, buf[i]);
            return PATCH_INVALID_HEADER;
        }
    }

    return PATCH_OK;
}

void patch_status_init(rombp_patch_status* status) {
    status->is_done = 0;
    status->iter_status = HUNK_NONE;
    status->err = PATCH_OK;
    status->hunk_count = 0;

    int rc = pthread_mutex_init(&status->lock, NULL);
    if (rc != 0) {
        rombp_log_err("Failed to initalize mutex: %d\n", rc);
        exit(-1);
    }
}

void patch_status_destroy(rombp_patch_status* status) {
    int rc = pthread_mutex_destroy(&status->lock);
    if (rc != 0) {
        rombp_log_err("Failed to destroy mutex: %d\n", rc);
    }
}

