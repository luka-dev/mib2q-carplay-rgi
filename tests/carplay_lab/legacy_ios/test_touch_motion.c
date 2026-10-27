#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include "mt-spi-motion.h"

int main(void)
{
    assert(apple_mt_spi_velocity_ns(100, 50000000) == 2000);
    assert(apple_mt_spi_velocity_ns(-100, 50000000) == 2000);
    assert(apple_mt_spi_velocity_ns(0, 0) == 0);
    assert(apple_mt_spi_velocity_ns(100, 0) == 0);
    assert(apple_mt_spi_velocity_ns(INT32_MIN, 1) == UINT16_MAX);
    assert(apple_mt_spi_velocity_ns(INT32_MAX, UINT64_MAX) == 0);
    uint64_t state = 1234567;
    for (unsigned i = 0; i < 100000; i++) {
        state = state * 6364136223846793005ULL + 1;
        int32_t delta = (int32_t)(uint32_t)(state >> 32);
        /* Mix frame-scale intervals, tiny intervals and the full 64-bit range. */
        uint64_t dt = i % 3 == 0 ? state % 1000000001ULL :
                      i % 3 == 1 ? state % 1001ULL : state ^ (state >> 27);
        uint64_t mag = delta < 0 ? -(int64_t)delta : (uint64_t)delta;
        uint64_t expected = dt ? mag * 1000000000ULL / dt : 0;
        assert(apple_mt_spi_velocity_ns(delta, dt) == (expected > 65535 ? 65535 : expected));
    }
    puts("Touch velocity boundaries and 100000 transactions passed");
}
