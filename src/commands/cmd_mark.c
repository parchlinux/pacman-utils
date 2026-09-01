#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_mark(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils mark <as-explicit | as-dep> <package_name...>\n\n");
        printf("Changes install reason for packages (equivalent to apt-mark manual/auto).\n");
        return PU_SUCCESS;
    }

    alpm_pkgreason_t reason;
    if (strcmp(argv[1], "as-explicit") == 0 || strcmp(argv[1], "explicit") == 0 || strcmp(argv[1], "manual") == 0) {
        reason = ALPM_PKG_REASON_EXPLICIT;
    } else if (strcmp(argv[1], "as-dep") == 0 || strcmp(argv[1], "as-deps") == 0 || strcmp(argv[1], "auto") == 0) {
        reason = ALPM_PKG_REASON_DEPEND;
    } else {
        term_error("Unknown mark option '%s'. Use 'as-explicit' or 'as-dep'.", argv[1]);
        return PU_ERR_USAGE;
    }

    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    for (int i = 2; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        alpm_mark_pkg_reason(alpm, argv[i], reason);
    }

    alpm_helper_free(alpm);
    return PU_SUCCESS;
}
