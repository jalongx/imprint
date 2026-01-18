#ifndef BACKUP_H
#define BACKUP_H

#include <stdbool.h>

/*
 * Backup engine.
 * Handles:
 *  - selecting the correct partclone backend based on filesystem
 *  - constructing and running the partclone + compression pipeline
 *  - integrating with the UI layer for directory, partition, and filename
 *
 * This header supports both interactive (GUI) mode and
 * fully non-interactive CLI mode with override flags.
 */

/*
 * CLI argument structure for imprintb.
 *
 * Notes:
 *  - chunk_override holds the numeric value from --chunk
 *  - chunk_override_set tells us whether the user explicitly provided --chunk
 *    (this allows distinguishing "no override" from "--chunk 0")
 */
typedef struct {
    bool cli_mode;
    bool parse_error;

    const char *source;
    const char *target;
    const char *compress_override;

    int  chunk_override;
    bool chunk_override_set;

    bool force;   /* NEW */
} BackupCLIArgs;

/*
 * Parse CLI arguments for imprintb.
 * Supports:
 *   --source <device>
 *   --target <path>
 *   --compress <type>
 *   --chunk <size_mb>
 *   <device> <path>   (positional fallback)
 *
 * Returns true if CLI mode should be used.
 * Returns false if arguments are insufficient and GUI mode should run.
 */
bool parse_backup_cli_args(int argc, char **argv, BackupCLIArgs *out);

/*
 * Run a CLI backup session (no Zenity).
 * Uses:
 *   - device
 *   - target path
 *   - effective compressor (config or override)
 *   - effective chunk size (config or override)
 *
 * Returns true on success, false on failure.
 */
bool backup_run_cli(const char *device,
                    const char *output_path,
                    const char *compressor,
                    int chunk_mb,
                    bool force);


/*
 * Run an interactive backup session:
 *  - choose directory
 *  - choose partition
 *  - detect filesystem
 *  - choose filename
 *  - run partclone + compression
 *
 * Returns true on success, false on failure or user cancel.
 */
bool backup_run_interactive(void);

/*
 * Core backup pipeline:
 *  - backend selection
 *  - partclone streaming
 *  - compression
 *  - checksum
 *  - metadata
 *
 * compressor = effective compressor (config or override)
 * chunk_mb   = effective chunk size (config or override)
 */
bool run_backup_pipeline(const char *backend,
                         const char *device,
                         const char *fs_type,
                         const char *output_path,
                         const char *compressor,
                         int chunk_mb);

void print_backup_usage(void);

#endif /* BACKUP_H */
