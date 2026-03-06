/*
 * CarPlay Hook Framework - PPS Writer Implementation
 */

#include "pps_writer.h"
#include "logging.h"

DEFINE_LOG_MODULE(PPS);

#define MAX_PPS_LINE 1024
#define MAX_PPS_BUFFER 8192
#define MAX_PPS_SNAPSHOT_TEXT 1024
#define MAX_PPS_SNAPSHOT_HEX 32

/* Real write function */
typedef ssize_t (*write_func_t)(int fd, const void* buf, size_t count);
static write_func_t real_write_fn = NULL;

static ssize_t do_write(int fd, const void* buf, size_t count) {
    if (!real_write_fn) {
        real_write_fn = (write_func_t)dlsym(RTLD_NEXT, "write");
    }
    if (real_write_fn) {
        return real_write_fn(fd, buf, count);
    }
    return -1;
}

/* PPS handle structure */
struct pps_handle {
    int fd;
    pps_config_t config;
    pthread_mutex_t lock;
    /* Write buffer for batching */
    char buffer[MAX_PPS_BUFFER];
    size_t buf_len;
    bool in_batch;
    bool chunked; /* true if a mid-batch chunk flush has occurred */
};

static void log_pps_text_snapshot(const pps_handle_t* h, const char* data, size_t len) {
    if (!h || !data || len == 0) return;

    size_t cap = len;
    if (cap > MAX_PPS_SNAPSHOT_TEXT) {
        cap = MAX_PPS_SNAPSHOT_TEXT;
    }

    char snap[MAX_PPS_SNAPSHOT_TEXT + 1];
    memcpy(snap, data, cap);
    snap[cap] = '\0';

    LOG_DEBUG(LOG_MODULE, "PPS write snapshot [%s] (%zu bytes)%s:\n%s",
              h->config.path ? h->config.path : "(null)",
              len,
              len > cap ? " [truncated]" : "",
              snap);
}

static void log_pps_binary_snapshot(const pps_handle_t* h, const uint8_t* data, size_t len) {
    if (!h || !data || len == 0) return;

    size_t preview = len;
    if (preview > MAX_PPS_SNAPSHOT_HEX) {
        preview = MAX_PPS_SNAPSHOT_HEX;
    }

    char hex[(MAX_PPS_SNAPSHOT_HEX * 3) + 1];
    size_t off = 0;
    size_t i;
    for (i = 0; i < preview && off + 4 < sizeof(hex); i++) {
        off += (size_t)snprintf(hex + off, sizeof(hex) - off,
                                "%02X%s", data[i], (i + 1 < preview) ? " " : "");
    }
    hex[off] = '\0';

    LOG_DEBUG(LOG_MODULE, "PPS write snapshot [%s] binary (%zu bytes)%s: %s",
              h->config.path ? h->config.path : "(null)",
              len,
              len > preview ? " [preview]" : "",
              hex);
}

/* Chunk continuation marker — Java PPS reader accumulates until this is absent */
static const char PPS_MORE_MARKER[] = "_more:b:true\n";
#define PPS_MORE_MARKER_LEN (sizeof(PPS_MORE_MARKER) - 1)

/*
 * Flush the current buffer as an intermediate chunk.
 * Appends _more:b:true so Java knows to accumulate and wait.
 * Must be called with h->lock held.
 */
static void pps_flush_chunk_locked(pps_handle_t* h) {
    if (h->fd < 0 || h->buf_len == 0) return;

    /* Append _more marker — space is guaranteed by pps_append_locked reserving
     * PPS_MORE_MARKER_LEN bytes in its overflow threshold. */
    memcpy(h->buffer + h->buf_len, PPS_MORE_MARKER, PPS_MORE_MARKER_LEN);
    h->buf_len += PPS_MORE_MARKER_LEN;

    log_pps_text_snapshot(h, h->buffer, h->buf_len);
    ssize_t written = do_write(h->fd, h->buffer, h->buf_len);
    if (written != (ssize_t)h->buf_len) {
        LOG_WARN(LOG_MODULE, "pps_chunk: partial write %zd/%zu (errno=%d %s)",
                 written, h->buf_len, errno, strerror(errno));
    }

    LOG_DEBUG(LOG_MODULE, "PPS chunk flushed: %zu bytes [%s]",
              h->buf_len, h->config.path ? h->config.path : "(null)");

    /* Reset buffer with header for next chunk */
    h->buf_len = 0;
    h->chunked = true;
    if (h->config.object_name) {
        char line[MAX_PPS_LINE];
        int n = snprintf(line, sizeof(line), "@%s\n", h->config.object_name);
        if (n > 0 && (size_t)n < MAX_PPS_BUFFER) {
            memcpy(h->buffer, line, (size_t)n);
            h->buf_len = (size_t)n;
        }
    }
}

