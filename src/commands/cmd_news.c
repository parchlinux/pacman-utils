#include "commands.h"
#include "pacman_utils.h"
#include "net_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_news(int argc, char **argv) {
    int limit = 5;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--limit") == 0 || strcmp(argv[i], "-n") == 0) && i + 1 < argc) {
            limit = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: pacman-utils news [options]\n\n");
            printf("Fetches and displays recent distribution announcements and update warnings.\n\n");
            printf("Options:\n");
            printf("  -n, --limit <N>     Number of articles to fetch (default: 5)\n");
            return PU_SUCCESS;
        }
    }

    return net_fetch_and_display_news(limit);
}
