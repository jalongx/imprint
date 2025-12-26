#include "backup.h"
#include "utils.h"
#include "ui.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Map filesystem type to partclone backend. */
static const char *partclone_backend_for_fs(const char *fs_type)
{
    if (!fs_type)
        return NULL;

    if (strcmp(fs_type, "ext2") == 0 || strcmp(fs_type, "ext3") == 0 || strcmp(fs_type, "ext4") == 0)
        return "partclone.extfs";

    if (strcmp(fs_type, "btrfs") == 0)
        return "partclone.btrfs";

    if (strcmp(fs_type, "xfs") == 0)
        return "partclone.xfs";

    if (strcmp(fs_type, "ntfs") == 0)
        return "partclone.ntfs";

    if (strcmp(fs_type, "vfat") == 0 || strcmp(fs_type, "fat32") == 0 || strcmp(fs_type, "fat") == 0)
        return "partclone.fat";

    if (strcmp(fs_type, "exfat") == 0)
        return "partclone.exfat";

    /* Add more mappings as needed. */

    return NULL;
}


/* Build a default filename based on device and filesystem. */
static void build_default_filename(const char *device, const char *fs_type,
                                   char *out, size_t out_len)
{
    const char *dev = device;
    if (strncmp(dev, "/dev/", 5) == 0)
        dev += 5;

    /* Replace any '/' or other weird chars with '_' just in case. */
    char safe[128];
    size_t j = 0;
    for (size_t i = 0; dev[i] != '\0' && j < sizeof(safe) - 1; i++) {
        char c = dev[i];
        if (c == '/' || c == ' ' || c == '\t')
            c = '_';
        safe[j++] = c;
    }
    safe[j] = '\0';

    snprintf(out, out_len, "%s_%s.img.gz",
             safe,
             fs_type ? fs_type : "fs");
}

/* Build full path: dir + "/" + filename. Caller must free. */
static char *join_path(const char *dir, const char *filename)
{
    size_t len = strlen(dir) + 1 + strlen(filename) + 1;
    char *path = malloc(len);
    if (!path)
        return NULL;

    snprintf(path, len, "%s/%s", dir, filename);
    return path;
}

/* Run partclone + gzip pipeline. */
bool run_backup_pipeline(const char *backend,
                         const char *device,
                         const char *fs_type,
                         const char *output_path)
{
    if (!backend || !device || !output_path)
        return false;

    /* Build the partclone command WITHOUT lz4 or redirection */
    char partclone_cmd[1024];
    snprintf(partclone_cmd, sizeof(partclone_cmd),
             "%s -c -s '%s'",
             backend,
             device);

    /* pkexec only wraps partclone */
    char pk_partclone[2048];
    snprintf(pk_partclone, sizeof(pk_partclone),
             "pkexec %s",
             partclone_cmd);

    /* Full pipeline: root partclone → user lz4 → user output file */
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd),
             "%s | lz4 -1 > '%s'",
             pk_partclone,
             output_path);

    ui_info("Backup will now start.\n\n"
    "Please monitor the terminal for partclone progress.\n"
    "This may take some time.");

    fprintf(stderr, YELLOW "Starting partclone...\n\n" RESET);

    /* Run the pipeline */
    int rc = system(full_cmd);

    /* Decode exit status */
    int exit_code = -1;
    if (rc != -1)
        exit_code = WEXITSTATUS(rc);

    /* Detect failure from partclone or lz4 */
    if (rc == -1 || exit_code != 0) {

        /* Delete partial/corrupted image */
        unlink(output_path);

        ui_error(
            "Backup failed.\n\n"
            "Partclone reported an error (often caused by a dirty NTFS volume).\n"
            "No backup image was created.\n\n"
            "If this is an NTFS partition, boot into Windows and run:\n"
            "    chkdsk /f\n"
            "Then try again."
        );

        return false;
    }

    /* Success */
    ui_info("Backup completed successfully.");

    /* Write metadata only for valid backups */
    write_metadata(output_path, device, fs_type, backend);

    return true;
}


bool backup_run_interactive(void)
{
    /* 1. Choose partition. */
    char *device = ui_choose_partition();
    if (!device) {
        return false;
    }
    // fprintf(stderr, "DEBUG: partition selected: '%s'\n", device);

    /* 2. Choose backup directory. */
    char *dir = ui_choose_directory();
    if (!dir) {
        free(device);
        return false;
    }
    // fprintf(stderr, "DEBUG: directory selected: '%s'\n", dir);

    /* 3. Detect filesystem type. */
    char fs_type[64] = {0};
    if (!get_fs_type(device, fs_type, sizeof(fs_type))) {
        ui_error("Could not detect filesystem type for the selected partition.");
        free(dir);
        free(device);
        return false;
    }
    // fprintf(stderr, "DEBUG: filesystem detected: '%s'\n", fs_type);

    /* 4. Map to partclone backend. */
    const char *backend = partclone_backend_for_fs(fs_type);
    if (!backend) {
        ui_error("No supported partclone backend for this filesystem type.");
        free(dir);
        free(device);
        return false;
    }
    // fprintf(stderr, "DEBUG: backend selected: '%s'\n", backend);

    /* 5. Build default filename and ask user to confirm/modify. */
    char default_name[256];
    build_default_filename(device, fs_type, default_name, sizeof(default_name));

    char *filename = ui_enter_filename(default_name);
    if (!filename) {
        free(dir);
        free(device);
        return false;
    }
    // fprintf(stderr, "DEBUG: filename returned to backup: '%s'\n", filename);

    /* 6. Build full output path. */
    char *output_path = join_path(dir, filename);
    if (!output_path) {
        ui_error("Failed to build output path.");
        free(dir);
        free(device);
        free(filename);
        return false;
    }
    // fprintf(stderr, "DEBUG: output path: '%s'\n", output_path);

    /* 7. Run backup pipeline (now with fs_type). */
    bool ok = run_backup_pipeline(backend, device, fs_type, output_path);

    free(dir);
    free(device);
    free(filename);
    free(output_path);

    return ok;
}


