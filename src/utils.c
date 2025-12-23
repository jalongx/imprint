#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

long long get_partition_size_bytes(const char *device)
{
    if (!device)
        return 0;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "lsblk -bno SIZE '%s' 2>/dev/null", device);

    fprintf(stderr, "DEBUG: running size command: %s\n", cmd);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        fprintf(stderr, "DEBUG: popen failed for lsblk\n");
        return 0;
    }

    char buf[64] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        fprintf(stderr, "DEBUG: fgets failed reading lsblk output\n");
        pclose(fp);
        return 0;
    }

    pclose(fp);

    /* Strip whitespace/newlines */
    for (char *p = buf; *p; ++p) {
        if (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')
            *p = '\0';
    }

    fprintf(stderr, "DEBUG: lsblk raw size string: '%s'\n", buf);

    if (buf[0] == '\0') {
        fprintf(stderr, "DEBUG: lsblk returned empty size\n");
        return 0;
    }

    long long val = atoll(buf);
    fprintf(stderr, "DEBUG: parsed size: %lld bytes\n", val);
    return val;
}


bool compute_sha256(const char *filepath, char *out, size_t out_len)
{
    if (!filepath || !out || out_len < 65)
        return false;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "sha256sum '%s' 2>/dev/null", filepath);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return false;

    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return false;
    }

    pclose(fp);

    /* sha256sum output format:
     *       <hash>  <filename>
     *       We only want the hash (first 64 chars)
     */
    if (strlen(buf) < 64)
        return false;

    strncpy(out, buf, 64);
    out[64] = '\0';

    return true;
}

bool write_metadata(const char *image_path,
                    const char *device,
                    const char *fs_type,
                    const char *backend)
{
    if (!image_path || !device || !fs_type || !backend)
        return false;

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_path);

    time_t now = time(NULL);

    long long part_size = get_partition_size_bytes(device);

    char checksum[65] = {0};
    compute_sha256(image_path, checksum, sizeof(checksum));

    FILE *fp = fopen(meta_path, "w");
    if (!fp)
        return false;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"tool_version\": \"1.0\",\n");
    fprintf(fp, "  \"timestamp\": %ld,\n", now);
    fprintf(fp, "  \"device\": \"%s\",\n", device);
    fprintf(fp, "  \"filesystem\": \"%s\",\n", fs_type);
    fprintf(fp, "  \"backend\": \"%s\",\n", backend);
    fprintf(fp, "  \"partition_size_bytes\": %lld,\n", part_size);
    fprintf(fp, "  \"image_filename\": \"%s\",\n", image_path);
    fprintf(fp, "  \"image_checksum_sha256\": \"%s\",\n", checksum);
    fprintf(fp, "  \"notes\": \"\"\n");
    fprintf(fp, "}\n");

    fclose(fp);
    return true;
}

/*
 * Run a command given as argv[] (NULL-terminated).
 * Returns the child exit status (0 = success, non-zero = failure).
 */
int run_command(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        execvp(argv[0], argv);
        /* If execvp returns, it failed */
        perror("execvp");
        _exit(127);
    }

    /* Parent process */
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    /* Abnormal termination */
    return -1;
}

/*
 * Check if a program is available in PATH using `which`.
 * Returns true if exit status is 0 (found), false otherwise.
 */
bool is_program_available(const char *name)
{
    char *argv[] = { "which", (char *)name, NULL };
    int rc = run_command(argv);
    return (rc == 0);
}

/*
 * Check that core dependencies are available:
 * - zenity
 * - partclone
 * - gzip
 *
 * On failure, prints an error to stderr and exits.
 * We do not use zenity here because we may not have it yet.
 */
void check_core_dependencies(void)
{
    bool ok = true;

    if (!is_program_available("zenity")) {
        fprintf(stderr,
                "Error: 'zenity' is not installed or not in PATH.\n");
        ok = false;
    }

    /* Check for any partclone backend */
    const char *backends[] = {
        "partclone.extfs",
        "partclone.ext4",
        "partclone.btrfs",
        "partclone.ntfs",
        "partclone.fat",
        "partclone.fat32",
        "partclone.xfs",
        NULL
    };

    bool found_backend = false;
    for (int i = 0; backends[i]; i++) {
        if (is_program_available(backends[i])) {
            found_backend = true;
            break;
        }
    }

    if (!found_backend) {
        fprintf(stderr,
                "Error: No partclone backend found in PATH.\n"
                "Please install partclone (e.g., partclone.ext4, partclone.btrfs, etc.) and try again.\n");
        ok = false;
    }

    if (!is_program_available("gzip")) {
        fprintf(stderr,
                "Error: 'gzip' is not installed or not in PATH.\n");
        ok = false;
    }

    if (!ok) {
        exit(EXIT_FAILURE);
    }
}


/*
 * Get filesystem type of a device using `lsblk`.
 * Command: lsblk -no FSTYPE /dev/...
 *
 * On success: returns true and writes a null-terminated string into fs_type.
 * On failure: returns false and leaves fs_type unspecified.
 */
bool get_fs_type(const char *device, char *fs_type, int fs_type_len)
{
    if (!device || !fs_type || fs_type_len <= 0) {
        return false;
    }

    /* We will use popen for convenience here. */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "lsblk -no FSTYPE '%s' 2>/dev/null", device);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen");
        return false;
    }

    char buf[128] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        /* No output or error */
        pclose(fp);
        return false;
    }

    pclose(fp);

    /* Strip newline if present */
    buf[strcspn(buf, "\r\n")] = '\0';

    if (buf[0] == '\0') {
        /* Empty string -> unknown */
        return false;
    }

    /* Copy into caller buffer */
    strncpy(fs_type, buf, fs_type_len - 1);
    fs_type[fs_type_len - 1] = '\0';

    return true;
}
