#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compare_pkg_sizes(const void *a, const void *b) {
    const pu_pkg_info_t *pa = a;
    const pu_pkg_info_t *pb = b;
    if (pa->isize > pb->isize) return -1;
    if (pa->isize < pb->isize) return 1;
    return 0;
}

int cmd_top_sizes(int argc, char **argv) {
    int limit = 25;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--limit") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
            limit = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pacman-utils top-sizes [options]\n\n");
            printf("Lists installed packages sorted by on-disk size footprint.\n\n");
            printf("Options:\n");
            printf("  -n, --limit <N>     Number of packages to display (default: 25)\n");
            return PU_SUCCESS;
        }
    }

    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    pu_pkg_list_t *pkgs = alpm_get_installed_pkgs(alpm);
    if (!pkgs || pkgs->count == 0) {
        term_warn("No installed packages found.");
        if (pkgs) alpm_free_pkg_list(pkgs);
        alpm_helper_free(alpm);
        return PU_SUCCESS;
    }

    qsort(pkgs->items, pkgs->count, sizeof(pu_pkg_info_t), compare_pkg_sizes);

    char *total_sz = fs_format_size(pkgs->total_size);
    term_header("Largest Installed Packages");
    printf("Total installed system size: %s%s%s (across %zu packages)\n\n",
           term_color(ANSI_BOLD_GREEN), total_sz ? total_sz : "0 B", term_color(ANSI_RESET), pkgs->count);
    free(total_sz);

    int max = (limit > 0 && (size_t)limit < pkgs->count) ? limit : (int)pkgs->count;
    for (int i = 0; i < max; i++) {
        pu_pkg_info_t *p = &pkgs->items[i];
        char *sz = fs_format_size(p->isize);
        double pct = ((double)p->isize / (double)(pkgs->total_size > 0 ? pkgs->total_size : 1)) * 100.0;

        printf("  %s%2d.%s %s%-28s%s %s%-16s%s %s%10s%s  %s(%4.1f%%)%s\n",
               term_color(ANSI_BOLD_YELLOW), i + 1, term_color(ANSI_RESET),
               term_color(ANSI_BOLD), p->name, term_color(ANSI_RESET),
               term_color(ANSI_DIM), p->version, term_color(ANSI_RESET),
               term_color(ANSI_BOLD_CYAN), sz ? sz : "", term_color(ANSI_RESET),
               term_color(ANSI_DIM), pct, term_color(ANSI_RESET));
        free(sz);
    }

    alpm_free_pkg_list(pkgs);
    alpm_helper_free(alpm);
    return PU_SUCCESS;
}
