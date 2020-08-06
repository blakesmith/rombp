#include <errno.h>

#include "bps.h"
#include "log.h"

static const uint8_t BPS_EXPECTED_MARKER[] = {
    0x42, 0x50, 0x53, 0x31 // BPS1
};
static const size_t BPS_MARKER_SIZE = sizeof(BPS_EXPECTED_MARKER) / sizeof(uint8_t);

static const size_t FOOTER_LENGTH = 12;

static int decode_varint(FILE* bps_file, uint64_t* out) {
    uint64_t data = 0;
    uint64_t shift = 1;

    while (1) {
        uint8_t ch;
        size_t nread = fread(&ch, 1, sizeof(uint8_t), bps_file);
        if (nread != 1) {
            rombp_log_err("Failed to read next byte, read: %ld, error code: %d\n", nread, ferror(bps_file));
            return -1;
        }
        data += (ch & 0x7F) * shift;
        if (ch & 0x80) {
            break;
        }
        shift <<= 7;
        data += shift;
    }

    *out = data;
    return 0;
}

rombp_patch_err bps_verify_marker(FILE* bps_file) {
    return patch_verify_marker(bps_file, BPS_EXPECTED_MARKER, BPS_MARKER_SIZE);
}

rombp_patch_err bps_start(FILE* bps_file, bps_file_header* file_header) {
    int rc = fseek(bps_file, 0, SEEK_END);
    if (rc == -1) {
        rombp_log_err("Failed to seek to the end of patch file, error: %d\n", errno);
        return PATCH_ERR_IO;
    }
    file_header->patch_size = ftell(bps_file);
    if (file_header->patch_size == -1) {
        rombp_log_err("Failed to get end of bps patch file length, error: %d\n", errno);
        return PATCH_ERR_IO;
    }
    // Reset back to after the marker
    rc = fseek(bps_file, BPS_MARKER_SIZE, SEEK_SET);
    if (rc == -1) {
        rombp_log_err("Failed to seek bps file back to beginning before reading metadata, error: %d\n", errno);
        return PATCH_ERR_IO;
    }
    rc = decode_varint(bps_file, &file_header->source_size);
    if (rc == -1) {
        rombp_log_err("BPS file: Failed to read source size\n");
        return PATCH_ERR_IO;
    }
    rc = decode_varint(bps_file, &file_header->target_size);
    if (rc == -1) {
        rombp_log_err("BPS file: Failed to read target size\n");
        return PATCH_ERR_IO;
    }
    rc = decode_varint(bps_file, &file_header->metadata_size);
    if (rc == -1) {
        rombp_log_err("BPS file: Failed to read metadata size\n");
        return PATCH_ERR_IO;
    }
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
    long pos = ftell(bps_file);
    if (pos == -1) {
        rombp_log_err("Failed to get current file position, error: %d\n", errno);
        return HUNK_ERR_IO;
    }
    rombp_log_info("Position is: %ld\n", pos);
    if (pos >= file_header->patch_size - FOOTER_LENGTH) {
        return HUNK_DONE;
    }
    uint64_t data;
    int rc = decode_varint(bps_file, &data);
    if (rc == -1) {
        rombp_log_err("Couldn't get data for command and length\n");
        return HUNK_ERR_IO;
    }
    uint64_t command = data & 3;
    uint64_t length = (data >> 2) + 1;

    rombp_log_info("Command is: %ld, length is: %ld\n", command, length);

    // Skip command for now!
    int skip_pos = fseek(bps_file, length, SEEK_CUR);
    if (skip_pos == -1) {
        rombp_log_err("Failed to skip command. error: %d\n", errno);
        return HUNK_ERR_IO;
    }

    return HUNK_NEXT;
}
