#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

void set_log_level(log_level_t level);
void log_msg(log_level_t level, const char *fmt, ...);

#define log_debug(...) log_msg(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define log_info(...)  log_msg(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...)  log_msg(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) log_msg(LOG_LEVEL_ERROR, __VA_ARGS__)

/* Monotonic Clock Time Helper (Seconds) */
double get_time_seconds(void);

/* Path Safety & Joining Utilities */
bool is_safe_relative_path(const char *path);
int join_paths(char *out, size_t out_len, const char *base, const char *sub);

/* Strict String Numeric Parsers */
bool parse_uint16(const char *str, uint16_t *out);
bool parse_uint32(const char *str, uint32_t *out);

/*
 * Generic string-to-float parser.
 * Range validation (0.0 <= failure_probability <= 1.0) is the explicit
 * responsibility of higher-level CLI option validation in Part II.
 */
bool parse_float(const char *str, float *out);

#endif /* UTILS_H */
