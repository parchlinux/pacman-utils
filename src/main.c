#include "pacman_utils.h"
#include "terminal.h"
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libgen.h>

pu_global_options_t g_options = {
    .color_enabled = true,
    .verbose = false,
    .quiet = false,
    .dry_run = false,
    .config_path = NULL,
    .root_dir = NULL,
    .db_path = NULL
};

static const pu_command_t s_commands[] = {
    {"edit-sources",  "edit",        "Safely edit /etc/pacman.conf and mirrorlists",       cmd_edit_sources},
    {"repo",          "repository",  "Manage repositories (list, add, remove, enable)",    cmd_repo},
    {"hold",          "lock",        "Prevent packages from being upgraded (IgnorePkg)",   cmd_hold},
    {"unhold",        "unlock",      "Resume package upgrades (remove from IgnorePkg)",    cmd_unhold},
    {"show-held",     "held",        "List all held/ignored packages",                     cmd_show_held},
    {"mark",          "as",          "Change install reason (as-explicit, as-dep)",        cmd_mark},
    {"autoremove",    "orphans",     "Find and remove unused orphan packages",             cmd_autoremove},
    {"cache",         "clean",       "Inspect and clean package cache",                    cmd_cache},
    {"check-updates", "check",       "Safely check for updates without locking database",  cmd_check_updates},
    {"why",           "explain",     "Explain why a package is installed",                 cmd_why},
    {"deptree",       "tree",        "Display dependency tree for a package",              cmd_deptree},
    {"search-file",   "pkgfile",     "Find which package provides a specific file",        cmd_search_file},
    {"history",       "log",         "Inspect pacman transaction history",                 cmd_history},
    {"top-sizes",     "sizes",       "List installed packages by disk space",              cmd_top_sizes},
    {"doctor",        "health",      "Diagnose system health and database locks",          cmd_doctor},
    {"verify",        "check-files", "Verify integrity of installed package files",        cmd_verify},
    {"diff",          "pacdiff",     "Manage and merge .pacnew / .pacsave files",          cmd_diff},
    {"rank-mirrors",  "fast-mirrors","Benchmark and rank mirrors by download speed",       cmd_rank_mirrors},
    {"keys",          "keyring",     "Manage and repair pacman GPG keyring",               cmd_keys},
    {"news",          "feed",        "Fetch latest distribution announcements",            cmd_news},
    {NULL, NULL, NULL, NULL}
};

const pu_command_t *find_command(const char *name) {
    if (!name) return NULL;
    for (int i = 0; s_commands[i].name != NULL; i++) {
        if (strcmp(s_commands[i].name, name) == 0 ||
            (s_commands[i].alias != NULL && strcmp(s_commands[i].alias, name) == 0)) {
            return &s_commands[i];
        }
    }
    return NULL;
}

