#ifndef COMMANDS_H
#define COMMANDS_H

typedef struct {
    const char *name;
    const char *alias;
    const char *summary;
    int (*handler)(int argc, char **argv);
} pu_command_t;

int cmd_edit_sources(int argc, char **argv);
int cmd_repo(int argc, char **argv);
int cmd_hold(int argc, char **argv);
int cmd_unhold(int argc, char **argv);
int cmd_show_held(int argc, char **argv);
int cmd_mark(int argc, char **argv);
int cmd_autoremove(int argc, char **argv);
int cmd_cache(int argc, char **argv);
int cmd_check_updates(int argc, char **argv);
int cmd_why(int argc, char **argv);
int cmd_deptree(int argc, char **argv);
int cmd_search_file(int argc, char **argv);
int cmd_history(int argc, char **argv);
int cmd_top_sizes(int argc, char **argv);
int cmd_doctor(int argc, char **argv);
int cmd_verify(int argc, char **argv);
int cmd_diff(int argc, char **argv);
int cmd_rank_mirrors(int argc, char **argv);
int cmd_keys(int argc, char **argv);
int cmd_news(int argc, char **argv);

const pu_command_t *find_command(const char *name);
void print_all_commands(void);

#endif /* COMMANDS_H */
