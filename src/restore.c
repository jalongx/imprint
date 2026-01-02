#include "restore.h"
#include "utils.h"
#include "ui.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
        fprintf(stderr, YELLOW "\n\nSuccessfully determined backend from metadata: '%s'" RESET "\n\n", backend);
    }

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
        fprintf(stderr, YELLOW "Successfully read compression from metadata: '%s'" RESET "\n\n",
                compression);
    }

    return found;
}

/* Run selected decompressor + partclone restore pipeline. */
bool run_restore_pipeline(const char *backend,
                          const char *image_path,
                          const char *device,
                          const char *compression)
{
    if (!backend || !image_path || !device)
        return false;

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

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "%s '%s' | %s -r -s - -o '%s'",
             decomp,
             image_path,
             backend,
             device);

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

    ui_info("Restore completed successfully.");
    printf(YELLOW "\nDone.\n" RESET);
    return true;
}

bool restore_run_interactive(void)
{
    /* 1. Choose image file. */
    char *image_path = ui_choose_image_file();
    if (!image_path) {
        return false;
    }

    /* 2. Choose target partition. */
    char *device = ui_choose_partition_with_title(
        "Select destination partition for restore",
        "Choose the partition that will be overwritten:"
    );
    if (!device) {
        free(image_path);
        return false;
    }

    /* 3. Read backend from metadata JSON. */
    char backend[128] = {0};
    if (!read_backend_from_metadata(image_path, backend, sizeof(backend))) {
        ui_error("Could not read backend from metadata.\n"
        "Make sure the .json file exists next to the image.");
        free(device);
        free(image_path);
        return false;
    }

    /* 4. Read compression from metadata JSON. */
    char compression[64] = {0};
    if (!read_compression_from_metadata(image_path, compression, sizeof(compression))) {
        fprintf(stderr, YELLOW "No compression field found in metadata. Assuming lz4.\n" RESET);
        strcpy(compression, "lz4");
    }

    /* 5. Run restore pipeline. */
    bool ok = run_restore_pipeline(backend, image_path, device, compression);

    free(device);
    free(image_path);

    return ok;
}
