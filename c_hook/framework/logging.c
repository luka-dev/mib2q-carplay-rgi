/*
 * CarPlay Hook Framework - Logging Implementation
 */

#include "logging.h"

#if ENABLE_LOGGING

/* Maximum tracked dump files for once-only dumps */
#define MAX_DUMP_FILES 32
#define MAX_LOG_LINE 1024

/* Module state */
static struct {
    int fd;
    log_config_t config;
    pthread_mutex_t lock;
    bool initialized;
    bool lock_initialized;
    /* Tracked dump files */
    char* dumped_files[MAX_DUMP_FILES];
    int dump_count;
} g_log = {
    .fd = -1,
    .initialized = false,
    .lock_initialized = false,
    .dump_count = 0
};

static void ensure_lock_init(void) {
    if (!g_log.lock_initialized) {
        pthread_mutex_init(&g_log.lock, NULL);
        g_log.lock_initialized = true;
    }
}

/* Real write function pointer */
typedef ssize_t (*write_func_t)(int fd, const void* buf, size_t count);
static write_func_t real_write_fn = NULL;

static ssize_t do_write(int fd, const void* buf, size_t count) {
    if (!real_write_fn) {
        real_write_fn = (write_func_t)dlsym(RTLD_NEXT, "write");
    }
    if (real_write_fn) {
        return real_write_fn(fd, buf, count);
    }
    /* Fallback - should not happen */
    return -1;
}

static const char* level_str(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DBG";
        case LOG_LEVEL_INFO:  return "INF";
        case LOG_LEVEL_WARN:  return "WRN";
        case LOG_LEVEL_ERROR: return "ERR";
        default:              return "???";
    }
}

static void rotate_logs(void) {
    if (!g_log.config.log_path || g_log.config.max_files <= 0) return;

    char old_path[256], new_path[256];

    /* Remove oldest */
    snprintf(old_path, sizeof(old_path), "%s.%d",
             g_log.config.log_path, g_log.config.max_files);
    (void)unlink(old_path);

    /* Rotate existing */
    for (int i = g_log.config.max_files - 1; i >= 1; i--) {
        snprintf(old_path, sizeof(old_path), "%s.%d", g_log.config.log_path, i);
        snprintf(new_path, sizeof(new_path), "%s.%d", g_log.config.log_path, i + 1);
        (void)rename(old_path, new_path);
    }

    /* Current becomes .1 */
    snprintf(new_path, sizeof(new_path), "%s.1", g_log.config.log_path);
    (void)rename(g_log.config.log_path, new_path);
}

