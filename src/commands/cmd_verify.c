#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_verify(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils verify [--all | <package_name...>]\n\n");
        printf("Verifies file integrity and checks for missing package files.\n");
        return PU_SUCCESS;
    }

    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    if (strcmp(argv[1], "--all") == 0 || strcmp(argv[1], "-a") == 0) {
        int ret = alpm_verify_all(alpm);
        alpm_helper_free(alpm);
        return ret;
    }

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        pu_verify_result_t res;
        int ret = alpm_verify_package(alpm, argv[i], &res);
        if (ret == PU_ERR_NOT_FOUND) {
            term_error("Package '%s' is not installed.", argv[i]);
        } else {
            if (res.missing_files == 0) {
                term_success("Package '%s' verified (%d files OK).", argv[i], res.total_files);
            } else {
                term_warn("Package '%s': %d of %d files are MISSING!", argv[i], res.missing_files, res.total_files);
            }
        }
    }

    alpm_helper_free(alpm);
    return PU_SUCCESS;
}
