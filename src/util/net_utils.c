#include "net_utils.h"
#include "pacman_utils.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

pu_mirror_list_t *net_load_mirrorlist(const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return NULL;

    pu_mirror_list_t *list = calloc(1, sizeof(pu_mirror_list_t));
    if (!list) {
        fclose(fp);
        return NULL;
    }

    size_t cap = 32;
    list->mirrors = malloc(sizeof(pu_mirror_result_t) * cap);
    if (!list->mirrors) {
        free(list);
        fclose(fp);
        return NULL;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        if (strncmp(p, "Server", 6) == 0) {
            char *eq = strchr(p, '=');
            if (eq) {
                char *url = eq + 1;
                while (*url == ' ' || *url == '\t') url++;
                char *end = url + strlen(url) - 1;
                while (end > url && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
                    *end = '\0';
                    end--;
                }

                if (*url) {
                    if (list->count >= cap) {
                        cap *= 2;
                        pu_mirror_result_t *new_mirrors = realloc(list->mirrors, sizeof(pu_mirror_result_t) * cap);
                        if (!new_mirrors) break;
                        list->mirrors = new_mirrors;
                    }

                    pu_mirror_result_t *m = &list->mirrors[list->count++];
                    m->url = strdup(url);
                    m->latency_ms = 99999.0;
                    m->speed_kbps = 0.0;
                    m->is_reachable = false;
                    m->http_status = 0;
                }
            }
        }
    }

    fclose(fp);
    return list;
}

void net_free_mirror_list(pu_mirror_list_t *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        free(list->mirrors[i].url);
    }
    free(list->mirrors);
    free(list);
}

static int mirror_cmp(const void *a, const void *b) {
    const pu_mirror_result_t *ma = a;
    const pu_mirror_result_t *mb = b;

    if (ma->is_reachable && !mb->is_reachable) return -1;
    if (!ma->is_reachable && mb->is_reachable) return 1;
    if (!ma->is_reachable && !mb->is_reachable) return 0;

    if (ma->latency_ms < mb->latency_ms) return -1;
    if (ma->latency_ms > mb->latency_ms) return 1;
    return 0;
}

int net_rank_mirrors(pu_mirror_list_t *list, int timeout_seconds, size_t max_test) {
    if (!list || list->count == 0) return PU_ERR_USAGE;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) return PU_ERR_GENERAL;

    size_t total_to_test = (max_test > 0 && max_test < list->count) ? max_test : list->count;
    term_header("Ranking Mirrors");
    term_info("Testing %zu mirrors with %d second timeout...", total_to_test, timeout_seconds);

    for (size_t i = 0; i < total_to_test; i++) {
        pu_mirror_result_t *m = &list->mirrors[i];

        /* Replace $repo/$arch with core/os/x86_64/core.db if present for real probe */
        char test_url[1024];
        char *repo_var = strstr(m->url, "$repo");
        if (repo_var) {
            char base[512] = {0};
            strncpy(base, m->url, repo_var - m->url);
            snprintf(test_url, sizeof(test_url), "%score/os/x86_64/core.db", base);
        } else {
            snprintf(test_url, sizeof(test_url), "%s", m->url);
        }

        curl_easy_setopt(curl, CURLOPT_URL, test_url);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_seconds);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "pacman-utils/" PACMAN_UTILS_VERSION);

        printf("Testing [%zu/%zu] %s ... ", i + 1, total_to_test, m->url);
        fflush(stdout);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            double total_time;
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
            long http_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

            m->latency_ms = total_time * 1000.0;
            m->http_status = (int)http_code;
            m->is_reachable = (http_code >= 200 && http_code < 400);

            if (m->is_reachable) {
                printf("%s%.1f ms%s (HTTP %ld)\n",
                       term_color(ANSI_BOLD_GREEN), m->latency_ms, term_color(ANSI_RESET), http_code);
            } else {
                printf("%sHTTP %ld%s\n", term_color(ANSI_BOLD_YELLOW), http_code, term_color(ANSI_RESET));
            }
        } else {
            printf("%sfailed: %s%s\n",
                   term_color(ANSI_BOLD_RED), curl_easy_strerror(res), term_color(ANSI_RESET));
        }
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* Sort ranked mirrors */
    qsort(list->mirrors, total_to_test, sizeof(pu_mirror_result_t), mirror_cmp);

    term_success("Ranking complete. Fastest mirror: %s (%.1f ms)",
                 list->mirrors[0].url, list->mirrors[0].latency_ms);

    return PU_SUCCESS;
}

