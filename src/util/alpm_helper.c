#include "alpm_helper.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "fs_utils.h"
#include "config_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

static void add_servers_from_file(alpm_db_t *db, const char *mirrorfile) {
    FILE *fp = fopen(mirrorfile, "r");
    if (!fp) return;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        if (strncmp(p, "Server", 6) == 0) {
            char *eq = strchr(p, '=');
            if (eq) {
                char *url = eq + 1;
                while (*url == ' ' || *url == '\t') url++;
                char *end = url + strlen(url) - 1;
                while (end > url && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
                    *end = '\0';
                    end--;
                }
                if (*url) {
                    alpm_db_add_server(db, url);
                }
            }
        }
    }
    fclose(fp);
}

static void configure_syncdbs(pu_alpm_t *ctx, pu_config_t *cfg) {
    if (!ctx || !cfg) return;

    for (pu_config_section_t *sec = cfg->sections; sec; sec = sec->next) {
        if (sec->is_commented || strcmp(sec->name, "options") == 0 || strcmp(sec->name, "global") == 0) {
            continue;
        }

        alpm_db_t *db = alpm_register_syncdb(ctx->handle, sec->name, ALPM_SIG_DATABASE_OPTIONAL);
        if (!db) continue;

        for (pu_config_entry_t *e = sec->entries; e; e = e->next) {
            if (e->is_commented || !e->key) continue;

            if (strcasecmp(e->key, "Server") == 0 && e->value) {
                alpm_db_add_server(db, e->value);
            } else if (strcasecmp(e->key, "Include") == 0 && e->value) {
                add_servers_from_file(db, e->value);
            }
        }
    }
}

pu_alpm_t *alpm_helper_init(const char *config_path, bool use_temp_db) {
    pu_alpm_t *ctx = calloc(1, sizeof(pu_alpm_t));
    if (!ctx) return NULL;

    const char *cfg_file = config_path ? config_path : (g_options.config_path ? g_options.config_path : DEFAULT_PACMAN_CONF);
    const char *root_dir = g_options.root_dir ? g_options.root_dir : DEFAULT_ROOT_DIR;
    const char *db_path = g_options.db_path ? g_options.db_path : DEFAULT_DBPATH;

    ctx->is_temp = use_temp_db;
    if (use_temp_db) {
        char temp_dir[] = "/tmp/pacman-utils-db-XXXXXX";
        if (mkdtemp(temp_dir) == NULL) {
            term_error("Failed to create temporary ALPM dbpath: %s", strerror(errno));
            free(ctx);
            return NULL;
        }
        ctx->temp_dbpath = strdup(temp_dir);

        /* Copy local DB if exists so it has knowledge of installed packages */
        char local_src[1024];
        char local_dst[1024];
        snprintf(local_src, sizeof(local_src), "%s/local", db_path);
        snprintf(local_dst, sizeof(local_dst), "%s/local", temp_dir);
        mkdir(local_dst, 0755);

        db_path = ctx->temp_dbpath;
    }

    alpm_errno_t err;
    ctx->handle = alpm_initialize(root_dir, db_path, &err);
    if (!ctx->handle) {
        term_error("Failed to initialize libalpm: %s", alpm_strerror(err));
        if (ctx->temp_dbpath) {
            free(ctx->temp_dbpath);
        }
        free(ctx);
        return NULL;
    }

    pu_config_t *cfg = config_parse_file(cfg_file);
    if (cfg) {
        configure_syncdbs(ctx, cfg);
        config_free(cfg);
    }

    ctx->localdb = alpm_get_localdb(ctx->handle);
    ctx->syncdbs = alpm_get_syncdbs(ctx->handle);

    return ctx;
}

void alpm_helper_free(pu_alpm_t *ctx) {
    if (!ctx) return;
    if (ctx->handle) {
        alpm_release(ctx->handle);
    }
    if (ctx->temp_dbpath) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", ctx->temp_dbpath);
        system(cmd);
        free(ctx->temp_dbpath);
    }
    free(ctx);
}

