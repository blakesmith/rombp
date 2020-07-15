#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "ips.h"

// Copy the input file to the output file, it is assumed
// that both files will be at position 0 before this function
// is called.
#define BUF_SIZE 32768

static int copy_file(FILE* input_file, FILE* output_file) {
    uint8_t buf[BUF_SIZE];
    int rc;

    int infd = fileno(input_file);
    if (infd == -1) {
        fprintf(stderr, "Bad input file, is the stream closed?\n");
        return infd;
    }
    struct stat input_file_stat;
    rc = fstat(infd, &input_file_stat);
    if (rc == -1) {
        fprintf(stderr, "Failed to stat input file, errno: %d\n", errno);
        return rc;
    }

    off_t input_file_size = input_file_stat.st_size;
    size_t total_read = 0;
    while (1) {
        size_t nread = fread(&buf, 1, BUF_SIZE, input_file);
        total_read += nread;
        if (nread < BUF_SIZE) {
            if (total_read < input_file_size) {
                fprintf(stderr, "Failed to read the entire input file, read: %ld bytes, input file size: %ld\n", (long int)total_read, (long int)input_file_size);
                return -1;
            } else {
                return 0;
            }
        }
        size_t nwritten = fwrite(&buf, 1, nread, output_file);
        if (nwritten < nread) {
            fprintf(stderr, "Tried to copy %ld bytes to the output file, but only copied: %ld\n", (long int)nread, (long int)nwritten);
            return -1;
        }
    }

    return 0;
}

static const unsigned char IPS_EXPECTED_HEADER[] = {
    0x50, 0x41, 0x54, 0x43, 0x48 // PATCH
};
static const size_t IPS_HEADER_SIZE = sizeof(IPS_EXPECTED_HEADER) / sizeof(unsigned char);

static int verify_ips_header(FILE* ips_file) {
    uint8_t buf[IPS_HEADER_SIZE];

    size_t nread = fread(&buf, 1, IPS_HEADER_SIZE, ips_file);
    if (nread < IPS_HEADER_SIZE) {
        fprintf(stderr, "IPS header malformed, expected to get at least %ld bytes in the IPS file, read: %ld\n", IPS_HEADER_SIZE, (long int)nread);
        return IPS_INVALID_HEADER;
    }

    for (int i = 0; i < IPS_HEADER_SIZE; i++) {
        if (IPS_EXPECTED_HEADER[i] != buf[i]) {
            fprintf(stderr, "IPS header at byte %d doesn't match. Value: %d\n", i, buf[i]);
            return IPS_INVALID_HEADER;
        }
    }

    return 0;
}

static const size_t HUNK_PREAMBLE_BYTE_SIZE = 5;

static int ips_next_hunk_header(FILE* ips_file, ips_hunk_header* header) {
    uint8_t buf[HUNK_PREAMBLE_BYTE_SIZE];

    assert(header != NULL);

    // Read the hunk preamble
    // 3 byte offset
    // 2 byte payload length.
    size_t nread = fread(&buf, 1, HUNK_PREAMBLE_BYTE_SIZE, ips_file);
    if (nread < HUNK_PREAMBLE_BYTE_SIZE) {
        int err = ferror(ips_file);
        if (err != 0) {
            fprintf(stderr, "Error reading from IPS file, error: %d\n", err);
            return HUNK_ERR_IPS;
        }
        if (feof(ips_file) != 0) {
            return HUNK_DONE;
        }
    }

    // We have a 5 byte buffer of the hunk preamble, decode values:
    header->offset = ((buf[0] << 16) & 0x00FF0000) |
        ((buf[1] << 8) & 0x0000FF00) |
        (buf[2] & 0x000000FF);
    header->length = ((buf[3] << 8) & 0xFF00) |
        (buf[4] & 0x00FF);

    return HUNK_NEXT;
}

int ips_patch(FILE* input_file, FILE* output_file, FILE* ips_file) {
    int rc;

    // First, copy the input to output
    rc = copy_file(input_file, output_file);
    if (rc != 0) {
        fprintf(stderr, "Failed to seek to copy input file to output file: %d\n", rc);
        return rc;
    }

    // Sanity check that we're looking at an IPS file.
    rc = verify_ips_header(ips_file);
    if (rc == IPS_INVALID_HEADER) {
        fprintf(stderr, "IPS input file doesn't start with a PATCH header\n");
        return rc;
    } else if (rc != 0) {
        fprintf(stderr, "Failed to verify IPS header: %d\n", rc);
        return rc;
    }

    // Then, iterate through hunks
    int hunk_count = 0;
    ips_hunk_header hunk_header;
    while (1) {
        rc = ips_next_hunk_header(ips_file, &hunk_header);
        if (rc < 0) {
            fprintf(stderr, "Error getting next hunk, at hunk count: %d\n", hunk_count);
            return rc;
        } else if (rc == HUNK_DONE) {
            return hunk_count;
        } else {
            assert(rc == HUNK_NEXT);

            hunk_count++;

            printf("Hunk RLE: %d, offset: %d, length: %d\n",
                   hunk_header.length == 0,
                   hunk_header.offset,
                   hunk_header.length);

            // Temporarily: Skip the payload
            rc = fseek(ips_file, hunk_header.length, SEEK_CUR);
            if (rc < 0) {
                printf("Failed to skip the hunk payload, err: %d\n", errno);
                return rc;
            }
        }
    }

    return 0;
}
