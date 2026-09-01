#include "commands.h"
#include "pacman_utils.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_keys(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printf("Usage: pacman-utils keys <fix | refresh | list | reset>\n\n");
        printf("Pacman GPG keyring management and repair utility.\n\n");
        printf("Commands:\n");
        printf("  fix         Initialize and populate Arch & Parch keyrings\n");
        printf("  refresh     Refresh keys from keyservers\n");
        printf("  list        List trusted keys in keyring\n");
        printf("  reset       Remove and rebuild corrupted pacman keyring from scratch\n");
        return PU_SUCCESS;
    }

    const char *action = argv[1];

    if (strcmp(action, "fix") == 0) {
        term_info("Reinitializing and populating pacman keyrings...");
        int r1 = system("pacman-key --init");
        int r2 = system("pacman-key --populate archlinux parch 2>/dev/null || pacman-key --populate archlinux");
        if (r1 == 0 && r2 == 0) {
            term_success("Pacman keyrings initialized successfully.");
            return PU_SUCCESS;
        }
        return PU_ERR_GENERAL;
    } else if (strcmp(action, "refresh") == 0) {
        term_info("Refreshing keys from keyserver...");
        int r = system("pacman-key --refresh-keys");
        return (r == 0) ? PU_SUCCESS : PU_ERR_GENERAL;
    } else if (strcmp(action, "list") == 0) {
        return system("pacman-key --list-keys");
    } else if (strcmp(action, "reset") == 0) {
        if (!term_prompt_yes_no("This will delete /etc/pacman.d/gnupg and regenerate it. Continue?", false)) {
            term_info("Cancelled.");
            return PU_ERR_CANCELLED;
        }
        term_info("Resetting keyring...");
        system("rm -rf /etc/pacman.d/gnupg");
        system("pacman-key --init");
        system("pacman-key --populate archlinux parch 2>/dev/null || pacman-key --populate archlinux");
        term_success("Keyring reset complete.");
        return PU_SUCCESS;
    }

    term_error("Unknown action '%s'. Use 'fix', 'refresh', 'list', or 'reset'.", action);
    return PU_ERR_USAGE;
}
