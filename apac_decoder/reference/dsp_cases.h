// Shared test-vector generator for the stage-d (transform) differential tests.
//
// Included by BOTH sides of the comparison:
//
//   * reference/apac_fn_ref.cpp, which runs inside the arm64 container and
//     calls the real library;
//   * apac_decoder/tests/test_*.cpp, which run on the host against the port.
//
// It exists so the fixtures under tests/fixtures/ only have to carry the
// EXPECTED OUTPUT.  Carrying the inputs too would be a megabyte of hex for the
// filterbank alone, and the point of a fixture is to be readable enough that a
// wrong one is noticeable.
//
// Nothing here may change without regenerating every stage-d fixture.  The
// generator is a plain xorshift32 so that it is identical on any compiler and
// any word size; no <random>, no floating-point accumulation.

#ifndef APAC_DSP_CASES_H
#define APAC_DSP_CASES_H

#include <stdint.h>
#include <string.h>

namespace apac_cases {

inline uint32_t xorshift32(uint32_t* s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

// A float in [-1, 1) built from the top 24 bits, so the value depends only on
// integer arithmetic and an exact power-of-two divide.
inline float rnd_unit(uint32_t* s)
{
    int32_t v = (int32_t)(xorshift32(s) >> 8) - 0x800000;  // -2^23 .. 2^23-1
    return (float)v * (1.0f / 8388608.0f);
}

// Bit pattern -> float without aliasing games the compiler is allowed to undo.
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

enum { kNumCases = 16 };

inline const char* case_name(unsigned c)
{
    static const char* kNames[kNumCases] = {
        "zeros",        "impulse0",     "impulseN1",    "impulseHalf",
        "alternating",  "dc",           "denormal",     "tiny",
        "huge",         "rand1",        "rand2",        "rand3",
        "rand-tiny",    "rand-huge",    "ramp",         "sparse"
    };
    return c < kNumCases ? kNames[c] : "?";
}

// Fill `out[0 .. n)` for case `c`.  Deterministic and independent of n except
// where the case name says otherwise.
inline void case_fill(unsigned c, float* out, uint32_t n)
{
    uint32_t s = 0x9e3779b9u ^ (c * 2654435761u) ^ (n * 40503u);
    if (s == 0) s = 1;
    uint32_t i;
    for (i = 0; i < n; ++i) out[i] = 0.0f;
    switch (c) {
    case 0:                                  break;   // zeros
    case 1: out[0] = 1.0f;                   break;
    case 2: out[n - 1] = 1.0f;               break;
    case 3: out[n / 2] = 1.0f;               break;
    case 4: for (i = 0; i < n; ++i) out[i] = (i & 1) ? -1.0f : 1.0f;   break;
    case 5: for (i = 0; i < n; ++i) out[i] = 1.0f;                     break;
    case 6:  // denormals: 0x00000001 .. and a few normal-boundary values
        for (i = 0; i < n; ++i)
            out[i] = bits_to_float((xorshift32(&s) & 0x007fffffu) |
                                   ((i & 1) ? 0x80000000u : 0u));
        break;
    case 7: for (i = 0; i < n; ++i) out[i] = rnd_unit(&s) * 1e-30f;     break;
    case 8: for (i = 0; i < n; ++i) out[i] = rnd_unit(&s) * 1e8f;       break;
    case 9:
    case 10:
    case 11: for (i = 0; i < n; ++i) out[i] = rnd_unit(&s);             break;
    case 12: for (i = 0; i < n; ++i) out[i] = rnd_unit(&s) * 1e-12f;    break;
    case 13: for (i = 0; i < n; ++i) out[i] = rnd_unit(&s) * 1e4f;      break;
    case 14: for (i = 0; i < n; ++i) out[i] = (float)i / (float)n;      break;
    case 15:  // mostly zero, a handful of large coefficients
        for (i = 0; i < n; ++i)
            out[i] = ((xorshift32(&s) & 31u) == 0) ? rnd_unit(&s) * 100.0f : 0.0f;
        break;
    default: break;
    }
}

// ------------------------------------------------------- filterbank plan ----
// One entry per FrequencyToTimeInPlace call.  The filterbank is stateful in two
// ways - the per-channel overlap buffer and the per-channel window SHAPE from
// the previous frame - so the plan deliberately interleaves channels, flips the
// shape mid-stream, and walks every window sequence including the transitions
// that are illegal in a real bitstream but still have to agree.
//
// windowSequence: 0 LONG, 1 START, 2 EIGHT_SHORT, 3 STOP.  windowShape: 0 sine,
// 1 KBD.
struct FBStep {
    uint8_t  channel;
    uint8_t  shape;
    uint8_t  sequence;
    uint8_t  caseIdx;
};

enum { kFBChannels = 3, kFBSteps = 30 };

inline const FBStep* fb_plan()
{
    static const FBStep kPlan[kFBSteps] = {
        { 0, 0, 0,  9 }, { 1, 0, 0, 10 }, { 2, 0, 0, 11 },
        { 0, 0, 1,  9 }, { 1, 0, 1,  4 }, { 2, 0, 1,  0 },
        { 0, 0, 2, 10 }, { 1, 0, 2, 11 }, { 2, 0, 2,  1 },
        { 0, 0, 3,  9 }, { 1, 0, 3, 10 }, { 2, 0, 3,  5 },
        { 0, 1, 0, 11 }, { 1, 1, 0,  9 }, { 2, 1, 0, 14 },
        { 0, 1, 1, 10 }, { 1, 1, 1, 11 }, { 2, 1, 1, 15 },
        { 0, 1, 2,  9 }, { 1, 1, 2, 13 }, { 2, 1, 2,  8 },
        { 0, 1, 3, 11 }, { 1, 0, 3, 12 }, { 2, 1, 3,  7 },
        { 0, 0, 0,  6 }, { 1, 1, 2,  6 }, { 2, 0, 1,  2 },
        { 0, 1, 2,  3 }, { 1, 0, 0, 15 }, { 2, 1, 0,  9 }
    };
    return kPlan;
}

}  // namespace apac_cases

#endif  // APAC_DSP_CASES_H
