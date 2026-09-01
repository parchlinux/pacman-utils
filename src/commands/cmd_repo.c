#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "config_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_repo_help(void) {
    printf("Usage: pacman-utils repo <subcommand> [options]\n\n");
    printf("Subcommands:\n");
    printf("  list                          List all configured repositories and their status\n");
    printf("  add <name> [--server <url>]   Add a new repository stanza\n");
    printf("             [--include <path>]\n");
    printf("  remove <name>                 Remove a repository stanza\n");
    printf("  enable <name>                 Enable a disabled repository\n");
    printf("  disable <name>                Disable an active repository\n");
}

static int repo_list(const char *cfg_path) {
    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) {
        term_error("Failed to parse config file: %s", cfg_path);
        return PU_ERR_CONFIG;
    }

    term_header("Configured Repositories");

    for (pu_config_section_t *sec = cfg->sections; sec; sec = sec->next) {
        if (strcmp(sec->name, "options") == 0 || strcmp(sec->name, "global") == 0) {
            continue;
        }

        const char *status_str = sec->is_commented ? "[DISABLED]" : "[ENABLED]";
        const char *status_color = sec->is_commented ? ANSI_BOLD_RED : ANSI_BOLD_GREEN;

        printf("%s%s%s %s%s%s\n",
               term_color(status_color), status_str, term_color(ANSI_RESET),
               term_color(ANSI_BOLD_CYAN), sec->name, term_color(ANSI_RESET));

        for (pu_config_entry_t *e = sec->entries; e; e = e->next) {
            if (e->type == ENTRY_LINE_KEYVALUE && e->key) {
                printf("    %s%s%s = %s\n",
                       term_color(ANSI_DIM), e->key, term_color(ANSI_RESET),
                       e->value ? e->value : "");
            }
        }
        printf("\n");
    }

    config_free(cfg);
    return PU_SUCCESS;
}

static int repo_add(const char *cfg_path, int argc, char **argv) {
    if (argc < 3) {
        term_error("Repository name is required.");
        print_repo_help();
        return PU_ERR_USAGE;
    }

    const char *name = argv[2];
    const char *server = NULL;
    const char *include = NULL;
    const char *siglevel = "PackageRequired";

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            server = argv[++i];
        } else if (strcmp(argv[i], "--include") == 0 && i + 1 < argc) {
            include = argv[++i];
        } else if (strcmp(argv[i], "--siglevel") == 0 && i + 1 < argc) {
            siglevel = argv[++i];
        }
    }

    if (!server && !include) {
        term_error("Please specify either --server <URL> or --include <PATH>.");
        return PU_ERR_USAGE;
    }

    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) return PU_ERR_CONFIG;

    if (config_find_section(cfg, name) != NULL) {
        term_error("Repository '[%s]' already exists.", name);
        config_free(cfg);
        return PU_ERR_GENERAL;
    }

    pu_config_section_t *sec = config_add_section(cfg, name);
    if (!sec) {
        config_free(cfg);
        return PU_ERR_GENERAL;
    }

    config_add_entry(sec, "SigLevel", siglevel);
    if (server) config_add_entry(sec, "Server", server);
    if (include) config_add_entry(sec, "Include", include);

    if (config_write_file(cfg, cfg_path) != 0) {
        term_error("Failed to write to %s (do you need sudo?)", cfg_path);
        config_free(cfg);
        return PU_ERR_PERM;
    }

    config_free(cfg);
    term_success("Repository '[%s]' added successfully to %s", name, cfg_path);
    return PU_SUCCESS;
}

static int repo_remove(const char *cfg_path, const char *name) {
    if (!name) return PU_ERR_USAGE;

    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) return PU_ERR_CONFIG;

    if (!config_remove_section(cfg, name)) {
        term_error("Repository '[%s]' not found in %s", name, cfg_path);
        config_free(cfg);
        return PU_ERR_NOT_FOUND;
    }

    if (config_write_file(cfg, cfg_path) != 0) {
        term_error("Failed to update %s (do you need sudo?)", cfg_path);
        config_free(cfg);
        return PU_ERR_PERM;
    }

    config_free(cfg);
    term_success("Repository '[%s]' removed from %s", name, cfg_path);
    return PU_SUCCESS;
}

static int repo_toggle(const char *cfg_path, const char *name, bool enable) {
    if (!name) return PU_ERR_USAGE;

    pu_config_t *cfg = config_parse_file(cfg_path);
    if (!cfg) return PU_ERR_CONFIG;

    if (!config_set_section_enabled(cfg, name, enable)) {
        term_error("Repository '[%s]' not found in %s", name, cfg_path);
        config_free(cfg);
        return PU_ERR_NOT_FOUND;
    }

    if (config_write_file(cfg, cfg_path) != 0) {
        term_error("Failed to update %s (do you need sudo?)", cfg_path);
        config_free(cfg);
        return PU_ERR_PERM;
    }

    config_free(cfg);
    term_success("Repository '[%s]' %s in %s", name, enable ? "enabled" : "disabled", cfg_path);
    return PU_SUCCESS;
}

int cmd_repo(int argc, char **argv) {
    const char *cfg_path = g_options.config_path ? g_options.config_path : DEFAULT_PACMAN_CONF;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_repo_help();
        return PU_SUCCESS;
    }

    const char *sub = argv[1];
    if (strcmp(sub, "list") == 0) {
        return repo_list(cfg_path);
    } else if (strcmp(sub, "add") == 0) {
        return repo_add(cfg_path, argc, argv);
    } else if (strcmp(sub, "remove") == 0 || strcmp(sub, "rm") == 0) {
        if (argc < 3) {
            term_error("Repository name required.");
            return PU_ERR_USAGE;
        }
        return repo_remove(cfg_path, argv[2]);
    } else if (strcmp(sub, "enable") == 0) {
        if (argc < 3) {
            term_error("Repository name required.");
            return PU_ERR_USAGE;
        }
        return repo_toggle(cfg_path, argv[2], true);
    } else if (strcmp(sub, "disable") == 0) {
        if (argc < 3) {
            term_error("Repository name required.");
            return PU_ERR_USAGE;
        }
        return repo_toggle(cfg_path, argv[2], false);
    }

    term_error("Unknown repo subcommand: '%s'", sub);
    print_repo_help();
    return PU_ERR_USAGE;
}