pu_pkg_list_t *alpm_get_orphans(pu_alpm_t *ctx) {
    if (!ctx || !ctx->localdb) return NULL;

    pu_pkg_list_t *list = calloc(1, sizeof(pu_pkg_list_t));
    if (!list) return NULL;

    size_t cap = 32;
    list->items = calloc(cap, sizeof(pu_pkg_info_t));
    if (!list->items) {
        free(list);
        return NULL;
    }

    alpm_list_t *pkgcache = alpm_db_get_pkgcache(ctx->localdb);
    for (alpm_list_t *i = pkgcache; i; i = alpm_list_next(i)) {
        alpm_pkg_t *pkg = i->data;
        if (alpm_pkg_get_reason(pkg) == ALPM_PKG_REASON_DEPEND) {
            alpm_list_t *reqby = alpm_pkg_compute_requiredby(pkg);
            alpm_list_t *optfor = alpm_pkg_compute_optionalfor(pkg);

            if (!reqby && !optfor) {
                if (list->count >= cap) {
                    size_t new_cap = cap * 2;
                    pu_pkg_info_t *new_items = realloc(list->items, sizeof(pu_pkg_info_t) * new_cap);
                    if (!new_items) {
                        alpm_list_free_inner(reqby, free);
                        alpm_list_free(reqby);
                        alpm_list_free_inner(optfor, free);
                        alpm_list_free(optfor);
                        break;
                    }
                    memset(new_items + cap, 0, sizeof(pu_pkg_info_t) * (new_cap - cap));
                    list->items = new_items;
                    cap = new_cap;
                }

                pu_pkg_info_t *item = &list->items[list->count++];
                item->name = strdup(alpm_pkg_get_name(pkg));
                item->version = strdup(alpm_pkg_get_version(pkg));
                const char *desc = alpm_pkg_get_desc(pkg);
                item->description = desc ? strdup(desc) : strdup("");
                item->repo = NULL;
                item->isize = alpm_pkg_get_isize(pkg);
                item->reason = ALPM_PKG_REASON_DEPEND;
                list->total_size += item->isize;
            }

            if (reqby) {
                alpm_list_free_inner(reqby, free);
                alpm_list_free(reqby);
            }
            if (optfor) {
                alpm_list_free_inner(optfor, free);
                alpm_list_free(optfor);
            }
        }
    }

    return list;
}

void alpm_free_pkg_list(pu_pkg_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].name);
        free(list->items[i].version);
        free(list->items[i].description);
        free(list->items[i].repo);
    }
    free(list->items);
    free(list);
}

pu_pkg_list_t *alpm_get_installed_pkgs(pu_alpm_t *ctx) {
    if (!ctx || !ctx->localdb) return NULL;

    pu_pkg_list_t *list = calloc(1, sizeof(pu_pkg_list_t));
    if (!list) return NULL;

    alpm_list_t *pkgcache = alpm_db_get_pkgcache(ctx->localdb);
    size_t count = alpm_list_count(pkgcache);

    list->items = calloc(count > 0 ? count : 1, sizeof(pu_pkg_info_t));
    if (!list->items) {
        free(list);
        return NULL;
    }

    for (alpm_list_t *i = pkgcache; i; i = alpm_list_next(i)) {
        alpm_pkg_t *pkg = i->data;
        pu_pkg_info_t *item = &list->items[list->count++];
        item->name = strdup(alpm_pkg_get_name(pkg));
        item->version = strdup(alpm_pkg_get_version(pkg));
        const char *desc = alpm_pkg_get_desc(pkg);
        item->description = desc ? strdup(desc) : strdup("");
        item->repo = NULL;
        item->isize = alpm_pkg_get_isize(pkg);
        item->reason = alpm_pkg_get_reason(pkg);
        list->total_size += item->isize;
    }

    return list;
}

