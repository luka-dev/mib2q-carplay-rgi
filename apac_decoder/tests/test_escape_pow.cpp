/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Codebook 11's escape magnitude, pow(v, 4/3), on whatever libm is underneath.
//
// PORT_STATUS listed this as the one open portability question: the port's
// escapeMagnitude() calls pow() for v > 1023, and "macOS libm and glibc/aarch64
// agree bit-for-bit over the 562 escapes the fixture drives through that path"
// was an observation about two libms, not a guarantee for QNX 6.5's.
//
// This test settles it three ways.
//
//  1. The domain is BOUNDED and small.  The escape reads a unary prefix k
//     against the mask ~w | 0x800000, so bit 23 is always set and k <= 8;
//     nb = k + 4 <= 12, and v = (1 << nb) + nb bits <= 8191.  checkDomain()
//     below re-derives that rather than asserting it.  So the pow path has
//     exactly 7168 reachable inputs, 1024..8191, and they can all be checked.
//
//  2. The reference is NOT another libm.  tests/escape_pow_table.h holds the
//     correctly-rounded float32 of the exact real v ** y, y being the double
//     0x3FF5555555555555 that the library materialises, computed at 80 decimal
//     digits by tools/gen_escape_pow_table.py.
//
//  3. The table's own header records the worst distance from any of those exact
//     values to a float32 rounding boundary: 5.7e4 double-ULPs.  A pow wrong by
//     less than that cannot change the float32 result, so any libm accurate to
//     better than ~15 bits - which is every libm that exists - lands on this
//     table.  QNX's is not a special case; running the test there confirms it.
//
// Cross-compiled for armle-v7 and executed on QNX 6.5 under QEMU by
// tools/build_qnx.sh --qemu.  On the host it is part of `make check`.

#include "escape_pow_table.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

uint32_t bits(float f)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

// The port's escapeMagnitude(), for v past the i^(4/3) table.
float escapeMagnitude(uint32_t v)
{
    return (float)pow((double)v, 4.0 / 3.0);
}

// Re-derive the reachable escape range from the decoder's own expression
// instead of trusting the comment: v = (1 << nb) + (nb-bit field), nb = k + 4,
// and k = clz(~w | 0x800000) which cannot exceed 8.
void checkDomain(uint32_t* lo, uint32_t* hi)
{
    // clz(~w | 0x800000) can only depend on w's top eight bits: the OR forces
    // bit 23, so the scan stops there at the latest and k <= 8.  Sweeping that
    // byte therefore sweeps every k the escape can produce.
    uint32_t vmin = 0xFFFFFFFFu, vmax = 0;
    for (uint32_t t = 0; t < 256; ++t) {
        const uint32_t w  = t << 24;
        const uint32_t k  = (uint32_t)__builtin_clz(~w | 0x800000u);
        assert(k <= 8);
        const uint32_t nb = k + 4;
        const uint32_t v0 = 1u << nb;                       // field all zero
        const uint32_t v1 = (1u << nb) + ((1u << nb) - 1u); // field all ones
        if (v0 < vmin) vmin = v0;
        if (v1 > vmax) vmax = v1;
    }
    *lo = vmin;
    *hi = vmax;
}

} // namespace

int main(void)
{
    uint32_t dlo = 0, dhi = 0;
    checkDomain(&dlo, &dhi);
    if (dlo != 16 || dhi != 8191) {
        printf("FAIL escape domain is [%u, %u], expected [16, 8191]\n", dlo, dhi);
        return 1;
    }
    if (kEscapePowHi != dhi) {
        printf("FAIL table covers up to %u but the escape reaches %u\n", kEscapePowHi, dhi);
        return 1;
    }

    int bad = 0;
    for (uint32_t v = kEscapePowLo; v <= kEscapePowHi; ++v) {
        uint32_t got  = bits(escapeMagnitude(v));
        uint32_t want = kEscapePowBits[v - kEscapePowLo];
        if (got == want) continue;
        if (bad < 8)
            printf("MISMATCH v=%u got %08x want %08x\n",
                   (unsigned)v, (unsigned)got, (unsigned)want);
        ++bad;
    }
    if (bad) {
        printf("FAIL %d of %u escape magnitudes differ from the exact value\n",
               bad, (unsigned)(kEscapePowHi - kEscapePowLo + 1));
        return 1;
    }

    // Below the escape range the port must take the table, not pow: the two
    // are not interchangeable there, which is why the boundary is checked.
    if (kEscapePowLo != 1024) { printf("FAIL table starts at %u\n", kEscapePowLo); return 1; }

    printf("OK  %u escape magnitudes (v %u..%u) bit-exact vs the correctly-rounded value\n",
           (unsigned)(kEscapePowHi - kEscapePowLo + 1),
           (unsigned)kEscapePowLo, (unsigned)kEscapePowHi);
    return 0;
}
