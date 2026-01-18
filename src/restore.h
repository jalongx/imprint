#ifndef RESTORE_H
#define RESTORE_H

#include <stdbool.h>
#include "utils.h"
#include "ui.h"

/*
 * Restore engine.
 * Handles:
 *  - reading the partclone backend from metadata JSON
 *  - constructing and running the decompression + partclone restore pipeline
 *  - integrating with the UI layer for image file and target partition
 */

/* ---------------------------------------------------------
 * Usage / Help
 * --------------------------------------------------------- */
typedef struct parse_output {
    bool parse_error;
    bool cli_mode;
} parse_output;


bool print_restore_usage(struct parse_output *out);

/* ---------------------------------------------------------
 * Interactive restore (GUI)
 * --------------------------------------------------------- */
bool restore_run_interactive(void);

/* ---------------------------------------------------------
 * Restore pipeline
 *   gzip -dc <image> | backend -r -s - -o <device>
 * --------------------------------------------------------- */
bool run_restore_pipeline(const char *backend,
                          const char *image_base,
                          const char *device,
                          const char *compression,
                          bool chunked);

/* ---------------------------------------------------------
 * CLI argument structure
 * --------------------------------------------------------- */
typedef struct {
    bool cli_mode;       /* true if valid CLI args were provided */
    bool parse_error;    /* true if CLI args were invalid */

    const char *image;   /* --image <path> or positional #1 */
    const char *target;  /* --target <device> or positional #2 */

    bool force;          /* --force flag */
} RestoreCLIArgs;

/* ---------------------------------------------------------
 * CLI parsing
 * --------------------------------------------------------- */
bool parse_restore_cli_args(int argc, char **argv, RestoreCLIArgs *out);

/* ---------------------------------------------------------
 * Non-interactive restore
 * --------------------------------------------------------- */
bool restore_run_cli(const char *image,
                     const char *target,
                     bool force);

#endif /* RESTORE_H */
