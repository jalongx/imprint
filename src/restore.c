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
        fprintf(stderr, YELLOW "Successfully determined backend from metadata: '%s'" RESET "\n\n", backend);
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
    return true;
}

bool restore_run_interactive(void)
{
    /* 1. Choose image file. */
    char *image_path = ui_choose_image_file();
    if (!image_path) {
        return false;
    }
    // fprintf(stderr, "DEBUG: image selected: '%s'\n", image_path);

    /* 2. Choose target partition. (reuses your existing mount-safety checks) */
    char *device = ui_choose_partition_with_title(
        "Select destination partition for restore",
        "Choose the partition that will be overwritten:"
    );
    if (!device) {
        free(image_path);
        return false;
    }
    // fprintf(stderr, "DEBUG: target partition selected: '%s'\n", device);

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
