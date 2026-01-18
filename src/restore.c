#include "restore.h"
#include "utils.h"
#include "ui.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>

/* -------------------------------------------------------------
 * Metadata structure
 * ------------------------------------------------------------- */
typedef struct {
    long long partition_size_bytes;
    char backend[128];
    char compression[64];
    bool chunked;
    int chunk_count;
    int chunk_size_mb;   // ← add this
} MetadataInfo;


/* -------------------------------------------------------------
 * Detect chunk suffix ".000".."999" and compute base path
 * ------------------------------------------------------------- */
static void
get_image_base_and_chunked(const char *selected_path,
                           char *base_path,
                           size_t base_len,
                           bool *chunked)
{
    *chunked = false;

    if (!selected_path || !base_path || base_len == 0)
        return;

    /* Copy full path first */
    strncpy(base_path, selected_path, base_len - 1);
    base_path[base_len - 1] = '\0';

    /* ---------------------------------------------------------
     * Detect chunk suffix .000 .. .999
     * (Do NOT strip compression extensions here)
     * --------------------------------------------------------- */
    size_t len = strlen(base_path);

    if (len > 4 &&
        base_path[len - 4] == '.' &&
        isdigit((unsigned char)base_path[len - 3]) &&
        isdigit((unsigned char)base_path[len - 2]) &&
        isdigit((unsigned char)base_path[len - 1]))
    {
        /* Strip .DDD suffix */
        base_path[len - 4] = '\0';
        *chunked = true;
    }
    else {
        *chunked = false;
    }
}

static bool validate_chunk_set(const char *base, int chunk_count)
{
    if (chunk_count <= 1) {
        return true;   // single-file image
    }
        char prefix[2048];
    snprintf(prefix, sizeof(prefix), "%s.", base);

    for (int i = 0; i < chunk_count; i++) {
        char path[4096];
        snprintf(path, sizeof(path), "%s%03d", prefix, i);

        struct stat st;
        if (stat(path, &st) != 0) {
            fprintf(stderr,
                    RED "Missing chunk: %s\n"
                    "Restore aborted.\n" RESET,
                    path);
            return false;
        }
    }

    return true;
}

/* -------------------------------------------------------------
 * Early-gate: load metadata or exit restore
 * ------------------------------------------------------------- */
static bool
load_metadata_or_exit(const char *image_base, MetadataInfo *meta)
{
    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_base);

    FILE *fp = fopen(meta_path, "r");
    if (!fp) {
        ui_error(
            WHITE "This image does not have a matching metadata file.\n\n"
            "       Imprint requires metadata to safely restore an image.\n"
            "       Restore cannot continue." RESET
        );
        return false;
    }

    memset(meta, 0, sizeof(*meta));

    char line[512];

    while (fgets(line, sizeof(line), fp)) {

        char *p;

        /* partition_size_bytes */
        p = strstr(line, "\"partition_size_bytes\"");
        if (p) {
            p = strchr(p, ':');
            if (p) meta->partition_size_bytes = atoll(p + 1);
            continue;
        }

        /* backend */
        p = strstr(line, "\"backend\"");
        if (p) {
            p = strchr(p, ':');
            if (!p) continue;
            p = strchr(p, '"');
            if (!p) continue;
            p++; /* now at first char of value */

            char *end = strchr(p, '"');
            if (!end) continue;

            size_t len = (size_t)(end - p);
            if (len >= sizeof(meta->backend))
                len = sizeof(meta->backend) - 1;

            memcpy(meta->backend, p, len);
            meta->backend[len] = '\0';
            continue;
        }

        /* compression */
        p = strstr(line, "\"compression\"");
        if (p) {
            p = strchr(p, ':');
            if (!p) continue;
            p = strchr(p, '"');
            if (!p) continue;
            p++; /* now at first char of value */

            char *end = strchr(p, '"');
            if (!end) continue;

            size_t len = (size_t)(end - p);
            if (len >= sizeof(meta->compression))
                len = sizeof(meta->compression) - 1;

            memcpy(meta->compression, p, len);
            meta->compression[len] = '\0';
            continue;
        }

        /* chunked */
        p = strstr(line, "\"chunked\"");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                p++;
                while (*p == ' ' || *p == '\t') p++;
                meta->chunked = (strncmp(p, "true", 4) == 0);
            }
            continue;
        }

        /* chunk_size_mb */
        p = strstr(line, "\"chunk_size_mb\"");
        if (p) {
            p = strchr(p, ':');
            if (p) meta->chunk_size_mb = atoi(p + 1);
            continue;
        }

        /* chunk_count */
        p = strstr(line, "\"chunk_count\"");
        if (p) {
            p = strchr(p, ':');
            if (p) meta->chunk_count = atoi(p + 1);
            continue;
        }
    }

    fclose(fp);

    /* Validate required fields */
    if (meta->partition_size_bytes > 0 && meta->backend[0] != '\0') {

        /* default compression if missing */
        if (meta->compression[0] == '\0')
            strcpy(meta->compression, "lz4");

        /* default chunk_count for single-file images */
        if (!meta->chunked && meta->chunk_count < 1)
            meta->chunk_count = 1;

        return true;
    }

    ui_error(
        "The metadata file exists but is incomplete or invalid.\n\n"
        "Imprint requires valid metadata to safely restore an image.\n"
        "Restore cannot continue."
    );
    return false;
}


