/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "frame_io.h"
#include "protocol.h"

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

int av_read_exact(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r == 0) return -1;                 /* EOF */
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

int av_read_frame(int fd, uint8_t **buf, uint32_t *cap, uint32_t *out_len) {
    uint8_t hdr[4];
    if (av_read_exact(fd, hdr, 4) != 0) return -1;
    uint32_t len = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
                   ((uint32_t)hdr[2] << 8)  |  (uint32_t)hdr[3];
    if (len == 0) { *out_len = 0; return 0; }  /* empty keep-alive; nothing to do */
    if (len > ALTAV_MAX_MSG) return -2;        /* desync / hostile → drop link */

    if (len > *cap) {
        uint8_t *nb = (uint8_t *)realloc(*buf, len);
        if (!nb) return -2;
        *buf = nb;
        *cap = len;
    }
    if (av_read_exact(fd, *buf, len) != 0) return -1;
    *out_len = len;
    return 0;
}
