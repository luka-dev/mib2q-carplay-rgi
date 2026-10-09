/* Fixed-memory histograms. Timing is diagnostic only, never a frame-drop rule. */
#ifndef ALT_TIMING_STATS_H
#define ALT_TIMING_STATS_H
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#define ALT_TIMING_BUCKETS 17u
typedef struct {
    uint64_t sum_us, max_us;
    uint32_t samples, over_33ms, over_66ms, bins[ALT_TIMING_BUCKETS];
} alt_timing_stat_t;
static inline uint64_t alt_timing_now_us(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0)
        return 0;
    return (uint64_t)t.tv_sec * UINT64_C(1000000) + (uint64_t)t.tv_nsec / 1000u;
}
static inline void alt_timing_add(alt_timing_stat_t *s, uint64_t us) {
    uint32_t b = 0;
    uint64_t bound = 1;
    while (b + 1 < ALT_TIMING_BUCKETS && us > bound) {
        ++b;
        bound <<= 1;
    }
    s->sum_us += us;
    if (us > s->max_us)
        s->max_us = us;
    ++s->samples;
    ++s->bins[b];
    if (us > 33333u)
        ++s->over_33ms;
    if (us > 66666u)
        ++s->over_66ms;
}
static inline uint64_t alt_timing_percentile(const alt_timing_stat_t *s, unsigned pct) {
    uint64_t target = ((uint64_t)s->samples * pct + 99u) / 100u, seen = 0;
    uint32_t b;
    if (!s->samples)
        return 0;
    for (b = 0; b < ALT_TIMING_BUCKETS; ++b) {
        seen += s->bins[b];
        if (seen >= target) {
            uint64_t bound = UINT64_C(1) << b;
            return b + 1 == ALT_TIMING_BUCKETS || bound > s->max_us ? s->max_us : bound;
        }
    }
    return s->max_us;
}
static inline void alt_timing_elapsed(alt_timing_stat_t *s, uint64_t begin, uint64_t end) {
    if (begin && end >= begin)
        alt_timing_add(s, end - begin);
}
static inline void alt_timing_report(FILE *out, const char *domain, const char *name,
                                     alt_timing_stat_t *s) {
    fprintf(out,
            "%s timing %s n=%u mean_us=%llu p50_us=%llu p95_us=%llu p99_us=%llu max_us=%llu "
            "over33=%u over66=%u\n",
            domain, name, s->samples, (unsigned long long)(s->samples ? s->sum_us / s->samples : 0),
            (unsigned long long)alt_timing_percentile(s, 50),
            (unsigned long long)alt_timing_percentile(s, 95),
            (unsigned long long)alt_timing_percentile(s, 99), (unsigned long long)s->max_us,
            s->over_33ms, s->over_66ms);
    memset(s, 0, sizeof(*s));
}
#endif
