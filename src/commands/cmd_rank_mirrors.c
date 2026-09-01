#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "net_utils.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_rank_mirrors(int argc, char **argv) {
    const char *mirror_file = "/etc/pacman.d/mirrorlist";
    const char *output_file = NULL;
    int top_n = 10;
    int timeout = 5;
    size_t max_test = 0;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--top") == 0 || strcmp(argv[i], "-t") == 0) && i + 1 < argc) {
            top_n = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--timeout") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc) {
            timeout = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((strcmp(argv[i], "--file") == 0 || strcmp(argv[i], "-f") == 0) && i + 1 < argc) {
            mirror_file = argv[++i];
        } else if ((strcmp(argv[i], "--max") == 0 || strcmp(argv[i], "-m") == 0) && i + 1 < argc) {
            max_test = (size_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pacman-utils rank-mirrors [options]\n\n");
            printf("Benchmarks and ranks package mirrors for fastest download speeds.\n\n");
            printf("Options:\n");
            printf("  -f, --file <path>     Input mirrorlist file (default: /etc/pacman.d/mirrorlist)\n");
            printf("  -o, --output <path>   Output path for ranked mirrorlist\n");
            printf("  -t, --top <N>         Number of fastest mirrors to keep in output (default: 10)\n");
            printf("  -s, --timeout <sec>   Connection timeout in seconds per mirror (default: 5)\n");
            printf("  -m, --max <N>         Maximum number of mirrors to test\n");
            return PU_SUCCESS;
        }
    }

    if (!fs_file_exists(mirror_file)) {
        term_error("Mirrorlist file not found: %s", mirror_file);
        return PU_ERR_NOT_FOUND;
    }

    pu_mirror_list_t *list = net_load_mirrorlist(mirror_file);
    if (!list || list->count == 0) {
        term_error("No mirrors found in %s", mirror_file);
        if (list) net_free_mirror_list(list);
        return PU_ERR_GENERAL;
    }

    int ret = net_rank_mirrors(list, timeout, max_test);
    if (ret == PU_SUCCESS && output_file != NULL) {
        ret = net_write_ranked_mirrorlist(list, output_file, top_n);
        if (ret == PU_SUCCESS) {
            term_success("Ranked mirrorlist written to %s", output_file);
        }
    }

    net_free_mirror_list(list);
    return ret;
}
