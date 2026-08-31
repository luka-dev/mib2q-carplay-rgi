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

// -------------------------------------------------------------- TNS plan ----
// One entry per APACTNSData::ParseTNSData + Apply pair.
//
// The bit payload is pseudo-random rather than hand-built, which is deliberate:
// TNS's own fields are wide enough that a random buffer is a legal filter set
// most of the time (a long block's band length is six bits and only zero is
// rejected), while the cases that do trip the zero-length rejection exercise
// the error path for free.  A parse that fails still leaves a partly filled
// object, and Apply is run on it anyway - both sides see the same bytes, so
// that is a real comparison and not an accident.
//
// windowSequence 2 means eight short windows; anything else is a long block.
// maxSfb is what the syntactic element would have taken from APACICSInfo, and
// it clamps the TNS band cap (40 long / 14 short at 48 kHz).
struct TNSCase {
    uint8_t  windowSequence;
    uint8_t  maxSfb;
    uint8_t  spectrumCase;   // index into case_fill, 1024 floats
    uint8_t  bitSeed;
};

enum { kTNSCases = 32, kTNSBitBytes = 64 };

inline const TNSCase* tns_plan()
{
    static const TNSCase kPlan[kTNSCases] = {
        { 0, 49,  9,  1 }, { 0, 49, 10,  2 }, { 0, 49, 11,  3 }, { 0, 49, 14,  4 },
        { 0, 40,  9,  5 }, { 0, 30, 10,  6 }, { 0, 20, 11,  7 }, { 0, 10, 15,  8 },
        { 0,  1,  9,  9 }, { 0,  0, 10, 10 }, { 0, 49,  8, 11 }, { 0, 49,  6, 12 },
        { 0, 49,  7, 13 }, { 0, 49,  0, 14 }, { 0, 49,  4, 15 }, { 0, 49,  5, 16 },
        { 2, 14,  9, 17 }, { 2, 14, 10, 18 }, { 2, 14, 11, 19 }, { 2, 14, 14, 20 },
        { 2, 10,  9, 21 }, { 2,  8, 10, 22 }, { 2,  4, 11, 23 }, { 2,  1, 15, 24 },
        { 2,  0,  9, 25 }, { 2, 14,  8, 26 }, { 2, 14,  6, 27 }, { 2, 14,  7, 28 },
        { 2, 14,  0, 29 }, { 2, 14,  4, 30 }, { 2, 14,  5, 31 }, { 2, 14, 12, 32 }
    };
    return kPlan;
}

inline void tns_fill_bits(unsigned c, uint8_t* buf, uint32_t len)
{
    uint32_t s = 0x243f6a88u ^ (tns_plan()[c].bitSeed * 2246822519u);
    if (s == 0) s = 1;
    for (uint32_t i = 0; i < len; ++i)
        buf[i] = (uint8_t)(xorshift32(&s) >> 19);
}

// -------------------------------------------------------------- LPC plan ----
// Inputs for apac::LPCBase and the stateless half of apac::LSFTools.
//
// These are not arbitrary noise.  Levinson-Durbin needs a positive-definite
// autocorrelation or it divides by a negative error; LSP_To_LPC needs line
// spectral pairs that are actually a descending sequence in (-1, 1); and
// Condition_LSFs is only interesting on line spectral frequencies that are
// ascending and inside (0, fs/2).  So each generator produces the right SHAPE
// and randomises inside it, and the autocorrelation is derived from a real
// signal rather than made up, which guarantees it is positive definite.
//
// The deliberately hostile cases are still there: `order` runs down to 2, and
// the `tight` cases bunch the LSFs far below Condition_LSFs' 50 Hz minimum and
// push the last one up against fs/2 so both of its clamping passes run.
struct LPCCase {
    uint8_t order;      // even, 2 .. 16
    uint8_t seed;
    uint8_t tight;
};

enum { kLPCCases = 16, kLPCSignal = 256 };

inline const LPCCase* lpc_plan()
{
    static const LPCCase kPlan[kLPCCases] = {
        {  2, 1, 0 }, {  4, 2, 0 }, {  6, 3, 0 }, {  8, 4, 0 },
        { 10, 5, 0 }, { 12, 6, 0 }, { 14, 7, 0 }, { 16, 8, 0 },
        {  2, 9, 1 }, {  4,10, 1 }, {  8,11, 1 }, { 10,12, 1 },
        { 12,13, 1 }, { 16,14, 1 }, { 16,15, 0 }, {  6,16, 1 }
    };
    return kPlan;
}

inline uint32_t lpc_seed(unsigned c, uint32_t salt)
{
    uint32_t s = 0xb7e15163u ^ (lpc_plan()[c].seed * 2654435761u) ^ salt;
    return s ? s : 1u;
}

// A plain random signal; the autocorrelation below is taken from it.
inline void lpc_fill_signal(unsigned c, float* x, uint32_t n)
{
    uint32_t s = lpc_seed(c, 0x51ed270bu);
    for (uint32_t i = 0; i < n; ++i) x[i] = rnd_unit(&s);
}

// An analysis window: symmetric, built from integers plus a little noise so it
// is not accidentally a special case.
inline void lpc_fill_window(unsigned c, float* w, uint32_t n)
{
    uint32_t s = lpc_seed(c, 0x2b992ddfu);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t d = (i < n / 2) ? i : (n - 1 - i);
        w[i] = (float)(2 * d + 1) / (float)n + 0.03125f * rnd_unit(&s);
    }
}

// r[k] = sum_n x[n] * x[n+k], positive definite by construction.
inline void lpc_fill_autocorr(unsigned c, float* r, uint32_t order)
{
    float x[kLPCSignal + 64];
    uint32_t i, k;
    lpc_fill_signal(c, x, kLPCSignal);
    for (i = 0; i < 64; ++i) x[kLPCSignal + i] = 0.0f;
    for (k = 0; k <= order; ++k) {
        float acc = 0.0f;
        for (i = 0; i < kLPCSignal; ++i) acc += x[i] * x[i + k];
        r[k] = acc;
    }
    r[0] *= 1.0001f;                 // the ridge the original applies anyway
}

// Ascending LSFs in (0, fs/2).  `tight` bunches them so Condition_LSFs has
// something to do and pushes the last one up against the top clamp.
inline void lpc_fill_lsf(unsigned c, float* lsf, uint32_t order, float fs)
{
    uint32_t s = lpc_seed(c, 0x9d2c5680u);
    const float span = fs * 0.5f;
    const float tight = lpc_plan()[c].tight ? 0.06f : 1.0f;
    float v = span * 0.01f;
    for (uint32_t i = 0; i < order; ++i) {
        v += tight * span / (float)(order + 1) *
             (0.4f + 0.6f * (0.5f * (rnd_unit(&s) + 1.0f)));
        lsf[i] = v;
    }
    if (lpc_plan()[c].tight && order)
        lsf[order - 1] = span - 3.0f;
}

// Descending LSPs in (-1, 1).  cos() of an ascending LSF run would do it too,
// but keeping the generator free of trig means both sides agree exactly.
inline void lpc_fill_lsp(unsigned c, float* lsp, uint32_t order)
{
    uint32_t s = lpc_seed(c, 0x1f123bb5u);
    float v = 0.97f;
    for (uint32_t i = 0; i < order; ++i) {
        v -= (1.9f / (float)(order + 1)) *
             (0.5f + 0.5f * (0.5f * (rnd_unit(&s) + 1.0f)));
        lsp[i] = v;
    }
}

}  // namespace apac_cases

#endif  // APAC_DSP_CASES_H
