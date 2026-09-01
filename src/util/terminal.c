#include "terminal.h"
#include "pacman_utils.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static bool s_color_enabled = true;

void term_init_color(const char *color_arg) {
    if (getenv("NO_COLOR") != NULL) {
        s_color_enabled = false;
        return;
    }

    if (color_arg == NULL || strcmp(color_arg, "auto") == 0) {
        s_color_enabled = isatty(STDOUT_FILENO) != 0;
    } else if (strcmp(color_arg, "always") == 0) {
        s_color_enabled = true;
    } else if (strcmp(color_arg, "never") == 0) {
        s_color_enabled = false;
    } else {
        s_color_enabled = isatty(STDOUT_FILENO) != 0;
    }
}

bool term_is_color(void) {
    return s_color_enabled;
}

const char *term_color(const char *ansi_code) {
    if (!s_color_enabled) {
        return "";
    }
    return ansi_code;
}

void term_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (s_color_enabled) {
        fprintf(stdout, "%s::%s ", ANSI_BOLD_BLUE, ANSI_RESET);
    } else {
        fprintf(stdout, ":: ");
    }
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void term_success(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (s_color_enabled) {
        fprintf(stdout, "%s==>%s ", ANSI_BOLD_GREEN, ANSI_RESET);
    } else {
        fprintf(stdout, "==> ");
    }
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void term_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (s_color_enabled) {
        fprintf(stderr, "%swarning:%s ", ANSI_BOLD_YELLOW, ANSI_RESET);
    } else {
        fprintf(stderr, "warning: ");
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void term_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (s_color_enabled) {
        fprintf(stderr, "%serror:%s ", ANSI_BOLD_RED, ANSI_RESET);
    } else {
        fprintf(stderr, "error: ");
    }
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void term_header(const char *title) {
    if (s_color_enabled) {
        fprintf(stdout, "%s%s=== %s ===%s\n", ANSI_BOLD, ANSI_CYAN, title, ANSI_RESET);
    } else {
        fprintf(stdout, "=== %s ===\n", title);
    }
}

bool term_prompt_yes_no(const char *question, bool default_val) {
    char prompt[16];
    if (default_val) {
        snprintf(prompt, sizeof(prompt), "[Y/n]");
    } else {
        snprintf(prompt, sizeof(prompt), "[y/N]");
    }

    if (s_color_enabled) {
        fprintf(stdout, "%s%s %s%s ", ANSI_BOLD, question, ANSI_BOLD_CYAN, prompt);
        fprintf(stdout, "%s", ANSI_RESET);
    } else {
        fprintf(stdout, "%s %s ", question, prompt);
    }
    fflush(stdout);

    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin)) {
        return default_val;
    }

    char *p = buf;
    while (isspace((unsigned char)*p)) p++;

    if (*p == '\0') {
        return default_val;
    }

    if (*p == 'y' || *p == 'Y') {
        return true;
    }
    if (*p == 'n' || *p == 'N') {
        return false;
    }

    return default_val;
}

int term_prompt_menu(const char *title, const char **options, int count) {
    if (count <= 0) return -1;

    term_header(title);
    for (int i = 0; i < count; i++) {
        if (s_color_enabled) {
            fprintf(stdout, "  %s%d)%s %s\n", ANSI_BOLD_YELLOW, i + 1, ANSI_RESET, options[i]);
        } else {
            fprintf(stdout, "  %d) %s\n", i + 1, options[i]);
        }
    }

    while (1) {
        if (s_color_enabled) {
            fprintf(stdout, "%sEnter choice [1-%d, 'q' to cancel]:%s ", ANSI_BOLD, count, ANSI_RESET);
        } else {
            fprintf(stdout, "Enter choice [1-%d, 'q' to cancel]: ", count);
        }
        fflush(stdout);

        char buf[64];
        if (!fgets(buf, sizeof(buf), stdin)) {
            return -1;
        }

        char *p = buf;
        while (isspace((unsigned char)*p)) p++;

        if (*p == 'q' || *p == 'Q') {
            return -1;
        }

        int val = atoi(p);
        if (val >= 1 && val <= count) {
            return val - 1;
        }

        term_warn("Invalid selection. Please try again.");
    }
}
