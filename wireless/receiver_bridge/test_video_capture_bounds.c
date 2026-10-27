#include <assert.h>
#include <stdint.h>
#include "video_capture_bounds.h"
int main(void)
{
    assert(video_capture_bounds(32, 24, 8));
    assert(!video_capture_bounds(32, 25, 8));
    assert(!video_capture_bounds(32, 1, 33));
    assert(!video_capture_bounds(UINT32_MAX, 24, 0));
    assert(!video_capture_bounds(2097152, 262145, 0));
    assert(!video_capture_bounds(2097152, 0, 0));
    uint32_t rng = 8030;
    unsigned accepted = 0;
    for (unsigned i = 0; i < 10000; ++i) {
        rng = rng*1664525+1013904223; uint32_t allocated = rng;
        rng = rng*1664525+1013904223; uint32_t filled = rng;
        rng = rng*1664525+1013904223; uint32_t offset = rng;
        if (i%2) {
            allocated = allocated%2097152+1;
            filled = filled%262145;
            offset %= allocated+2;
        }
        int expected = allocated && allocated <= 2097152 && filled && filled <= 262144 &&
            (uint64_t)offset+filled <= allocated;
        assert(video_capture_bounds(allocated, filled, offset) == expected);
        accepted += expected != 0;
    }
    assert(accepted > 1000);
    return 0;
}
