#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
    char **pacnew_files;
    size_t count;
    size_t capacity;
} pacnew_collector_t;

static bool collect_pacnew_cb(const char *path, const char *filename, void *user_data) {
    (void)filename;
    pacnew_collector_t *col = user_data;
    if (col->count >= col->capacity) {
        col->capacity *= 2;
        char **new_files = realloc(col->pacnew_files, sizeof(char *) * col->capacity);
        if (!new_files) return false;
        col->pacnew_files = new_files;
    }
    col->pacnew_files[col->count++] = strdup(path);
    return true;
}

static void show_file_diff(const char *orig, const char *pacnew) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"diff", "-u", "--color=auto", (char *)orig, (char *)pacnew, NULL};
        execvp("diff", args);
        _exit(1);
    }
    int status;
    waitpid(pid, &status, 0);
}

static void launch_merge_tool(const char *orig, const char *pacnew) {
    const char *tool = getenv("DIFFPROG");
    if (!tool || !*tool) {
        if (access("/usr/bin/vimdiff", X_OK) == 0) tool = "vimdiff";
        else if (access("/usr/bin/nvim", X_OK) == 0) tool = "nvim -d";
        else tool = "diff";
    }

    char cmd[8192];
    snprintf(cmd, sizeof(cmd), "%s '%s' '%s'", tool, orig, pacnew);
    system(cmd);
}

int cmd_diff(int argc, char **argv) {
    (void)argc;
    (void)argv;

    pacnew_collector_t col;
    col.capacity = 16;
    col.count = 0;
    col.pacnew_files = malloc(sizeof(char *) * col.capacity);
    if (!col.pacnew_files) return PU_ERR_GENERAL;

    term_header("Scanning for .pacnew and .pacsave Files");
    fs_walk_dir("/etc", ".pacnew", collect_pacnew_cb, &col);
    fs_walk_dir("/etc", ".pacsave", collect_pacnew_cb, &col);

    if (col.count == 0) {
        term_success("No .pacnew or .pacsave files found in /etc.");
        free(col.pacnew_files);
        return PU_SUCCESS;
    }

    term_info("Found %zu configuration merge candidate(s):", col.count);

    for (size_t i = 0; i < col.count; i++) {
        char *pacnew = col.pacnew_files[i];

        /* Derive original file path by stripping extension */
        char orig[4096];
        snprintf(orig, sizeof(orig), "%s", pacnew);
        char *dot = strrchr(orig, '.');
        if (dot) *dot = '\0';

        printf("\n%s[%zu/%zu]%s %sCandidate:%s %s\n",
               term_color(ANSI_BOLD_CYAN), i + 1, col.count, term_color(ANSI_RESET),
               term_color(ANSI_BOLD), term_color(ANSI_RESET), pacnew);
        printf("       %sOriginal:%s  %s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET), orig);

        bool resolved = false;
        while (!resolved) {
            const char *options[] = {
                "View unified diff",
                "Open interactive merge tool",
                "Replace original file with new version",
                "Delete new file (.pacnew/.pacsave)",
                "Skip this file"
            };

            int choice = term_prompt_menu("Select Action", options, 5);
            if (choice == 0) {
                show_file_diff(orig, pacnew);
            } else if (choice == 1) {
                launch_merge_tool(orig, pacnew);
            } else if (choice == 2) {
                if (fs_atomic_replace(pacnew, orig) == 0) {
                    term_success("Replaced %s with %s (backup kept as %s.bak)", orig, pacnew, orig);
                    resolved = true;
                } else {
                    term_error("Failed to replace file (try with sudo).");
                }
            } else if (choice == 3) {
                if (unlink(pacnew) == 0) {
                    term_success("Deleted %s", pacnew);
                    resolved = true;
                } else {
                    term_error("Failed to delete %s (try with sudo).", pacnew);
                }
            } else {
                term_info("Skipped.");
                resolved = true;
            }
        }

        free(col.pacnew_files[i]);
    }

    free(col.pacnew_files);
    return PU_SUCCESS;
}
