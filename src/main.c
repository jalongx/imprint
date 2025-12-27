#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "backup.h"

#include "config.h"

int main(void)
{
    ghostx_config_load();
    check_core_dependencies();

    bool ok = backup_run_interactive();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