/* -------------------------------------------------------------
 * Interactive restore flow
 * ------------------------------------------------------------- */
bool
restore_run_interactive(void)
{
    /* 1. Choose image file */
    char *selected_path = ui_choose_image_file();
    if (!selected_path)
        return false;

    /* 2. Normalize to base path */
    char base_image[1024] = {0};
    bool chunked_by_name = false;

    get_image_base_and_chunked(selected_path,
                               base_image,
                               sizeof(base_image),
                               &chunked_by_name);

    /* 2. Load metadata (authoritative) */
    MetadataInfo meta;
    if (!load_metadata_or_exit(base_image, &meta))
        return false;

    /* 2a. Validate chunk set using normalized base path */
    if (!validate_chunk_set(base_image, meta.chunk_count)) {
        ui_error("Missing chunk(s). Restore aborted.");
        return false;
    }

    /* 4. Choose target partition */
    char *device = ui_choose_partition_with_title(
        "Select destination partition for restore",
        "Choose the partition that will be overwritten:"
    );
    if (!device) {
        free(selected_path);
        return false;
    }

    /* 5. Partition size check */
    long long tgt_bytes = get_partition_size_bytes(device);
    if (tgt_bytes <= 0) {
        ui_error("Could not determine size of the target partition.");
        free(device);
        free(selected_path);
        return false;
    }

    if (tgt_bytes < meta.partition_size_bytes) {
        char msg[512];

        snprintf(msg, sizeof(msg),
                 "The target partition is smaller than the original partition.\n\n"
                 "Original partition size: %.2f GB\n"
                 "Target partition size: %.2f GB\n\n"
                 "Partclone cannot restore an image to a smaller partition.\n"
                 "Restore cannot continue.",
                 meta.partition_size_bytes / 1e9,
                 tgt_bytes / 1e9
        );

        ui_error(msg);

        free(device);
        free(selected_path);
        return false;
    }

    /* 6. Run restore pipeline */
    bool ok = run_restore_pipeline(
        meta.backend,
        base_image,
        device,
        meta.compression,
        meta.chunked
    );

    free(device);
    free(selected_path);
    return ok;
}

