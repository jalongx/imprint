#define _POSIX_C_SOURCE 200809L

#include <time.h>
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

    if (gx_config.chunk_size_mb > 0) {
        fprintf(stderr,
                YELLOW "Output chunking: On (%d MB)\n",
                gx_config.chunk_size_mb);
    } else {
        fprintf(stderr,
                YELLOW "Output chunking: Off\n");
    }

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

    /* --------------------------------------------------
     * Determine where FIFOs should live.
     * -------------------------------------------------- */
    char fifo_dir[1024];

    if (gx_workdir_override[0] != '\0') {
        snprintf(fifo_dir, sizeof(fifo_dir), "%s", gx_workdir_override);
    } else {
        const char *slash = strrchr(output_path, '/');
        if (slash) {
            size_t len = (size_t)(slash - output_path);
            if (len >= sizeof(fifo_dir))
                len = sizeof(fifo_dir) - 1;
            memcpy(fifo_dir, output_path, len);
            fifo_dir[len] = '\0';
        } else {
            snprintf(fifo_dir, sizeof(fifo_dir), ".");
        }
    }

    mkdir(fifo_dir, 0700);

    /* -------------------------------
     * 1. Create FIFO for checksum
     * ------------------------------- */
    char checksum_fifo[1024];

    /* Safe, warning‑free construction */
    size_t need = strlen(fifo_dir) + strlen("/sha256pipe.fifo") + 1;
    if (need >= sizeof(checksum_fifo)) {
        ui_error("FIFO path too long.");
        return false;
    }

    snprintf(checksum_fifo, sizeof(checksum_fifo),
             "%s/sha256pipe.fifo",
             fifo_dir);

    if (mkfifo(checksum_fifo, 0600) != 0) {
        perror("mkfifo (checksum fifo)");
        ui_error("Failed to create checksum FIFO.\n\n"
        "The destination or work directory may not support FIFOs.");
        return false;
    }

    /* -------------------------------
     * 2. Start sha256sum in background
     * ------------------------------- */
    pid_t sha_pid = fork();
    if (sha_pid < 0) {
        perror("fork (sha256sum)");
        unlink(checksum_fifo);
        ui_error("Failed to start checksum process.");
        return false;
    }

    if (sha_pid == 0) {
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
     * 3. Build the main pipeline.
     */
    char full_cmd[4096];

    if (gx_config.chunk_size_mb > 0) {
        snprintf(full_cmd, sizeof(full_cmd),
                 "set -o pipefail; "
                 "%s | tee '%s' | %s | split -b %dM -d -a 3 - '%s.'",
                 pk_partclone,
                 checksum_fifo,
                 comp_cmd,
                 gx_config.chunk_size_mb,
                 output_path);
    } else {
        snprintf(full_cmd, sizeof(full_cmd),
                 "set -o pipefail; "
                 "%s | tee '%s' | %s > '%s'",
                 pk_partclone,
                 checksum_fifo,
                 comp_cmd,
                 output_path);
    }

     fprintf(stderr,
            YELLOW "Starting partclone with streaming checksum using the command...\n" RESET);

    fprintf(stderr,
            GREEN "     %s\n\n" RESET,
            full_cmd);

    /* 4. Execute pipeline */
    int rc = system(full_cmd);

    /* 5. Close FIFO */
    unlink(checksum_fifo);

    /* 6. Wait for sha256sum */
    int sha_status = 0;
    if (waitpid(sha_pid, &sha_status, 0) < 0) {
        perror("waitpid (sha256sum)");
    }

    int exit_code = -1;
    if (rc != -1)
        exit_code = WEXITSTATUS(rc);

    // fprintf(stderr,
    //         YELLOW "DEBUG: system() returned rc=%d, exit_code=%d, sha_status=%d\n" RESET,
    //         rc, exit_code, sha_status);

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

    write_metadata(output_path, device, fs_type, backend, gx_config.compression);

    return true;
}