/*
 * Append data to the PPS buffer.  If the buffer would overflow and we're
 * in a batch, auto-flush the current buffer as a chunk with _more marker.
 * Must be called with h->lock held.
 */
static bool pps_append_locked(pps_handle_t* h, const char* data, size_t len) {
    /*
     * In batch mode, reserve space for the _more marker so that
     * pps_flush_chunk_locked() can always append it without overflow.
     */
    size_t limit = h->in_batch
        ? (MAX_PPS_BUFFER - PPS_MORE_MARKER_LEN)
        : (size_t)MAX_PPS_BUFFER;

    if (h->buf_len + len < limit) {
        memcpy(h->buffer + h->buf_len, data, len);
        h->buf_len += len;
        return true;
    }

    if (h->in_batch) {
        /* Buffer full — flush current chunk and retry */
        LOG_WARN(LOG_MODULE, "PPS buffer full (%zu + %zu >= %zu), flushing chunk",
                 h->buf_len, len, limit);
        pps_flush_chunk_locked(h);

        if (h->buf_len + len < limit) {
            memcpy(h->buffer + h->buf_len, data, len);
            h->buf_len += len;
            return true;
        }
    }

    LOG_WARN(LOG_MODULE, "PPS write dropped: buffer full (%zu + %zu >= %zu)",
             h->buf_len, len, limit);
    return false;
}

static void create_parent_dirs(const char* path) {
    if (!path) return;

    char tmp[256];
    safe_strcpy(tmp, path, sizeof(tmp));

    /* Find and create each directory component */
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            (void)mkdir(tmp, 0755);
            *p = '/';
        }
    }
}

pps_handle_t* pps_open(const pps_config_t* config) {
    if (!config || !config->path) {
        LOG_ERROR(LOG_MODULE, "pps_open: missing path");
        return NULL;
    }

    pps_handle_t* h = (pps_handle_t*)calloc(1, sizeof(pps_handle_t));
    if (!h) {
        LOG_ERROR(LOG_MODULE, "pps_open: out of memory");
        return NULL;
    }

    h->config = *config;
    h->config.path = strdup(config->path);
    if (config->object_name) {
        h->config.object_name = strdup(config->object_name);
    }

    pthread_mutex_init(&h->lock, NULL);
    h->fd = -1;
    h->buf_len = 0;
    h->in_batch = false;

    /* Create parent directories if needed */
    if (h->config.create_dirs) {
        create_parent_dirs(h->config.path);
    }

    /* Open file */
    int flags = O_WRONLY | O_CREAT;
    if (!h->config.keep_open) {
        flags |= O_TRUNC;
    }

    h->fd = open(h->config.path, flags, 0644);
    if (h->fd < 0) {
        LOG_ERROR(LOG_MODULE, "pps_open: failed to open %s: %s",
                  h->config.path, strerror(errno));
        free((void*)h->config.path);
        if (h->config.object_name) free((void*)h->config.object_name);
        free(h);
        return NULL;
    }

    LOG_INFO(LOG_MODULE, "Opened PPS: %s (fd=%d, mode=%s)",
             h->config.path, h->fd,
             h->config.mode == PPS_MODE_TEXT ? "text" : "binary");

    return h;
}

void pps_close(pps_handle_t* h) {
    if (!h) return;

    pthread_mutex_lock(&h->lock);

    if (h->fd >= 0) {
        /* Flush any pending data */
        if (h->buf_len > 0) {
            (void)do_write(h->fd, h->buffer, h->buf_len);
        }
        close(h->fd);
        h->fd = -1;
    }

    pthread_mutex_unlock(&h->lock);

    pthread_mutex_destroy(&h->lock);
    if (h->config.path) free((void*)h->config.path);
    if (h->config.object_name) free((void*)h->config.object_name);
    free(h);
}

const char* pps_get_path(pps_handle_t* h) {
    return h ? h->config.path : NULL;
}

