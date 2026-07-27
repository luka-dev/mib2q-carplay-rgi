/*
 * CarPlay Hook Framework - Logging API
 *
 * Unified logging solution with:
 * - Multiple log levels (DEBUG, INFO, WARN, ERROR)
 * - Per-module tagging
 * - File output with rotation support
 * - Thread-safe operation
 * - Optional syslog integration
 *
 * Define ENABLE_LOGGING=1 to enable logging, or ENABLE_LOGGING=0 to disable.
 * When disabled, all logging macros become no-ops with zero overhead.
 */

#ifndef CARPLAY_LOGGING_H
#define CARPLAY_LOGGING_H

#include "common.h"

/* Global logging switch - define ENABLE_LOGGING=0 to disable all logging */
#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING 1
#endif

/* Log levels */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE = 4
} log_level_t;

/* Log configuration */
typedef struct {
    const char* log_path;       /* Path to log file (NULL = stdout) */
    log_level_t min_level;      /* Minimum level to log */
    size_t max_size;            /* Max file size before rotation (0 = no limit) */
    int max_files;              /* Max number of rotated files to keep */
    bool include_timestamp;     /* Include timestamp in output */
    bool include_level;         /* Include log level in output */
    bool include_module;        /* Include module name in output */
    bool flush_immediate;       /* Flush after each write */
} log_config_t;

/* Default configuration */
#define LOG_CONFIG_DEFAULT { \
    .log_path = "/tmp/carplay_hook.log", \
    .min_level = LOG_LEVEL_DEBUG, \
    .max_size = 1024 * 1024, \
    .max_files = 3, \
    .include_timestamp = true, \
    .include_level = true, \
    .include_module = true, \
    .flush_immediate = true \
}

#if ENABLE_LOGGING

/* Initialize logging system */
hook_result_t log_init(const log_config_t* config);

/* Shutdown logging system */
void log_shutdown(void);

/* Set minimum log level at runtime */
void log_set_level(log_level_t level);

/* Get current log level */
log_level_t log_get_level(void);

/* Core logging function */
void log_write(log_level_t level, const char* module, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Log with hex dump */
void log_hexdump(log_level_t level, const char* module, const char* prefix,
                 const uint8_t* data, size_t len, size_t max_bytes);

#else /* ENABLE_LOGGING == 0 */

/* No-op versions when logging is disabled */
static inline hook_result_t log_init(const log_config_t* config) {
    (void)config; return HOOK_OK;
}
static inline void log_shutdown(void) {}
static inline void log_set_level(log_level_t level) { (void)level; }
static inline log_level_t log_get_level(void) { return LOG_LEVEL_NONE; }
static inline void log_write(log_level_t level, const char* module, const char* fmt, ...) {
    (void)level; (void)module; (void)fmt;
}
static inline void log_hexdump(log_level_t level, const char* module, const char* prefix,
                               const uint8_t* data, size_t len, size_t max_bytes) {
    (void)level; (void)module; (void)prefix; (void)data; (void)len; (void)max_bytes;
}

#endif /* ENABLE_LOGGING */

#if ENABLE_LOGGING

/* Convenience macros for logging */
#define LOG_DEBUG(module, fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, module, fmt, ##__VA_ARGS__)

#define LOG_INFO(module, fmt, ...) \
    log_write(LOG_LEVEL_INFO, module, fmt, ##__VA_ARGS__)

#define LOG_WARN(module, fmt, ...) \
    log_write(LOG_LEVEL_WARN, module, fmt, ##__VA_ARGS__)

#define LOG_ERROR(module, fmt, ...) \
    log_write(LOG_LEVEL_ERROR, module, fmt, ##__VA_ARGS__)

/* Module-specific logging helpers */
#define DEFINE_LOG_MODULE(name) \
    static const char* LOG_MODULE = #name

/* Hex dump convenience */
#define LOG_HEXDUMP(module, prefix, data, len) \
    log_hexdump(LOG_LEVEL_DEBUG, module, prefix, data, len, 32)

#define LOG_HEXDUMP_FULL(module, prefix, data, len) \
    log_hexdump(LOG_LEVEL_DEBUG, module, prefix, data, len, 0)

/* Dump binary data to file (for debugging) */
hook_result_t log_dump_file(const char* path, const uint8_t* data, size_t len);

/* Dump binary data to file only once (tracks by path) */
hook_result_t log_dump_file_once(const char* path, const uint8_t* data, size_t len);

#else /* ENABLE_LOGGING == 0 */

/* No-op versions when logging is disabled */
#define LOG_DEBUG(module, fmt, ...)      ((void)0)
#define LOG_INFO(module, fmt, ...)       ((void)0)
#define LOG_WARN(module, fmt, ...)       ((void)0)
#define LOG_ERROR(module, fmt, ...)      ((void)0)
#define DEFINE_LOG_MODULE(name)          /* empty */
#define LOG_HEXDUMP(module, prefix, data, len)       ((void)0)
#define LOG_HEXDUMP_FULL(module, prefix, data, len)  ((void)0)

/* No-op dump functions */
static inline hook_result_t log_dump_file(const char* path, const uint8_t* data, size_t len) {
    (void)path; (void)data; (void)len; return HOOK_OK;
}
static inline hook_result_t log_dump_file_once(const char* path, const uint8_t* data, size_t len) {
    (void)path; (void)data; (void)len; return HOOK_OK;
}

#endif /* ENABLE_LOGGING */

#endif /* CARPLAY_LOGGING_H */