bool backup_run_interactive(void)
{
    /* Always start with no override to avoid stale state. */
    gx_workdir_override[0] = '\0';

    /* Capture start time for duration/throughput reporting */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* 1. Choose partition. */
    char *device = ui_choose_partition();
    if (!device) {
        return false;
    }

    /* 2. Choose backup directory. */
    char *dir = ui_choose_directory();
    if (!dir) {
        free(device);
        return false;
    }

    /* 3. Detect filesystem type. */
    char fs_type[64] = {0};
    if (!get_fs_type(device, fs_type, sizeof(fs_type))) {
        ui_error("Could not detect filesystem type for the selected partition.");
        free(dir);
        free(device);
        return false;
    }

    /* 4. Map to partclone backend. */
    const char *backend = partclone_backend_for_fs(fs_type);
    if (!backend) {
        ui_error("No supported partclone backend for this filesystem type.");
        free(dir);
        free(device);
        return false;
    }

    /* 5. Build default filename and ask user to confirm/modify. */
    char default_name[256];
    build_default_filename(device, fs_type, default_name, sizeof(default_name));

    char *filename = ui_enter_filename(default_name);
    if (!filename) {
        free(dir);
        free(device);
        return false;
    }

    /* 6. Build full output path. */
    char *output_path = join_path(dir, filename);
    if (!output_path) {
        ui_error("Failed to build output path.");
        free(dir);
        free(device);
        free(filename);
        return false;
    }

    /* 6a. Test FIFO capability on the chosen directory. */
    if (!gx_test_fifo_capability(dir)) {

        fprintf(stderr,
                RED "Non-FIFO filesystem detected: %s\n"
                "Using temporary work directory: %s\n\n" RESET,
                fs_type,
                "/tmp/imprint_work");

        snprintf(gx_workdir_override, sizeof(gx_workdir_override),
                 "/tmp/imprint_work");

        mkdir(gx_workdir_override, 0700);
    }

    /* 7. Run backup pipeline */
    bool ok = run_backup_pipeline(backend, device, fs_type, output_path);

    /* Capture end time */
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    /* Compute duration */
    double duration_sec =
    (t_end.tv_sec - t_start.tv_sec) +
    (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    /* Compute throughput (MB/s) and human-readable size */
    double mb_per_sec = 0.0;
    char size_str[64] = "Unknown";
    char alloc_str[64] = "Unknown";
    int chunk_count = 0;
    off_t total_bytes = 0;
    off_t total_allocated = 0;

    if (gx_config.chunk_size_mb > 0) {
        /* Chunked output: sum all chunk files */

        /* Prefix is always small, so use a small buffer to silence GCC */
        char prefix[256];
        snprintf(prefix, sizeof(prefix), "%s.", output_path);

        char chunk_path[2048];

        /* Hard limit: 0–999 chunks → silences GCC warning */
        for (unsigned i = 0; i < 1000; i++) {

            /* Step 1: copy prefix */
            strncpy(chunk_path, prefix, sizeof(chunk_path));
            chunk_path[sizeof(chunk_path) - 1] = '\0';

            /* Step 2: append chunk number safely */
            snprintf(chunk_path + strlen(chunk_path),
                     sizeof(chunk_path) - strlen(chunk_path),
                     "%03u",
                     i);

            struct stat st;
            if (stat(chunk_path, &st) != 0)
                break;

            total_bytes += st.st_size;
            total_allocated += (off_t)st.st_blocks * 512;
            chunk_count++;
        }

        if (chunk_count > 0 && duration_sec > 0.0) {
            double size_mb = total_bytes / (1024.0 * 1024.0);
            double size_gb = size_mb / 1024.0;

            if (size_gb >= 1.0)
                snprintf(size_str, sizeof(size_str), "%.2f GB (%d chunks)", size_gb, chunk_count);
            else
                snprintf(size_str, sizeof(size_str), "%.2f MB (%d chunks)", size_mb, chunk_count);

            double alloc_mb = total_allocated / (1024.0 * 1024.0);
            double alloc_gb = alloc_mb / 1024.0;

            if (alloc_gb >= 1.0)
                snprintf(alloc_str, sizeof(alloc_str), "%.2f GB", alloc_gb);
            else
                snprintf(alloc_str, sizeof(alloc_str), "%.2f MB", alloc_mb);

            mb_per_sec = size_mb / duration_sec;
        }
    }
    else {
        /* Single file output */
        struct stat st;
        if (stat(output_path, &st) == 0 && duration_sec > 0.0) {
            total_bytes = st.st_size;
            total_allocated = (off_t)st.st_blocks * 512;

            double size_mb = total_bytes / (1024.0 * 1024.0);
            double size_gb = size_mb / 1024.0;

            if (size_gb >= 1.0)
                snprintf(size_str, sizeof(size_str), "%.2f GB", size_gb);
            else
                snprintf(size_str, sizeof(size_str), "%.2f MB", size_mb);

            double alloc_mb = total_allocated / (1024.0 * 1024.0);
            double alloc_gb = alloc_mb / 1024.0;

            if (alloc_gb >= 1.0)
                snprintf(alloc_str, sizeof(alloc_str), "%.2f GB", alloc_gb);
            else
                snprintf(alloc_str, sizeof(alloc_str), "%.2f MB", alloc_mb);

            mb_per_sec = size_mb / duration_sec;
        }
    }

    if (ok) {
        /* Remember the directory for next time */
        strncpy(gx_config.backup_dir, dir, sizeof(gx_config.backup_dir) - 1);
        gx_config.backup_dir[sizeof(gx_config.backup_dir) - 1] = '\0';
        ghostx_config_save();

        /* Build paths for checksum and metadata */
        char sha_path[2048];
        snprintf(sha_path, sizeof(sha_path), "%s.sha256", output_path);

        char meta_path[2048];
        snprintf(meta_path, sizeof(meta_path), "%s.json", output_path);

        /* Console output */
        fprintf(stderr,
                YELLOW "\nImage written to:\n"
                "    %s\n\n"
                "Checksum written to:\n"
                "    %s\n\n"
                "Metadata written to:\n"
                "    %s\n\n" RESET,
                output_path,
                sha_path,
                meta_path);

        fprintf(stderr,
                WHITE "Backup file size: %s\n"
                "Allocated on disk: %s\n"
                "Duration: %.2f seconds\n"
                "Average throughput: %.2f MB/s\n\n" RESET,
                size_str,
                alloc_str,
                duration_sec,
                mb_per_sec);

        fprintf(stderr,
                WHITE "----------------------------------------\n" RESET);
        fprintf(stderr,
                GREEN "Backup completed successfully.\n" RESET);
        fprintf(stderr,
                WHITE "----------------------------------------\n" RESET);

        ui_info("Backup completed successfully.");
    }

    free(dir);
    free(device);
    free(filename);
    free(output_path);

    return ok;
}
