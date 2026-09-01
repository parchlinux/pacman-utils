#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_search_file(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils search-file <filename_or_pattern>\n\n");
        printf("Searches installed packages for files matching the given pattern (like apt-file/pkgfile).\n");
        return PU_SUCCESS;
    }

    const char *search_term = argv[1];
    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    int ret = alpm_search_file(alpm, search_term);
    alpm_helper_free(alpm);
    return ret;
}