void print_all_commands(void) {
    printf("%s%s (pacu) %s%s\n", term_color(ANSI_BOLD_CYAN), PARCH_BANNER, PACMAN_UTILS_VERSION, term_color(ANSI_RESET));
    printf("%sA modern, fast pacman utility suite for Parch Linux%s\n\n", term_color(ANSI_DIM), term_color(ANSI_RESET));
    printf("%sUsage:%s pacu [global options] <command> [command options]  (or: pu <command>)\n\n",
           term_color(ANSI_BOLD), term_color(ANSI_RESET));

    printf("%sAvailable Commands:%s\n", term_color(ANSI_BOLD_YELLOW), term_color(ANSI_RESET));

    printf("\n  %sRepositories & Sources:%s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET));
    printf("    %-16s %s\n", "edit-sources", "Safely edit /etc/pacman.conf and mirrorlists (pacedit)");
    printf("    %-16s %s\n", "repo", "Manage repositories: list, add, remove, enable, disable (pacrepo)");
    printf("    %-16s %s\n", "rank-mirrors", "Benchmark and rank mirrors by latency and throughput");

    printf("\n  %sPackage Management & Marking:%s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET));
    printf("    %-16s %s\n", "hold", "Hold packages to prevent upgrades (apt-mark hold)");
    printf("    %-16s %s\n", "unhold", "Unhold packages to resume upgrades");
    printf("    %-16s %s\n", "show-held", "List all held packages (IgnorePkg)");
    printf("    %-16s %s\n", "mark", "Change install reason (as-explicit / as-dep)");
    printf("    %-16s %s\n", "check-updates", "Check for package updates without locking database");

    printf("\n  %sDependencies & Queries:%s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET));
    printf("    %-16s %s\n", "why", "Explain why an installed package is on the system (pacwhy)");
    printf("    %-16s %s\n", "deptree", "Print ASCII/Unicode dependency tree (forward/reverse)");
    printf("    %-16s %s\n", "search-file", "Search which package owns a specific file");
    printf("    %-16s %s\n", "history", "View pacman install/upgrade/remove history");

    printf("\n  %sDiagnostics & Maintenance:%s\n", term_color(ANSI_BOLD), term_color(ANSI_RESET));
    printf("    %-16s %s\n", "doctor", "Diagnose system health, locks, and broken configs (pacdoctor)");
    printf("    %-16s %s\n", "verify", "Verify package file integrity and missing files");
    printf("    %-16s %s\n", "diff", "Interactive .pacnew / .pacsave review & merger");
    printf("    %-16s %s\n", "autoremove", "Find and remove unneeded dependency orphan trees (pacorphans)");
    printf("    %-16s %s\n", "cache", "Analyze and clean old package cache tarballs (pacclean)");
    printf("    %-16s %s\n", "top-sizes", "List installed packages sorted by disk footprint");
    printf("    %-16s %s\n", "keys", "Repair and manage pacman GPG keyring");
    printf("    %-16s %s\n", "news", "Fetch latest Parch Linux announcements");

    printf("\n%sGlobal Options:%s\n", term_color(ANSI_BOLD_YELLOW), term_color(ANSI_RESET));
    printf("  --color=<when>    Colorize output: 'auto', 'always', or 'never'\n");
    printf("  --config=<path>   Use alternative pacman.conf\n");
    printf("  --dbpath=<path>   Use alternative database path\n");
    printf("  --root=<path>     Use alternative root directory\n");
    printf("  -v, --verbose     Enable verbose output\n");
    printf("  -q, --quiet       Quiet output\n");
    printf("  -n, --dry-run     Simulate actions without making filesystem modifications\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -V, --version     Show version information\n");
}

int main(int argc, char **argv) {
    term_init_color("auto");

    /* Check multi-call binary invocation (e.g. symlinks like 'pacedit' or 'pacdoctor') */
    char *exec_name = basename(argv[0]);
    if (strcmp(exec_name, "pacedit") == 0 || strcmp(exec_name, "pacman-edit-sources") == 0) {
        return cmd_edit_sources(argc, argv);
    } else if (strcmp(exec_name, "pacrepo") == 0) {
        return cmd_repo(argc, argv);
    } else if (strcmp(exec_name, "pacdoctor") == 0) {
        return cmd_doctor(argc, argv);
    } else if (strcmp(exec_name, "pacwhy") == 0) {
        return cmd_why(argc, argv);
    } else if (strcmp(exec_name, "pacclean") == 0) {
        return cmd_cache(argc, argv);
    } else if (strcmp(exec_name, "pacautoremove") == 0 || strcmp(exec_name, "pacorphans") == 0) {
        return cmd_autoremove(argc, argv);
    }

    static struct option long_opts[] = {
        {"help",      no_argument,       0, 'h'},
        {"version",   no_argument,       0, 'V'},
        {"verbose",   no_argument,       0, 'v'},
        {"quiet",     no_argument,       0, 'q'},
        {"dry-run",   no_argument,       0, 'n'},
        {"color",     required_argument, 0, 'c'},
        {"config",    required_argument, 0, 'C'},
        {"dbpath",    required_argument, 0, 'b'},
        {"root",      required_argument, 0, 'r'},
        {0, 0, 0, 0}
    };

    int opt;
    int opt_idx = 0;
    while ((opt = getopt_long(argc, argv, "+hVvanc:C:b:r:", long_opts, &opt_idx)) != -1) {
        switch (opt) {
            case 'h':
                print_all_commands();
                return PU_SUCCESS;
            case 'V':
                printf("pacu version %s (Parch Linux)\n", PACMAN_UTILS_VERSION);
                return PU_SUCCESS;
            case 'v':
                g_options.verbose = true;
                break;
            case 'q':
                g_options.quiet = true;
                break;
            case 'n':
                g_options.dry_run = true;
                break;
            case 'c':
                term_init_color(optarg);
                break;
            case 'C':
                g_options.config_path = optarg;
                break;
            case 'b':
                g_options.db_path = optarg;
                break;
            case 'r':
                g_options.root_dir = optarg;
                break;
            default:
                print_all_commands();
                return PU_ERR_USAGE;
        }
    }

    if (optind >= argc) {
        print_all_commands();
        return PU_SUCCESS;
    }

    const char *cmd_name = argv[optind];
    const pu_command_t *cmd = find_command(cmd_name);
    if (!cmd) {
        term_error("Unknown command '%s'. Run 'pacu --help' for a list of available commands.", cmd_name);
        return PU_ERR_USAGE;
    }

    /* Shift arguments for the subcommand */
    return cmd->handler(argc - optind, &argv[optind]);
}
