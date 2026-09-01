#include "log_parser.h"
#include "pacman_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

static void test_log_parser(void) {
    char mock_log[] = "/tmp/test_pacman_log_XXXXXX";
    int fd = mkstemp(mock_log);
    assert(fd >= 0);

    const char *log_content =
        "[2026-08-30T10:00:00+0330] [PACMAN] Running 'pacman -S neovim'\n"
        "[2026-08-30T10:00:05+0330] [ALPM] installed msgpack-c (6.0.0-1)\n"
        "[2026-08-30T10:00:06+0330] [ALPM] installed neovim (0.10.0-1)\n"
        "[2026-08-31T11:00:00+0330] [ALPM] upgraded neovim (0.10.0-1 -> 0.10.1-1)\n"
        "[2026-09-01T12:00:00+0330] [ALPM] removed msgpack-c (6.0.0-1)\n";

    write(fd, log_content, strlen(log_content));
    close(fd);

    pu_log_list_t *list = log_parse_file(mock_log);
    assert(list != NULL);
    assert(list->count == 4);

    assert(strcmp(list->entries[0].pkgname, "msgpack-c") == 0);
    assert(list->entries[0].action == LOG_ACTION_INSTALLED);
    assert(strcmp(list->entries[0].new_ver, "6.0.0-1") == 0);

    assert(strcmp(list->entries[2].pkgname, "neovim") == 0);
    assert(list->entries[2].action == LOG_ACTION_UPGRADED);
    assert(strcmp(list->entries[2].old_ver, "0.10.0-1") == 0);
    assert(strcmp(list->entries[2].new_ver, "0.10.1-1") == 0);

    assert(list->entries[3].action == LOG_ACTION_REMOVED);

    log_free_list(list);
    unlink(mock_log);
}

int main(void) {
    printf("Running log_parser unit tests...\n");
    test_log_parser();
    printf("All log_parser tests passed!\n");
    return 0;
}
