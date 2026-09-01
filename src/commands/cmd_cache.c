#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "fs_utils.h"
#include "alpm_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

static int cache_status(const char *cache_dir) {
    pu_cache_list_t *cache = fs_scan_cache_dir(cache_dir);
    if (!cache) {
        term_error("Failed to read cache directory: %s", cache_dir);
        return PU_ERR_IO;
    }

    char *size_str = fs_format_size(cache->total_size);

    term_header("Pacman Cache Status");
    printf("  %sCache Directory:%s %s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET), cache_dir);
    printf("  %sTotal Packages:%s  %zu\n", term_color(ANSI_BOLD), term_color(ANSI_RESET), cache->count);
    printf("  %sTotal Disk Size:%s %s%s%s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET),
           term_color(ANSI_BOLD_CYAN), size_str ? size_str : "0 B", term_color(ANSI_RESET));

    free(size_str);
    fs_free_cache_list(cache);
    return PU_SUCCESS;
}

static int compare_cache_items(const void *a, const void *b) {
    const pu_cache_pkg_t *pa = a;
    const pu_cache_pkg_t *pb = b;

    int name_cmp = strcmp(pa->pkgname, pb->pkgname);
    if (name_cmp != 0) return name_cmp;

    /* Sort by mtime descending (newest first) */
    if (pa->mtime > pb->mtime) return -1;
    if (pa->mtime < pb->mtime) return 1;
    return 0;
}

static int cache_clean(const char *cache_dir, int keep_versions, bool uninstalled_only, bool dry_run, bool auto_yes) {
    pu_cache_list_t *cache = fs_scan_cache_dir(cache_dir);
    if (!cache) {
        term_error("Failed to read cache directory: %s", cache_dir);
        return PU_ERR_IO;
    }

    if (cache->count == 0) {
        term_info("Cache directory is empty.");
        fs_free_cache_list(cache);
        return PU_SUCCESS;
    }

    /* Sort by package name then date */
    qsort(cache->items, cache->count, sizeof(pu_cache_pkg_t), compare_cache_items);

    pu_alpm_t *alpm = NULL;
    if (uninstalled_only) {
        alpm = alpm_helper_init(NULL, false);
    }

    size_t delete_count = 0;
    off_t reclaimable_bytes = 0;
    bool *to_delete = calloc(cache->count, sizeof(bool));

    char *current_pkg = "";
    int current_version_count = 0;

    for (size_t i = 0; i < cache->count; i++) {
        pu_cache_pkg_t *item = &cache->items[i];

        if (strcmp(item->pkgname, current_pkg) != 0) {
            current_pkg = item->pkgname;
            current_version_count = 0;
        }

        current_version_count++;

        bool should_remove = false;
        if (uninstalled_only) {
            if (alpm && alpm->localdb) {
                alpm_pkg_t *p = alpm_db_get_pkg(alpm->localdb, item->pkgname);
                if (!p) {
                    should_remove = true;
                }
            }
        } else if (current_version_count > keep_versions) {
            should_remove = true;
        }

        if (should_remove) {
            to_delete[i] = true;
            delete_count++;
            reclaimable_bytes += item->size;
        }
    }

    if (alpm) alpm_helper_free(alpm);

    if (delete_count == 0) {
        term_success("Cache is already clean. No files to remove.");
        free(to_delete);
        fs_free_cache_list(cache);
        return PU_SUCCESS;
    }

    char *size_str = fs_format_size(reclaimable_bytes);
    term_header("Cache Cleaning Candidates");
    printf("Found %zu old package tarball(s) to remove (reclaiming %s%s%s):\n\n",
           delete_count, term_color(ANSI_BOLD_GREEN), size_str ? size_str : "0 B", term_color(ANSI_RESET));
    free(size_str);

    for (size_t i = 0; i < cache->count; i++) {
        if (to_delete[i]) {
            char *s = fs_format_size(cache->items[i].size);
            printf("  %s-%s %s (%s)\n",
                   term_color(ANSI_BOLD_RED), term_color(ANSI_RESET),
                   cache->items[i].filename, s ? s : "");
            free(s);
        }
    }
    printf("\n");

    if (dry_run) {
        term_info("Dry-run mode: no files were deleted.");
        free(to_delete);
        fs_free_cache_list(cache);
        return PU_SUCCESS;
    }

    if (!auto_yes && !term_prompt_yes_no("Proceed with deleting these cached packages?", true)) {
        term_info("Operation cancelled.");
        free(to_delete);
        fs_free_cache_list(cache);
        return PU_ERR_CANCELLED;
    }

    size_t deleted_ok = 0;
    char path_buf[4096];
    for (size_t i = 0; i < cache->count; i++) {
        if (to_delete[i]) {
            snprintf(path_buf, sizeof(path_buf), "%s/%s", cache_dir, cache->items[i].filename);
            if (unlink(path_buf) == 0) {
                deleted_ok++;
            }
        }
    }

    /* Clean .part files if any */
    DIR *dir = opendir(cache_dir);
    if (dir) {
        struct dirent *de;
        while ((de = readdir(dir)) != NULL) {
            if (strstr(de->d_name, ".part") != NULL) {
                snprintf(path_buf, sizeof(path_buf), "%s/%s", cache_dir, de->d_name);
                unlink(path_buf);
            }
        }
        closedir(dir);
    }

    free(to_delete);
    fs_free_cache_list(cache);

    term_success("Cleaned %zu cached files successfully.", deleted_ok);
    return PU_SUCCESS;
}

int cmd_cache(int argc, char **argv) {
    const char *cache_dir = DEFAULT_CACHE_DIR;
    int keep_versions = 2;
    bool uninstalled_only = false;
    bool dry_run = g_options.dry_run;
    bool auto_yes = false;

    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils cache <status | clean> [options]\n\n");
        printf("Commands:\n");
        printf("  status                Show cache statistics and size\n");
        printf("  clean                 Remove old versions and clean cache\n\n");
        printf("Clean Options:\n");
        printf("  -k, --keep <N>        Number of recent versions to retain (default: 2)\n");
        printf("  -u, --uninstalled     Remove all cached versions of uninstalled packages\n");
        printf("  -n, --dry-run         Simulate cleanup without deleting\n");
        printf("  -y, --yes             Assume yes to prompt\n");
        return PU_SUCCESS;
    }

    const char *action = argv[1];

    for (int i = 2; i < argc; i++) {
        if ((strcmp(argv[i], "--keep") == 0 || strcmp(argv[i], "-k") == 0) && i + 1 < argc) {
            keep_versions = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--uninstalled") == 0 || strcmp(argv[i], "-u") == 0) {
            uninstalled_only = true;
        } else if (strcmp(argv[i], "--dry-run") == 0 || strcmp(argv[i], "-n") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
            auto_yes = true;
        }
    }

    if (strcmp(action, "status") == 0) {
        return cache_status(cache_dir);
    } else if (strcmp(action, "clean") == 0) {
        return cache_clean(cache_dir, keep_versions, uninstalled_only, dry_run, auto_yes);
    }

    term_error("Unknown cache action '%s'. Use 'status' or 'clean'.", action);
    return PU_ERR_USAGE;
}
