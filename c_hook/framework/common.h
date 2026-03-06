/*
 * CarPlay Hook Framework - Common Types and Utilities
 * Copyright (c) 2024
 */

#ifndef CARPLAY_COMMON_H
#define CARPLAY_COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#ifdef __QNX__
#include <sys/neutrino.h>
#endif

/* Boolean type */
#ifndef __cplusplus
#include <stdbool.h>
#endif

/* Result codes */
typedef enum {
    HOOK_OK = 0,
    HOOK_ERR_INIT = -1,
    HOOK_ERR_PARAM = -2,
    HOOK_ERR_MEMORY = -3,
    HOOK_ERR_IO = -4,
    HOOK_ERR_NOT_FOUND = -5,
    HOOK_ERR_BUSY = -6
} hook_result_t;

/* Hook priority (lower = earlier) */
typedef enum {
    HOOK_PRIORITY_FIRST = 0,
    HOOK_PRIORITY_HIGH = 25,
    HOOK_PRIORITY_NORMAL = 50,
    HOOK_PRIORITY_LOW = 75,
    HOOK_PRIORITY_LAST = 100
} hook_priority_t;

/* Message direction */
typedef enum {
    MSG_DIR_INCOMING = 0,   /* From iPhone to HU (decode path) */
    MSG_DIR_OUTGOING = 1    /* From HU to iPhone (encode/send path) */
} msg_direction_t;

/* Portable strcasestr for QNX 6.5 */
static inline char* cp_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)haystack;
    size_t needle_len = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
    }
    return NULL;
}

/* Binary read helpers (big-endian) */
static inline uint16_t read_be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t read_be64(const uint8_t* p) {
    return ((uint64_t)read_be32(p) << 32) | (uint64_t)read_be32(p + 4);
}

/* Binary write helpers (big-endian) */
static inline void write_be16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

static inline void write_be64(uint8_t* p, uint64_t v) {
    write_be32(p, (uint32_t)(v >> 32));
    write_be32(p + 4, (uint32_t)(v & 0xFFFFFFFF));
}

/* Safe string copy */
static inline void safe_strcpy(char* dst, const char* src, size_t dst_size) {
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dst_size - 1) ? src_len : dst_size - 1;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

/* Current timestamp in milliseconds */
static inline uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

#endif /* CARPLAY_COMMON_H */
