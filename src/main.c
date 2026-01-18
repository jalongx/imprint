#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "backup.h"
#include "config.h"

/* Forward declaration so we can call it early */
void print_backup_usage(void);

int main(int argc, char **argv)
{
    /* ---------------------------------------------------------
     * EARLY HELP DETECTION
     * Must run BEFORE banner, terminal spawning, or deps.
     * --------------------------------------------------------- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_backup_usage();
            return EXIT_SUCCESS;
        }
    }

    /* ---------------------------------------------------------
     * Normal startup path
     * --------------------------------------------------------- */
    gx_ensure_terminal(argc, argv);

    ghostx_print_banner("Imprint Disk Imager");

    ghostx_config_load();
    check_core_dependencies();

    BackupCLIArgs args = {0};
    bool parsed = parse_backup_cli_args(argc, argv, &args);

    /* ---------------------------------------------------------
     * Case 1: CLI args were provided but invalid
     * --------------------------------------------------------- */
    if (!parsed) {
        if (args.parse_error) {
            return EXIT_FAILURE;   /* syntax error */
        }
        /* No parse error + no help flag → GUI mode */
    }

    /* ---------------------------------------------------------
     * Case 2: Valid CLI mode
     * --------------------------------------------------------- */
    if (parsed && args.cli_mode) {
        gx_no_gui = true;

        /* Apply CLI compression override to config for this run */
        if (args.compress_override && args.compress_override[0] != '\0') {
            strncpy(gx_config.compression,
                    args.compress_override,
                    sizeof(gx_config.compression) - 1);
            gx_config.compression[sizeof(gx_config.compression) - 1] = '\0';
        }

        /*
         * Determine effective chunk size (MB)
         */
        int chunk_mb;
        if (args.chunk_override_set) {
            chunk_mb = args.chunk_override;   /* may be 0 → disables chunking */
        } else {
            chunk_mb = gx_config.chunk_size_mb;
        }

        /* -----------------------------------------------------
         * Run CLI backup
         * ----------------------------------------------------- */
        bool ok = backup_run_cli(args.source,
                                 args.target,
                                 gx_config.compression,   /* effective compressor */
                                 chunk_mb,                /* effective chunk size */
                                 args.force);             /* NEW */

        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* ---------------------------------------------------------
     * Case 3: No CLI args → GUI mode
     * --------------------------------------------------------- */
    bool ok = backup_run_interactive();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
