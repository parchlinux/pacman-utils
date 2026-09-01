#include "log_parser.h"
#include "pacman_utils.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *clean_trim_str(const char *src) {
    if (!src) return NULL;
    while (isspace((unsigned char)*src)) src++;
    if (*src == '\0') return strdup("");

    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char)src[len - 1])) {
        len--;
    }
    return strndup(src, len);
}

pu_log_list_t *log_parse_file(const char *logfile_path) {
    const char *path = logfile_path ? logfile_path : DEFAULT_LOG_FILE;
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    pu_log_list_t *list = calloc(1, sizeof(pu_log_list_t));
    if (!list) {
        fclose(fp);
        return NULL;
    }

    list->capacity = 128;
    list->entries = malloc(sizeof(pu_log_entry_t) * list->capacity);
    if (!list->entries) {
        free(list);
        fclose(fp);
        return NULL;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        /* Filter for [ALPM] lines */
        char *alpm_pos = strstr(line, "[ALPM]");
        if (!alpm_pos) continue;

        /* Extract timestamp [YYYY-MM-DDTHH:MM:SS...] */
        char *ts_start = strchr(line, '[');
        char *ts_end = strchr(line, ']');
        if (!ts_start || !ts_end || ts_end >= alpm_pos) continue;

        char ts[32] = {0};
        size_t ts_len = ts_end - (ts_start + 1);
        if (ts_len < sizeof(ts)) {
            strncpy(ts, ts_start + 1, ts_len);
        }

        /* Parse action */
        char *p = alpm_pos + 6;
        while (*p == ' ' || *p == '\t') p++;

        pu_log_action_t action = LOG_ACTION_OTHER;
        if (strncmp(p, "installed", 9) == 0) {
            action = LOG_ACTION_INSTALLED;
            p += 9;
        } else if (strncmp(p, "upgraded", 8) == 0) {
            action = LOG_ACTION_UPGRADED;
            p += 8;
        } else if (strncmp(p, "removed", 7) == 0) {
            action = LOG_ACTION_REMOVED;
            p += 7;
        } else if (strncmp(p, "reinstalled", 11) == 0) {
            action = LOG_ACTION_REINSTALLED;
            p += 11;
        } else {
            continue;
        }

        while (*p == ' ' || *p == '\t') p++;

        /* Parse package name */
        char *paren_start = strchr(p, '(');
        if (!paren_start) continue;

        char *paren_end = strchr(paren_start, ')');
        if (!paren_end) continue;

        char raw_pkg[256] = {0};
        size_t raw_pkg_len = paren_start - p;
        if (raw_pkg_len >= sizeof(raw_pkg)) raw_pkg_len = sizeof(raw_pkg) - 1;
        strncpy(raw_pkg, p, raw_pkg_len);
        char *pkgname = clean_trim_str(raw_pkg);

        char raw_ver[512] = {0};
        size_t raw_ver_len = paren_end - (paren_start + 1);
        if (raw_ver_len >= sizeof(raw_ver)) raw_ver_len = sizeof(raw_ver) - 1;
        strncpy(raw_ver, paren_start + 1, raw_ver_len);

        char *old_ver = NULL;
        char *new_ver = NULL;

        if (action == LOG_ACTION_UPGRADED) {
            char *arrow = strstr(raw_ver, "->");
            if (arrow) {
                *arrow = '\0';
                old_ver = clean_trim_str(raw_ver);
                new_ver = clean_trim_str(arrow + 2);
            } else {
                new_ver = clean_trim_str(raw_ver);
            }
        } else {
            new_ver = clean_trim_str(raw_ver);
        }

        if (list->count >= list->capacity) {
            list->capacity *= 2;
            pu_log_entry_t *new_entries = realloc(list->entries, sizeof(pu_log_entry_t) * list->capacity);
            if (!new_entries) {
                free(pkgname);
                free(old_ver);
                free(new_ver);
                break;
            }
            list->entries = new_entries;
        }

        pu_log_entry_t *entry = &list->entries[list->count++];
        snprintf(entry->timestamp_str, sizeof(entry->timestamp_str), "%s", ts);
        entry->action = action;
        entry->pkgname = pkgname;
        entry->old_ver = old_ver;
        entry->new_ver = new_ver;
        entry->raw_message = strdup(line);
    }

    fclose(fp);
    return list;
}

void log_free_list(pu_log_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->entries[i].pkgname);
        free(list->entries[i].old_ver);
        free(list->entries[i].new_ver);
        free(list->entries[i].raw_message);
    }
    free(list->entries);
    free(list);
}

int log_print_history(const pu_log_list_t *list, const char *pkg_filter, int limit, bool reverse) {
    if (!list) return PU_ERR_NOT_FOUND;

    term_header("Pacman Transaction History");

    int shown = 0;
    int max = (limit > 0) ? limit : 50;

    int start = reverse ? 0 : (int)list->count - 1;
    int end = reverse ? (int)list->count : -1;
    int step = reverse ? 1 : -1;

    for (int i = start; i != end && shown < max; i += step) {
        const pu_log_entry_t *e = &list->entries[i];
        if (pkg_filter != NULL && strcasecmp(e->pkgname, pkg_filter) != 0) {
            continue;
        }

        const char *act_color = ANSI_GREEN;
        const char *act_label = "INSTALLED";

        switch (e->action) {
            case LOG_ACTION_INSTALLED:
                act_color = ANSI_BOLD_GREEN;
                act_label = "INSTALLED";
                break;
            case LOG_ACTION_UPGRADED:
                act_color = ANSI_BOLD_CYAN;
                act_label = "UPGRADED ";
                break;
            case LOG_ACTION_REMOVED:
                act_color = ANSI_BOLD_RED;
                act_label = "REMOVED  ";
                break;
            case LOG_ACTION_REINSTALLED:
                act_color = ANSI_BOLD_YELLOW;
                act_label = "REINSTALL";
                break;
            default:
                act_color = ANSI_WHITE;
                act_label = "OTHER    ";
                break;
        }

        printf("%s%s%s  %s%s%s  %s%-24s%s ",
               term_color(ANSI_DIM), e->timestamp_str, term_color(ANSI_RESET),
               term_color(act_color), act_label, term_color(ANSI_RESET),
               term_color(ANSI_BOLD), e->pkgname, term_color(ANSI_RESET));

        if (e->action == LOG_ACTION_UPGRADED && e->old_ver && e->new_ver) {
            printf("(%s -> %s%s%s)\n", e->old_ver, term_color(ANSI_BOLD_GREEN), e->new_ver, term_color(ANSI_RESET));
        } else if (e->new_ver) {
            printf("(%s)\n", e->new_ver);
        } else {
            printf("\n");
        }

        shown++;
    }

    if (shown == 0) {
        term_info("No transaction history matching criteria.");
    }

    return PU_SUCCESS;
}
