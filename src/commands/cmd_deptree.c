#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_deptree(int argc, char **argv) {
    bool reverse = false;
    int max_depth = 8;
    const char *pkgname = NULL;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils deptree [options] <package_name>\n\n");
        printf("Displays an ASCII/Unicode dependency tree for an installed package.\n\n");
        printf("Options:\n");
        printf("  -r, --reverse        Show reverse dependencies (packages that depend on this)\n");
        printf("  -d, --depth <N>      Maximum depth level (default: 8)\n");
        return PU_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--reverse") == 0 || strcmp(argv[i], "-r") == 0) {
            reverse = true;
        } else if ((strcmp(argv[i], "--depth") == 0 || strcmp(argv[i], "-d") == 0) && i + 1 < argc) {
            max_depth = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            pkgname = argv[i];
        }
    }

    if (!pkgname) {
        term_error("Package name is required.");
        return PU_ERR_USAGE;
    }

    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    int ret = alpm_print_deptree(alpm, pkgname, reverse, max_depth);
    alpm_helper_free(alpm);
    return ret;
}
