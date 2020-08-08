#include <errno.h>
#include <sys/param.h>

#include "bps.h"
#include "log.h"

static const uint8_t BPS_EXPECTED_MARKER[] = {
    0x42, 0x50, 0x53, 0x31 // BPS1
};
static const size_t BPS_MARKER_SIZE = sizeof(BPS_EXPECTED_MARKER) / sizeof(uint8_t);

static const size_t FOOTER_LENGTH = 12;
static const size_t FOOTER_ITEMS = 12 / sizeof(uint32_t);
static const size_t BUF_SIZE = 32768;

typedef enum bps_command_type {
    BPS_SOURCE_READ = 0,
    BPS_TARGET_READ = 1,
    BPS_SOURCE_COPY = 2,
    BPS_TARGET_COPY = 3,
} bps_command_type;

static uint32_t crc32_for_byte(uint32_t r) {
    for(int i = 0; i < 8; ++i) {
        r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;
    }

    return r ^ (uint32_t)0xFF000000L;
}

static void crc32(const void *data, size_t n_bytes, uint32_t* crc) {
    static uint32_t table[0x100];

    if(!*table) {
        for(size_t i = 0; i < 0x100; ++i) {
            table[i] = crc32_for_byte(i);
        }
    }

    for(size_t i = 0; i < n_bytes; ++i) {
        *crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
    }
}

