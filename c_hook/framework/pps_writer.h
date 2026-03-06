/*
 * CarPlay Hook Framework - PPS Writer API
 *
 * Unified PPS (Persistent Publish/Subscribe) writing solution:
 * - Text mode: key:type:value format (QNX PPS standard)
 * - Binary mode: raw binary data with optional header
 * - Thread-safe operation
 * - Persistent file descriptors for proper ?wait notification
 */

#ifndef CARPLAY_PPS_WRITER_H
#define CARPLAY_PPS_WRITER_H

#include "common.h"

/* PPS value types for text mode */
typedef enum {
    PPS_TYPE_STRING = 's',      /* String value */
    PPS_TYPE_NUMBER = 'n',      /* Numeric value */
    PPS_TYPE_BOOL = 'b',        /* Boolean (true/false) */
    PPS_TYPE_JSON = 'j',        /* JSON object/array */
    PPS_TYPE_BINARY = 'B'       /* Base64-encoded binary */
} pps_type_t;

/* PPS write mode */
typedef enum {
    PPS_MODE_TEXT = 0,          /* Text key:type:value format */
    PPS_MODE_BINARY = 1         /* Raw binary data */
} pps_mode_t;

/* PPS handle (opaque) */
typedef struct pps_handle pps_handle_t;

/* Configuration */
typedef struct {
    const char* path;           /* PPS file path */
    const char* object_name;    /* Object name for @object header (text mode) */
    pps_mode_t mode;            /* Write mode */
    bool create_dirs;           /* Create parent directories if needed */
    bool keep_open;             /* Keep fd open for ?wait subscribers */
    bool truncate_on_write;     /* Truncate file before each write batch */
} pps_config_t;

#define PPS_CONFIG_DEFAULT { \
    .path = NULL, \
    .object_name = NULL, \
    .mode = PPS_MODE_TEXT, \
    .create_dirs = true, \
    .keep_open = true, \
    .truncate_on_write = true \
}

/* Open/create PPS file */
pps_handle_t* pps_open(const pps_config_t* config);

/* Close PPS file */
void pps_close(pps_handle_t* h);

/* Get file path */
const char* pps_get_path(pps_handle_t* h);

/* ============================================================
 * Text Mode Operations
 * ============================================================ */

/* Begin a new write batch (truncates if configured) */
hook_result_t pps_begin(pps_handle_t* h);

/* Write object header (@objectname) */
hook_result_t pps_write_header(pps_handle_t* h);

/* Write string value */
hook_result_t pps_write_string(pps_handle_t* h, const char* key, const char* value);

/* Write integer value */
hook_result_t pps_write_int(pps_handle_t* h, const char* key, int64_t value);

/* Write unsigned integer value */
hook_result_t pps_write_uint(pps_handle_t* h, const char* key, uint64_t value);

/* Write boolean value */
hook_result_t pps_write_bool(pps_handle_t* h, const char* key, bool value);

/* Write formatted value */
hook_result_t pps_write_fmt(pps_handle_t* h, const char* key, pps_type_t type,
                            const char* fmt, ...) __attribute__((format(printf, 4, 5)));

/* Write raw line (no formatting) */
hook_result_t pps_write_raw(pps_handle_t* h, const char* line);

/* End write batch (flush) */
hook_result_t pps_end(pps_handle_t* h);

/* ============================================================
 * Binary Mode Operations
 * ============================================================ */

/* Write raw binary data */
hook_result_t pps_write_binary(pps_handle_t* h, const uint8_t* data, size_t len);

/* Write binary with simple header (magic + version + length) */
hook_result_t pps_write_binary_with_header(pps_handle_t* h, uint32_t magic,
                                           uint16_t version, const uint8_t* data, size_t len);

/* ============================================================
 * Convenience: Write multiple values in one call
 * ============================================================ */

/* Key-value pair for batch writes */
typedef struct {
    const char* key;
    pps_type_t type;
    union {
        const char* str;
        int64_t i64;
        uint64_t u64;
        bool b;
    } value;
} pps_kv_t;

/* Helper macros for creating kv pairs */
#define PPS_KV_STR(k, v)   { .key = (k), .type = PPS_TYPE_STRING, .value.str = (v) }
#define PPS_KV_INT(k, v)   { .key = (k), .type = PPS_TYPE_NUMBER, .value.i64 = (v) }
#define PPS_KV_UINT(k, v)  { .key = (k), .type = PPS_TYPE_NUMBER, .value.u64 = (v) }
#define PPS_KV_BOOL(k, v)  { .key = (k), .type = PPS_TYPE_BOOL, .value.b = (v) }
#define PPS_KV_END         { .key = NULL }

/* Write multiple key-value pairs */
hook_result_t pps_write_batch(pps_handle_t* h, const pps_kv_t* kvs, size_t count);

/* Write array: writes multiple values with indexed keys (key0, key1, ...) */
hook_result_t pps_write_array_int(pps_handle_t* h, const char* key_prefix,
                                  const int64_t* values, size_t count);
hook_result_t pps_write_array_str(pps_handle_t* h, const char* key_prefix,
                                  const char* const* values, size_t count);

/* Write comma-separated list as single value */
hook_result_t pps_write_list_int(pps_handle_t* h, const char* key,
                                 const int64_t* values, size_t count);
hook_result_t pps_write_list_str(pps_handle_t* h, const char* key,
                                 const char* const* values, size_t count, const char* sep);

#endif /* CARPLAY_PPS_WRITER_H */