bool
run_restore_pipeline(const char *backend,
                     const char *image_base,
                     const char *device,
                     const char *compression,
                     bool chunked)
{
    if (!backend || !image_base || !device)
        return false;

    /* ---------------------------------------------------------
     * 1. Select decompressor
     * --------------------------------------------------------- */
    const char *decomp = NULL;

    if (compression && strcmp(compression, "gzip") == 0)
        decomp = "gzip -dc";
    else if (compression && strcmp(compression, "zstd") == 0)
        decomp = "zstd -dc";
    else
        decomp = "lz4 -dc";

    fprintf(stderr, YELLOW "Using decompressor: %s\n" RESET, decomp);

    /* ---------------------------------------------------------
     * 2. Prepare image path for restore
     *
     * IMPORTANT:
     *   - For chunked images: DO NOT strip compression extension.
     *     The chunk files are named:
     *         base.ext.000
     *         base.ext.001
     *         ...
     *
     *   - For single-file images: use the full filename exactly
     *     as provided (with .zst/.lz4/.gz).
     *
     *   Metadata lookup ALWAYS uses the full filename.
     * --------------------------------------------------------- */
    char img_for_restore[1024];
    strncpy(img_for_restore, image_base, sizeof(img_for_restore));
    img_for_restore[sizeof(img_for_restore) - 1] = '\0';

    /* ---------------------------------------------------------
     * 3. Build restore command
     * --------------------------------------------------------- */
    char cmd[3072];

    if (chunked) {
        /*
         * Chunked restore:
         *   cat base.ext.??? | decomp | backend -r -s - -o device
         *
         * Example:
         *   cat nvme.img.zst.??? | zstd -dc | partclone.extfs -r -s - -o /dev/sda1
         */
        snprintf(cmd, sizeof(cmd),
                 "cat '%s'.??? | %s | %s -r -s - -o '%s'",
                 img_for_restore,
                 decomp,
                 backend,
                 device);
    } else {
        /*
         * Single-file restore:
         *   decomp base.ext | backend -r -s - -o device
         *
         * Example:
         *   zstd -dc nvme.img.zst | partclone.extfs -r -s - -o /dev/sda1
         */
        snprintf(cmd, sizeof(cmd),
                 "%s '%s' | %s -r -s - -o '%s'",
                 decomp,
                 image_base,
                 backend,
                 device);
    }

    /* ---------------------------------------------------------
     * 4. pkexec wrapper
     * --------------------------------------------------------- */
    char pk_cmd[4096];
    uid_t euid = geteuid();

    /* ---------------------------------------------------------
     * CLI mode: gx_no_gui == true
     *   - Require sudo
     *   - Do NOT use pkexec
     * GUI mode:
     *   - Use pkexec to elevate
     * --------------------------------------------------------- */
    if (gx_no_gui) {
        /* CLI mode: require sudo */
        if (euid != 0) {
            fprintf(stderr,
                   RED "ERROR:" WHITE " This operation requires root privileges.\n"
                    "       Please run imprintr with sudo.\n\n");
            return false;
        }

        /* CLI mode: run command directly */
        snprintf(pk_cmd, sizeof(pk_cmd), "%s", cmd);

    } else {
        /* GUI mode: use pkexec */
        snprintf(pk_cmd, sizeof(pk_cmd),
                 "pkexec sh -c \"%s\"",
                 cmd);
    }

    if (!gx_no_gui) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "You are about to overwrite the following partition:\n\n"
                 "    %s\n\n"
                 "All data on this partition will be permanently lost.\n"
                 "This action cannot be undone.\n\n"
                 "Do you want to proceed?",
                 device);

        if (!ui_confirm(msg)) {
            ui_info("Restore cancelled.");
            return false;
        }
    }

    fprintf(stderr,
            YELLOW "Running elevated restore command:\n" RESET
            GREEN "  %s\n\n" RESET,
            pk_cmd);

    int rc = system(pk_cmd);
    if (rc != 0) {
        ui_error("Restore failed. Please check the terminal output for details.");
        return false;
    }

    fprintf(stderr,
            WHITE "\n----------------------------------------\n" RESET);
    fprintf(stderr,
            GREEN "Restore completed successfully.\n" RESET);
    fprintf(stderr,
            WHITE "----------------------------------------\n" RESET);

    if (!gx_no_gui) {
        ui_info("Restore completed successfully.");
    }

    return true;
}

bool parse_restore_cli_args(int argc, char **argv, RestoreCLIArgs *out)
{
    memset(out, 0, sizeof(*out));
    out->cli_mode = false;
    out->parse_error = false;
    out->image = NULL;
    out->target = NULL;
    out->force = false;

    bool saw_cli_flag = false;
    int positional_count = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];


        /* CLI flags */
        if (strcmp(arg, "--image") == 0) {
            saw_cli_flag = true;
            if (i + 1 < argc) {
                out->image = argv[++i];
                continue;
            }
            fprintf(stderr, RED "ERROR:" WHITE " --image requires a value\n");
            out->parse_error = true;
            return true;    /* CLI mode, but invalid */
        }

        if (strcmp(arg, "--target") == 0) {
            saw_cli_flag = true;
            if (i + 1 < argc) {
                out->target = argv[++i];
                continue;
            }
            fprintf(stderr, RED "ERROR:" WHITE " --target requires a value\n");
            out->parse_error = true;
            return true;
        }

        if (strcmp(arg, "--force") == 0) {
            saw_cli_flag = true;
            out->force = true;
            continue;
        }

        /* Positional arguments */
        if (arg[0] != '-') {
            if (positional_count == 0)
                out->image = arg;
            else if (positional_count == 1)
                out->target = arg;
            else {
                fprintf(stderr, RED "ERROR:" WHITE  "too many positional arguments\n");
                out->parse_error = true;
                return true;
            }
            positional_count++;
            continue;
        }

        /* Unknown flag */
        fprintf(stderr, RED "ERROR:" WHITE " unknown option '%s'\n", arg);
        out->parse_error = true;
        return true;
    }

    /* If CLI flags were used, require both image + target */
    if (saw_cli_flag) {
        if (!out->image) {
            fprintf(stderr, RED "ERROR:" WHITE " missing required --image argument\n");
            out->parse_error = true;
            return true;
        }
        if (!out->target) {
            fprintf(stderr, RED "ERROR:" WHITE " missing required --target argument\n");
            out->parse_error = true;
            return true;
        }
        out->cli_mode = true;
        return true;
    }

    /* Positional form: require both */
    if (positional_count == 1) {
        fprintf(stderr, RED "ERROR:" WHITE " missing target device\n");
        out->parse_error = true;
        return true;
    }

    if (positional_count == 2) {
        out->cli_mode = true;
        return true;
    }

    /* No CLI args → GUI mode */
    return false;
}

