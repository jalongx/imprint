#include "backup.h"
#include "utils.h"
#include "ui.h"
#include "colors.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Map compression string to compressor command */
static const char *get_compressor_cmd(const char *comp)
{
    if (!comp)
        return "lz4 -1";

    if (strcmp(comp, "gzip") == 0)
        return "gzip -3";

    if (strcmp(comp, "zstd") == 0)
        return "zstd -6";

    return "lz4 -1";  /* default */
}

/* Map compression string to filename extension */
static const char *get_compression_ext(const char *comp)
{
    if (!comp)
        return "lz4";

    if (strcmp(comp, "gzip") == 0)
        return "gz";

    if (strcmp(comp, "zstd") == 0)
        return "zst";

    return "lz4";  /* default */
}


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


/* Build a default filename based on device, filesystem, and compression. */
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

    /* Determine extension based on config */
    const char *ext = get_compression_ext(gx_config.compression);

    snprintf(out, out_len, "%s_%s.img.%s",
             safe,
             fs_type ? fs_type : "fs",
             ext);
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
/* Run partclone + compressor + streaming checksum pipeline. */
bool run_backup_pipeline(const char *backend,
                         const char *device,
                         const char *fs_type,
                         const char *output_path)
{
    if (!backend || !device || !output_path)
        return false;

    /* Determine compressor command based on config */
    const char *comp_cmd = get_compressor_cmd(gx_config.compression);

    fprintf(stderr,
            YELLOW "Using compressor: %s\n" RESET,
            comp_cmd);

    /* Build the partclone command */
    char partclone_cmd[1024];
    snprintf(partclone_cmd, sizeof(partclone_cmd),
             "%s -c -s '%s'",
             backend,
             device);

    /* pkexec wrapper for partclone ONLY */
    char pk_partclone[2048];
    snprintf(pk_partclone, sizeof(pk_partclone),
             "pkexec %s",
             partclone_cmd);

    /* -------------------------------
     * 1. Create FIFO for checksum
     * ------------------------------- */
    char checksum_fifo[1024];
    snprintf(checksum_fifo, sizeof(checksum_fifo),
             "%s.sha256pipe", output_path);

    if (mkfifo(checksum_fifo, 0600) != 0) {
        perror("mkfifo (checksum fifo)");
        ui_error("Failed to create checksum FIFO.");
        return false;
    }

    /* -------------------------------
     * 2. Start sha256sum in background
     *    sha256sum < fifo > output.sha256
     * ------------------------------- */
    pid_t sha_pid = fork();
    if (sha_pid < 0) {
        perror("fork (sha256sum)");
        unlink(checksum_fifo);
        ui_error("Failed to start checksum process.");
        return false;
    }

    if (sha_pid == 0) {
        /* Child: run sha256sum streaming from FIFO */
        char sha_cmd[2048];
        snprintf(sha_cmd, sizeof(sha_cmd),
                 "sha256sum < '%s' > '%s.sha256'",
                 checksum_fifo,
                 output_path);

        execl("/bin/sh", "sh", "-c", sha_cmd, (char *)NULL);
        perror("execl (sha256sum)");
        _exit(127);
    }

    /*
     * 3. Build the main pipeline with tee:
     *
     *    pk_partclone | tee 'fifo' | comp_cmd > 'output_path'
     *
     *    - set -o pipefail ensures we see partclone failures.
     */
    char full_cmd[4096];
    snprintf(full_cmd, sizeof(full_cmd),
             "set -o pipefail; %s | tee '%s' | %s > '%s'",
             pk_partclone,
             checksum_fifo,
             comp_cmd,
             output_path);

    fprintf(stderr, YELLOW "Starting partclone with streaming checksum...\n\n" RESET);

    /* 4. Execute pipeline */
    int rc = system(full_cmd);

    /* 5. Close FIFO (removes name; readers still complete) */
    unlink(checksum_fifo);

    /* 6. Wait for sha256sum to finish */
    int sha_status = 0;
    if (waitpid(sha_pid, &sha_status, 0) < 0) {
        perror("waitpid (sha256sum)");
    }

    /* Decode exit status of the pipeline */
    int exit_code = -1;
    if (rc != -1)
        exit_code = WEXITSTATUS(rc);

    fprintf(stderr,
            YELLOW "DEBUG: system() returned rc=%d, exit_code=%d, sha_status=%d\n" RESET,
            rc, exit_code, sha_status);

    /*
     * FAILURE HANDLING:
     * If partclone or compressor fails, pipefail ensures exit_code != 0.
     * We delete the partial image and any checksum file, and show the NTFS warning.
     */
    if (rc == -1 || exit_code != 0) {

        unlink(output_path);

        char sha_file[1024];
        snprintf(sha_file, sizeof(sha_file), "%s.sha256", output_path);
        unlink(sha_file);

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

    /* Write metadata only for valid backups */
    write_metadata(output_path, device, fs_type, backend, gx_config.compression);

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

    if (ok) {
        // Remember the directory for next time
        strncpy(gx_config.backup_dir, dir, sizeof(gx_config.backup_dir)-1);
        ghostx_config_save();

        ui_info("Backup completed successfully.");
    }

    free(dir);
    free(device);
    free(filename);
    free(output_path);

    return ok;

}


