#include "utils.h"
#include "colors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/sha.h>

static void get_parent_disk(const char *device, char *out, size_t out_len)
{
    /* NVMe: /dev/nvme0n1p6 → /dev/nvme0n1 */
    const char *p = strstr(device, "nvme");
    if (p) {
        /* strip trailing 'p<partition>' */
        const char *last_p = strrchr(device, 'p');
        if (last_p && last_p > p) {
            size_t len = (size_t)(last_p - device);
            if (len >= out_len) len = out_len - 1;
            memcpy(out, device, len);
            out[len] = '\0';
            return;
        }
    }

    /* SATA: /dev/sda3 → /dev/sda */
    size_t len = strlen(device);
    if (len > 0 && isdigit((unsigned char)device[len - 1])) {

        size_t i = len - 1;
        while (i > 0 && isdigit((unsigned char)device[i])) {
            i--;
        }

        /* i now points to the last non-digit character */
        size_t plen = i + 1;
        if (plen >= out_len)
            plen = out_len - 1;

        memcpy(out, device, plen);
        out[plen] = '\0';
        return;
    }


    /* fallback: copy as-is */
    snprintf(out, out_len, "%s", device);
}

static bool get_partition_layout_json(const char *disk, char *out, size_t out_len)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "lsblk -J -o NAME,SIZE,FSTYPE,TYPE,MOUNTPOINT '%s'", disk);

    FILE *fp = popen(cmd, "r");
    if (!fp) return false;

    size_t total = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && total + 1 < out_len) {
        out[total++] = (char)c;
    }
    out[total] = '\0';

    pclose(fp);
    return total > 0;
}

/* ---------------------------------------------------------
 * Get partition size in bytes using lsblk
 * --------------------------------------------------------- */
long long get_partition_size_bytes(const char *device)
{
    if (!device)
        return 0;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "lsblk -bno SIZE '%s' 2>/dev/null", device);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return 0;

    char buf[64] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return 0;
    }

    pclose(fp);

    /* Strip whitespace */
    for (char *p = buf; *p; ++p) {
        if (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')
            *p = '\0';
    }

    if (buf[0] == '\0')
        return 0;

    return atoll(buf);
}

/* ---------------------------------------------------------
 * SHA‑256 with progress + hex output
 * --------------------------------------------------------- */
bool compute_sha256(const char *filepath, char *out, size_t out_len)
{
     if (!filepath || !out || out_len < 65)
        return false;

    FILE *fp = fopen(filepath, "rb");
    if (!fp)
        return false;

    struct stat st;
    if (stat(filepath, &st) != 0) {
        fclose(fp);
        return false;
    }

    off_t total = st.st_size;
    off_t processed = 0;

    printf(YELLOW "\nCalculating backup file checksum...\n" RESET);

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    unsigned char buf[1024 * 1024]; // 1MB buffer
    size_t read;

    while ((read = fread(buf, 1, sizeof(buf), fp)) > 0) {

        /* DEBUG: show how much was read */

        SHA256_Update(&ctx, buf, read);
        processed += read;

        double pct = (double)processed / total * 100.0;

        /* Print progress */
        fprintf(stderr, WHITE "\r%.1f%% " RESET, pct);
        fflush(stderr);
    }

    fclose(fp);

    /* Finalize hash */
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    /* Convert to hex string */
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        sprintf(out + (i * 2), "%02x", hash[i]);

    out[64] = '\0';

    return true;
}

/* ---------------------------------------------------------
 * Write metadata JSON
 * --------------------------------------------------------- */
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
    if (!compute_sha256(image_path, checksum, sizeof(checksum))) {
        fprintf(stderr, RED "Failed to compute checksum!\n" RESET);
        return false;
    }

    char parent_disk[128] = {0};
    get_parent_disk(device, parent_disk, sizeof(parent_disk));

    /* Partition layout (safe without root) */
    char layout_json[8192] = "null";
    get_partition_layout_json(parent_disk, layout_json, sizeof(layout_json));

    FILE *fp = fopen(meta_path, "w");
    if (!fp)
        return false;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"tool_version\": \"1.1\",\n");
    fprintf(fp, "  \"timestamp\": %ld,\n", now);
    fprintf(fp, "  \"device\": \"%s\",\n", device);
    fprintf(fp, "  \"filesystem\": \"%s\",\n", fs_type);
    fprintf(fp, "  \"backend\": \"%s\",\n", backend);
    fprintf(fp, "  \"partition_size_bytes\": %lld,\n", part_size);
    fprintf(fp, "  \"image_filename\": \"%s\",\n", image_path);
    fprintf(fp, "  \"image_checksum_sha256\": \"%s\",\n", checksum);

    fprintf(fp, "  \"source_disk\": \"%s\",\n", parent_disk);
    fprintf(fp, "  \"source_partition_layout\": %s,\n", layout_json);

    fprintf(fp, "  \"notes\": \"\"\n");
    fprintf(fp, "}\n");

    fclose(fp);

    printf(YELLOW "\nMetadata written successfully.\n" RESET);
    return true;
}

/* ---------------------------------------------------------
 * Run a command via fork/exec
 * --------------------------------------------------------- */
int run_command(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return -1;
}

/* ---------------------------------------------------------
 * Check if a program exists in PATH
 * --------------------------------------------------------- */
bool is_program_available(const char *name)
{
    char *argv[] = { "which", (char *)name, NULL };
    return (run_command(argv) == 0);
}

/* ---------------------------------------------------------
 * Dependency checks
 * --------------------------------------------------------- */
void check_core_dependencies(void)
{
    bool ok = true;

    printf(YELLOW "Checking whether dependencies are installed...\n\n" RESET);

    if (!is_program_available("zenity")) {
        fprintf(stderr, "Error: 'zenity' is not installed.\n");
        ok = false;
    }

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
                "Error: No partclone backend found.\n");
        ok = false;
    }

    if (!is_program_available("lz4")) {
        fprintf(stderr, "Error: 'lz4' is not installed.\n");
        ok = false;
    }

    if (!is_program_available("pkexec")) {
        fprintf(stderr, "Error: 'pkexec' is not installed.\n");
        ok = false;
    }

    if (!ok)
        exit(EXIT_FAILURE);

    printf(GREEN "\nAll dependencies installed.\n\n" RESET);
}

/* ---------------------------------------------------------
 * Get filesystem type via lsblk
 * --------------------------------------------------------- */
bool get_fs_type(const char *device, char *fs_type, int fs_type_len)
{
    if (!device || !fs_type || fs_type_len <= 0)
        return false;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "lsblk -no FSTYPE '%s' 2>/dev/null", device);

    FILE *fp = popen(cmd, "r");
    if (!fp)
        return false;

    char buf[128] = {0};
    if (!fgets(buf, sizeof(buf), fp)) {
        pclose(fp);
        return false;
    }

    pclose(fp);

    buf[strcspn(buf, "\r\n")] = '\0';

    if (buf[0] == '\0')
        return false;

    strncpy(fs_type, buf, fs_type_len - 1);
    fs_type[fs_type_len - 1] = '\0';

    return true;
}
