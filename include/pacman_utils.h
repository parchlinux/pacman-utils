#ifndef PACMAN_UTILS_H
#define PACMAN_UTILS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#define PACMAN_UTILS_VERSION "1.0.0"
#define DEFAULT_PACMAN_CONF "/etc/pacman.conf"
#define DEFAULT_PACMAN_DIR  "/etc/pacman.d"
#define DEFAULT_CACHE_DIR   "/var/cache/pacman/pkg"
#define DEFAULT_DBPATH      "/var/lib/pacman"
#define DEFAULT_LOG_FILE    "/var/log/pacman.log"
#define DEFAULT_ROOT_DIR    "/"

#define PARCH_BANNER "Parch Linux Pacman Utilities"

typedef enum {
    PU_SUCCESS = 0,
    PU_ERR_GENERAL = 1,
    PU_ERR_USAGE = 2,
    PU_ERR_PERM = 3,
    PU_ERR_CONFIG = 4,
    PU_ERR_ALPM = 5,
    PU_ERR_NOT_FOUND = 6,
    PU_ERR_IO = 7,
    PU_ERR_CANCELLED = 8
} pu_error_t;

/* Global CLI options */
typedef struct {
    bool color_enabled;
    bool verbose;
    bool quiet;
    bool dry_run;
    const char *config_path;
    const char *root_dir;
    const char *db_path;
} pu_global_options_t;

extern pu_global_options_t g_options;

#endif /* PACMAN_UTILS_H */
