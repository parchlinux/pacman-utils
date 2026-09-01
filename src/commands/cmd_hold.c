#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "config_parser.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_show_held(int argc, char **argv) {
    (void)argc;
    (void)argv;
    const char *cfg_path = g_options.config_path ? g_options.config_path : DEFAULT_PACMAN_CONF;

    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) return PU_ERR_CONFIG;

    size_t count = 0;
    char **held = config_get_ignore_pkgs(cfg, &count);

    term_header("Ignored / Held Packages (IgnorePkg)");

    if (count == 0) {
        term_info("No packages are currently marked as held (IgnorePkg).");
    } else {
        pu_alpm_t *alpm = alpm_helper_init(cfg_path, false);

        for (size_t i = 0; i < count; i++) {
            const char *status = "not installed";
            const char *ver = "";
            if (alpm && alpm->localdb) {
                alpm_pkg_t *p = alpm_db_get_pkg(alpm->localdb, held[i]);
                if (p) {
                    status = "installed";
                    ver = alpm_pkg_get_version(p);
                }
            }

            printf("  %s●%s %s%-20s%s %s%-12s%s %s%s%s\n",
                   term_color(ANSI_BOLD_YELLOW), term_color(ANSI_RESET),
                   term_color(ANSI_BOLD), held[i], term_color(ANSI_RESET),
                   term_color(ANSI_DIM), status, term_color(ANSI_RESET),
                   term_color(ANSI_CYAN), ver, term_color(ANSI_RESET));
        }

        if (alpm) alpm_helper_free(alpm);
    }

    config_free_string_list(held, count);
    config_free(cfg);
    return PU_SUCCESS;
}

int cmd_hold(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils hold <package_name...>\n\n");
        printf("Prevents specified packages from being upgraded by adding them to IgnorePkg.\n");
        return PU_SUCCESS;
    }

    if (strcmp(argv[1], "--list") == 0 || strcmp(argv[1], "-l") == 0) {
        return cmd_show_held(argc, argv);
    }

    const char *cfg_path = g_options.config_path ? g_options.config_path : DEFAULT_PACMAN_CONF;
    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) return PU_ERR_CONFIG;

    int modified = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        if (config_add_ignore_pkg(cfg, argv[i])) {
            term_success("Package '%s' is now held (added to IgnorePkg).", argv[i]);
            modified++;
        } else {
            term_warn("Package '%s' was already held.", argv[i]);
        }
    }

    if (modified > 0) {
        if (config_write_file(cfg, cfg_path) != 0) {
            term_error("Failed to update %s (do you need sudo?)", cfg_path);
            config_free(cfg);
            return PU_ERR_PERM;
        }
    }

    config_free(cfg);
    return PU_SUCCESS;
}

int cmd_unhold(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils unhold <package_name...>\n\n");
        printf("Resumes upgrading specified packages by removing them from IgnorePkg.\n");
        return PU_SUCCESS;
    }

    const char *cfg_path = g_options.config_path ? g_options.config_path : DEFAULT_PACMAN_CONF;
    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) return PU_ERR_CONFIG;

    int modified = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        if (config_remove_ignore_pkg(cfg, argv[i])) {
            term_success("Package '%s' is unheld (removed from IgnorePkg).", argv[i]);
            modified++;
        } else {
            term_warn("Package '%s' was not in IgnorePkg.", argv[i]);
        }
    }

    if (modified > 0) {
        if (config_write_file(cfg, cfg_path) != 0) {
            term_error("Failed to update %s (do you need sudo?)", cfg_path);
            config_free(cfg);
            return PU_ERR_PERM;
        }
    }

    config_free(cfg);
    return PU_SUCCESS;
}
