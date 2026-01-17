#include "utils.h"
#include "colors.h"
#include "ui.h"
#include "config.h"

#include <stdbool.h>
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
#include <sys/statfs.h>
#include <errno.h>

bool gx_no_gui = false;

void ghostx_print_banner(const char *program_name)
{
    const char *line = "============================================================";

    printf(WHITE "\n%s\n\n" RESET, line);
    printf(GREEN "%s v%s, %s\n\n" RESET,
           program_name,
           GHOSTX_VERSION,
           GHOSTX_BUILD_DATE);
    printf(WHITE "%s\n\n" RESET, line);
}


bool is_program_available(const char *name)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", name);
    return (system(cmd) == 0);
}

void gx_ensure_terminal(int argc, char **argv)
{
    /* If already in a real terminal, do nothing */
    if (isatty(STDOUT_FILENO)) {
        return;
    }

    /* Terminal emulators to try */
    const char *terms[] = {
        "konsole",
        "gnome-terminal",
        "xfce4-terminal",
        "x-terminal-emulator",
        "kitty",
        "alacritty",
        "xterm",
        NULL
    };

    for (int i = 0; terms[i]; i++) {
        if (is_program_available(terms[i])) {

            char cmd[4096];

            if (strcmp(terms[i], "konsole") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "konsole -e \"%s", argv[0]);
            }
            else if (strcmp(terms[i], "gnome-terminal") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "gnome-terminal -- bash -c \"%s", argv[0]);
            }
            else if (strcmp(terms[i], "xfce4-terminal") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "xfce4-terminal -e \"%s", argv[0]);
            }
            else if (strcmp(terms[i], "x-terminal-emulator") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "x-terminal-emulator -e \"%s", argv[0]);
            }
            else if (strcmp(terms[i], "kitty") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "kitty \"%s", argv[0]);
            }
            else if (strcmp(terms[i], "alacritty") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "alacritty -e \"%s", argv[0]);
            }
            else {
                snprintf(cmd, sizeof(cmd),
                         "xterm -hold -e \"%s", argv[0]);
            }

            /* Append arguments */
            for (int j = 1; j < argc; j++) {
                strcat(cmd, " ");
                strcat(cmd, argv[j]);
            }

            strcat(cmd, "\"");

            /* XFCE fix: fork, child launches terminal, parent waits then exits */
            pid_t pid = fork();
            if (pid == 0) {
                execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                _exit(1);
            }

            waitpid(pid, NULL, 0);
            exit(0);
        }
    }

    fprintf(stderr,
            "Imprint needs a terminal to display progress, but no terminal emulator was found.\n");
    exit(1);
}

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
             "lsblk -J -o NAME,SIZE,FSTYPE,LABEL,TYPE,MOUNTPOINT '%s'", disk);

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
                    const char *backend,
                    const char *compression,
                    int effective_chunk_mb,
                    int chunk_count)

{
    if (!image_path || !device || !fs_type || !backend)
        return false;

    char meta_path[1024];
    snprintf(meta_path, sizeof(meta_path), "%s.json", image_path);

    time_t now = time(NULL);
    long long part_size = get_partition_size_bytes(device);

    /* -----------------------------------------------------------------
     * Read checksum from the streamed sha256 output file: <image>.sha256
     * Format: "<hex>  filename"
     * ----------------------------------------------------------------- */
    char checksum_file[1024];
    snprintf(checksum_file, sizeof(checksum_file), "%s.sha256", image_path);

    char checksum[65] = {0};
    bool have_checksum = false;

    FILE *cfp = fopen(checksum_file, "r");
    if (cfp) {
        if (fscanf(cfp, "%64s", checksum) == 1) {
            have_checksum = true;
        }
        fclose(cfp);
    }

    if (!have_checksum) {
        fprintf(stderr, YELLOW "WARNING: Could not read streamed checksum. Recomputing locally...\n" RESET);

        if (!compute_sha256(image_path, checksum, sizeof(checksum))) {
            fprintf(stderr, RED "Failed to compute checksum!\n" RESET);
            return false;
        }
    }

    char parent_disk[128] = {0};
    get_parent_disk(device, parent_disk, sizeof(parent_disk));

    /* Partition layout (safe without root) */
    char layout_json[8192] = "null";
    get_partition_layout_json(parent_disk, layout_json, sizeof(layout_json));

    /* Chunking flags from config */
    bool chunked = (effective_chunk_mb > 0);
    int  chunk_size_mb = effective_chunk_mb;

    FILE *fp = fopen(meta_path, "w");
    if (!fp)
        return false;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"tool_version\": \"1.2\",\n");
    fprintf(fp, "  \"timestamp\": %ld,\n", now);
    fprintf(fp, "  \"device\": \"%s\",\n", device);
    fprintf(fp, "  \"filesystem\": \"%s\",\n", fs_type);
    fprintf(fp, "  \"backend\": \"%s\",\n", backend);
    fprintf(fp, "  \"compression\": \"%s\",\n", compression);
    fprintf(fp, "  \"partition_size_bytes\": %lld,\n", part_size);
    fprintf(fp, "  \"image_filename\": \"%s\",\n", image_path);
    fprintf(fp, "  \"image_checksum_sha256\": \"%s\",\n", checksum);
    fprintf(fp, "  \"chunked\": %s,\n", chunked ? "true" : "false");
    fprintf(fp, "  \"chunk_size_mb\": %d,\n", chunk_size_mb);
    fprintf(fp, "  \"chunk_count\": %d,\n", chunk_count);
    fprintf(fp, "  \"source_disk\": \"%s\",\n", parent_disk);
    fprintf(fp, "  \"source_partition_layout\": %s,\n", layout_json);
    fprintf(fp, "  \"notes\": \"\"\n");
    fprintf(fp, "}\n");

    fclose(fp);

    // printf(YELLOW "\nMetadata written successfully.\n" RESET);
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
 * Dependency checks
 * --------------------------------------------------------- */
