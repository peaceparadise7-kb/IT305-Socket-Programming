#include "utils.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <errno.h>

static log_level_t g_current_log_level = LOG_LEVEL_INFO;

void set_log_level(log_level_t level) {
    g_current_log_level = level;
}

void log_msg(log_level_t level, const char *fmt, ...) {
    if (level < g_current_log_level) {
        return;
    }

    const char *level_str = "INFO";
    switch (level) {
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO";  break;
        case LOG_LEVEL_WARN:  level_str = "WARN";  break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    fprintf(stderr, "[%s.%03ld] [%s] ", time_buf, ts.tv_nsec / 1000000L, level_str);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}

double get_time_seconds(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
    }
    return 0.0;
}

bool is_safe_relative_path(const char *path) {
    if (path == NULL || *path == '\0') {
        return false;
    }

    size_t len = strlen(path);
    if (len > MAX_PATH_LEN) {
        return false;
    }

    /* Reject absolute paths */
    if (path[0] == '/') {
        return false;
    }

    const char *ptr = path;
    while (*ptr != '\0') {
        /* Reject empty component created by repeated slashes (e.g. "a//b") */
        if (ptr[0] == '/' && ptr[1] == '/') {
            return false;
        }

        const char *seg_start = (*ptr == '/') ? ptr + 1 : ptr;
        if (*seg_start == '\0') {
            /* Trailing slash e.g. "dir/" */
            return false;
        }

        const char *seg_end = seg_start;
        while (*seg_end != '\0' && *seg_end != '/') {
            seg_end++;
        }
        size_t seg_len = (size_t)(seg_end - seg_start);

        /* Component-based checks */
        if (seg_len == 1 && seg_start[0] == '.') {
            return false; /* "." component */
        }
        if (seg_len == 2 && seg_start[0] == '.' && seg_start[1] == '.') {
            return false; /* ".." component */
        }

        ptr = seg_end;
    }

    return true;
}

int join_paths(char *out, size_t out_len, const char *base, const char *sub) {
    if (out == NULL || out_len == 0 || base == NULL || sub == NULL) {
        return -1;
    }
    if (!is_safe_relative_path(sub)) {
        return -1;
    }

    size_t base_len = strlen(base);
    bool base_has_slash = (base_len > 0 && base[base_len - 1] == '/');

    int written = snprintf(out, out_len, base_has_slash ? "%s%s" : "%s/%s", base, sub);
    if (written < 0 || (size_t)written >= out_len) {
        return -1;
    }
    return 0;
}

bool parse_uint16(const char *str, uint16_t *out) {
    if (str == NULL || out == NULL || *str == '\0') {
        return false;
    }
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return false;
        }
    }
    char *endptr = NULL;
    errno = 0;
    unsigned long val = strtoul(str, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || val > 65535UL) {
        return false;
    }
    *out = (uint16_t)val;
    return true;
}

bool parse_uint32(const char *str, uint32_t *out) {
    if (str == NULL || out == NULL || *str == '\0') {
        return false;
    }
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return false;
        }
    }
    char *endptr = NULL;
    errno = 0;
    unsigned long val = strtoul(str, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || val > 4294967295UL) {
        return false;
    }
    *out = (uint32_t)val;
    return true;
}

bool parse_float(const char *str, float *out) {
    if (str == NULL || out == NULL || *str == '\0') {
        return false;
    }
    char *endptr = NULL;
    errno = 0;
    float val = strtof(str, &endptr);
    if (errno != 0 || *endptr != '\0') {
        return false;
    }
    *out = val;
    return true;
}
