#include "config_parser.h"
#include "pacman_utils.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static pu_config_section_t *create_section(const char *name, bool is_commented) {
    pu_config_section_t *sec = calloc(1, sizeof(pu_config_section_t));
    if (!sec) return NULL;
    sec->name = strdup(name);
    sec->is_commented = is_commented;
    return sec;
}

static pu_config_entry_t *create_entry(pu_entry_type_t type, const char *raw_line,
                                       const char *key, const char *value, bool is_commented) {
    pu_config_entry_t *entry = calloc(1, sizeof(pu_config_entry_t));
    if (!entry) return NULL;
    entry->type = type;
    if (raw_line) entry->raw_line = strdup(raw_line);
    if (key) entry->key = strdup(key);
    if (value) entry->value = strdup(value);
    entry->is_commented = is_commented;
    return entry;
}

pu_config_t *config_parse_file(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return NULL;

    pu_config_t *cfg = calloc(1, sizeof(pu_config_t));
    if (!cfg) {
        fclose(fp);
        return NULL;
    }
    cfg->filepath = strdup(filepath);

    pu_config_section_t *current_sec = NULL;
    pu_config_section_t *last_sec = NULL;

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        char *trimmed = line;
        while (isspace((unsigned char)*trimmed)) trimmed++;

        /* Check for blank line */
        if (*trimmed == '\0') {
            pu_config_entry_t *e = create_entry(ENTRY_LINE_BLANK, "", NULL, NULL, false);
            if (current_sec) {
                pu_config_entry_t *tail = current_sec->entries;
                if (!tail) current_sec->entries = e;
                else {
                    while (tail->next) tail = tail->next;
                    tail->next = e;
                }
            }
            continue;
        }

        /* Check for section header: [name] or #[name] */
        bool commented_sec = false;
        char *sec_start = trimmed;
        if (trimmed[0] == '#' && trimmed[1] == '[') {
            commented_sec = true;
            sec_start = trimmed + 1;
        }

        if (sec_start[0] == '[') {
            char *sec_end = strchr(sec_start, ']');
            if (sec_end) {
                *sec_end = '\0';
                char *sec_name = sec_start + 1;
                pu_config_section_t *new_sec = create_section(sec_name, commented_sec);
                if (!cfg->sections) {
                    cfg->sections = new_sec;
                } else {
                    last_sec->next = new_sec;
                }
                last_sec = new_sec;
                current_sec = new_sec;
                continue;
            }
        }

        /* If no section yet, create a default global section */
        if (!current_sec) {
            current_sec = create_section("global", false);
            cfg->sections = current_sec;
            last_sec = current_sec;
        }

        /* Check for comment line */
        if (trimmed[0] == '#') {
            /* Check if it looks like a commented key = value */
            char *eq = strchr(trimmed + 1, '=');
            if (eq) {
                char *k = strndup(trimmed + 1, eq - (trimmed + 1));
                char *v = strdup(eq + 1);
                char *tk = trim_whitespace(k);
                char *tv = trim_whitespace(v);
                pu_config_entry_t *e = create_entry(ENTRY_LINE_KEYVALUE, line, tk, tv, true);
                free(k);
                free(v);

                pu_config_entry_t *tail = current_sec->entries;
                if (!tail) current_sec->entries = e;
                else {
                    while (tail->next) tail = tail->next;
                    tail->next = e;
                }
                continue;
            }

            pu_config_entry_t *e = create_entry(ENTRY_LINE_COMMENT, line, NULL, NULL, true);
            pu_config_entry_t *tail = current_sec->entries;
            if (!tail) current_sec->entries = e;
            else {
                while (tail->next) tail = tail->next;
                tail->next = e;
            }
            continue;
        }

        /* Key = Value or Flag line */
        char *eq = strchr(trimmed, '=');
        if (eq) {
            char *k = strndup(trimmed, eq - trimmed);
            char *v = strdup(eq + 1);
            char *tk = trim_whitespace(k);
            char *tv = trim_whitespace(v);

            pu_config_entry_t *e = create_entry(ENTRY_LINE_KEYVALUE, line, tk, tv, false);
            free(k);
            free(v);

            pu_config_entry_t *tail = current_sec->entries;
            if (!tail) current_sec->entries = e;
            else {
                while (tail->next) tail = tail->next;
                tail->next = e;
            }
        } else {
            /* Option flag or other line */
            pu_config_entry_t *e = create_entry(ENTRY_LINE_RAW, line, trimmed, NULL, false);
            pu_config_entry_t *tail = current_sec->entries;
            if (!tail) current_sec->entries = e;
            else {
                while (tail->next) tail = tail->next;
                tail->next = e;
            }
        }
    }

    fclose(fp);
    return cfg;
}