pu_upgrade_list_t *alpm_get_upgrades(pu_alpm_t *ctx) {
    if (!ctx || !ctx->localdb || !ctx->syncdbs) return NULL;

    pu_upgrade_list_t *list = calloc(1, sizeof(pu_upgrade_list_t));
    if (!list) return NULL;

    size_t cap = 32;
    list->items = calloc(cap, sizeof(pu_upgrade_info_t));
    if (!list->items) {
        free(list);
        return NULL;
    }

    alpm_list_t *pkgcache = alpm_db_get_pkgcache(ctx->localdb);
    for (alpm_list_t *i = pkgcache; i; i = alpm_list_next(i)) {
        alpm_pkg_t *local_pkg = i->data;
        const char *name = alpm_pkg_get_name(local_pkg);

        alpm_pkg_t *candidate = NULL;
        alpm_db_t *cand_db = NULL;

        for (alpm_list_t *s = ctx->syncdbs; s; s = alpm_list_next(s)) {
            alpm_db_t *sdb = s->data;
            alpm_pkg_t *spkg = alpm_db_get_pkg(sdb, name);
            if (spkg) {
                if (!candidate || alpm_pkg_vercmp(alpm_pkg_get_version(spkg), alpm_pkg_get_version(candidate)) > 0) {
                    candidate = spkg;
                    cand_db = sdb;
                }
            }
        }

        if (candidate && alpm_pkg_vercmp(alpm_pkg_get_version(candidate), alpm_pkg_get_version(local_pkg)) > 0) {
            if (list->count >= cap) {
                size_t new_cap = cap * 2;
                pu_upgrade_info_t *new_items = realloc(list->items, sizeof(pu_upgrade_info_t) * new_cap);
                if (!new_items) break;
                memset(new_items + cap, 0, sizeof(pu_upgrade_info_t) * (new_cap - cap));
                list->items = new_items;
                cap = new_cap;
            }

            pu_upgrade_info_t *item = &list->items[list->count++];
            item->pkgname = strdup(name);
            item->oldversion = strdup(alpm_pkg_get_version(local_pkg));
            item->newversion = strdup(alpm_pkg_get_version(candidate));
            item->repo = cand_db ? strdup(alpm_db_get_name(cand_db)) : strdup("unknown");
            item->download_size = alpm_pkg_get_size(candidate);
            list->total_download_size += item->download_size;
        }
    }

    return list;
}

void alpm_free_upgrade_list(pu_upgrade_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].pkgname);
        free(list->items[i].oldversion);
        free(list->items[i].newversion);
        free(list->items[i].repo);
    }
    free(list->items);
    free(list);
}

/* Recursive helper to trace dependency chain to an explicit package */
static bool trace_why_rec(pu_alpm_t *ctx, const char *pkgname, const char **visited, int depth) {
    if (depth > 20) return false;

    alpm_pkg_t *pkg = alpm_db_get_pkg(ctx->localdb, pkgname);
    if (!pkg) return false;

    if (alpm_pkg_get_reason(pkg) == ALPM_PKG_REASON_EXPLICIT) {
        printf("%s%s[Explicit]%s %s (%s)\n",
               term_color(ANSI_BOLD_GREEN), term_color(ANSI_BOLD), term_color(ANSI_RESET),
               alpm_pkg_get_name(pkg), alpm_pkg_get_version(pkg));
        return true;
    }

    alpm_list_t *reqby = alpm_pkg_compute_requiredby(pkg);
    if (!reqby) {
        printf("%s[Orphan / Not Required]%s %s\n",
               term_color(ANSI_BOLD_YELLOW), term_color(ANSI_RESET), pkgname);
        return false;
    }

    bool found = false;
    for (alpm_list_t *i = reqby; i; i = alpm_list_next(i)) {
        const char *parent_name = i->data;
        for (int d = 0; d < depth; d++) printf("  ");
        printf("└─ required by %s%s%s\n", term_color(ANSI_BOLD_CYAN), parent_name, term_color(ANSI_RESET));

        if (trace_why_rec(ctx, parent_name, visited, depth + 1)) {
            found = true;
            break;
        }
    }

    alpm_list_free_inner(reqby, free);
    alpm_list_free(reqby);
    return found;
}

