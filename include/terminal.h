#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stdio.h>

/* ANSI Color Escape Codes */
#define ANSI_RESET       "\033[0m"
#define ANSI_BOLD        "\033[1m"
#define ANSI_DIM         "\033[2m"
#define ANSI_UNDERLINE   "\033[4m"
#define ANSI_RED         "\033[31m"
#define ANSI_GREEN       "\033[32m"
#define ANSI_YELLOW      "\033[33m"
#define ANSI_BLUE        "\033[34m"
#define ANSI_MAGENTA     "\033[35m"
#define ANSI_CYAN        "\033[36m"
#define ANSI_WHITE       "\033[37m"
#define ANSI_BOLD_RED    "\033[1;31m"
#define ANSI_BOLD_GREEN  "\033[1;32m"
#define ANSI_BOLD_YELLOW "\033[1;33m"
#define ANSI_BOLD_BLUE   "\033[1;34m"
#define ANSI_BOLD_MAGENTA "\033[1;35m"
#define ANSI_BOLD_CYAN   "\033[1;36m"
#define ANSI_BOLD_WHITE  "\033[1;37m"

void term_init_color(const char *color_arg);
bool term_is_color(void);

const char *term_color(const char *ansi_code);

void term_info(const char *fmt, ...);
void term_success(const char *fmt, ...);
void term_warn(const char *fmt, ...);
void term_error(const char *fmt, ...);
void term_header(const char *title);

bool term_prompt_yes_no(const char *question, bool default_val);
int term_prompt_menu(const char *title, const char **options, int count);

#endif /* TERMINAL_H */
