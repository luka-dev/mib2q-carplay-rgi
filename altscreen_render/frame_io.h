/*
 * altscreen_render — framed message reader (isolated so it is unit-testable
 * over a socketpair without the network/OMX/display stack).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_RENDER_FRAME_IO_H
#define ALTSCREEN_RENDER_FRAME_IO_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>   /* size_t/ssize_t on QNX 6.5 (its <stddef.h> is stingy under gnu99) */
#include "../common/alt_video_wire.h"

/* Read exactly n bytes.  0 = ok, -1 = EOF/error. */
int av_read_exact(int fd, void *buf, size_t n);

/* Read one [u32 be len][payload] message.  buf/cap is a caller-owned growable
 * buffer (start with {NULL,0}); grows via realloc as needed and is reused across
 * calls.  On success out_len holds the payload length in buf.
 * Returns:  0 = ok,  -1 = disconnect (EOF/error),  -2 = protocol error (len over
 * max / OOM) — caller should drop the link. */
int av_read_frame(int fd, uint8_t **buf, uint32_t *cap, uint32_t *out_len);
int av_read_frame_mode(int fd,uint8_t **buf,uint32_t *cap,uint32_t *out_len,
                       alt_frame_meta_t *meta,unsigned mode);
int av_read_frame_ex(int fd, uint8_t **buf, uint32_t *cap, uint32_t *out_len,
                     alt_frame_meta_t *meta);

#endif /* ALTSCREEN_RENDER_FRAME_IO_H */
