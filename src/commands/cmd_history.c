#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "log_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_history(int argc, char **argv) {
    const char *pkg_filter = NULL;
    int limit = 50;
    bool reverse = false;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--limit") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
            limit = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--reverse") == 0 || strcmp(argv[i], "-r") == 0) {
            reverse = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pacman-utils history [options] [package_name]\n\n");
            printf("Inspects /var/log/pacman.log to display package transactions.\n\n");
            printf("Options:\n");
            printf("  -n, --limit <N>     Limit output to N most recent entries (default: 50)\n");
            printf("  -r, --reverse       Display in chronological order (oldest first)\n");
            return PU_SUCCESS;
        } else if (argv[i][0] != '-') {
            pkg_filter = argv[i];
        }
    }

    pu_log_list_t *list = log_parse_file(DEFAULT_LOG_FILE);
    if (!list) {
        term_error("Failed to open or parse log file: %s", DEFAULT_LOG_FILE);
        return PU_ERR_IO;
    }

    int ret = log_print_history(list, pkg_filter, limit, reverse);
    log_free_list(list);
    return ret;
}