hook_result_t pps_begin(pps_handle_t* h) {
    if (!h) return HOOK_ERR_PARAM;

    pthread_mutex_lock(&h->lock);

    if (h->fd < 0) {
        pthread_mutex_unlock(&h->lock);
        return HOOK_ERR_IO;
    }

    /* Truncate and reset */
    if (h->config.truncate_on_write) {
        (void)ftruncate(h->fd, 0);
        (void)lseek(h->fd, 0, SEEK_SET);
    }

    h->buf_len = 0;
    h->in_batch = true;
    h->chunked = false;

    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_header(pps_handle_t* h) {
    if (!h || !h->config.object_name) return HOOK_ERR_PARAM;

    char line[MAX_PPS_LINE];
    int n = snprintf(line, sizeof(line), "@%s\n", h->config.object_name);

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, (size_t)n);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_string(pps_handle_t* h, const char* key, const char* value) {
    if (!h || !key) return HOOK_ERR_PARAM;

    char line[MAX_PPS_LINE];
    int n = snprintf(line, sizeof(line), "%s:s:%s\n", key, value ? value : "");

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, (size_t)n);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_int(pps_handle_t* h, const char* key, int64_t value) {
    if (!h || !key) return HOOK_ERR_PARAM;

    char line[MAX_PPS_LINE];
    int n = snprintf(line, sizeof(line), "%s:n:%lld\n", key, (long long)value);

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, (size_t)n);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_uint(pps_handle_t* h, const char* key, uint64_t value) {
    if (!h || !key) return HOOK_ERR_PARAM;

    char line[MAX_PPS_LINE];
    int n = snprintf(line, sizeof(line), "%s:n:%llu\n", key, (unsigned long long)value);

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, (size_t)n);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_bool(pps_handle_t* h, const char* key, bool value) {
    if (!h || !key) return HOOK_ERR_PARAM;

    char line[MAX_PPS_LINE];
    int n = snprintf(line, sizeof(line), "%s:b:%s\n", key, value ? "true" : "false");

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, (size_t)n);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_fmt(pps_handle_t* h, const char* key, pps_type_t type,
                            const char* fmt, ...) {
    if (!h || !key || !fmt) return HOOK_ERR_PARAM;

    char value[MAX_PPS_LINE];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(value, sizeof(value), fmt, ap);
    va_end(ap);

    char line[MAX_PPS_LINE];
    int n = snprintf(line, sizeof(line), "%s:%c:%s\n", key, (char)type, value);

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, (size_t)n);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_raw(pps_handle_t* h, const char* line) {
    if (!h || !line) return HOOK_ERR_PARAM;

    size_t len = strlen(line);

    pthread_mutex_lock(&h->lock);
    pps_append_locked(h, line, len);
    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_end(pps_handle_t* h) {
    if (!h) return HOOK_ERR_PARAM;

    pthread_mutex_lock(&h->lock);

    if (h->fd >= 0 && h->buf_len > 0) {
        log_pps_text_snapshot(h, h->buffer, h->buf_len);
        ssize_t written = do_write(h->fd, h->buffer, h->buf_len);
        if (written != (ssize_t)h->buf_len) {
            LOG_WARN(LOG_MODULE, "pps_end: partial write %zd/%zu (errno=%d %s)",
                     written, h->buf_len, errno, strerror(errno));
        }
    }

    h->buf_len = 0;
    h->in_batch = false;

    pthread_mutex_unlock(&h->lock);
    return HOOK_OK;
}

hook_result_t pps_write_binary(pps_handle_t* h, const uint8_t* data, size_t len) {
    if (!h || !data || len == 0) return HOOK_ERR_PARAM;

    pthread_mutex_lock(&h->lock);

    if (h->fd < 0) {
        pthread_mutex_unlock(&h->lock);
        return HOOK_ERR_IO;
    }

    /* Truncate for fresh write */
    if (h->config.truncate_on_write) {
        (void)ftruncate(h->fd, 0);
        (void)lseek(h->fd, 0, SEEK_SET);
    }

    log_pps_binary_snapshot(h, data, len);
    ssize_t written = do_write(h->fd, data, len);

    pthread_mutex_unlock(&h->lock);

    if (written != (ssize_t)len) {
        LOG_WARN(LOG_MODULE, "pps_write_binary: partial write %zd/%zu", written, len);
        return HOOK_ERR_IO;
    }

    return HOOK_OK;
}

hook_result_t pps_write_binary_with_header(pps_handle_t* h, uint32_t magic,
                                           uint16_t version, const uint8_t* data, size_t len) {
    if (!h || !data || len == 0) return HOOK_ERR_PARAM;

    /* Build header: magic(4) + version(2) + length(4) = 10 bytes */
    uint8_t header[10];
    write_be32(header, magic);
    write_be16(header + 4, version);
    write_be32(header + 6, (uint32_t)len);

    pthread_mutex_lock(&h->lock);

    if (h->fd < 0) {
        pthread_mutex_unlock(&h->lock);
        return HOOK_ERR_IO;
    }

    /* Truncate for fresh write */
    if (h->config.truncate_on_write) {
        (void)ftruncate(h->fd, 0);
        (void)lseek(h->fd, 0, SEEK_SET);
    }

    log_pps_binary_snapshot(h, header, sizeof(header));
    log_pps_binary_snapshot(h, data, len);
    ssize_t w1 = do_write(h->fd, header, sizeof(header));
    ssize_t w2 = do_write(h->fd, data, len);

    pthread_mutex_unlock(&h->lock);

    if (w1 != sizeof(header) || w2 != (ssize_t)len) {
        return HOOK_ERR_IO;
    }

    return HOOK_OK;
}

hook_result_t pps_write_batch(pps_handle_t* h, const pps_kv_t* kvs, size_t count) {
    if (!h || !kvs) return HOOK_ERR_PARAM;

    hook_result_t res = pps_begin(h);
    if (res != HOOK_OK) return res;

    if (h->config.object_name) {
        pps_write_header(h);
    }

    for (size_t i = 0; i < count; i++) {
        const pps_kv_t* kv = &kvs[i];
        if (!kv->key) break;

        switch (kv->type) {
            case PPS_TYPE_STRING:
                pps_write_string(h, kv->key, kv->value.str);
                break;
            case PPS_TYPE_NUMBER:
                pps_write_int(h, kv->key, kv->value.i64);
                break;
            case PPS_TYPE_BOOL:
                pps_write_bool(h, kv->key, kv->value.b);
                break;
            default:
                break;
        }
    }

    return pps_end(h);
}

hook_result_t pps_write_array_int(pps_handle_t* h, const char* key_prefix,
                                  const int64_t* values, size_t count) {
    if (!h || !key_prefix || !values) return HOOK_ERR_PARAM;

    char key[64];
    for (size_t i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "%s%zu", key_prefix, i);
        pps_write_int(h, key, values[i]);
    }

    return HOOK_OK;
}

hook_result_t pps_write_array_str(pps_handle_t* h, const char* key_prefix,
                                  const char* const* values, size_t count) {
    if (!h || !key_prefix || !values) return HOOK_ERR_PARAM;

    char key[64];
    for (size_t i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "%s%zu", key_prefix, i);
        pps_write_string(h, key, values[i]);
    }

    return HOOK_OK;
}

