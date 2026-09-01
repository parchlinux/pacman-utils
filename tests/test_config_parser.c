#include "config_parser.h"
#include "pacman_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static void test_parse_and_modify(void) {
    char mock_path[] = "/tmp/test_pacman_conf_XXXXXX";
    int fd = mkstemp(mock_path);
    assert(fd >= 0);

    const char *initial_cfg = 
        "# Sample pacman configuration\n"
        "[options]\n"
        "HoldPkg     = pacman glibc\n"
        "Architecture = auto\n"
        "IgnorePkg   = linux nvidia\n"
        "\n"
        "[core]\n"
        "Include = /etc/pacman.d/mirrorlist\n"
        "\n"
        "#[testing]\n"
        "#Include = /etc/pacman.d/mirrorlist\n";

    write(fd, initial_cfg, strlen(initial_cfg));
    close(fd);

    pu_config_t *cfg = config_parse_file(mock_path);
    assert(cfg != NULL);

    /* Verify options */
    pu_config_section_t *opts = config_find_section(cfg, "options");
    assert(opts != NULL);
    assert(strcmp(config_get_value(opts, "Architecture"), "auto") == 0);

    /* Verify IgnorePkg */
    size_t count = 0;
    char **held = config_get_ignore_pkgs(cfg, &count);
    assert(count == 2);
    assert(strcmp(held[0], "linux") == 0);
    assert(strcmp(held[1], "nvidia") == 0);
    config_free_string_list(held, count);

    /* Test adding an ignored package */
    bool added = config_add_ignore_pkg(cfg, "firefox");
    assert(added == true);
    assert(config_is_pkg_ignored(cfg, "firefox") == true);

    /* Test removing an ignored package */
    bool removed = config_remove_ignore_pkg(cfg, "linux");
    assert(removed == true);
    assert(config_is_pkg_ignored(cfg, "linux") == false);

    /* Test adding a new repository section */
    pu_config_section_t *parch_repo = config_add_section(cfg, "parch");
    assert(parch_repo != NULL);
    config_add_entry(parch_repo, "SigLevel", "PackageRequired");
    config_add_entry(parch_repo, "Server", "https://repo.parchlinux.com/$arch");

    /* Test section enable/disable */
    bool disabled = config_set_section_enabled(cfg, "core", false);
    assert(disabled == true);
    pu_config_section_t *core_sec = config_find_section(cfg, "core");
    assert(core_sec->is_commented == true);

    /* Write modified file back and parse again */
    int write_res = config_write_file(cfg, mock_path);
    assert(write_res == 0);
    config_free(cfg);

    /* Re-parse */
    pu_config_t *reloaded = config_parse_file(mock_path);
    assert(reloaded != NULL);

    pu_config_section_t *reloaded_parch = config_find_section(reloaded, "parch");
    assert(reloaded_parch != NULL);
    assert(strcmp(config_get_value(reloaded_parch, "Server"), "https://repo.parchlinux.com/$arch") == 0);

    assert(config_is_pkg_ignored(reloaded, "firefox") == true);
    assert(config_is_pkg_ignored(reloaded, "linux") == false);

    config_free(reloaded);
    unlink(mock_path);
}

int main(void) {
    printf("Running config_parser unit tests...\n");
    test_parse_and_modify();
    printf("All config_parser tests passed!\n");
    return 0;
}
