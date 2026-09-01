#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "editor.h"
#include "config_parser.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static bool validate_config(const char *temp_file, char **error_out) {
    return config_validate_syntax(temp_file, error_out);
}

typedef struct {
    char *label;
    char *path;
} source_file_item_t;

static void add_source_item(source_file_item_t *items, int *count, int max_items, const char *label, const char *path) {
    if (*count >= max_items || !path) return;

    /* Check if path already added */
    for (int i = 0; i < *count; i++) {
        if (strcmp(items[i].path, path) == 0) {
            return;
        }
    }

    items[*count].label = strdup(label);
    items[*count].path = strdup(path);
    (*count)++;
}

int cmd_edit_sources(int argc, char **argv) {
    char *target = NULL;
    bool should_free_target = false;
    const char *cfg_path = g_options.config_path ? g_options.config_path : DEFAULT_PACMAN_CONF;

    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: pacu edit-sources [pacman.conf | mirrorlist | parch-mirrors | <filepath>]\n\n");
            printf("Safely edits pacman configuration or mirrorlist files in your preferred editor.\n");
            return PU_SUCCESS;
        }

        if (strcmp(argv[1], "pacman.conf") == 0 || strcmp(argv[1], "config") == 0) {
            target = (char *)cfg_path;
        } else if (strcmp(argv[1], "mirrorlist") == 0 || strcmp(argv[1], "mirrors") == 0) {
            target = "/etc/pacman.d/mirrorlist";
        } else if (strcmp(argv[1], "parch") == 0 || strcmp(argv[1], "parch-mirrors") == 0) {
            target = "/etc/pacman.d/parch-mirrors";
        } else if (strcmp(argv[1], "chaotic") == 0 || strcmp(argv[1], "chaotic-mirrorlist") == 0) {
            target = "/etc/pacman.d/chaotic-mirrorlist";
        } else {
            target = argv[1];
        }
    }

    if (!target) {
        /* Dynamically discover all configured source and mirror files */
        source_file_item_t items[32];
        int count = 0;

        /* 1. Main configuration */
        add_source_item(items, &count, 32, "Main Pacman Configuration (/etc/pacman.conf)", cfg_path);

        /* 2. Dynamically parse pacman.conf for all 'Include = ...' directives in repos */
        pu_config_t *cfg = config_parse_file(cfg_path);
        if (cfg) {
            for (pu_config_section_t *sec = cfg->sections; sec; sec = sec->next) {
                for (pu_config_entry_t *e = sec->entries; e; e = e->next) {
                    if (e->type == ENTRY_LINE_KEYVALUE && e->key && strcasecmp(e->key, "Include") == 0 && e->value) {
                        if (fs_file_exists(e->value)) {
                            char label[2048];
                            snprintf(label, sizeof(label), "[%s] Mirrorlist (%s)", sec->name, e->value);
                            add_source_item(items, &count, 32, label, e->value);
                        }
                    }
                }
            }
            config_free(cfg);
        }

        /* 3. Scan /etc/pacman.d/ for any additional mirrorlist or config files */
        DIR *dir = opendir(DEFAULT_PACMAN_DIR);
        if (dir) {
            struct dirent *de;
            char full_p[1024];
            while ((de = readdir(dir)) != NULL) {
                if (de->d_name[0] == '.') continue;
                if (strstr(de->d_name, ".pacnew") || strstr(de->d_name, ".pacsave") || strstr(de->d_name, ".bak")) {
                    continue;
                }

                snprintf(full_p, sizeof(full_p), "%s/%s", DEFAULT_PACMAN_DIR, de->d_name);
                if (fs_file_exists(full_p)) {
                    char label[2048];
                    snprintf(label, sizeof(label), "%s (%s)", de->d_name, full_p);
                    add_source_item(items, &count, 32, label, full_p);
                }
            }
            closedir(dir);
        }

        const char *menu_labels[32];
        for (int i = 0; i < count; i++) {
            menu_labels[i] = items[i].label;
        }

        int choice = term_prompt_menu("Select Sources File to Edit", menu_labels, count);
        if (choice < 0 || choice >= count) {
            term_info("Cancelled.");
            for (int i = 0; i < count; i++) {
                free(items[i].label);
                free(items[i].path);
            }
            return PU_ERR_CANCELLED;
        }

        target = strdup(items[choice].path);
        should_free_target = true;

        for (int i = 0; i < count; i++) {
            free(items[i].label);
            free(items[i].path);
        }
    }

    /* Check if target is a pacman config file (to apply syntax validation) */
    editor_validator_fn val = NULL;
    if (strstr(target, "pacman.conf") != NULL) {
        val = validate_config;
    }

    int ret = editor_safe_edit(target, val);
    if (should_free_target) {
        free(target);
    }
    return ret;
}