static void free_entries(pu_config_entry_t *entry) {
    while (entry) {
        pu_config_entry_t *next = entry->next;
        free(entry->raw_line);
        free(entry->key);
        free(entry->value);
        free(entry);
        entry = next;
    }
}

void config_free(pu_config_t *cfg) {
    if (!cfg) return;
    pu_config_section_t *sec = cfg->sections;
    while (sec) {
        pu_config_section_t *next = sec->next;
        free(sec->name);
        free_entries(sec->entries);
        free(sec);
        sec = next;
    }
    free(cfg->filepath);
    free(cfg);
}

int config_write_file(const pu_config_t *cfg, const char *filepath) {
    if (!cfg || !filepath) return -1;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return -1;

    for (pu_config_section_t *sec = cfg->sections; sec; sec = sec->next) {
        if (strcmp(sec->name, "global") != 0) {
            if (sec->is_commented) {
                fprintf(fp, "#[%s]\n", sec->name);
            } else {
                fprintf(fp, "[%s]\n", sec->name);
            }
        }

        for (pu_config_entry_t *e = sec->entries; e; e = e->next) {
            if (e->type == ENTRY_LINE_BLANK) {
                fprintf(fp, "\n");
            } else if (e->type == ENTRY_LINE_COMMENT) {
                fprintf(fp, "%s\n", e->raw_line ? e->raw_line : "#");
            } else if (e->type == ENTRY_LINE_KEYVALUE) {
                if (e->is_commented) {
                    fprintf(fp, "#%s = %s\n", e->key, e->value ? e->value : "");
                } else {
                    fprintf(fp, "%s = %s\n", e->key, e->value ? e->value : "");
                }
            } else if (e->type == ENTRY_LINE_RAW) {
                if (e->is_commented) {
                    fprintf(fp, "#%s\n", e->key ? e->key : (e->raw_line ? e->raw_line : ""));
                } else {
                    fprintf(fp, "%s\n", e->key ? e->key : (e->raw_line ? e->raw_line : ""));
                }
            }
        }
    }

    fclose(fp);
    return 0;
}

pu_config_section_t *config_find_section(pu_config_t *cfg, const char *name) {
    if (!cfg || !name) return NULL;
    for (pu_config_section_t *sec = cfg->sections; sec; sec = sec->next) {
        if (strcasecmp(sec->name, name) == 0) {
            return sec;
        }
    }
    return NULL;
}

pu_config_section_t *config_add_section(pu_config_t *cfg, const char *name) {
    if (!cfg || !name) return NULL;

    pu_config_section_t *existing = config_find_section(cfg, name);
    if (existing) return existing;

    pu_config_section_t *sec = create_section(name, false);
    if (!sec) return NULL;

    if (!cfg->sections) {
        cfg->sections = sec;
    } else {
        pu_config_section_t *cur = cfg->sections;
        while (cur->next) cur = cur->next;
        cur->next = sec;
    }
    return sec;
}

bool config_remove_section(pu_config_t *cfg, const char *name) {
    if (!cfg || !name) return false;

    pu_config_section_t *prev = NULL;
    pu_config_section_t *cur = cfg->sections;

    while (cur) {
        if (strcasecmp(cur->name, name) == 0) {
            if (prev) {
                prev->next = cur->next;
            } else {
                cfg->sections = cur->next;
            }
            free(cur->name);
            free_entries(cur->entries);
            free(cur);
            return true;
        }
        prev = cur;
        cur = cur->next;
    }
    return false;
}

bool config_set_section_enabled(pu_config_t *cfg, const char *name, bool enabled) {
    pu_config_section_t *sec = config_find_section(cfg, name);
    if (!sec) return false;
    sec->is_commented = !enabled;
    for (pu_config_entry_t *e = sec->entries; e; e = e->next) {
        e->is_commented = !enabled;
    }
    return true;
}

const char *config_get_value(pu_config_section_t *section, const char *key) {
    if (!section || !key) return NULL;
    for (pu_config_entry_t *e = section->entries; e; e = e->next) {
        if (e->type == ENTRY_LINE_KEYVALUE && !e->is_commented && e->key) {
            if (strcasecmp(e->key, key) == 0) {
                return e->value;
            }
        }
    }
    return NULL;
}

bool config_set_value(pu_config_section_t *section, const char *key, const char *value) {
    if (!section || !key) return false;
    for (pu_config_entry_t *e = section->entries; e; e = e->next) {
        if (e->type == ENTRY_LINE_KEYVALUE && e->key && strcasecmp(e->key, key) == 0) {
            free(e->value);
            e->value = value ? strdup(value) : NULL;
            e->is_commented = false;
            return true;
        }
    }
    return config_add_entry(section, key, value);
}

