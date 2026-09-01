#include "editor.h"
#include "pacman_utils.h"
#include "terminal.h"
#include "fs_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

const char *editor_get_command(void) {
    const char *vis = getenv("VISUAL");
    if (vis && *vis) return vis;

    const char *ed = getenv("EDITOR");
    if (ed && *ed) return ed;

    const char *candidates[] = {
        "/usr/bin/nano",
        "/usr/bin/micro",
        "/usr/bin/vim",
        "/usr/bin/nvim",
        "/usr/bin/vi",
        "nano",
        "vim",
        "vi"
    };

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if (access(candidates[i], X_OK) == 0) {
            return candidates[i];
        }
    }

    return "nano";
}

int editor_safe_edit(const char *filepath, editor_validator_fn validator) {
    if (!filepath) return PU_ERR_USAGE;

    if (!fs_file_exists(filepath)) {
        term_error("File not found: %s", filepath);
        return PU_ERR_NOT_FOUND;
    }

    if (access(filepath, W_OK) != 0 && geteuid() != 0) {
        term_warn("File '%s' is read-only for current user. Changes may fail without root privileges (sudo).", filepath);
    }

    /* Create temporary copy */
    char temp_template[] = "/tmp/pacman-utils-edit-XXXXXX";
    int fd = mkstemp(temp_template);
    if (fd < 0) {
        term_error("Failed to create temporary editing file: %s", strerror(errno));
        return PU_ERR_IO;
    }
    close(fd);

    if (fs_copy_file(filepath, temp_template) != 0) {
        term_error("Failed to prepare temporary copy of %s", filepath);
        unlink(temp_template);
        return PU_ERR_IO;
    }

    struct stat st_before;
    stat(temp_template, &st_before);

    const char *editor = editor_get_command();

    while (1) {
        term_info("Opening %s with '%s'...", filepath, editor);

        pid_t pid = fork();
        if (pid < 0) {
            term_error("Fork failed: %s", strerror(errno));
            unlink(temp_template);
            return PU_ERR_GENERAL;
        }

        if (pid == 0) {
            /* Child process */
            char *args[] = {(char *)editor, temp_template, NULL};
            execvp(editor, args);
            /* If exec fails */
            perror("execvp");
            _exit(127);
        }

        /* Parent process */
        int status;
        waitpid(pid, &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            term_warn("Editor exited with non-zero status.");
        }

        struct stat st_after;
        stat(temp_template, &st_after);

        if (st_before.st_mtime == st_after.st_mtime && st_before.st_size == st_after.st_size) {
            term_info("No changes were made to %s.", filepath);
            unlink(temp_template);
            return PU_SUCCESS;
        }

        /* Validate changes if validator is provided */
        if (validator != NULL) {
            char *err_msg = NULL;
            if (!validator(temp_template, &err_msg)) {
                term_error("Syntax validation failed: %s", err_msg ? err_msg : "Invalid configuration");
                free(err_msg);

                const char *choices[] = {
                    "Re-edit the temporary file",
                    "Discard changes and exit",
                    "Save anyway (ignore validation error)"
                };
                int choice = term_prompt_menu("Validation Error Action", choices, 3);
                if (choice == 0) {
                    continue; /* Loop back and open editor again */
                } else if (choice == 1 || choice == -1) {
                    term_info("Changes discarded.");
                    unlink(temp_template);
                    return PU_ERR_CANCELLED;
                }
                /* Choice 2: proceed with save */
            }
        }

        /* Atomically replace the file */
        if (fs_atomic_replace(temp_template, filepath) != 0) {
            term_error("Failed to save changes to %s: %s", filepath, strerror(errno));
            if (geteuid() != 0) {
                term_warn("Try running with sudo.");
            }
            unlink(temp_template);
            return PU_ERR_PERM;
        }

        term_success("Successfully updated %s (backup saved as %s.bak)", filepath, filepath);
        unlink(temp_template);
        return PU_SUCCESS;
    }
}
