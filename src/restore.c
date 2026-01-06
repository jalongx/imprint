#include "restore.h"
#include "utils.h"
#include "ui.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/* Detect chunk suffix ".000".."999" and compute base path */
static void get_image_base_and_chunked(const char *selected_path,
                                       char *base_path,
                                       size_t base_len,
                                       bool *chunked)
{
    *chunked = false;

    if (!selected_path || !base_path || base_len == 0)
        return;

    size_t len = strlen(selected_path);

    /* Detect suffix .000 .. .999 */
    if (len > 4 &&
        selected_path[len - 4] == '.' &&
        isdigit((unsigned char)selected_path[len - 3]) &&
        isdigit((unsigned char)selected_path[len - 2]) &&
        isdigit((unsigned char)selected_path[len - 1]))
    {
        /* Strip the .DDD suffix */
        size_t new_len = len - 4;
        if (new_len >= base_len)
            new_len = base_len - 1;

        memcpy(base_path, selected_path, new_len);
        base_path[new_len] = '\0';

        *chunked = true;
    }
    else {
        /* Not chunked — use as-is */
        strncpy(base_path, selected_path, base_len - 1);
        base_path[base_len - 1] = '\0';
        *chunked = false;
    }
}

/* Very simple metadata parser: read backend from <image>.json */
static bool read_backend_from_metadata(const char *image_path,
                                       char *backend,
                                       size_t backend_len)
{
    if (!image_path || !backend || backend_len == 0)
        return false;

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_path);

    FILE *fp = fopen(meta_path, "r");
    if (!fp) {
        fprintf(stderr, "DEBUG: could not open metadata file '%s'\n", meta_path);
        return false;
    }

    char line[512];
    bool found = false;

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "\"backend\"");
        if (!p)
            continue;

        p = strchr(p, ':');
        if (!p)
            continue;
        p = strchr(p, '"');
        if (!p)
            continue;
        p++;

        char *end = strchr(p, '"');
        if (!end)
            continue;

        size_t len = (size_t)(end - p);
        if (len >= backend_len)
            len = backend_len - 1;

        memcpy(backend, p, len);
        backend[len] = '\0';
        found = true;
        break;
    }

    fclose(fp);

    if (!found) {
        fprintf(stderr, "DEBUG: backend not found in metadata '%s'\n", meta_path);
    } else {
        fprintf(stderr, YELLOW "Successfully determined backend from metadata: '%s'" RESET "\n", backend);
    }

    return found;
}

/* Read "chunked": true/false from metadata */
static bool read_chunked_from_metadata(const char *image_base,
                                       bool *chunked_out)
{
    if (!image_base || !chunked_out)
        return false;

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_base);

    FILE *fp = fopen(meta_path, "r");
    if (!fp)
        return false;

    char line[512];
    bool found = false;

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "\"chunked\"");
        if (!p)
            continue;

        p = strchr(p, ':');
        if (!p)
            continue;
        p++;

        /* Skip whitespace */
        while (*p == ' ' || *p == '\t')
            p++;

        if (strncmp(p, "true", 4) == 0) {
            *chunked_out = true;
            found = true;
            break;
        }
        if (strncmp(p, "false", 5) == 0) {
            *chunked_out = false;
            found = true;
            break;
        }
    }

    fclose(fp);
    return found;
}

/* Read "chunk_size_mb": integer from metadata */
static bool read_chunk_size_from_metadata(const char *image_base,
                                          int *size_out)
{
    if (!image_base || !size_out)
        return false;

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_base);

    FILE *fp = fopen(meta_path, "r");
    if (!fp)
        return false;

    char line[512];
    bool found = false;

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "\"chunk_size_mb\"");
        if (!p)
            continue;

        p = strchr(p, ':');
        if (!p)
            continue;
        p++;

        *size_out = atoi(p);
        found = true;
        break;
    }

    fclose(fp);
    return found;
}


/* Read compression method from <image>.json */
static bool read_compression_from_metadata(const char *image_path,
                                           char *compression,
                                           size_t compression_len)
{
    if (!image_path || !compression || compression_len == 0)
        return false;

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_path);

    FILE *fp = fopen(meta_path, "r");
    if (!fp) {
        fprintf(stderr, "DEBUG: could not open metadata file '%s'\n", meta_path);
        return false;
    }

    char line[512];
    bool found = false;

    while (fgets(line, sizeof(line), fp)) {
        char *p = strstr(line, "\"compression\"");
        if (!p)
            continue;

        p = strchr(p, ':');
        if (!p)
            continue;
        p = strchr(p, '"');
        if (!p)
            continue;
        p++;

        char *end = strchr(p, '"');
        if (!end)
            continue;

        size_t len = (size_t)(end - p);
        if (len >= compression_len)
            len = compression_len - 1;

        memcpy(compression, p, len);
        compression[len] = '\0';
        found = true;
        break;
    }

    fclose(fp);

    if (!found) {
        fprintf(stderr, "DEBUG: compression not found in metadata '%s'\n\n", meta_path);
    } else {
        fprintf(stderr, YELLOW "Successfully read compression from metadata: '%s'" RESET "\n",
                compression);
    }

    return found;
}

