#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include "sniffer.h"
#include "colors.h"

static void usage(void) {
    fprintf(stderr,
            YELLOW "\nUsage: " WHITE "imprint-sniffer [--make-json] <image-file>\n\n"
            YELLOW "Options:\n" WHITE
            "  --make-json   Create a minimal metadata JSON next to the image\n"
            "  --help        Show this help message\n" RESET
    );
}

static int
count_chunks_for_base(const char *base)
{
    char path[PATH_MAX];
    int count = 0;

    for (int i = 0; i < 1000; i++) {
        snprintf(path, sizeof(path), "%s.%03d", base, i);
        if (access(path, F_OK) == 0)
            count++;
        else
            break;
    }

    if (count == 0) {
        return 1;   // treat as single-file image
    }

    return count;
}


int main(int argc, char **argv) {
    int make_json = 0;

    /* No arguments at all */
    if (argc < 2) {
        usage();
        return 1;
    }

    int argi = 1;

    /* --help */
    if (strcmp(argv[argi], "--help") == 0) {
        usage();
        return 0;
    }

    /* --make-json */
    if (strcmp(argv[argi], "--make-json") == 0) {
        make_json = 1;
        argi++;
    }

    /* If next argument starts with '-' → unknown flag */
    if (argi < argc && argv[argi][0] == '-') {
        fprintf(stderr, RED "\nError: " WHITE "unknown option: %s\n", argv[argi]);
        usage();
        return 1;
    }

    /* Require image filename */
    if (argi >= argc) {
        fprintf(stderr, RED "\nError: " WHITE "missing image filename\n");
        usage();
        return 1;
    }

    const char *imagefile = argv[argi];

    /* Sniff image */
    SniffResult info;
    if (!sniff_image(imagefile, &info)) {
        fprintf(stderr, RED "\nERROR: " WHITE "Could not sniff image. Check the image path and filename.\n");
        return 1;
    }

    /* Print info */
    printf(YELLOW "\nImage: " WHITE "%s\n", imagefile);
    printf(YELLOW "Compression: " WHITE "%s\n", info.compression);
    printf(YELLOW "Backend: " WHITE "%s\n", info.backend);
    printf(YELLOW "Filesystem size: " WHITE "%llu bytes\n", (unsigned long long)info.fs_bytes);
    printf(YELLOW "Used bytes: " WHITE "%llu\n", (unsigned long long)info.used_bytes);
    printf(YELLOW "Block size: " WHITE "%u\n", info.block_size);
    printf(YELLOW "Chunked: " WHITE "%s\n", info.chunked ? "yes" : "no");

    /* JSON generation */
    if (make_json) {

        /* Normalize filename: strip .000 / .001 / .002 suffix */
        char base[PATH_MAX];
        strncpy(base, imagefile, sizeof(base));
        base[sizeof(base)-1] = '\0';

        size_t len = strlen(base);
        if (len > 4 &&
            base[len-4] == '.' &&
            isdigit((unsigned char)base[len-3]) &&
            isdigit((unsigned char)base[len-2]) &&
            isdigit((unsigned char)base[len-1])) {

            base[len-4] = '\0';   // strip .000
            }

            /* JSON filename must match base name */
            char jsonpath[PATH_MAX];
        snprintf(jsonpath, sizeof(jsonpath), "%s.json", base);

        if (access(jsonpath, F_OK) == 0) {
            fprintf(stderr,
                    RED "\nERROR:\n" WHITE "JSON file already exists: %s\n"
                    YELLOW "Refusing to overwrite.\n" RESET,
                    jsonpath
            );
            return 1;
        }

        /* Determine chunk_count accurately */
        int chunk_count = info.chunked ? count_chunks_for_base(base) : 1;

        FILE *f = fopen(jsonpath, "w");
        if (!f) {
            perror("fopen");
            return 1;
        }

        fprintf(f,
                "{\n"
                "  \"schema\": 1,\n"
                "  \"image\": \"%s\",\n"
                "  \"compression\": \"%s\",\n"
                "  \"backend\": \"%s\",\n"
                "  \"chunked\": %s,\n"
                "  \"chunk_count\": %d,\n"
                "  \"chunk_size_mb\": 0,\n"
                "  \"partition_size_bytes\": 1,\n"
                "  \"filesystem_size\": %llu,\n"
                "  \"used_bytes\": %llu,\n"
                "  \"block_size\": %u,\n"
                "  \"note\": \"This JSON was reconstructed by imprint-sniffer. Partition number, disk name, and original layout are unknown and must be provided by the user.\"\n"
                "}\n",
                imagefile,
                info.compression,
                info.backend,
                info.chunked ? "true" : "false",
                chunk_count,
                (unsigned long long)info.fs_bytes,
                (unsigned long long)info.used_bytes,
                info.block_size
        );

        fclose(f);
        printf(YELLOW "\nCreated metadata file: " WHITE "%s\n", jsonpath);
    }

    return 0;
}
