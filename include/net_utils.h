#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *url;
    double latency_ms;
    double speed_kbps;
    bool is_reachable;
    int http_status;
} pu_mirror_result_t;

typedef struct {
    pu_mirror_result_t *mirrors;
    size_t count;
} pu_mirror_list_t;

pu_mirror_list_t *net_load_mirrorlist(const char *filepath);
void net_free_mirror_list(pu_mirror_list_t *list);

int net_rank_mirrors(pu_mirror_list_t *list, int timeout_seconds, size_t max_test);
int net_write_ranked_mirrorlist(const pu_mirror_list_t *list, const char *output_file, size_t top_n);

int net_fetch_and_display_news(int limit);

#endif /* NET_UTILS_H */
