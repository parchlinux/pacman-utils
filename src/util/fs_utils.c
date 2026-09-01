#include "fs_utils.h"
#include "pacman_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

char *fs_format_size(off_t bytes) {
    char *result = malloc(64);
    if (!result) return NULL;

    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    int unit_idx = 0;
    double size = (double)bytes;

    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        unit_idx++;
    }

    if (unit_idx == 0) {
        snprintf(result, 64, "%ld B", (long)bytes);
    } else {
        snprintf(result, 64, "%.2f %s", size, units[unit_idx]);
    }
    return result;
}

bool fs_file_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

bool fs_dir_exists(const char *path) {
    if (!path) return false;
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int fs_create_dir_p(const char *path, mode_t mode) {
    if (!path) return -1;
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", path);
    size_t len = strlen(temp);

    if (temp[len - 1] == '/') {
        temp[len - 1] = '\0';
    }

    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp, mode) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(temp, mode) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

int fs_copy_file(const char *src, const char *dst) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -1;

    struct stat st;
    if (fstat(sfd, &st) != 0) {
        close(sfd);
        return -1;
    }

    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dfd < 0) {
        close(sfd);
        return -1;
    }

    char buf[65536];
    ssize_t bytes;
    while ((bytes = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < bytes) {
            ssize_t res = write(dfd, buf + written, bytes - written);
            if (res < 0) {
                close(sfd);
                close(dfd);
                return -1;
            }
            written += res;
        }
    }

    close(sfd);
    close(dfd);
    return 0;
}

int fs_atomic_replace(const char *temp_file, const char *target_file) {
    if (fs_file_exists(target_file)) {
        char bak_path[1024];
        snprintf(bak_path, sizeof(bak_path), "%s.bak", target_file);
        fs_copy_file(target_file, bak_path);
    }

    if (rename(temp_file, target_file) != 0) {
        /* If rename fails across filesystems, fallback to copy and delete */
        if (errno == EXDEV) {
            if (fs_copy_file(temp_file, target_file) == 0) {
                unlink(temp_file);
                return 0;
            }
        }
        return -1;
    }
    return 0;
}

bool fs_parse_pkg_filename(const char *filename, char **pkgname, char **pkgver, char **arch) {
    if (!filename || !pkgname || !pkgver || !arch) return false;

    /* Check for .pkg.tar.* extension */
    const char *ext = strstr(filename, ".pkg.tar.");
    if (!ext) return false;

    size_t base_len = ext - filename;
    char *base = strndup(filename, base_len);
    if (!base) return false;

    /* Base is formatted as: <pkgname>-<version>-<pkgrel>-<arch> */
    char *last_dash = strrchr(base, '-');
    if (!last_dash) {
        free(base);
        return false;
    }
    *arch = strdup(last_dash + 1);
    *last_dash = '\0';

    /* Now find pkgrel */
    char *pkgrel_dash = strrchr(base, '-');
    if (!pkgrel_dash) {
        free(*arch);
        free(base);
        return false;
    }
    *pkgrel_dash = '\0';
    char *pkgrel = pkgrel_dash + 1;

    /* Now find version dash */
    char *ver_dash = strrchr(base, '-');
    if (!ver_dash) {
        free(*arch);
        free(base);
        return false;
    }
    *ver_dash = '\0';
    char *ver_str = ver_dash + 1;

    char full_ver[256];
    snprintf(full_ver, sizeof(full_ver), "%s-%s", ver_str, pkgrel);
    *pkgver = strdup(full_ver);
    *pkgname = strdup(base);

    free(base);
    return true;
}

pu_cache_list_t *fs_scan_cache_dir(const char *cache_dir) {
    DIR *dir = opendir(cache_dir);
    if (!dir) return NULL;

    pu_cache_list_t *list = calloc(1, sizeof(pu_cache_list_t));
    if (!list) {
        closedir(dir);
        return NULL;
    }

    list->capacity = 128;
    list->items = malloc(sizeof(pu_cache_pkg_t) * list->capacity);
    if (!list->items) {
        free(list);
        closedir(dir);
        return NULL;
    }

    struct dirent *de;
    char full_path[4096];

    while ((de = readdir(dir)) != NULL) {
        if (de->d_name[0] == '.') continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", cache_dir, de->d_name);
        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        char *pkgname = NULL;
        char *pkgver = NULL;
        char *arch = NULL;

        if (fs_parse_pkg_filename(de->d_name, &pkgname, &pkgver, &arch)) {
            if (list->count >= list->capacity) {
                list->capacity *= 2;
                pu_cache_pkg_t *new_items = realloc(list->items, sizeof(pu_cache_pkg_t) * list->capacity);
                if (!new_items) {
                    free(pkgname);
                    free(pkgver);
                    free(arch);
                    break;
                }
                list->items = new_items;
            }

            pu_cache_pkg_t *item = &list->items[list->count++];
            item->filename = strdup(de->d_name);
            item->pkgname = pkgname;
            item->pkgver = pkgver;
            item->arch = arch;
            item->size = st.st_size;
            item->mtime = st.st_mtime;
            list->total_size += st.st_size;
        }
    }

    closedir(dir);
    return list;
}

void fs_free_cache_list(pu_cache_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].filename);
        free(list->items[i].pkgname);
        free(list->items[i].pkgver);
        free(list->items[i].arch);
    }
    free(list->items);
    free(list);
}

int fs_walk_dir(const char *base_path, const char *pattern, fs_file_cb callback, void *user_data) {
    DIR *dir = opendir(base_path);
    if (!dir) return -1;

    struct dirent *de;
    char path_buf[4096];

    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        snprintf(path_buf, sizeof(path_buf), "%s/%s", base_path, de->d_name);
        struct stat st;
        if (lstat(path_buf, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            fs_walk_dir(path_buf, pattern, callback, user_data);
        } else if (S_ISREG(st.st_mode)) {
            if (pattern == NULL || strstr(de->d_name, pattern) != NULL) {
                if (!callback(path_buf, de->d_name, user_data)) {
                    closedir(dir);
                    return 0;
                }
            }
        }
    }

    closedir(dir);
    return 0;
}
