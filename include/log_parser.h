#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include <stdbool.h>
#include <time.h>
#include <stddef.h>

typedef enum {
    LOG_ACTION_INSTALLED,
    LOG_ACTION_UPGRADED,
    LOG_ACTION_REMOVED,
    LOG_ACTION_REINSTALLED,
    LOG_ACTION_OTHER
} pu_log_action_t;

typedef struct {
    char timestamp_str[32];
    time_t timestamp;
    pu_log_action_t action;
    char *pkgname;
    char *old_ver;
    char *new_ver;
    char *raw_message;
} pu_log_entry_t;

typedef struct {
    pu_log_entry_t *entries;
    size_t count;
    size_t capacity;
} pu_log_list_t;

pu_log_list_t *log_parse_file(const char *logfile_path);
void log_free_list(pu_log_list_t *list);

int log_print_history(const pu_log_list_t *list, const char *pkg_filter, int limit, bool reverse);

#endif /* LOG_PARSER_H */
