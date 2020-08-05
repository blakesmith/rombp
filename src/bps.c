#include <errno.h>

#include "bps.h"
#include "log.h"

static const uint8_t BPS_EXPECTED_MARKER[] = {
    0x42, 0x50, 0x53, 0x31 // BPS1
};
static const size_t BPS_MARKER_SIZE = sizeof(BPS_EXPECTED_MARKER) / sizeof(uint8_t);

static uint64_t decode_varint(FILE* bps_file) {
    uint64_t data = 0;
    uint64_t shift = 1;

    while (1) {
        uint8_t ch;
        size_t nread = fread(&ch, 1, sizeof(uint8_t), bps_file);
        if (nread != 1) {
            rombp_log_err("Failed to read next byte, read: %ld, error code: %d\n", nread, ferror(bps_file));
            return 0;
        }
        data += (ch & 0x7F) * shift;
        if (ch & 0x80) {
            break;
        }
        shift <<= 7;
        data += shift;
    }
    return data;
}

rombp_patch_err bps_verify_marker(FILE* bps_file) {
    return patch_verify_marker(bps_file, BPS_EXPECTED_MARKER, BPS_MARKER_SIZE);
}

rombp_patch_err bps_start(FILE* bps_file, bps_file_header* file_header) {
    file_header->source_size = decode_varint(bps_file);
    if (file_header->source_size == 0) {
        rombp_log_err("BPS file: Failed to read source size\n");
        return PATCH_ERR_IO;
    }
    file_header->target_size = decode_varint(bps_file);
    if (file_header->target_size == 0) {
        rombp_log_err("BPS file: Failed to read target size\n");
        return PATCH_ERR_IO;
    }
    file_header->metadata_size = decode_varint(bps_file);
    if (file_header->metadata_size > 0) {
        // Skip over metadata. Don't need it!
        int pos = fseek(bps_file, file_header->metadata_size, SEEK_CUR);
        if (pos == -1) {
            rombp_log_err("Failed to skip metadata field. Err: %d\n", errno);
            return PATCH_ERR_IO;
        }
    }

    rombp_log_info("BPS file header, source_size: %ld, target_size: %ld, metadata_size: %ld\n",
                   file_header->source_size,
                   file_header->target_size,
                   file_header->metadata_size);

    return PATCH_OK;
}

rombp_hunk_iter_status bps_next(bps_file_header* file_header, FILE* input_file, FILE* output_file, FILE* bps_file) {
    return HUNK_DONE;
}