void check_core_dependencies(void)
{
    bool ok = true;

    printf(YELLOW "Checking whether dependencies are installed...\n" RESET);

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

    if (!is_program_available("gzip")) {
        fprintf(stderr, "Error: 'gzip' is not installed.\n");
        ok = false;
    }

    if (!is_program_available("zstd")) {
        fprintf(stderr, "Error: 'zstd' is not installed.\n");
        ok = false;
    }

    if (!is_program_available("pkexec")) {
        fprintf(stderr, "Error: 'pkexec' is not installed.\n");
        ok = false;
    }

    if (!ok)
        exit(EXIT_FAILURE);

    printf(GREEN "All dependencies installed.\n\n" RESET);
}

/* ---------------------------------------------------------
 * Get filesystem type via lsblk
 * --------------------------------------------------------- */
bool get_fs_type(const char *device, char *fs_type, int fs_type_len)
{
    if (!device || !fs_type || fs_type_len <= 0)
        return false;

    char buf[128] = {0};

    //
    // First attempt: lsblk (fast, works for most devices)
    //
    {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "lsblk -no FSTYPE '%s' 2>/dev/null", device);

        FILE *fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) {
                pclose(fp);

                // Trim newline
                buf[strcspn(buf, "\r\n")] = '\0';

                // Trim leading whitespace
                char *p = buf;
                while (isspace((unsigned char)*p)) p++;

                // If lsblk returned something meaningful, use it
                if (*p != '\0') {
                    strncpy(fs_type, p, fs_type_len - 1);
                    fs_type[fs_type_len - 1] = '\0';
                    return true;
                }
            } else {
                pclose(fp);
            }
        }
    }

    //
    // Second attempt: blkid -p (deep probing, works for LUKS mapper devices)
    //
    {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
                 "blkid -p -o value -s TYPE '%s' 2>/dev/null", device);

        FILE *fp = popen(cmd, "r");
        if (!fp)
            return false;

        memset(buf, 0, sizeof(buf));

        if (!fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            return false;
        }

        pclose(fp);

        // Trim newline
        buf[strcspn(buf, "\r\n")] = '\0';

        // Trim leading whitespace
        char *p = buf;
        while (isspace((unsigned char)*p)) p++;

        if (*p == '\0')
            return false;

        strncpy(fs_type, p, fs_type_len - 1);
        fs_type[fs_type_len - 1] = '\0';
        return true;
    }
}


bool gx_test_fifo_capability(const char *dir)
{
    if (!dir)
        return false;

    char test_fifo[1024];
    snprintf(test_fifo, sizeof(test_fifo), "%s/.imprint_fifo_test", dir);

    /* Try to create a FIFO */
    if (mkfifo(test_fifo, 0600) == 0) {
        unlink(test_fifo);
        return true;
    }

    /* If mkfifo failed, check errno */
    switch (errno) {
        case EROFS:   /* read-only filesystem */
        case ENOTSUP: /* filesystem does not support FIFOs */
        case EINVAL:  /* common for SMB/exFAT/FUSE */
        case EPERM:
            return false;

        default:
            /* Unexpected error — treat as unsupported */
            return false;
    }
}

bool gx_is_partition_mounted(const char *device)
{
    if (!device || device[0] == '\0')
        return false;

    FILE *fp = fopen("/proc/self/mounts", "r");
    if (!fp)
        return false;

    char line[1024];
    bool mounted = false;

    while (fgets(line, sizeof(line), fp)) {
        /*
         * /proc/self/mounts format:
         *   <source> <target> <fstype> <options> <dump> <pass>
         * we only care about <source>, which is the device path.
         */
        char src[256] = {0};

        if (sscanf(line, "%255s", src) != 1)
            continue;

        if (strcmp(src, device) == 0) {
            mounted = true;
            break;
        }
    }

    fclose(fp);
    return mounted;
}

