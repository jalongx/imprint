#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>   // <-- add this

/*
 * Basic command execution utilities and dependency checks.
 * These are used by both the UI and backup logic.
 */

/* Run a command and return its exit status (0 = success). */
int run_command(char *const argv[]);

/* Check if a program exists in PATH (using `which`). */
bool is_program_available(const char *name);

/* Check core dependencies (zenity, partclone, gzip). Exits on failure. */
void check_core_dependencies(void);

/*
 * Filesystem / partition helpers.
 *
 * These are early helpers we’ll expand:
 * - detect filesystem type for a given device
 * - later: lsblk parsing to show labels, sizes, etc.
 */

/* Get filesystem type of a device (e.g., "ext4", "btrfs", "ntfs", "vfat").
 * Returns true on success, and fills fs_type buffer (null-terminated).
 */
bool get_fs_type(const char *device, char *fs_type, int fs_type_len);

bool write_metadata(const char *image_path,
                    const char *device,
                    const char *fs_type,
                    const char *backend,
                    const char *compression);

bool compute_sha256(const char *filepath, char *out, size_t out_len);

long long get_partition_size_bytes(const char *device);

void gx_ensure_terminal(int argc, char **argv);

void ghostx_print_banner(const char *program_name);

bool gx_test_fifo_capability(const char *dir);

extern char gx_workdir_override[1024];

#endif /* UTILS_H */
