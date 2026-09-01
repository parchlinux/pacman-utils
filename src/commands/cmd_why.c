#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_why(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils why <package_name>\n\n");
        printf("Explains why a package is installed by tracing its reverse dependency chain.\n");
        return PU_SUCCESS;
    }

    const char *pkgname = argv[1];
    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    int ret = alpm_explain_why(alpm, pkgname);
    alpm_helper_free(alpm);
    return ret;
}
