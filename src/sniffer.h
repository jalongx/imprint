#ifndef SNIFFER_H
#define SNIFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;

    char compression[32];
    char backend[64];

    uint64_t fs_bytes;
    uint64_t used_bytes;
    uint32_t block_size;

    bool chunked;
    bool metadata_present;
    bool metadata_valid;
} SniffResult;

bool sniff_image(const char *path, SniffResult *out);

#endif
