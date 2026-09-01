#ifndef FS_UTILS_H
#define FS_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* File search callback: return true to continue, false to stop */
typedef bool (*fs_file_cb)(const char *path, const char *filename, void *user_data);

char *fs_format_size(off_t bytes);
bool fs_file_exists(const char *path);
bool fs_dir_exists(const char *path);
int fs_create_dir_p(const char *path, mode_t mode);
int fs_copy_file(const char *src, const char *dst);
int fs_atomic_replace(const char *temp_file, const char *target_file);

/* Cache utilities */
typedef struct {
    char *filename;
    char *pkgname;
    char *pkgver;
    char *arch;
    off_t size;
    time_t mtime;
} pu_cache_pkg_t;

typedef struct {
    pu_cache_pkg_t *items;
    size_t count;
    size_t capacity;
    off_t total_size;
} pu_cache_list_t;

bool fs_parse_pkg_filename(const char *filename, char **pkgname, char **pkgver, char **arch);
pu_cache_list_t *fs_scan_cache_dir(const char *cache_dir);
void fs_free_cache_list(pu_cache_list_t *list);

/* Recursive directory walker */
int fs_walk_dir(const char *base_path, const char *pattern, fs_file_cb callback, void *user_data);

#endif /* FS_UTILS_H */