static void check_rotation(void) {
    if (g_log.fd < 0 || g_log.config.max_size == 0) return;

    struct stat st;
    if (fstat(g_log.fd, &st) == 0 && (size_t)st.st_size >= g_log.config.max_size) {
        close(g_log.fd);
        g_log.fd = -1;
        rotate_logs();
        g_log.fd = open(g_log.config.log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    }
}

hook_result_t log_init(const log_config_t* config) {
    ensure_lock_init();
    ensure_lock_init();
    pthread_mutex_lock(&g_log.lock);

    if (g_log.initialized) {
        pthread_mutex_unlock(&g_log.lock);
        return HOOK_OK;
    }

    /* Apply config or defaults */
    if (config) {
        g_log.config = *config;
    } else {
        log_config_t def = LOG_CONFIG_DEFAULT;
        g_log.config = def;
    }

    /* Open log file */
    if (g_log.config.log_path) {
        g_log.fd = open(g_log.config.log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (g_log.fd < 0) {
            pthread_mutex_unlock(&g_log.lock);
            return HOOK_ERR_IO;
        }
    }

    g_log.initialized = true;
    pthread_mutex_unlock(&g_log.lock);

    /* Write startup message */
    log_write(LOG_LEVEL_INFO, "LOG", "=== CarPlay Hook Log Started (pid=%d) ===", (int)getpid());

    return HOOK_OK;
}

void log_shutdown(void) {
    ensure_lock_init();
    pthread_mutex_lock(&g_log.lock);

    if (g_log.fd >= 0) {
        close(g_log.fd);
        g_log.fd = -1;
    }

    /* Free tracked dump files */
    for (int i = 0; i < g_log.dump_count; i++) {
        if (g_log.dumped_files[i]) {
            free(g_log.dumped_files[i]);
            g_log.dumped_files[i] = NULL;
        }
    }
    g_log.dump_count = 0;

    g_log.initialized = false;
    pthread_mutex_unlock(&g_log.lock);
}

void log_set_level(log_level_t level) {
    ensure_lock_init();
    pthread_mutex_lock(&g_log.lock);
    g_log.config.min_level = level;
    pthread_mutex_unlock(&g_log.lock);
}

log_level_t log_get_level(void) {
    return g_log.config.min_level;
}

void log_write(log_level_t level, const char* module, const char* fmt, ...) {
    if (level < g_log.config.min_level) return;

    ensure_lock_init();
    pthread_mutex_lock(&g_log.lock);

    /* Auto-init if needed */
    if (!g_log.initialized) {
        log_config_t def = LOG_CONFIG_DEFAULT;
        g_log.config = def;
        if (g_log.config.log_path) {
            g_log.fd = open(g_log.config.log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        }
        g_log.initialized = true;
    }

    int out_fd = (g_log.fd >= 0) ? g_log.fd : STDERR_FILENO;

    char line[MAX_LOG_LINE];
    int off = 0;

    /* Timestamp */
    if (g_log.config.include_timestamp) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        struct tm tm;
        localtime_r(&ts.tv_sec, &tm);
        off += snprintf(line + off, sizeof(line) - off,
                       "%02d:%02d:%02d.%03d ",
                       tm.tm_hour, tm.tm_min, tm.tm_sec,
                       (int)(ts.tv_nsec / 1000000));
    }

    /* Level */
    if (g_log.config.include_level) {
        off += snprintf(line + off, sizeof(line) - off, "[%s] ", level_str(level));
    }

    /* Module */
    if (g_log.config.include_module && module) {
        off += snprintf(line + off, sizeof(line) - off, "[%s] ", module);
    }

    /* Message */
    va_list ap;
    va_start(ap, fmt);
    off += vsnprintf(line + off, sizeof(line) - off, fmt, ap);
    va_end(ap);

    /* Ensure newline */
    if (off > 0 && off < (int)sizeof(line) - 1 && line[off - 1] != '\n') {
        line[off++] = '\n';
    }

    (void)do_write(out_fd, line, (size_t)off);

    if (g_log.config.flush_immediate && g_log.fd >= 0) {
        (void)fsync(g_log.fd);
    }

    check_rotation();

    pthread_mutex_unlock(&g_log.lock);
}

void log_hexdump(log_level_t level, const char* module, const char* prefix,
                 const uint8_t* data, size_t len, size_t max_bytes) {
    if (level < g_log.config.min_level) return;
    if (!data || len == 0) return;

    size_t dump_len = (max_bytes > 0 && len > max_bytes) ? max_bytes : len;

    char line[MAX_LOG_LINE];
    int off = 0;

    if (prefix) {
        off += snprintf(line + off, sizeof(line) - off, "%s ", prefix);
    }
    off += snprintf(line + off, sizeof(line) - off, "len=%zu bytes=", len);

    for (size_t i = 0; i < dump_len && off < (int)sizeof(line) - 4; i++) {
        off += snprintf(line + off, sizeof(line) - off, "%02X ", data[i]);
    }

    if (dump_len < len) {
        off += snprintf(line + off, sizeof(line) - off, "...");
    }

    log_write(level, module, "%s", line);
}

hook_result_t log_dump_file(const char* path, const uint8_t* data, size_t len) {
    if (!path || !data || len == 0) return HOOK_ERR_PARAM;

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return HOOK_ERR_IO;

    ssize_t written = do_write(fd, data, len);
    close(fd);

    if (written != (ssize_t)len) return HOOK_ERR_IO;

    LOG_DEBUG("LOG", "Dumped %zu bytes to %s", len, path);
    return HOOK_OK;
}

hook_result_t log_dump_file_once(const char* path, const uint8_t* data, size_t len) {
    if (!path || !data || len == 0) return HOOK_ERR_PARAM;

    ensure_lock_init();
    pthread_mutex_lock(&g_log.lock);

    /* Check if already dumped */
    for (int i = 0; i < g_log.dump_count; i++) {
        if (g_log.dumped_files[i] && strcmp(g_log.dumped_files[i], path) == 0) {
            pthread_mutex_unlock(&g_log.lock);
            return HOOK_OK; /* Already dumped */
        }
    }

    /* Track this file */
    if (g_log.dump_count < MAX_DUMP_FILES) {
        g_log.dumped_files[g_log.dump_count] = strdup(path);
        if (g_log.dumped_files[g_log.dump_count]) {
            g_log.dump_count++;
        }
    }

    pthread_mutex_unlock(&g_log.lock);

    return log_dump_file(path, data, len);
}

#endif /* ENABLE_LOGGING */
