#include "sniffer.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <zstd.h>
#include <stdlib.h>
#include <lz4frame.h>

static bool read_magic(const char *path, unsigned char *buf, size_t len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    size_t r = fread(buf, 1, len, f);
    fclose(f);

    return r == len;
}

static bool decompress_zstd_header(const char *path, unsigned char *out, size_t out_len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    ZSTD_DStream *dstream = ZSTD_createDStream();
    if (!dstream) {
        fclose(f);
        return false;
    }

    size_t initResult = ZSTD_initDStream(dstream);
    if (ZSTD_isError(initResult)) {
        ZSTD_freeDStream(dstream);
        fclose(f);
        return false;
    }

    unsigned char inbuf[1024 * 1024];   // 1 MB input buffer
    ZSTD_inBuffer input = (ZSTD_inBuffer){ inbuf, 0, 0 };
    ZSTD_outBuffer output = (ZSTD_outBuffer){ out, out_len, 0 };

    while (output.pos < out_len) {
        if (input.pos == input.size) {
            input.size = fread(inbuf, 1, sizeof(inbuf), f);
            input.pos = 0;

            if (input.size == 0) {
                break; // EOF
            }
        }

        size_t ret = ZSTD_decompressStream(dstream, &output, &input);

        if (ZSTD_isError(ret)) {
            printf("DEBUG: ZSTD_decompressStream error: %s\n",
                   ZSTD_getErrorName(ret));
            ZSTD_freeDStream(dstream);
            fclose(f);
            return false;
        }

        if (output.pos > 0) {
            // We have decompressed data!
            break;
        }
    }

    ZSTD_freeDStream(dstream);
    fclose(f);

    return output.pos > 0;
}


static bool decompress_lz4_header(const char *path, unsigned char *out, size_t out_len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    LZ4F_dctx *dctx;
    size_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(err)) {
        fclose(f);
        return false;
    }

    unsigned char inbuf[1024 * 1024];   // 1 MB input buffer
    size_t out_pos = 0;

    while (out_pos < out_len) {
        size_t in_size = fread(inbuf, 1, sizeof(inbuf), f);
        if (in_size == 0)
            break;

        size_t in_pos = 0;

        while (in_pos < in_size && out_pos < out_len) {
            size_t src_size = in_size - in_pos;
            size_t dst_size = out_len - out_pos;

            size_t ret = LZ4F_decompress(dctx,
                                         out + out_pos, &dst_size,
                                         inbuf + in_pos, &src_size,
                                         NULL);

            if (LZ4F_isError(ret)) {
                printf("DEBUG: LZ4F_decompress error: %s\n",
                       LZ4F_getErrorName(ret));
                LZ4F_freeDecompressionContext(dctx);
                fclose(f);
                return false;
            }

            in_pos += src_size;
            out_pos += dst_size;

            if (dst_size > 0)
                goto done; // we got some decompressed bytes
        }
    }

    done:
    LZ4F_freeDecompressionContext(dctx);
    fclose(f);

    return out_pos > 0;
}



typedef struct {
    char magic[8];        // "partclon"
    uint32_t version;
    uint32_t header_size;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t used_blocks;
    char fs[16];          // filesystem name (e.g. "ext4", "btrfs")
} __attribute__((packed)) PartcloneHeader;



bool sniff_image(const char *path, SniffResult *out) {
    memset(out, 0, sizeof(SniffResult));

    unsigned char magic[4];
    if (!read_magic(path, magic, sizeof(magic))) {
        printf("DEBUG: read_magic() failed\n");
        return false;
    }

    printf("DEBUG: magic = %02X %02X %02X %02X\n",
           magic[0], magic[1], magic[2], magic[3]);

    unsigned char headerbuf[65536];

    //
    // ────────────────────────────────────────────────
    //  Z S T D
    // ────────────────────────────────────────────────
    //
    if (magic[0] == 0x28 && magic[1] == 0xB5 &&
        magic[2] == 0x2F && magic[3] == 0xFD) {

        strcpy(out->compression, "zstd");
    printf("DEBUG: Detected zstd, attempting partial decompression...\n");

    if (!decompress_zstd_header(path, headerbuf, sizeof(headerbuf))) {
        printf("DEBUG: decompress_zstd_header() failed\n");
        return false;
    }

    printf("DEBUG: Partial decompression succeeded\n");
        }

        //
        // ────────────────────────────────────────────────
        //  L Z 4
        // ────────────────────────────────────────────────
        //
        else if (magic[0] == 0x04 && magic[1] == 0x22 &&
            magic[2] == 0x4D && magic[3] == 0x18) {

            strcpy(out->compression, "lz4");
        printf("DEBUG: Detected lz4, attempting partial decompression...\n");

        if (!decompress_lz4_header(path, headerbuf, sizeof(headerbuf))) {
            printf("DEBUG: decompress_lz4_header() failed\n");
            return false;
        }

        printf("DEBUG: Partial decompression succeeded\n");
            }

            //
            // ────────────────────────────────────────────────
            //  UNKNOWN COMPRESSION
            // ────────────────────────────────────────────────
            //
            else {
                strcpy(out->compression, "unknown");
                printf("DEBUG: Unknown compression\n");
                out->valid = true;
                return true;
            }

            //
            // ────────────────────────────────────────────────
            //  PARTCLONE TEXTUAL HEADER DETECTION
            // ────────────────────────────────────────────────
            //
            if (memcmp(headerbuf, "partclon", 8) != 0) {
                printf("DEBUG: Partclone magic not found\n");
                strcpy(out->backend, "unknown");
                out->valid = true;
                return true;
            }

            printf("DEBUG: Partclone header detected\n");

            printf("DEBUG: first 64 bytes of decompressed data:\n");
            for (int i = 0; i < 64; i++) {
                printf("%02X ", headerbuf[i]);
            }
            printf("\n");

            //
            // ────────────────────────────────────────────────
            //  FILESYSTEM NAME EXTRACTION
            // ────────────────────────────────────────────────
            //
            const char *fs_names[] = {
                "BTRFS", "EXT4", "EXT3", "EXT2",
                "XFS", "NTFS", "FAT32", "F2FS"
            };
            const size_t fs_count = sizeof(fs_names) / sizeof(fs_names[0]);
            const char *found_fs = NULL;

            for (size_t i = 0; i < fs_count; i++) {
                const char *p = memmem(headerbuf, 128, fs_names[i], strlen(fs_names[i]));
                if (p) {
                    found_fs = fs_names[i];
                    break;
                }
            }

            if (found_fs)
                strcpy(out->backend, found_fs);
    else
        strcpy(out->backend, "unknown");

    //
    // ────────────────────────────────────────────────
    //  NUMERIC FIELDS (NOT PARSED YET)
    // ────────────────────────────────────────────────
    //
    out->block_size = 0;
    out->fs_bytes   = 0;
    out->used_bytes = 0;

    out->valid = true;
    return true;
}