bool restore_run_cli(const char *image_path,
                     const char *target_device,
                     bool force)
{
    if (!image_path || !target_device) {
        fprintf(stderr, RED "ERROR:" WHITE " missing required arguments.\n");
        return false;
    }

    /* Check image file existence */
    if (access(image_path, F_OK) != 0) {
        fprintf(stderr,
                RED "ERROR:" WHITE " image file does not exist: %s\n",
                image_path);
        return false;
    }

    /* ---------------------------------------------------------
     * 1. Normalize image base path (strip .000 if chunked)
     * --------------------------------------------------------- */
    char base_image[1024] = {0};
    bool chunked_by_name = false;

    get_image_base_and_chunked(image_path,
                               base_image,
                               sizeof(base_image),
                               &chunked_by_name);

    /* ---------------------------------------------------------
     * 2. Load metadata (authoritative)
     * --------------------------------------------------------- */
    MetadataInfo meta;
    if (!load_metadata_or_exit(base_image, &meta)) {
        return false;
    }

    /* ---------------------------------------------------------
     * 2a. Validate chunk set using normalized base path
     * --------------------------------------------------------- */
    if (!validate_chunk_set(base_image, meta.chunk_count))
        return false;

    /* ---------------------------------------------------------
     * 3. Validate target device exists
     * --------------------------------------------------------- */
    struct stat st;
    if (stat(target_device, &st) != 0) {
        fprintf(stderr, RED "ERROR:" WHITE " target device does not exist: %s\n",
                target_device);
        return false;
    }

    /* ---------------------------------------------------------
     * 4. Partition size check (unless --force)
     * --------------------------------------------------------- */
    long long tgt_bytes = get_partition_size_bytes(target_device);
    if (tgt_bytes <= 0) {
        fprintf(stderr, RED "ERROR:" WHITE " could not determine size of target partition.\n");
        return false;
    }

    if (tgt_bytes < meta.partition_size_bytes) {
        fprintf(stderr,
                RED "ERROR:" WHITE " Target partition is smaller than the original.\n"
                "       Original: %.2f GB\n"
                "       Target:   %.2f GB\n"
                YELLOW "       This is a hard limitation of partclone and cannot be overridden.\n" RESET,
                meta.partition_size_bytes / 1e9,
                tgt_bytes / 1e9);
        return false;
    }

    /* ---------------------------------------------------------
     * 4b. CLI confirmation (unless --force)
     * --------------------------------------------------------- */
    if (!force) {
        fprintf(stderr,
                RED "WARNING:\n"
                WHITE "You are about to overwrite the partition:" YELLOW "  %s\n\n" WHITE
                "All data on this partition will be permanently lost.\n"
                "This action cannot be undone.\n\n"
                "Proceed? [y/N]: " RESET,
                target_device);

        fflush(stderr);

        char buf[16] = {0};
        if (!fgets(buf, sizeof(buf), stdin)) {
            fprintf(stderr, RED "ERROR:" WHITE " Failed to read input.\n");
            return false;
        }

        if (buf[0] != 'y' && buf[0] != 'Y') {
            fprintf(stderr, YELLOW "\nRestore cancelled.\n");
            return false;
        }
    }

    /* ---------------------------------------------------------
     * 5. Run restore pipeline
     * --------------------------------------------------------- */
    bool ok = run_restore_pipeline(
        meta.backend,
        base_image,
        target_device,
        meta.compression,
        meta.chunked
    );

    if (!ok) {
        fprintf(stderr, "Restore failed.\n");
        return false;
    }

    return true;
}


bool print_restore_usage(struct parse_output *out)
{
    fprintf(stderr,
            YELLOW "\nUsage:" WHITE " imprintr --image <path> --target <device>\n"
            "       imprintr <image> <device>\n\n"
            YELLOW "Options:\n" WHITE
            "        --image <image file>      Path and filename of backup image (.img.zst, .img.lz4, .000, etc.)\n"
            "        --target <device>         Destination block device (e.g. /dev/nvme0n1p3)\n"
            "        --force                   Skip confirmation prompts when overwriting partitions\n"
            "        --help                    Show this help message\n"
            RESET
    );

    out->parse_error = false;
    out->cli_mode = false;
    return true;    /* signal: help shown, suppress GUI and CLI */
}