int net_write_ranked_mirrorlist(const pu_mirror_list_t *list, const char *output_file, size_t top_n) {
    if (!list || !output_file) return PU_ERR_USAGE;

    FILE *fp = fopen(output_file, "w");
    if (!fp) {
        term_error("Failed to write mirrorlist to %s: %s", output_file, strerror(errno));
        return PU_ERR_IO;
    }

    fprintf(fp, "##\n");
    fprintf(fp, "## Parch Linux Ranked Mirrorlist (Generated by pacman-utils)\n");
    fprintf(fp, "##\n\n");

    size_t limit = (top_n > 0 && top_n < list->count) ? top_n : list->count;
    for (size_t i = 0; i < limit; i++) {
        const pu_mirror_result_t *m = &list->mirrors[i];
        if (m->is_reachable) {
            fprintf(fp, "# Latency: %.1f ms\n", m->latency_ms);
            fprintf(fp, "Server = %s\n\n", m->url);
        }
    }

    fclose(fp);
    return PU_SUCCESS;
}

int net_fetch_and_display_news(int limit) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) return PU_ERR_GENERAL;

    const char *url = "https://archlinux.org/feeds/news/";
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "pacman-utils/" PACMAN_UTILS_VERSION);

    term_info("Fetching latest news from %s...", url);
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        term_error("Failed to fetch news: %s", curl_easy_strerror(res));
        free(chunk.memory);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return PU_ERR_IO;
    }

    term_header("Recent Distribution Announcements");

    /* Quick XML item parsing */
    char *p = chunk.memory;
    int count = 0;
    int max = (limit > 0) ? limit : 5;

    while ((p = strstr(p, "<item>")) != NULL && count < max) {
        char *title_start = strstr(p, "<title>");
        char *title_end = strstr(p, "</title>");
        char *link_start = strstr(p, "<link>");
        char *link_end = strstr(p, "</link>");
        char *pub_start = strstr(p, "<pubDate>");
        char *pub_end = strstr(p, "</pubDate>");

        if (title_start && title_end) {
            char title[256] = {0};
            size_t t_len = title_end - (title_start + 7);
            if (t_len < sizeof(title)) strncpy(title, title_start + 7, t_len);

            char date[64] = {0};
            if (pub_start && pub_end) {
                size_t d_len = pub_end - (pub_start + 9);
                if (d_len < sizeof(date)) strncpy(date, pub_start + 9, d_len);
            }

            char link[256] = {0};
            if (link_start && link_end) {
                size_t l_len = link_end - (link_start + 6);
                if (l_len < sizeof(link)) strncpy(link, link_start + 6, l_len);
            }

            printf("\n%s● %s%s\n", term_color(ANSI_BOLD_CYAN), title, term_color(ANSI_RESET));
            if (date[0]) printf("  %sDate:%s %s\n", term_color(ANSI_DIM), term_color(ANSI_RESET), date);
            if (link[0]) printf("  %sLink:%s %s\n", term_color(ANSI_UNDERLINE), link, term_color(ANSI_RESET));

            count++;
        }
        p += 6;
    }

    free(chunk.memory);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (count == 0) {
        term_info("No news articles parsed.");
    }

    return PU_SUCCESS;
}
