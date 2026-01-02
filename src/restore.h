#ifndef RESTORE_H
#define RESTORE_H

#include <stdbool.h>

/*
 * Restore engine.
 * Handles:
 *  - reading the partclone backend from the metadata JSON
 *  - constructing and running the gzip + partclone restore pipeline
 *  - integrating with the UI layer for image file and target partition
 */

/* Run an interactive restore session:
 *  - choose image file
 *  - choose target partition
 *  - read backend from metadata JSON
 *  - run gzip + partclone restore
 *
 * Returns true on success, false on failure or user cancel.
 */
bool restore_run_interactive(void);

/* Non-interactive restore pipeline:
 *  gzip -dc <image> | backend -r -s - -o <device>
 */
bool run_restore_pipeline(const char *backend,
                          const char *image_path,
                          const char *device,
                          const char *compression,
                          bool chunked);

#endif /* RESTORE_H */
