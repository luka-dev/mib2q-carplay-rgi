/* Local monotonic timing; no timestamp or framing changes on the hook wire. */
#ifndef ALTSCREEN_PIPELINE_TIMING_H
#define ALTSCREEN_PIPELINE_TIMING_H
#include <stdint.h>

#define ALT_LOADER_STOP_WAIT_MS 750
/* How long _FeedInput waits for a free OMX input buffer before it declares the
 * decoder dead.  The decoder returns an input buffer only after consuming it,
 * and it cannot consume without a free output; the outputs belong to the
 * presenter, so a presenter pause of ~570 ms (12 buffers at 21 fps) starves
 * the inputs about 150 ms later.  1000 ms turned every such pause into a
 * teardown (car, 2026-10-09); 3000 ms leaves the 20 s worker watchdog and the
 * state waits as the real fault detectors.  Measured on CLOCK_MONOTONIC. */
#define ALT_INPUT_WAIT_MS 3000
#define ALT_FRAME_PERIOD_NS 33333333ULL
#define ALT_LOADER_DECODE_PERIOD_NS 66666667ULL

/* Account for time already spent in draw/swap: do not add a second vsync wait.
 * A hidden managed window may return from SwapBuffers without blocking. */
static inline uint64_t alt_frame_remaining_ns(uint64_t begin, uint64_t end) {
    if (!begin || end < begin) return ALT_FRAME_PERIOD_NS;
    return end - begin < ALT_FRAME_PERIOD_NS
        ? ALT_FRAME_PERIOD_NS - (end - begin) : 0;
}

static inline uint64_t alt_loader_decode_remaining_ns(uint64_t begin, uint64_t end) {
    if (!begin || end < begin) return ALT_LOADER_DECODE_PERIOD_NS;
    return end - begin < ALT_LOADER_DECODE_PERIOD_NS
        ? ALT_LOADER_DECODE_PERIOD_NS - (end - begin) : 0;
}

#endif
