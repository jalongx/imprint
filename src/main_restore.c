#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils.h"
#include "restore.h"

int main(void)
{
    /* 1. Early: verify core dependencies. */
    check_core_dependencies();

    /* 2. Run a single interactive restore session. */
    bool ok = restore_run_interactive();

    if (!ok) {
        /* Either user cancelled or restore failed (UI already reported). */
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
