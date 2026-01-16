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
                          const char *image_base,
                          const char *device,
                          const char *compression,
                          bool chunked);

typedef struct {
    bool cli_mode;       /* true if valid CLI args were provided */
    bool parse_error;    /* true if CLI args were invalid */

    const char *image;   /* --image <path> or positional #1 */
    const char *target;  /* --target <device> or positional #2 */

    bool force;          /* --force flag */
} RestoreCLIArgs;

/* Parse CLI args for imprintr */
bool parse_restore_cli_args(int argc, char **argv, RestoreCLIArgs *out);

/* Run non-interactive restore */
bool restore_run_cli(const char *image,
                     const char *target,
                     bool force);


#endif /* RESTORE_H */
