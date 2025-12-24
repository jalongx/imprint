#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "backup.h"

int main(void)
{
    /* 1. Early: verify core dependencies. */
    check_core_dependencies();

    /* 2. Run a single interactive backup session. */
    bool ok = backup_run_interactive();

    if (!ok) {
        /* Either user cancelled or backup failed (UI already reported). */
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