/* Run selected decompressor + partclone restore pipeline. */
bool run_restore_pipeline(const char *backend,
                          const char *image_path,
                          const char *device,
                          const char *compression,
                          bool chunked)
{
    if (!backend || !image_path || !device)
        return false;

    /* Select decompressor */
    const char *decomp = NULL;

    if (compression && strcmp(compression, "gzip") == 0)
        decomp = "gzip -dc";
    else if (compression && strcmp(compression, "zstd") == 0)
        decomp = "zstd -dc";
    else
        decomp = "lz4 -dc";

    fprintf(stderr,
            YELLOW "Using decompressor: %s\n" RESET,
            decomp);

    char cmd[3072];

    if (chunked) {
        /*
         * Chunked restore:
         *   cat base.??? | decomp | backend -r -s - -o device
         *
         * image_path is the *base* path (no .000 suffix).
         */
        snprintf(cmd, sizeof(cmd),
                 "cat '%s'.??? | %s | %s -r -s - -o '%s'",
                 image_path,
                 decomp,
                 backend,
                 device);
    } else {
        /*
         * Single-file restore:
         *   decomp base | backend -r -s - -o device
         */
        snprintf(cmd, sizeof(cmd),
                 "%s '%s' | %s -r -s - -o '%s'",
                 decomp,
                 image_path,
                 backend,
                 device);
    }

    /* pkexec wrapper */
    char pk_cmd[4096];
    snprintf(pk_cmd, sizeof(pk_cmd),
             "pkexec sh -c \"%s\"",
             cmd);

    ui_info("Restore will now start.\n\n"
    "Please monitor the terminal for partclone progress.\n"
    "This will overwrite all data on the selected partition.");

    fprintf(stderr,
            YELLOW "Running elevated restore command:" RESET "\n"
            "  " GREEN "%s" RESET "\n\n",
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


    ui_info("Restore completed successfully.");
    printf(YELLOW "\nDone.\n" RESET);
    return true;
}

bool restore_run_interactive(void)
{
    /* 1. Choose image file. */
    char *selected_path = ui_choose_image_file();
    if (!selected_path) {
        return false;
    }

    /* 1b. Normalize to base path and detect chunked series by filename */
    char base_image[1024] = {0};
    bool chunked_by_name = false;
    get_image_base_and_chunked(selected_path,
                               base_image,
                               sizeof(base_image),
                               &chunked_by_name);

    /* 1c. Try reading chunking info from metadata */
    bool chunked_by_meta = false;
    int  chunk_size_meta = 0;

    bool have_chunked_meta = read_chunked_from_metadata(base_image,
                                                        &chunked_by_meta);

    /* Read chunk size (optional; we don't need the boolean) */
    read_chunk_size_from_metadata(base_image, &chunk_size_meta);

    /*
     * Final decision:
     *   - If metadata exists, trust it.
     *   - Otherwise fall back to filename detection.
     */
    bool chunked = have_chunked_meta ? chunked_by_meta : chunked_by_name;

    // fprintf(stderr,
    //         YELLOW "DEBUG: selected='%s', base='%s', "
    //         "chunked_by_name=%s, chunked_by_meta=%s, final=%s\n" RESET,
    //         selected_path,
    //         base_image,
    //         chunked_by_name ? "true" : "false",
    //         have_chunked_meta ? (chunked_by_meta ? "true" : "false") : "N/A",
    //         chunked ? "true" : "false");

    /* 2. Choose target partition. */
    char *device = ui_choose_partition_with_title(
        "Select destination partition for restore",
        "Choose the partition that will be overwritten:"
    );
    if (!device) {
        free(selected_path);
        return false;
    }

    /* 3. Read backend from metadata JSON (based on base image path). */
    char backend[128] = {0};
    if (!read_backend_from_metadata(base_image, backend, sizeof(backend))) {
        ui_error("Could not read backend from metadata.\n"
        "Make sure the .json file exists next to the image.");
        free(device);
        free(selected_path);
        return false;
    }

    /* 4. Read compression from metadata JSON (based on base image path). */
    char compression[64] = {0};
    if (!read_compression_from_metadata(base_image,
        compression,
        sizeof(compression))) {
        fprintf(stderr,
                YELLOW "No compression field found in metadata. "
                "Assuming lz4.\n" RESET);
        strcpy(compression, "lz4");
        }

        /* 5. Run restore pipeline using base path and chunked flag. */
        bool ok = run_restore_pipeline(backend,
                                       base_image,
                                       device,
                                       compression,
                                       chunked);

        free(device);
        free(selected_path);

        return ok;
}

