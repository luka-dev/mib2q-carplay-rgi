/*
 * Opt-in presented-picture luma probe (diagnostic only).
 *
 * Armed once per hook stream by the RAM marker /tmp/carplay_luma_probe (QNX
 * builds only; host tests inject their own path).  While armed, the presenter
 * reads three full rows of the RGBA back buffer after glFinish and before
 * eglSwapBuffers: at most once a second, plus the first picture after a gap
 * of more than a second.  The numbers tell "the phone encodes black" apart
 * from a presentation fault.  The control loop re-logs the last sample every
 * 10 s, so a static black scene that stopped sending frames stays visible.
 *
 * Everything here is pure integer code a host test can call.  The GL readback
 * stays in screen_egl_present.c: the QNX build generates its GLES import
 * stubs from the .c sources, never from headers.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_LUMA_H
#define ALTSCREEN_LUMA_H

#include <stdint.h>

#define ALT_LUMA_ROWS        3u
#define ALT_LUMA_PERIOD_NS   1000000000ULL   /* at most one sample per second */
#define ALT_LUMA_GAP_NS      1000000000ULL   /* first picture after a longer gap */
#define ALT_LUMA_LOG_NS      10000000000ULL  /* steady presenter log cadence */
#define ALT_LUMA_REPORT_MS   10000u          /* control-loop "last" cadence */
#define ALT_LUMA_DARK_MEAN   6u              /* dark: mean < 6 and max < 24 */
#define ALT_LUMA_DARK_MAX    24u

typedef struct {
    unsigned min, mean, max;   /* 0..255 */
} alt_luma_t;

/* GL row (bottom-left origin) of sample index 0..2: height/4, height/2 and
 * 3*height/4, i.e. 135, 271 and 406 on the 542-row cluster. */
static inline int alt_luma_row_y(int height, unsigned index) {
    return (int)(((long)height * (long)(index + 1u)) / (long)(ALT_LUMA_ROWS + 1u));
}

/* Integer BT.601 luma (77 R + 150 G + 29 B) >> 8 over tightly packed RGBA8888
 * pixels; pure white is exactly 255.  The 32-bit sum holds 16 M pixels. */
static inline void alt_luma_reduce_rgba(const uint8_t *rgba, uint32_t pixels,
                                        alt_luma_t *out) {
    uint32_t i, lo = 255u, hi = 0u, sum = 0u;
    if (!rgba || !pixels) {
        out->min = out->mean = out->max = 0u;
        return;
    }
    for (i = 0; i < pixels; ++i) {
        const uint8_t *px = rgba + i * 4u;
        uint32_t y = (77u * px[0] + 150u * px[1] + 29u * px[2]) >> 8;
        sum += y;
        if (y < lo) lo = y;
        if (y > hi) hi = y;
    }
    out->min = lo;
    out->mean = sum / pixels;
    out->max = hi;
}

static inline int alt_luma_is_dark(const alt_luma_t *l) {
    return l->mean < ALT_LUMA_DARK_MEAN && l->max < ALT_LUMA_DARK_MAX;
}

/* Presentation-worker sampling state, reset at every armed stream. */
typedef struct {
    uint64_t last_sample_ns, last_present_ns, last_log_ns;
    alt_luma_t last;
    int have_sample, dark, failed;
} alt_luma_sampler_t;

/* Call once per presented picture while armed.  Returns 1 when this picture
 * is due: the first after the PRISM flight, the first after a presentation
 * gap > 1 s, or 1 s after the previous sample.  Flight pictures are skipped. */
static inline int alt_luma_sample_due(alt_luma_sampler_t *s, uint64_t now_ns,
                                      int transition_active) {
    uint64_t previous = s->last_present_ns;
    s->last_present_ns = now_ns;
    if (transition_active || s->failed)
        return 0;
    if (!s->have_sample)
        return 1;
    if (previous && now_ns - previous > ALT_LUMA_GAP_NS)
        return 1;
    return now_ns - s->last_sample_ns >= ALT_LUMA_PERIOD_NS;
}

/* Record a sample.  Returns 1 when it should be logged: the stream's first
 * sample, a dark <-> not-dark edge, or 10 s after the last logged sample. */
static inline int alt_luma_note_sample(alt_luma_sampler_t *s, const alt_luma_t *l,
                                       uint64_t now_ns) {
    int dark = alt_luma_is_dark(l);
    int log = !s->have_sample || dark != s->dark ||
              now_ns - s->last_log_ns >= ALT_LUMA_LOG_NS;
    s->last = *l;
    s->last_sample_ns = now_ns;
    s->have_sample = 1;
    s->dark = dark;
    if (log)
        s->last_log_ns = now_ns;
    return log;
}

/* Single-writer seqlock from the presentation worker to the control loop.
 * Only word-sized __atomic accesses (no 64-bit atomics on QNX ARMv7 needed);
 * the reader never waits and gives up after a few retries.  token is the armed
 * stream's token the sample was taken under, so the reader can drop a sample
 * left over from an earlier stream. */
typedef struct {
    uint32_t seq, value, stamp_ms, token;
} alt_luma_shared_t;

#define ALT_LUMA_VALID 0x1000000u

static inline uint32_t alt_luma_pack(const alt_luma_t *l) {
    uint32_t lo = l->min > 255u ? 255u : l->min;
    uint32_t mean = l->mean > 255u ? 255u : l->mean;
    uint32_t hi = l->max > 255u ? 255u : l->max;
    return ALT_LUMA_VALID | (lo << 16) | (mean << 8) | hi;
}

static inline void alt_luma_shared_publish(alt_luma_shared_t *s, const alt_luma_t *l,
                                           uint32_t stamp_ms, uint32_t token) {
    uint32_t seq = __atomic_load_n(&s->seq, __ATOMIC_RELAXED);
    __atomic_store_n(&s->seq, seq + 1u, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&s->value, alt_luma_pack(l), __ATOMIC_RELAXED);
    __atomic_store_n(&s->stamp_ms, stamp_ms, __ATOMIC_RELAXED);
    __atomic_store_n(&s->token, token, __ATOMIC_RELAXED);
    __atomic_store_n(&s->seq, seq + 2u, __ATOMIC_RELEASE);
}

/* 1 with a consistent snapshot; 0 when nothing was published yet or every
 * retry overlapped a write. */
static inline int alt_luma_shared_read(alt_luma_shared_t *s, alt_luma_t *l,
                                       uint32_t *stamp_ms, uint32_t *token) {
    unsigned tries;
    for (tries = 0; tries < 4u; ++tries) {
        uint32_t before = __atomic_load_n(&s->seq, __ATOMIC_ACQUIRE), value, stamp, tok;
        if (before & 1u)
            continue;
        value = __atomic_load_n(&s->value, __ATOMIC_RELAXED);
        stamp = __atomic_load_n(&s->stamp_ms, __ATOMIC_RELAXED);
        tok = __atomic_load_n(&s->token, __ATOMIC_RELAXED);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&s->seq, __ATOMIC_RELAXED) != before)
            continue;
        if (!(value & ALT_LUMA_VALID))
            return 0;
        l->min = (value >> 16) & 0xffu;
        l->mean = (value >> 8) & 0xffu;
        l->max = value & 0xffu;
        *stamp_ms = stamp;
        *token = tok;
        return 1;
    }
    return 0;
}

#endif /* ALTSCREEN_LUMA_H */
