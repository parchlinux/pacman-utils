#ifndef ALPM_HELPER_H
#define ALPM_HELPER_H

#include <alpm.h>
#include <stdbool.h>

typedef struct {
    alpm_handle_t *handle;
    alpm_db_t *localdb;
    alpm_list_t *syncdbs;
    char *temp_dbpath;
    bool is_temp;
} pu_alpm_t;

typedef struct {
    char *name;
    char *version;
    char *description;
    char *repo;
    off_t isize;
    alpm_pkgreason_t reason;
} pu_pkg_info_t;

typedef struct {
    pu_pkg_info_t *items;
    size_t count;
    off_t total_size;
} pu_pkg_list_t;

typedef struct {
    char *pkgname;
    char *oldversion;
    char *newversion;
    char *repo;
    off_t download_size;
} pu_upgrade_info_t;

typedef struct {
    pu_upgrade_info_t *items;
    size_t count;
    off_t total_download_size;
} pu_upgrade_list_t;

/* Lifecycle */
pu_alpm_t *alpm_helper_init(const char *config_path, bool use_temp_db);
void alpm_helper_free(pu_alpm_t *ctx);

/* Queries */
pu_pkg_list_t *alpm_get_orphans(pu_alpm_t *ctx);
void alpm_free_pkg_list(pu_pkg_list_t *list);

pu_pkg_list_t *alpm_get_installed_pkgs(pu_alpm_t *ctx);
pu_upgrade_list_t *alpm_get_upgrades(pu_alpm_t *ctx);
void alpm_free_upgrade_list(pu_upgrade_list_t *list);

/* Dependency explanation & tree */
int alpm_explain_why(pu_alpm_t *ctx, const char *pkgname);
int alpm_print_deptree(pu_alpm_t *ctx, const char *pkgname, bool reverse, int max_depth);

/* File search */
int alpm_search_file(pu_alpm_t *ctx, const char *search_term);

/* Verification */
typedef struct {
    int missing_files;
    int modified_files;
    int perm_mismatches;
    int total_files;
} pu_verify_result_t;

int alpm_verify_package(pu_alpm_t *ctx, const char *pkgname, pu_verify_result_t *res);
int alpm_verify_all(pu_alpm_t *ctx);

/* Mark packages */
int alpm_mark_pkg_reason(pu_alpm_t *ctx, const char *pkgname, alpm_pkgreason_t reason);

#endif /* ALPM_HELPER_H */