static int decode_varint(FILE* bps_file, uint64_t* out) {
    uint64_t data = 0;
    uint64_t shift = 1;

    while (1) {
        uint8_t ch;
        size_t nread = fread(&ch, 1, sizeof(uint8_t), bps_file);
        if (nread != 1) {
            rombp_log_err("Failed to read next byte, read: %ld, error code: %d\n", (long)nread, errno);
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

    file_header->output_offset = 0;
    file_header->source_relative_offset = 0;
    file_header->target_relative_offset = 0;
    file_header->output_crc32 = 0;

    return PATCH_OK;
}

static rombp_hunk_iter_status bps_write_output(bps_file_header* file_header, FILE* output_file, uint8_t* buf, size_t len) {
    size_t nwritten = fwrite(buf, sizeof(uint8_t), len, output_file);
    if (nwritten < len && ferror(output_file)) {
        rombp_log_err("BPS output write error: %d\n", errno);
        return HUNK_ERR_IO;
    }

    crc32(buf, len, &file_header->output_crc32);
    file_header->output_offset += len;

    return HUNK_NEXT;
}

static rombp_hunk_iter_status bps_source_read(bps_file_header* file_header, uint64_t length, FILE* input_file, FILE* output_file) {
    int pos = fseek(input_file, file_header->output_offset, SEEK_SET);
    if (pos == -1) {
        rombp_log_err("Failed to seek source file. err: %d\n", errno);
        return HUNK_ERR_IO;
    }

    pos = fseek(output_file, file_header->output_offset, SEEK_SET);
    if (pos == -1) {
        rombp_log_err("Failed to seek target file. err: %d\n", errno);
        return HUNK_ERR_IO;
    }

    uint64_t remaining = length;
    uint8_t buf[BUF_SIZE];

    while (remaining > 0) {
        uint64_t target_read = MIN(BUF_SIZE, remaining);
        size_t nread = fread(buf, sizeof(uint8_t), target_read, input_file);
        if (nread < target_read && ferror(input_file)) {
            rombp_log_err("Error during BPS source read, error: %d\n", errno);
            return HUNK_ERR_IO;
        }

        rombp_hunk_iter_status werror = bps_write_output(file_header, output_file, buf, nread);
        if (werror != HUNK_NEXT) {
            rombp_log_err("Error during BPS source read, write error\n");
            return werror;
        }

        remaining -= nread;
    }

    return HUNK_NEXT;
}

static rombp_hunk_iter_status bps_target_read(bps_file_header* file_header, uint64_t length, FILE* output_file, FILE* bps_file) {
    int pos = fseek(output_file, file_header->output_offset, SEEK_SET);
    if (pos == -1) {
        rombp_log_err("Failed to seek target file. err: %d\n", errno);
        return HUNK_ERR_IO;
    }

    uint64_t remaining = length;
    uint8_t buf[BUF_SIZE];
        
    while (remaining > 0) {
        uint64_t target_read = MIN(BUF_SIZE, remaining);
        size_t nread = fread(buf, sizeof(uint8_t), target_read, bps_file);
        if (nread < target_read && ferror(bps_file)) {
            rombp_log_err("Error during BPS target read, read patch error: %d\n", errno);
            return HUNK_ERR_IO;
        }
        
        rombp_hunk_iter_status werror = bps_write_output(file_header, output_file, buf, nread);
        if (werror != HUNK_NEXT) {
            rombp_log_err("Error during BPS target read, write error\n");
            return werror;
        }

        remaining -= nread;
    }

    return HUNK_NEXT;
}

static rombp_hunk_iter_status bps_source_copy(bps_file_header* file_header, uint64_t length, FILE* input_file, FILE* output_file, FILE* bps_file) {
    uint64_t data;
    int rc = decode_varint(bps_file, &data);
    if (rc == -1) {
        rombp_log_err("Failed to decode source relative offset data\n");
        return HUNK_ERR_IO;
    }
    file_header->source_relative_offset += (data & 1 ? -1 : 1) * (data >> 1);
    rombp_log_info("Source relative offset is: %ld\n", file_header->source_relative_offset);

    int pos = fseek(output_file, file_header->output_offset, SEEK_SET);
    if (pos == -1) {
        rombp_log_err("Failed to seek target file. err: %d\n", errno);
        return HUNK_ERR_IO;
    }

    pos = fseek(input_file, file_header->source_relative_offset, SEEK_SET);
    if (pos == -1) {
        rombp_log_err("Failed to seek target file. err: %d\n", errno);
        return HUNK_ERR_IO;
    }

    uint64_t remaining = length;
    uint8_t buf[BUF_SIZE];

    while (remaining > 0) {
        uint64_t target_read = MIN(BUF_SIZE, remaining);
        size_t nread = fread(buf, sizeof(uint8_t), target_read, input_file);
        if (nread < target_read && ferror(input_file)) {
            rombp_log_err("Error during BPS source read, error: %d\n", errno);
            return HUNK_ERR_IO;
        }
        rombp_hunk_iter_status werror = bps_write_output(file_header, output_file, buf, nread);
        if (werror != HUNK_NEXT) {
            rombp_log_err("Error during BPS source copy, write error\n");
            return werror;
        }
        file_header->source_relative_offset += nread;
        remaining -= nread;
    }

    return HUNK_NEXT;
}

static rombp_hunk_iter_status bps_target_copy(bps_file_header* file_header, uint64_t length, FILE* output_file, FILE* bps_file) {
    uint64_t data;
    int rc = decode_varint(bps_file, &data);
    if (rc == -1) {
        rombp_log_err("Failed to decode target relative offset data\n");
        return HUNK_ERR_IO;
    }
    file_header->target_relative_offset += (data & 1 ? -1 : 1) * (data >> 1);
    rombp_log_info("Target relative offset is: %ld\n", file_header->target_relative_offset);

    uint64_t remaining = length;
    uint8_t buf[BUF_SIZE];

    while (remaining > 0) {
        int pos = fseek(output_file, file_header->target_relative_offset, SEEK_SET);
        if (pos == -1) {
            rombp_log_err("Failed to seek target file. err: %d\n", errno);
            return HUNK_ERR_IO;
        }

        uint64_t target_read = MIN(BUF_SIZE, remaining);
        size_t nread = fread(buf, sizeof(uint8_t), target_read, output_file);
        if (nread < target_read && ferror(output_file)) {
            rombp_log_err("Error during BPS target read, error: %d\n", errno);
            return HUNK_ERR_IO;
        }

        pos = fseek(output_file, file_header->output_offset, SEEK_SET);
        if (pos == -1) {
            rombp_log_err("Failed to seek target file. err: %d\n", errno);
            return HUNK_ERR_IO;
        }

        rombp_hunk_iter_status werror = bps_write_output(file_header, output_file, buf, nread);
        if (werror != HUNK_NEXT) {
            rombp_log_err("Error during BPS target copy, write error\n");
            return werror;
        }

        file_header->target_relative_offset += nread;
        remaining -= nread;
    }

    return HUNK_NEXT;
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

    switch (command) {
        case BPS_SOURCE_READ:
            return bps_source_read(file_header,
                                   length,
                                   input_file,
                                   output_file);
        case BPS_TARGET_READ:
            return bps_target_read(file_header,
                                   length,
                                   output_file,
                                   bps_file);
        case BPS_SOURCE_COPY:
            return bps_source_copy(file_header,
                                   length,
                                   input_file,
                                   output_file,
                                   bps_file);
        case BPS_TARGET_COPY: {
            return bps_target_copy(file_header,
                                   length,
                                   output_file,
                                   bps_file);
        }
        default:
            rombp_log_err("Unknown BPS command: %ld, aborting!\n", (long)command);
            return HUNK_ERR_IO;
    }

    return HUNK_NEXT;
}

rombp_patch_err bps_end(bps_file_header* file_header, FILE* bps_file) {
    uint32_t footer[FOOTER_ITEMS];

    size_t nread = fread(&footer, sizeof(uint32_t), FOOTER_ITEMS, bps_file);
    if (nread < FOOTER_LENGTH && ferror(bps_file)) {
        rombp_log_err("Error reading BPS footer: %d\n", errno);
        return PATCH_ERR_IO;
    }

    uint32_t expected_output_crc32 = footer[1];
    if (file_header->output_crc32 != expected_output_crc32) {
        rombp_log_err("Footer output CRC32 and expected CRC32 do not match! Expected: %d, got: %d\n",
                      expected_output_crc32, file_header->output_crc32);
        return PATCH_INVALID_OUTPUT_CHECKSUM;
    }

    rombp_log_info("Output file CRC32 is correct\n");
    return PATCH_OK;
}
