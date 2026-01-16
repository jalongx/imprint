#include "sniffer.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <zstd.h>
#include <stdlib.h>
#include <lz4frame.h>
#include <stdbool.h>
#include <ctype.h>

/* Read first len bytes from file */
static bool
read_magic(const char *path, unsigned char *buf, size_t len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    size_t r = fread(buf, 1, len, f);
    fclose(f);

    return r == len;
}

/* Decompress up to out_len bytes of a zstd stream into out */
static bool
decompress_zstd_header(const char *path, unsigned char *out, size_t out_len)
{
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

    unsigned char inbuf[1024 * 1024];
    ZSTD_inBuffer input = { inbuf, 0, 0 };
    ZSTD_outBuffer output = { out, out_len, 0 };

    while (output.pos < out_len) {
        if (input.pos == input.size) {
            input.size = fread(inbuf, 1, sizeof(inbuf), f);
            input.pos = 0;

            if (input.size == 0)
                break;
        }

        size_t ret = ZSTD_decompressStream(dstream, &output, &input);
        if (ZSTD_isError(ret)) {
            /* printf("DEBUG: ZSTD_decompressStream error: %s\n",
             *                   ZSTD_getErrorName(ret)); */
            ZSTD_freeDStream(dstream);
            fclose(f);
            return false;
        }

        if (ret == 0)
            break;
    }

    ZSTD_freeDStream(dstream);
    fclose(f);

    return output.pos > 0;
}

/* Decompress up to out_len bytes of an LZ4 stream into out */
static bool
decompress_lz4_header(const char *path, unsigned char *out, size_t out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    LZ4F_dctx *dctx;
    size_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(err)) {
        fclose(f);
        return false;
    }

    unsigned char inbuf[1024 * 1024];
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
                /* printf("DEBUG: LZ4F_decompress error: %s\n",
                 *                       LZ4F_getErrorName(ret)); */
                LZ4F_freeDecompressionContext(dctx);
                fclose(f);
                return false;
            }

            in_pos += src_size;
            out_pos += dst_size;

            if (ret == 0)
                goto done;
        }
    }

    done:
    LZ4F_freeDecompressionContext(dctx);
    fclose(f);

    return out_pos > 0;
}

/* Infer backend from filesystem name */
static void
infer_backend_from_fs(const char *fs, char *backend, size_t backend_len)
{
    if (!fs || !*fs) {
        snprintf(backend, backend_len, "unknown");
        return;
    }

    if (strstr(fs, "EXT") || strstr(fs, "EXTFS")) {
        snprintf(backend, backend_len, "partclone.extfs");
    } else if (strstr(fs, "NTFS")) {
        snprintf(backend, backend_len, "partclone.ntfs");
    } else if (strstr(fs, "BTRFS")) {
        snprintf(backend, backend_len, "partclone.btrfs");
    } else if (strstr(fs, "XFS")) {
        snprintf(backend, backend_len, "partclone.xfs");
    } else if (strstr(fs, "FAT")) {
        snprintf(backend, backend_len, "partclone.fat");
    } else if (strstr(fs, "F2FS")) {
        snprintf(backend, backend_len, "partclone.f2fs");
    } else if (strstr(fs, "HFS")) {
        snprintf(backend, backend_len, "partclone.hfsp");
    } else {
        snprintf(backend, backend_len, "unknown");
    }
}

/* Infer chunked from filename suffix ".000".."999" */
static bool
infer_chunked_from_path(const char *path)
{
    if (!path)
        return false;

    size_t len = strlen(path);
    if (len < 4)
        return false;

    const char *p = path + len - 4;
    if (p[0] != '.')
        return false;

    return isdigit((unsigned char)p[1]) &&
    isdigit((unsigned char)p[2]) &&
    isdigit((unsigned char)p[3]);
}

