#include "restore.h"
#include "utils.h"
#include "ui.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Very simple metadata parser: read checksum from <image>.json */
static bool read_checksum_from_metadata(const char *image_path,
                                        char *checksum,
                                        size_t checksum_len)
{
    if (!image_path || !checksum || checksum_len == 0)
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
        /* Look for: "image_checksum_sha256": "..." */
        char *p = strstr(line, "\"image_checksum_sha256\"");
        if (!p)
            continue;

        /* Move to colon */
        p = strchr(p, ':');
        if (!p)
            continue;

        /* Move to first quote after colon */
        p = strchr(p, '"');
        if (!p)
            continue;
        p++; /* move past the quote */

        /* Find closing quote */
        char *end = strchr(p, '"');
        if (!end)
            continue;

        size_t len = (size_t)(end - p);
        if (len >= checksum_len)
            len = checksum_len - 1;

        memcpy(checksum, p, len);
        checksum[len] = '\0';
        found = true;
        break;
    }

    fclose(fp);

    if (!found) {
        fprintf(stderr, "DEBUG: checksum not found in metadata '%s'\n", meta_path);
    } else {
        fprintf(stderr, YELLOW "Successfully read checksum from metadata: '%s'" RESET "\n",
                checksum);
    }

    return found;
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
        /* Look for a line like: "  \"backend\": \"partclone.extfs\"," */
        char *p = strstr(line, "\"backend\"");
        if (!p)
            continue;

        /* Find first quote after colon */
        p = strchr(p, ':');
        if (!p)
            continue;
        p = strchr(p, '"');
        if (!p)
            continue;
        p++; /* move past the quote */

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

/* Run gzip + partclone restore pipeline. */
bool run_restore_pipeline(const char *backend,
                          const char *image_path,
                          const char *device)
{
    if (!backend || !image_path || !device)
        return false;

    /* Build the actual partclone restore pipeline:
     *   gzip -dc 'image' | backend -r -s - -o 'device'
     */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "lz4 -dc '%s' | %s -r -s - -o '%s'",
             image_path,
             backend,
             device);

    /* Wrap it in pkexec so only THIS command runs as root */
    char pk_cmd[2048];
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

    /* 1b. Verify checksum before doing anything destructive. */
    char expected_sha[128] = {0};
    if (!read_checksum_from_metadata(image_path, expected_sha, sizeof(expected_sha))) {
        ui_error("Could not read checksum from metadata.\n"
        "Make sure the .json file exists next to the image.");
        free(image_path);
        return false;
    }

    char actual_sha[128] = {0};
    if (!compute_sha256(image_path, actual_sha, sizeof(actual_sha))) {
        ui_error("Could not compute checksum for the image file.");
        free(image_path);
        return false;
    }

    if (strcmp(expected_sha, actual_sha) != 0) {
        ui_error("Checksum mismatch!\n"
        "The backup image may be corrupted.\n"
        "Restore aborted.");
        free(image_path);
        return false;
    }

    ui_info("Checksum verified successfully.\n");

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

    /* 4. Run restore pipeline. */
    bool ok = run_restore_pipeline(backend, image_path, device);

    free(device);
    free(image_path);

    return ok;
}

