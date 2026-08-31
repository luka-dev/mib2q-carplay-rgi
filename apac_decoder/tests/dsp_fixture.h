// Shared helpers for the stage-d (transform) differential tests: reading a
// fixture's hex float words, and deciding whether two float32 vectors agree.
//
// WHY THERE IS A TOLERANCE AT ALL.  Everything in the transform path is
// transcribed instruction for instruction out of the library except one thing:
// the complex FFT.  `MultiRadixFFT::CplxOutOfPlaceTransform` is a wrapper
// around Apple's `vDSP_DFT_Execute`, which is closed source and is not on the
// head unit, so the port has its own radix-2 FFT.  A different butterfly order
// gives a different rounding pattern, and the difference does not shrink.
//
// The comparison is therefore
//
//     |ours - theirs| <= kRelTol * scale + kAbsFloor
//
// where `scale` is the largest magnitude that WENT INTO the block, not the
// largest that came out.  That distinction is not pedantry: a stop-window frame
// fed a DC spectrum cancels almost completely, so its output peak is 7e-4 while
// the terms being summed are of order 1.  Judging a 1e-9 residual against 7e-4
// would call two-ULP arithmetic a bug.  Callers pass max(|input|, |expected|),
// and for the filter bank the input peak is a running maximum over the
// channel, because the overlap-add tail of earlier frames is still in play.
//
//   kRelTol   = 4e-7, about 3 float32 ULP.  Measured worst case over every
//               case and both block sizes is 1.4e-7, i.e. 1.2 ULP.
//   kAbsFloor = 64 * the smallest denormal.  Only the denormal case needs it:
//               there the inputs are 1e-39-ish, every product underflows, and a
//               few denormal ULPs is the entire precision that exists.
//
// This file is header-only and is not part of the port.

#ifndef APAC_TESTS_DSP_FIXTURE_H
#define APAC_TESTS_DSP_FIXTURE_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dspfx {

const float kRelTol = 4e-7f;
// 64 * 1.40129846e-45 (FLT_TRUE_MIN); spelled out so no <cfloat> is needed.
const float kAbsFloor = 64.0f * 1.40129846432481707e-45f;

inline float bits_to_float(uint32_t u)
{
    float f;
    memcpy(&f, &u, 4);
    return f;
}

inline uint32_t float_to_bits(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

// Appends every whitespace-separated 8-hex-digit word on `line` to `out`.
// Returns how many it appended.
inline uint32_t parse_hex_floats(const char* line, float* out, uint32_t cap, uint32_t at)
{
    const char* p = line;
    uint32_t n = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (!*p) break;
        char* endp = 0;
        unsigned long u = strtoul(p, &endp, 16);
        if (endp == p) break;
        if (at + n >= cap) {
            fprintf(stderr, "\nfixture overflow at word %u\n", at + n);
            exit(1);
        }
        out[at + n] = bits_to_float((uint32_t)u);
        ++n;
        p = endp;
    }
    return n;
}

// Largest |v[i]|, the scale the relative tolerance is measured against.
inline float peak(const float* v, uint32_t n)
{
    float m = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        float a = fabsf(v[i]);
        if (a > m) m = a;
    }
    return m;
}

// Compares and, on failure, prints the first offending index and exits.
// Returns the worst |diff| / (peak + tiny) seen, so a test can report it.
//
// Non-finite values are rejected before the tolerance is applied, and that
// ordering is the point.  `d > tol` is FALSE when `d` is NaN, so a comparison
// written only from `>` cannot see a NaN at all: one side going non-finite, or
// both, would slip through and the test would report a clean worst-case number
// while nothing was actually being checked.  `peak()` has the same blindness
// for the same reason, so a NaN in the reference would also be dropped from the
// scale.  Found by apac-bwe2 in its own band gates - `bweEnv` can underflow to
// zero, `coreEnv / bweEnv` is then inf, and `0.0f * inf` is NaN - and the same
// shape was here.  Any predicate built from `<` or `>` is NaN-blind; say what
// is required, not what is forbidden.
// An exponent of all ones is inf or NaN.  Written on the bit pattern rather
// than with a comparison, for the reason given above.
inline bool isfinite_f(float v)
{
    return (float_to_bits(v) & 0x7f800000u) != 0x7f800000u;
}

inline double compare(const float* ours, const float* theirs, uint32_t n,
                      const char* what, float inputScale)
{
    float scale = peak(theirs, n);
    if (inputScale > scale) scale = inputScale;
    const float tol = kRelTol * scale + kAbsFloor;
    double worstRel = 0.0;
    for (uint32_t i = 0; i < n; ++i) {
        if (!isfinite_f(ours[i]) || !isfinite_f(theirs[i])) {
            fprintf(stderr,
                    "\nNON-FINITE %s at index %u: ours %.9g (%08x), "
                    "library %.9g (%08x)\n",
                    what, i, (double)ours[i], float_to_bits(ours[i]),
                    (double)theirs[i], float_to_bits(theirs[i]));
            exit(1);
        }
        const float d = fabsf(ours[i] - theirs[i]);
        if (d > tol) {
            fprintf(stderr,
                    "\nMISMATCH %s at index %u: ours %.9g (%08x), "
                    "library %.9g (%08x), diff %.9g > tol %.9g (peak %.9g)\n",
                    what, i, (double)ours[i], float_to_bits(ours[i]),
                    (double)theirs[i], float_to_bits(theirs[i]),
                    (double)d, (double)tol, (double)scale);
            exit(1);
        }
        if (scale > 0.0f) {
            double rel = (double)d / (double)scale;
            if (rel > worstRel) worstRel = rel;
        }
    }
    return worstRel;
}

}  // namespace dspfx

#endif  // APAC_TESTS_DSP_FIXTURE_H
