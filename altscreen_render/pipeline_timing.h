/* Local monotonic timing; no timestamp or framing changes on the hook wire. */
#ifndef ALTSCREEN_PIPELINE_TIMING_H
#define ALTSCREEN_PIPELINE_TIMING_H
#include <stdint.h>

#define ALT_LOADER_STOP_WAIT_MS 750
/* First FillDone joins the waiting loader before direct video takes over. */
#define ALT_INPUT_WAIT_MS (ALT_LOADER_STOP_WAIT_MS + 250)
#define ALT_FRAME_PERIOD_NS 33333333ULL
#define ALT_PRESENT_BACKLOG_MS 200ULL

/* Account for time already spent in draw/swap: do not add a second vsync wait.
 * A hidden managed window may return from SwapBuffers without blocking. */
static inline uint64_t alt_frame_remaining_ns(uint64_t begin, uint64_t end) {
    if (!begin || end < begin) return ALT_FRAME_PERIOD_NS;
    return end - begin < ALT_FRAME_PERIOD_NS
        ? ALT_FRAME_PERIOD_NS - (end - begin) : 0;
}

/* Drop only presentation of a decoded picture when newer pictures have
 * already entered OMX. An idle/static stream's final picture always survives.
 * Missing/invalid OMX timestamps fail open to normal presentation. */
static inline int alt_present_obsolete(uint64_t picture_ms, uint64_t newest_ms,
                                       uint64_t now_ms) {
    return picture_ms && newest_ms > picture_ms && now_ms > picture_ms &&
        now_ms - picture_ms > ALT_PRESENT_BACKLOG_MS;
}
#endif
