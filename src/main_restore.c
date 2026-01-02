#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils.h"
#include "restore.h"

#include "config.h"

int main(int argc, char **argv)
{
    gx_ensure_terminal(argc, argv);
    ghostx_print_banner("Imprint Disk Imager");

    ghostx_config_load();
    check_core_dependencies();

    bool ok = restore_run_interactive();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