int alpm_explain_why(pu_alpm_t *ctx, const char *pkgname) {
    if (!ctx || !pkgname) return PU_ERR_USAGE;

    alpm_pkg_t *pkg = alpm_db_get_pkg(ctx->localdb, pkgname);
    if (!pkg) {
        term_error("Package '%s' is not currently installed.", pkgname);
        return PU_ERR_NOT_FOUND;
    }

    term_header("Dependency Reason");
    alpm_pkgreason_t reason = alpm_pkg_get_reason(pkg);
    if (reason == ALPM_PKG_REASON_EXPLICIT) {
        term_success("Package '%s' was explicitly installed by user.", pkgname);
        return PU_SUCCESS;
    }

    term_info("Package '%s' was installed as a dependency. Tracing chain:", pkgname);
    const char *visited[64] = {0};
    trace_why_rec(ctx, pkgname, visited, 0);

    return PU_SUCCESS;
}

static void print_tree_rec(pu_alpm_t *ctx, const char *pkgname, bool reverse, int depth, int max_depth) {
    if (depth >= max_depth) return;

    alpm_pkg_t *pkg = alpm_db_get_pkg(ctx->localdb, pkgname);
    if (!pkg) return;

    for (int i = 0; i < depth; i++) {
        printf("│  ");
    }

    const char *color = (alpm_pkg_get_reason(pkg) == ALPM_PKG_REASON_EXPLICIT) ? ANSI_BOLD_GREEN : ANSI_CYAN;
    printf("├── %s%s%s (%s)\n", term_color(color), alpm_pkg_get_name(pkg), term_color(ANSI_RESET), alpm_pkg_get_version(pkg));

    if (reverse) {
        alpm_list_t *reqby = alpm_pkg_compute_requiredby(pkg);
        for (alpm_list_t *i = reqby; i; i = alpm_list_next(i)) {
            print_tree_rec(ctx, i->data, reverse, depth + 1, max_depth);
        }
        if (reqby) {
            alpm_list_free_inner(reqby, free);
            alpm_list_free(reqby);
        }
    } else {
        alpm_list_t *deps = alpm_pkg_get_depends(pkg);
        for (alpm_list_t *i = deps; i; i = alpm_list_next(i)) {
            alpm_depend_t *dep = i->data;
            alpm_pkg_t *dep_pkg = alpm_find_satisfier(alpm_db_get_pkgcache(ctx->localdb), dep->name);
            if (dep_pkg) {
                print_tree_rec(ctx, alpm_pkg_get_name(dep_pkg), reverse, depth + 1, max_depth);
            }
        }
    }
}

int alpm_print_deptree(pu_alpm_t *ctx, const char *pkgname, bool reverse, int max_depth) {
    if (!ctx || !pkgname) return PU_ERR_USAGE;

    alpm_pkg_t *pkg = alpm_db_get_pkg(ctx->localdb, pkgname);
    if (!pkg) {
        term_error("Package '%s' is not installed.", pkgname);
        return PU_ERR_NOT_FOUND;
    }

    term_header(reverse ? "Reverse Dependency Tree" : "Dependency Tree");
    print_tree_rec(ctx, pkgname, reverse, 0, max_depth > 0 ? max_depth : 10);
    return PU_SUCCESS;
}

