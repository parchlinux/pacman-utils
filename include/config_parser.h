#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ENTRY_LINE_COMMENT,
    ENTRY_LINE_BLANK,
    ENTRY_LINE_KEYVALUE,
    ENTRY_LINE_RAW
} pu_entry_type_t;

typedef struct pu_config_entry {
    pu_entry_type_t type;
    char *raw_line;
    char *key;
    char *value;
    bool is_commented;
    struct pu_config_entry *next;
} pu_config_entry_t;

typedef struct pu_config_section {
    char *name;
    bool is_commented;
    pu_config_entry_t *entries;
    struct pu_config_section *next;
} pu_config_section_t;

typedef struct {
    char *filepath;
    pu_config_section_t *sections;
} pu_config_t;

/* Parsing & Life Cycle */
pu_config_t *config_parse_file(const char *filepath);
void config_free(pu_config_t *cfg);
int config_write_file(const pu_config_t *cfg, const char *filepath);

/* Section operations */
pu_config_section_t *config_find_section(pu_config_t *cfg, const char *name);
pu_config_section_t *config_add_section(pu_config_t *cfg, const char *name);
bool config_remove_section(pu_config_t *cfg, const char *name);
bool config_set_section_enabled(pu_config_t *cfg, const char *name, bool enabled);

/* Key-Value operations */
const char *config_get_value(pu_config_section_t *section, const char *key);
bool config_set_value(pu_config_section_t *section, const char *key, const char *value);
bool config_add_entry(pu_config_section_t *section, const char *key, const char *value);

/* IgnorePkg (Hold / Unhold) helpers */
char **config_get_ignore_pkgs(pu_config_t *cfg, size_t *count);
bool config_add_ignore_pkg(pu_config_t *cfg, const char *pkgname);
bool config_remove_ignore_pkg(pu_config_t *cfg, const char *pkgname);
bool config_is_pkg_ignored(pu_config_t *cfg, const char *pkgname);
void config_free_string_list(char **list, size_t count);

/* Validation */
bool config_validate_syntax(const char *filepath, char **error_msg);

#endif /* CONFIG_PARSER_H */
