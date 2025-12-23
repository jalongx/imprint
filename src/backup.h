#ifndef BACKUP_H
#define BACKUP_H

#include <stdbool.h>

/*
 * Backup engine.
 * Handles:
 *  - selecting the correct partclone backend based on filesystem
 *  - constructing and running the partclone + gzip pipeline
 *  - integrating with the UI layer for directory, partition, and filename
 */

/* Run an interactive backup session:
 *  - choose directory
 *  - choose partition
 *  - detect filesystem
 *  - choose filename
 *  - run partclone + gzip
 *
 * Returns true on success, false on failure or user cancel.
 */
bool backup_run_interactive(void);

bool run_backup_pipeline(const char *backend,
                         const char *device,
                         const char *fs_type,
                         const char *output_path);

#endif /* BACKUP_H */
