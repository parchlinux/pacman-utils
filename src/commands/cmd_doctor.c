#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <unistd.h>

static bool count_pacnew_cb(const char *path, const char *filename, void *user_data) {
    (void)path;
    (void)filename;
    int *count = user_data;
    (*count)++;
    return true;
}

static void check_disk_space(const char *mount_point, const char *name) {
    struct statvfs vfs;
    if (statvfs(mount_point, &vfs) == 0) {
        off_t free_bytes = (off_t)vfs.f_bavail * vfs.f_frsize;
        off_t total_bytes = (off_t)vfs.f_blocks * vfs.f_frsize;
        double pct_free = total_bytes > 0 ? ((double)free_bytes / (double)total_bytes) * 100.0 : 0;

        char *free_sz = fs_format_size(free_bytes);
        char *tot_sz = fs_format_size(total_bytes);

        if (pct_free < 10.0) {
            printf("  %s[WARNING]%s Low disk space on %s (%s): %s free of %s (%.1f%% free)\n",
                   term_color(ANSI_BOLD_RED), term_color(ANSI_RESET), name, mount_point, free_sz, tot_sz, pct_free);
        } else {
            printf("  %s[OK]%s %s (%s): %s free of %s (%.1f%% free)\n",
                   term_color(ANSI_BOLD_GREEN), term_color(ANSI_RESET), name, mount_point, free_sz, tot_sz, pct_free);
        }

        free(free_sz);
        free(tot_sz);
    }
}

int cmd_doctor(int argc, char **argv) {
    (void)argc;
    (void)argv;

    term_header("Parch Linux Pacman Diagnostics Doctor");
    int issues = 0;

    /* 1. Check pacman lock */
    const char *lck_file = "/var/lib/pacman/db.lck";
    if (fs_file_exists(lck_file)) {
        printf("  %s[FAIL]%s Pacman database lock file exists: %s\n",
               term_color(ANSI_BOLD_RED), term_color(ANSI_RESET), lck_file);
        printf("         If no pacman process is running, remove it with: sudo rm %s\n", lck_file);
        issues++;
    } else {
        printf("  %s[OK]%s No active database locks found.\n",
               term_color(ANSI_BOLD_GREEN), term_color(ANSI_RESET));
    }

    /* 2. Check ALPM initialization & DBs */
    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) {
        printf("  %s[FAIL]%s Unable to initialize libalpm with system configuration.\n",
               term_color(ANSI_BOLD_RED), term_color(ANSI_RESET));
        issues++;
    } else {
        printf("  %s[OK]%s Pacman configuration and local database are readable.\n",
               term_color(ANSI_BOLD_GREEN), term_color(ANSI_RESET));

        /* 3. Check orphan packages */
        pu_pkg_list_t *orphans = alpm_get_orphans(alpm);
        if (orphans && orphans->count > 0) {
            char *sz = fs_format_size(orphans->total_size);
            printf("  %s[WARN]%s Found %zu unneeded orphan packages (%s). Run 'pacman-utils autoremove'.\n",
                   term_color(ANSI_BOLD_YELLOW), term_color(ANSI_RESET), orphans->count, sz ? sz : "");
            free(sz);
        } else {
            printf("  %s[OK]%s No orphan packages detected.\n",
                   term_color(ANSI_BOLD_GREEN), term_color(ANSI_RESET));
        }
        alpm_free_pkg_list(orphans);
        alpm_helper_free(alpm);
    }

    /* 4. Check pacnew / pacsave files */
    int pacnew_count = 0;
    fs_walk_dir("/etc", ".pacnew", count_pacnew_cb, &pacnew_count);
    if (pacnew_count > 0) {
        printf("  %s[WARN]%s Found %d unmerged .pacnew config file(s) in /etc. Run 'pacman-utils diff'.\n",
               term_color(ANSI_BOLD_YELLOW), term_color(ANSI_RESET), pacnew_count);
    } else {
        printf("  %s[OK]%s No unmerged .pacnew configuration files in /etc.\n",
               term_color(ANSI_BOLD_GREEN), term_color(ANSI_RESET));
    }

    /* 5. Check Disk Spaces */
    check_disk_space("/", "Root Partition");
    if (fs_dir_exists("/boot")) check_disk_space("/boot", "Boot Partition");
    if (fs_dir_exists("/var/cache/pacman/pkg")) check_disk_space("/var/cache/pacman/pkg", "Pacman Cache Partition");

    /* 6. Check Cache Size */
    pu_cache_list_t *cache = fs_scan_cache_dir(DEFAULT_CACHE_DIR);
    if (cache) {
        char *csz = fs_format_size(cache->total_size);
        printf("  %s[INFO]%s Cache size is %s (%zu packages). Run 'pacman-utils cache clean' to trim.\n",
               term_color(ANSI_BOLD_CYAN), term_color(ANSI_RESET), csz ? csz : "", cache->count);
        free(csz);
        fs_free_cache_list(cache);
    }

    printf("\n");
    if (issues == 0) {
        term_success("Doctor check finished: System is in great health!");
    } else {
        term_warn("Doctor check finished: Found %d potential issue(s).", issues);
    }

    return PU_SUCCESS;
}
