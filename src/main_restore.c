#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "utils.h"
#include "restore.h"
#include "config.h"
#include "colors.h"
#include "ui.h"

int main(int argc, char **argv)
{
    /* ---------------------------------------------------------
     * Parse output struct must exist BEFORE early help detection
     * --------------------------------------------------------- */
    struct parse_output out = {0};

    /* ---------------------------------------------------------
     * EARLY HELP DETECTION
     * Must run BEFORE banner, terminal spawning, or deps.
     * --------------------------------------------------------- */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_restore_usage(&out);
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

    /* ---------------------------------------------------------
     * CLI argument parsing
     * --------------------------------------------------------- */
    RestoreCLIArgs args;
    bool cli_parse = parse_restore_cli_args(argc, argv, &args);

    /*
     * If cli_parse == true:
     *   - Either valid CLI args were provided (args.cli_mode == true)
     *   - Or invalid CLI args were provided (args.parse_error == true)
     *   - Or --help was shown (args.cli_mode == false && !args.parse_error)
     *
     * In ALL THREE cases, GUI must be suppressed.
     */
    if (cli_parse) {

        /* Suppress Zenity globally */
        gx_no_gui = true;

        /* Invalid CLI arguments → exit with error */
        if (args.parse_error) {
            return EXIT_FAILURE;
        }

        /* Valid CLI mode → run non-interactive restore */
        if (args.cli_mode) {
            bool ok = restore_run_cli(args.image,
                                      args.target,
                                      args.force);
            return ok ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        /* --help was shown → exit cleanly */
        return EXIT_SUCCESS;
    }

    /* ---------------------------------------------------------
     * No CLI args → run GUI restore
     * --------------------------------------------------------- */
    bool ok = restore_run_interactive();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
