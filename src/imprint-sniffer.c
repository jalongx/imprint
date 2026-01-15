#include <stdio.h>
#include "sniffer.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: imprint-sniffer <image-file>\n");
        return 1;
    }

    SniffResult info;
    if (!sniff_image(argv[1], &info)) {
        fprintf(stderr, "ERROR: Could not sniff image.\n");
        return 1;
    }

    printf("Image: %s\n", argv[1]);
    printf("Compression: %s\n", info.compression);
    printf("Backend: %s\n", info.backend);
    printf("Filesystem size: %lu bytes\n", info.fs_bytes);
    printf("Used bytes: %lu\n", info.used_bytes);
    printf("Block size: %u\n", info.block_size);
    printf("Chunked: %s\n", info.chunked ? "yes" : "no");

    return 0;
}
