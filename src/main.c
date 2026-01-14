#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "backup.h"
#include "config.h"

int main(int argc, char **argv)
{
    gx_ensure_terminal(argc, argv);

    ghostx_print_banner("Imprint Disk Imager");

    ghostx_config_load();
    check_core_dependencies();

    BackupCLIArgs args = {0};

    bool parsed = parse_backup_cli_args(argc, argv, &args);

    /* Case 1: CLI args were provided but invalid */
    if (!parsed && args.parse_error) {
        return EXIT_FAILURE;
    }

    /* Case 2: Valid CLI mode */
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
         *
         * Correct logic:
         *   - If user explicitly provided --chunk <N>, use that value (even if N == 0)
         *   - Otherwise fall back to config
         */
        int chunk_mb;
        if (args.chunk_override_set) {
            chunk_mb = args.chunk_override;   /* may be 0 → disables chunking */
        } else {
            chunk_mb = gx_config.chunk_size_mb;
        }

        bool ok = backup_run_cli(args.source,
                                 args.target,
                                 gx_config.compression,
                                 chunk_mb);

        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* Case 3: No CLI args → GUI mode */
    bool ok = backup_run_interactive();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
