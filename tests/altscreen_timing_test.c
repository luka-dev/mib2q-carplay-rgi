#include <assert.h>
#include <stdio.h>
#include "../altscreen_render/pipeline_timing.h"

int main(void) {
    const uint64_t second = 1000000000ULL;
    uint64_t now = second;
    unsigned posts = 0;
    /* Reproduce a hidden QNX window: swap returns immediately or in 2 ms.
     * A second of loader animation must not become a tight posting loop. */
    while (now < 2 * second) {
        uint64_t begin = now;
        now += (posts % 2) * 2000000ULL;
        now += alt_frame_remaining_ns(begin, now);
        ++posts;
    }
    assert(posts >= 30 && posts <= 31);
    assert(alt_frame_remaining_ns(second, second + ALT_FRAME_PERIOD_NS) == 0);
    assert(alt_frame_remaining_ns(second, second + 80000000ULL) == 0);
    assert(alt_frame_remaining_ns(0, 0) == ALT_FRAME_PERIOD_NS);
    now = second; posts = 0;
    while (now < 2 * second) {
        uint64_t begin = now;
        now += (posts % 2) * 2000000ULL;
        now += alt_loader_decode_remaining_ns(begin, now);
        ++posts;
    }
    assert(posts >= 15 && posts <= 16);
    assert(alt_loader_decode_remaining_ns(second,
        second + ALT_LOADER_DECODE_PERIOD_NS) == 0);
    assert(ALT_INPUT_WAIT_MS > ALT_LOADER_STOP_WAIT_MS);
    puts("altscreen_timing_test: 30 Hz idle, 15 Hz decoder-pending PRISM and bounded input deadline PASS");
    return 0;
}
