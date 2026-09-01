#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>

typedef bool (*editor_validator_fn)(const char *temp_filepath, char **error_out);

/* Returns path to best available editor executable */
const char *editor_get_command(void);

/* Safely opens a file in user's editor with temporary isolation and optional validation callback */
int editor_safe_edit(const char *filepath, editor_validator_fn validator);

#endif /* EDITOR_H */
