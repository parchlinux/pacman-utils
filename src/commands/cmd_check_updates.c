#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_check_updates(int argc, char **argv) {
    bool quiet = g_options.quiet;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quiet") == 0 || strcmp(argv[i], "-q") == 0) {
            quiet = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pacman-utils check-updates [options]\n\n");
            printf("Safely checks for available package updates without acquiring pacman's DB lock.\n\n");
            printf("Options:\n");
            printf("  -q, --quiet   Print only package names (useful for bar widgets / scripts)\n");
            return PU_SUCCESS;
        }
    }

    if (!quiet) {
        term_info("Syncing databases in temporary sandbox...");
    }

    pu_alpm_t *alpm = alpm_helper_init(NULL, true);
    if (!alpm) return PU_ERR_ALPM;

    /* Sync databases */
    alpm_db_update(alpm->handle, alpm->syncdbs, 0);

    pu_upgrade_list_t *upgrades = alpm_get_upgrades(alpm);
    if (!upgrades || upgrades->count == 0) {
        if (!quiet) {
            term_success("Your Parch Linux system is up to date!");
        }
        alpm_free_upgrade_list(upgrades);
        alpm_helper_free(alpm);
        return PU_SUCCESS;
    }

    if (quiet) {
        for (size_t i = 0; i < upgrades->count; i++) {
            printf("%s\n", upgrades->items[i].pkgname);
        }
    } else {
        char *total_sz = fs_format_size(upgrades->total_download_size);
        term_header("Available Updates");
        printf("Found %zu pending update(s) (total download: %s%s%s):\n\n",
               upgrades->count, term_color(ANSI_BOLD_GREEN), total_sz ? total_sz : "0 B", term_color(ANSI_RESET));
        free(total_sz);

        for (size_t i = 0; i < upgrades->count; i++) {
            pu_upgrade_info_t *u = &upgrades->items[i];
            char *sz = fs_format_size(u->download_size);
            printf("  %s●%s %s%-22s%s %s%-18s%s -> %s%-18s%s [%s%s%s] (%s)\n",
                   term_color(ANSI_BOLD_CYAN), term_color(ANSI_RESET),
                   term_color(ANSI_BOLD), u->pkgname, term_color(ANSI_RESET),
                   term_color(ANSI_DIM), u->oldversion, term_color(ANSI_RESET),
                   term_color(ANSI_BOLD_GREEN), u->newversion, term_color(ANSI_RESET),
                   term_color(ANSI_MAGENTA), u->repo, term_color(ANSI_RESET),
                   sz ? sz : "");
            free(sz);
        }
    }

    alpm_free_upgrade_list(upgrades);
    alpm_helper_free(alpm);
    return PU_SUCCESS;
}