hook_result_t pps_write_list_int(pps_handle_t* h, const char* key,
                                 const int64_t* values, size_t count) {
    if (!h || !key || !values) return HOOK_ERR_PARAM;

    char line[MAX_PPS_LINE];
    int off = snprintf(line, sizeof(line), "%s:s:", key);

    for (size_t i = 0; i < count && off < (int)sizeof(line) - 16; i++) {
        off += snprintf(line + off, sizeof(line) - off,
                       "%s%lld", (i > 0 ? "," : ""), (long long)values[i]);
    }
    off += snprintf(line + off, sizeof(line) - off, "\n");

    return pps_write_raw(h, line);
}

hook_result_t pps_write_list_str(pps_handle_t* h, const char* key,
                                 const char* const* values, size_t count, const char* sep) {
    if (!h || !key || !values) return HOOK_ERR_PARAM;
    if (!sep) sep = ",";

    char line[MAX_PPS_LINE];
    int off = snprintf(line, sizeof(line), "%s:s:", key);

    for (size_t i = 0; i < count && off < (int)sizeof(line) - 64; i++) {
        if (values[i]) {
            off += snprintf(line + off, sizeof(line) - off,
                           "%s%s", (i > 0 ? sep : ""), values[i]);
        }
    }
    off += snprintf(line + off, sizeof(line) - off, "\n");

    return pps_write_raw(h, line);
}
