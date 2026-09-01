#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "alpm_helper.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int cmd_autoremove(int argc, char **argv) {
    bool dry_run = g_options.dry_run;
    bool auto_yes = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dry-run") == 0 || strcmp(argv[i], "-n") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0) {
            auto_yes = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pacman-utils autoremove [options]\n\n");
            printf("Finds and removes orphaned packages installed as dependencies that are no longer needed.\n\n");
            printf("Options:\n");
            printf("  -n, --dry-run   Show packages that would be removed without modifying system\n");
            printf("  -y, --yes       Assume yes to removal prompt\n");
            return PU_SUCCESS;
        }
    }

    pu_alpm_t *alpm = alpm_helper_init(NULL, false);
    if (!alpm) return PU_ERR_ALPM;

    pu_pkg_list_t *orphans = alpm_get_orphans(alpm);
    if (!orphans || orphans->count == 0) {
        term_success("No unneeded orphan packages found on the system.");
        alpm_free_pkg_list(orphans);
        alpm_helper_free(alpm);
        return PU_SUCCESS;
    }

    char *size_str = fs_format_size(orphans->total_size);
    term_header("Orphaned Packages to Remove");
    printf("The following %zu package(s) can be safely removed (reclaiming %s%s%s):\n\n",
           orphans->count, term_color(ANSI_BOLD_GREEN), size_str ? size_str : "0 B", term_color(ANSI_RESET));
    free(size_str);

    for (size_t i = 0; i < orphans->count; i++) {
        char *s = fs_format_size(orphans->items[i].isize);
        printf("  %s●%s %s%-24s%s %s%-16s%s %s%8s%s\n",
               term_color(ANSI_BOLD_RED), term_color(ANSI_RESET),
               term_color(ANSI_BOLD), orphans->items[i].name, term_color(ANSI_RESET),
               term_color(ANSI_DIM), orphans->items[i].version, term_color(ANSI_RESET),
               term_color(ANSI_CYAN), s ? s : "", term_color(ANSI_RESET));
        free(s);
    }
    printf("\n");

    if (dry_run) {
        term_info("Dry-run mode: no packages were removed.");
        alpm_free_pkg_list(orphans);
        alpm_helper_free(alpm);
        return PU_SUCCESS;
    }

    if (!auto_yes && !term_prompt_yes_no("Proceed with removing these orphan packages?", true)) {
        term_info("Removal aborted.");
        alpm_free_pkg_list(orphans);
        alpm_helper_free(alpm);
        return PU_ERR_CANCELLED;
    }

    /* Free ALPM ctx before invoking pacman to release db locks */
    size_t count = orphans->count;
    char **pkg_names = malloc(sizeof(char *) * count);
    for (size_t i = 0; i < count; i++) {
        pkg_names[i] = strdup(orphans->items[i].name);
    }

    alpm_free_pkg_list(orphans);
    alpm_helper_free(alpm);

    /* Construct pacman -Rns call */
    char **cmd_args = malloc(sizeof(char *) * (count + 4));
    cmd_args[0] = "pacman";
    cmd_args[1] = "-Rns";
    for (size_t i = 0; i < count; i++) {
        cmd_args[i + 2] = pkg_names[i];
    }
    cmd_args[count + 2] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        execvp("pacman", cmd_args);
        perror("execvp pacman");
        _exit(1);
    }

    int status;
    waitpid(pid, &status, 0);

    for (size_t i = 0; i < count; i++) {
        free(pkg_names[i]);
    }
    free(pkg_names);
    free(cmd_args);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        term_success("Autoremove completed successfully.");
        return PU_SUCCESS;
    }

    return PU_ERR_GENERAL;
}