int alpm_search_file(pu_alpm_t *ctx, const char *search_term) {
    if (!ctx || !search_term) return PU_ERR_USAGE;

    term_header("File Search Results");
    bool found = false;

    /* Search local installed database */
    alpm_list_t *pkgcache = alpm_db_get_pkgcache(ctx->localdb);
    for (alpm_list_t *i = pkgcache; i; i = alpm_list_next(i)) {
        alpm_pkg_t *pkg = i->data;
        alpm_filelist_t *files = alpm_pkg_get_files(pkg);
        if (!files) continue;

        for (size_t f = 0; f < files->count; f++) {
            if (strstr(files->files[f].name, search_term) != NULL) {
                printf("%s[installed]%s %s%s%s -> /%s\n",
                       term_color(ANSI_BOLD_GREEN), term_color(ANSI_RESET),
                       term_color(ANSI_BOLD_CYAN), alpm_pkg_get_name(pkg), term_color(ANSI_RESET),
                       files->files[f].name);
                found = true;
            }
        }
    }

    if (!found) {
        term_info("No installed package provides a file matching '%s'.", search_term);
    }

    return PU_SUCCESS;
}

int alpm_verify_package(pu_alpm_t *ctx, const char *pkgname, pu_verify_result_t *res) {
    if (!ctx || !pkgname || !res) return PU_ERR_USAGE;
    memset(res, 0, sizeof(pu_verify_result_t));

    alpm_pkg_t *pkg = alpm_db_get_pkg(ctx->localdb, pkgname);
    if (!pkg) return PU_ERR_NOT_FOUND;

    alpm_filelist_t *files = alpm_pkg_get_files(pkg);
    if (!files) return PU_SUCCESS;

    char full_path[4096];
    const char *root = alpm_option_get_root(ctx->handle);

    for (size_t i = 0; i < files->count; i++) {
        res->total_files++;
        snprintf(full_path, sizeof(full_path), "%s%s", root, files->files[i].name);

        struct stat st;
        if (lstat(full_path, &st) != 0) {
            res->missing_files++;
            printf("%s[MISSING]%s %s: /%s\n",
                   term_color(ANSI_BOLD_RED), term_color(ANSI_RESET),
                   pkgname, files->files[i].name);
        }
    }

    return PU_SUCCESS;
}

int alpm_verify_all(pu_alpm_t *ctx) {
    if (!ctx || !ctx->localdb) return PU_ERR_ALPM;

    term_header("Verifying Installed Packages Integrity");
    alpm_list_t *pkgcache = alpm_db_get_pkgcache(ctx->localdb);

    int total_pkgs = 0;
    int corrupt_pkgs = 0;

    for (alpm_list_t *i = pkgcache; i; i = alpm_list_next(i)) {
        alpm_pkg_t *pkg = i->data;
        total_pkgs++;
        pu_verify_result_t res;
        alpm_verify_package(ctx, alpm_pkg_get_name(pkg), &res);
        if (res.missing_files > 0) {
            corrupt_pkgs++;
        }
    }

    if (corrupt_pkgs == 0) {
        term_success("All %d packages verified successfully. No missing files detected.", total_pkgs);
    } else {
        term_warn("Verification finished: %d of %d packages had missing or corrupt files.", corrupt_pkgs, total_pkgs);
    }

    return PU_SUCCESS;
}

int alpm_mark_pkg_reason(pu_alpm_t *ctx, const char *pkgname, alpm_pkgreason_t reason) {
    if (!ctx || !pkgname) return PU_ERR_USAGE;

    alpm_pkg_t *pkg = alpm_db_get_pkg(ctx->localdb, pkgname);
    if (!pkg) {
        term_error("Package '%s' is not installed.", pkgname);
        return PU_ERR_NOT_FOUND;
    }

    if (alpm_pkg_get_reason(pkg) == reason) {
        term_info("Package '%s' is already marked as %s.", pkgname,
                  reason == ALPM_PKG_REASON_EXPLICIT ? "explicit" : "dependency");
        return PU_SUCCESS;
    }

    if (alpm_pkg_set_reason(pkg, reason) != 0) {
        term_error("Failed to update reason for '%s': %s", pkgname, alpm_strerror(alpm_errno(ctx->handle)));
        return PU_ERR_PERM;
    }

    term_success("Package '%s' marked as %s.", pkgname,
                 reason == ALPM_PKG_REASON_EXPLICIT ? "explicit" : "dependency");
    return PU_SUCCESS;
}
