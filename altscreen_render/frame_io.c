/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "frame_io.h"
#include "protocol.h"
#include "../common/alt_control_wire.h"

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <poll.h>

int av_read_exact(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r == 0) return -1;                 /* EOF */
        if (r < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd = {fd, POLLIN, 0};
                int ready;
                do {
                    ready = poll(&pfd, 1, -1);
                } while (ready < 0 && errno == EINTR);
                if (ready > 0 && !(pfd.revents & (POLLERR | POLLNVAL)))
                    continue;
            }
            return -1;
        }
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

int av_read_frame_mode(int fd, uint8_t **buf, uint32_t *cap, uint32_t *out_len,
                       alt_frame_meta_t *meta, unsigned mode) {
    uint8_t hdr[ALT_WIRE_HEADER_SIZE];
    alt_frame_meta_t ignored;
    uint32_t len;
    if (!buf || !cap || !out_len || (!*buf && *cap))
        return -2;
    if (!meta)
        meta = &ignored;
    memset(meta, 0, sizeof(*meta));
    *out_len = 0;
    if (mode == ALT_WIRE_VERSION) {
        if (av_read_exact(fd, hdr, sizeof(hdr)) != 0)
            return -1;
        if (alt_wire_decode(hdr, sizeof(hdr), &len, meta) != 0)
            return -2;
    } else {
        if (av_read_exact(fd, hdr, 4) != 0)
            return -1;
        len = alt_wire_u32(hdr);
        if (len == ALT_WIRE_ACK) {
            if (mode)
                return -2;
            if (av_read_exact(fd, hdr + 4, ALT_WIRE_ACK_SIZE - 4) != 0)
                return -1;
            alt_control_record_t record;
            if (alt_control_decode(hdr, ALT_WIRE_ACK_SIZE, &record) != 0)
                return -2;
            meta->generation = record.generation;
            meta->flags = ALT_WIRE_INTERNAL_ACK;
            return 0;
        }
        if (len == ALT_WIRE_MAGIC) {
            if (av_read_exact(fd, hdr + 4, sizeof(hdr) - 4) != 0)
                return -1;
            if (alt_wire_decode(hdr, sizeof(hdr), &len, meta) != 0)
                return -2;
        }
    }
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

int av_read_frame_ex(int fd, uint8_t **buf, uint32_t *cap, uint32_t *out_len,
                     alt_frame_meta_t *meta) {
    return av_read_frame_mode(fd, buf, cap, out_len, meta, 0);
}

int av_read_frame(int fd, uint8_t **buf, uint32_t *cap, uint32_t *out_len) {
    return av_read_frame_ex(fd, buf, cap, out_len, NULL);
}