/* Try to parse numeric fields based on FS position */
static bool
parse_partclone_numeric_fields(const unsigned char *buf,
                               size_t len,
                               size_t fs_offset,
                               uint32_t *block_size_out,
                               uint64_t *total_blocks_out,
                               uint64_t *used_blocks_out)
{
    if (!buf || len < 64)
        return false;

    uint32_t block_size = 0;
    uint64_t total_blocks = 0;
    uint64_t used_blocks = 0;

    size_t search_start = fs_offset + 8;
    if (search_start > len)
        return false;

    size_t search_end = (search_start + 64 < len) ? search_start + 64 : len;

    for (size_t i = search_start; i + 4 <= search_end; i += 4) {
        uint32_t candidate =
        (uint32_t)(buf[i] |
        (buf[i+1] << 8) |
        (buf[i+2] << 16) |
        (buf[i+3] << 24));

        if (candidate >= 512 && candidate <= 65536 &&
            (candidate & (candidate - 1)) == 0) {
            block_size = candidate;

        for (size_t j = search_start; j + 4 <= search_end; j += 4) {
            uint32_t c1 =
            (uint32_t)(buf[j] |
            (buf[j+1] << 8) |
            (buf[j+2] << 16) |
            (buf[j+3] << 24));
            for (size_t k = j + 4; k + 4 <= search_end; k += 4) {
                uint32_t c2 =
                (uint32_t)(buf[k] |
                (buf[k+1] << 8) |
                (buf[k+2] << 16) |
                (buf[k+3] << 24));
                if (c1 == c2 && c1 > 0) {
                    total_blocks = c1;
                    used_blocks  = c2;
                    goto done;
                }
            }
        }
            }
    }

    done:
    if (block_size == 0 || total_blocks == 0) {
        *block_size_out    = 0;
        *total_blocks_out  = 0;
        *used_blocks_out   = 0;
        return false;
    }

    *block_size_out    = block_size;
    *total_blocks_out  = total_blocks;
    *used_blocks_out   = used_blocks;
    return true;
}

bool
sniff_image(const char *path, SniffResult *out)
{
    memset(out, 0, sizeof(SniffResult));

    unsigned char magic[4];
    if (!read_magic(path, magic, sizeof(magic))) {
        /* printf("DEBUG: read_magic() failed\n"); */
        return false;
    }

    unsigned char headerbuf[65536];
    size_t header_len = sizeof(headerbuf);
    memset(headerbuf, 0, header_len);
    bool have_header = false;

    if (magic[0] == 0x28 && magic[1] == 0xB5 &&
        magic[2] == 0x2F && magic[3] == 0xFD) {

        strcpy(out->compression, "zstd");

    if (!decompress_zstd_header(path, headerbuf, header_len)) {
        /* printf("DEBUG: decompress_zstd_header() failed\n"); */
        return false;
    }

    have_header = true;

        } else if (magic[0] == 0x04 && magic[1] == 0x22 &&
            magic[2] == 0x4D && magic[3] == 0x18) {

            strcpy(out->compression, "lz4");

        if (!decompress_lz4_header(path, headerbuf, header_len)) {
            /* printf("DEBUG: decompress_lz4_header() failed\n"); */
            return false;
        }

        have_header = true;

            } else {
                strcpy(out->compression, "unknown");
                out->valid   = true;
                out->chunked = infer_chunked_from_path(path);
                return true;
            }

            if (!have_header) {
                out->valid = false;
                return false;
            }

            if (memcmp(headerbuf, "partclone-image", 15) != 0) {
                strcpy(out->backend, "unknown");
                out->valid   = true;
                out->chunked = infer_chunked_from_path(path);
                return true;
            }

            const char *fs_candidates[] = {
                "BTRFS", "EXTFS", "EXT4", "EXT3", "EXT2",
                "XFS", "NTFS", "FAT32", "FAT", "F2FS", "HFS", "REFS"
            };
            const size_t fs_count = sizeof(fs_candidates) / sizeof(fs_candidates[0]);

            const char *found_fs = NULL;
            size_t fs_offset = 0;

            for (size_t i = 0; i < fs_count; i++) {
                const char *p = memmem(headerbuf, header_len,
                                       fs_candidates[i],
                                       strlen(fs_candidates[i]));
                if (p) {
                    found_fs = fs_candidates[i];
                    fs_offset = (size_t)(p - (const char *)headerbuf);
                    break;
                }
            }

            char fs_name[32] = {0};
            if (found_fs)
                strncpy(fs_name, found_fs, sizeof(fs_name) - 1);

    infer_backend_from_fs(fs_name, out->backend, sizeof(out->backend));

    out->block_size = 0;
    out->fs_bytes   = 0;
    out->used_bytes = 0;

    if (found_fs) {
        uint32_t block_size   = 0;
        uint64_t total_blocks = 0;
        uint64_t used_blocks  = 0;

        bool ok = parse_partclone_numeric_fields(headerbuf,
                                                 header_len,
                                                 fs_offset,
                                                 &block_size,
                                                 &total_blocks,
                                                 &used_blocks);
        if (ok) {
            out->block_size = block_size;
            out->fs_bytes   = (uint64_t)block_size * total_blocks;
            out->used_bytes = (uint64_t)block_size * used_blocks;
        }
    }

    out->chunked = infer_chunked_from_path(path);
    out->valid   = true;
    return true;
}
