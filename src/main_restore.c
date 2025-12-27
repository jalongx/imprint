#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils.h"
#include "restore.h"

#include "config.h"

int main(void)
{
    ghostx_config_load();
    check_core_dependencies();

    bool ok = restore_run_interactive();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
