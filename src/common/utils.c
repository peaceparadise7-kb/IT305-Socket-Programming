#include "utils.h"
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

int join_paths(char *out, size_t out_len, const char *base, const char *sub) {
    if (out == NULL || out_len == 0 || base == NULL || sub == NULL) {
        return -1;
    }

    size_t base_len = strlen(base);
    bool base_has_slash = (base_len > 0 && base[base_len - 1] == '/');
    bool sub_has_slash = (sub[0] == '/');

    int written;
    if (base_has_slash && sub_has_slash) {
        written = snprintf(out, out_len, "%s%s", base, sub + 1);
    } else if (!base_has_slash && !sub_has_slash) {
        written = snprintf(out, out_len, "%s/%s", base, sub);
    } else {
        written = snprintf(out, out_len, "%s%s", base, sub);
    }

    if (written < 0 || (size_t)written >= out_len) {
        return -1;
    }
    return 0;
}

bool parse_uint16(const char *str, uint16_t *out) {
    if (str == NULL || out == NULL || *str == '\0') {
        return false;
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