bool config_add_entry(pu_config_section_t *section, const char *key, const char *value) {
    if (!section || !key) return false;
    pu_config_entry_t *e = create_entry(ENTRY_LINE_KEYVALUE, NULL, key, value, false);
    if (!e) return false;

    if (!section->entries) {
        section->entries = e;
    } else {
        pu_config_entry_t *cur = section->entries;
        while (cur->next) cur = cur->next;
        cur->next = e;
    }
    return true;
}

char **config_get_ignore_pkgs(pu_config_t *cfg, size_t *count) {
    if (!count) return NULL;
    *count = 0;

    pu_config_section_t *opts = config_find_section(cfg, "options");
    if (!opts) return NULL;

    size_t cap = 16;
    char **list = malloc(sizeof(char *) * cap);
    if (!list) return NULL;

    for (pu_config_entry_t *e = opts->entries; e; e = e->next) {
        if (e->type == ENTRY_LINE_KEYVALUE && !e->is_commented && e->key) {
            if (strcasecmp(e->key, "IgnorePkg") == 0 && e->value) {
                char *val_dup = strdup(e->value);
                char *tok = strtok(val_dup, " \t");
                while (tok) {
                    if (*count >= cap) {
                        cap *= 2;
                        char **new_list = realloc(list, sizeof(char *) * cap);
                        if (!new_list) break;
                        list = new_list;
                    }
                    list[(*count)++] = strdup(tok);
                    tok = strtok(NULL, " \t");
                }
                free(val_dup);
            }
        }
    }

    return list;
}

bool config_is_pkg_ignored(pu_config_t *cfg, const char *pkgname) {
    if (!cfg || !pkgname) return false;
    size_t count = 0;
    char **list = config_get_ignore_pkgs(cfg, &count);
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(list[i], pkgname) == 0) {
            found = true;
            break;
        }
    }
    config_free_string_list(list, count);
    return found;
}

bool config_add_ignore_pkg(pu_config_t *cfg, const char *pkgname) {
    if (!cfg || !pkgname) return false;
    if (config_is_pkg_ignored(cfg, pkgname)) return true;

    pu_config_section_t *opts = config_find_section(cfg, "options");
    if (!opts) {
        opts = config_add_section(cfg, "options");
    }

    /* Check if there's an existing IgnorePkg line */
    for (pu_config_entry_t *e = opts->entries; e; e = e->next) {
        if (e->type == ENTRY_LINE_KEYVALUE && !e->is_commented && e->key) {
            if (strcasecmp(e->key, "IgnorePkg") == 0) {
                char buf[1024];
                if (e->value && strlen(e->value) > 0) {
                    snprintf(buf, sizeof(buf), "%s %s", e->value, pkgname);
                } else {
                    snprintf(buf, sizeof(buf), "%s", pkgname);
                }
                free(e->value);
                e->value = strdup(buf);
                return true;
            }
        }
    }

    /* Not found, add new IgnorePkg */
    return config_add_entry(opts, "IgnorePkg", pkgname);
}

bool config_remove_ignore_pkg(pu_config_t *cfg, const char *pkgname) {
    if (!cfg || !pkgname) return false;
    pu_config_section_t *opts = config_find_section(cfg, "options");
    if (!opts) return false;

    for (pu_config_entry_t *e = opts->entries; e; e = e->next) {
        if (e->type == ENTRY_LINE_KEYVALUE && !e->is_commented && e->key) {
            if (strcasecmp(e->key, "IgnorePkg") == 0 && e->value) {
                char buf[2048] = {0};
                char *val_dup = strdup(e->value);
                char *tok = strtok(val_dup, " \t");
                bool modified = false;

                while (tok) {
                    if (strcmp(tok, pkgname) != 0) {
                        if (buf[0] != '\0') {
                            strcat(buf, " ");
                        }
                        strcat(buf, tok);
                    } else {
                        modified = true;
                    }
                    tok = strtok(NULL, " \t");
                }
                free(val_dup);

                if (modified) {
                    free(e->value);
                    e->value = strdup(buf);
                    return true;
                }
            }
        }
    }
    return false;
}

void config_free_string_list(char **list, size_t count) {
    if (!list) return;
    for (size_t i = 0; i < count; i++) {
        free(list[i]);
    }
    free(list);
}

bool config_validate_syntax(const char *filepath, char **error_msg) {
    if (!filepath) return false;
    pu_config_t *cfg = config_parse_file(filepath);
    if (!cfg) {
        if (error_msg) *error_msg = strdup("Cannot open or read configuration file");
        return false;
    }

    /* Basic sanity check: options section and valid brackets */
    bool has_options = (config_find_section(cfg, "options") != NULL);
    config_free(cfg);

    if (!has_options) {
        if (error_msg) *error_msg = strdup("Missing [options] section in pacman configuration");
        return false;
    }

    return true;
}
