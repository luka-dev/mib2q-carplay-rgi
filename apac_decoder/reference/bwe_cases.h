// The stage-e (bandwidth extension) differential-test plan, included by BOTH
// the oracle (reference/apac_bwe_ref.cpp, which calls the real library) and the
// host test (tests/test_bwe.cpp).  The committed fixtures therefore carry only
// the library's ANSWERS; the inputs are regenerated identically on both sides.
//
// Same arrangement as reference/lrvq_cases.h and reference/dsp_cases.h, and for
// the same reason: a case here is a whole 1024-coefficient spectrum plus a
// header, and shipping those would bury the expectations.
//
// The one thing this header does NOT hold is the scalefactor-band tables: each
// side gets those from its own apac::GetSFBOffsetTable (the library's or the
// port's) through BweCoreEdge, which the including translation unit defines.
// Retyping a table here would have made the two sides agree by construction.
//
// C++98-compatible, no STL: the oracle is built with the container's g++ and
// the host test with the port's -fno-exceptions -fno-rtti flags.

#ifndef APAC_BWE_CASES_H
#define APAC_BWE_CASES_H

#include <stdint.h>
#include <string.h>

// Defined by the includer, from its own copy of apac::GetSFBOffsetTable:
// the first coefficient of band `maxSfb`, times eight for a short block.
static uint32_t BweCoreEdge(uint32_t sampleRate, uint32_t windowSequence,
                            uint32_t maxSfb);

// xorshift64*, so the plan is identical on every host and compiler.
struct BweRandom {
    uint64_t s;
    void Seed(uint64_t v) { s = v ? v : 0x9e3779b97f4a7c15ULL; }
    uint32_t Next()
    {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return (uint32_t)(s >> 32);
    }
    uint32_t Below(uint32_t n) { return Next() % n; }
    // Uniform in [-1, 1) in exact 2^-20 steps, so both sides see the same
    // floats without either one rounding.
    float Signed()
    {
        return (float)((int32_t)(Next() & 0x1fffffu) - 0x100000) *
               (1.0f / 1048576.0f);
    }
};

enum { kBweLPCOrder = 16 };

// --------------------------------------------------------------------------
// 1. DecodeLSFsToLPCs - the two 9-bit codebook indices.
// --------------------------------------------------------------------------
//
// Both fields are 9 bits, so every legal pair is inside 512 x 512.  The corners
// are pinned explicitly and the rest is a sweep; index 0 of codebook 0 is the
// lowest LSF set there is and 511 the highest, so (0, 511) and (511, 0) are the
// two that drive Condition_LSFs' floor pass and its ceiling pass hardest.
enum { kBweLsfCases = 72 };

struct BweLsfCase {
    uint16_t idx[2];
};

static inline void BweBuildLsfCases(BweLsfCase* c)
{
    static const uint16_t kCorners[8][2] = {
        { 0, 0 }, { 511, 511 }, { 0, 511 }, { 511, 0 },
        { 1, 510 }, { 255, 256 }, { 256, 255 }, { 510, 1 },
    };
    int i;
    for (i = 0; i < 8; ++i) {
        c[i].idx[0] = kCorners[i][0];
        c[i].idx[1] = kCorners[i][1];
    }
    BweRandom r;
    r.Seed(0xb4f30ULL * 0x100000001b3ULL);
    for (; i < kBweLsfCases; ++i) {
        c[i].idx[0] = (uint16_t)r.Below(512);
        c[i].idx[1] = (uint16_t)r.Below(512);
    }
}

// --------------------------------------------------------------------------
// 2. LPCToMDCTSpectrum - one envelope per (LPC vector, n) pair.
// --------------------------------------------------------------------------
//
// The LPC vectors are built from reflection coefficients, so every one of them
// is minimum phase and the envelope cannot blow up: an arbitrary 17-float
// vector is almost never a stable predictor and would put infinities in the
// fixture instead of measuring anything.  The first six `n` values are exactly
// the six BWELPC::kDftSizes halves; the last two are values the switch does NOT
// name, which must therefore land on the 1024-point filter.
enum { kBweEnvVectors = 10, kBweEnvSizes = 8 };

static const uint32_t kBweEnvN[kBweEnvSizes] = {
    32, 48, 64, 256, 384, 512, /* not in the switch: */ 128, 96
};

static inline void BweBuildLPC(float* lpc, uint32_t which)
{
    BweRandom r;
    r.Seed(0x9e3779b9ULL * (which + 1) + 0xb1030ULL);
    float a[kBweLPCOrder + 1];
    a[0] = 1.0f;
    for (uint32_t m = 1; m <= (uint32_t)kBweLPCOrder; ++m) {
        const float k = r.Signed() * 0.95f;
        for (uint32_t i = 1; i <= m / 2; ++i) {
            const float lo = a[i], up = a[m - i];
            a[m - i] = up + k * lo;
            a[i] = lo + k * up;
        }
        a[m] = k;
    }
    for (uint32_t i = 0; i <= (uint32_t)kBweLPCOrder; ++i)
        lpc[i] = a[i];
}

// --------------------------------------------------------------------------
// 3. MDCTCoefficientsToLPC and 4. DecodeFrame - a whole spectrum and a header.
// --------------------------------------------------------------------------
//
// `maxSfb` is chosen against the 48 kHz and 24 kHz tables so that both sides of
// the one comparison the whole geometry hangs off - the band offset reaching
// 384 - are covered, in both block types:
//
//   48 kHz long   maxSfb 30 -> 352 (narrow)   maxSfb 31 -> 384 (wide)
//   48 kHz short  maxSfb  8 -> 352 (narrow)   maxSfb  9 -> 448 (wide)
//   24 kHz long   maxSfb 30 -> 260 (narrow)   maxSfb 40 -> 600 (wide)
//   24 kHz short  maxSfb  8 -> 288 (narrow)   maxSfb 12 -> 608 (wide)
//
// The spectrum is non-zero only below the core's own edge, which is what a real
// core decoder leaves behind, plus three deliberate degenerate shapes: an
// all-zero core (the find_if guard), a single non-zero bin, and a core whose
// content sits entirely in the top patch - the slice that gets replicated.
enum { kBweFrameCases = 44, kBweFrameLength = 1024, kBweScaleBands = 8 };

struct BweFrameCase {
    uint32_t sampleRate;
    uint32_t windowSequence;    // 2 == eight short windows
    uint32_t maxSfb;
    uint32_t windowGroupWord;   // one hex digit per group, counting windows
    uint16_t env[2];            // the two 9-bit LSF codebook indices
    uint32_t scale[kBweScaleBands];   // the 6-bit per-group gain indices
    uint32_t channel;           // the channel index the call is made for
    float    spec[kBweFrameLength];
};

static inline void BweFillSpectrum(BweFrameCase& c, uint32_t shape,
                                   uint32_t seed)
{
    BweRandom r;
    r.Seed(0x100000001b3ULL * (seed + 1) + 0x78490ULL);
    for (uint32_t i = 0; i < (uint32_t)kBweFrameLength; ++i)
        c.spec[i] = 0.0f;
    const uint32_t edge = BweCoreEdge(c.sampleRate, c.windowSequence, c.maxSfb);
    if (shape == 1)
        return;                                     // an all-zero core
    if (c.windowSequence == 2) {
        const uint32_t core = edge / 8;
        for (uint32_t w = 0; w < 8; ++w) {
            float* s = c.spec + w * 128;
            if (shape == 2) {                       // one non-zero bin, window 3
                if (w == 3 && core)
                    s[core - 1] = 0.5f;
                continue;
            }
            const uint32_t from = (shape == 3 && core > 16) ? core - 16 : 0;
            for (uint32_t i = from; i < core; ++i)
                s[i] = r.Signed() * 32.0f;
        }
        return;
    }
    if (shape == 2) {
        if (edge)
            c.spec[edge - 1] = 0.5f;
        return;
    }
    const uint32_t from = (shape == 3 && edge > 256) ? edge - 256 : 0;
    for (uint32_t i = from; i < edge; ++i)
        c.spec[i] = r.Signed() * 512.0f;
}

struct BweFramePlan {
    uint32_t rate;
    uint32_t seq;
    uint32_t sfb;
    uint32_t grp;
    uint32_t shape;
};

static const BweFramePlan kBweFramePlan[kBweFrameCases] = {
    // long blocks, narrow geometry (coreEnd < 384): bwStart 256, 4 x 128
    { 48000, 0, 30, 1, 0 }, { 48000, 0, 25, 1, 0 }, { 48000, 1, 30, 1, 0 },
    { 48000, 3, 20, 1, 0 }, { 48000, 0, 11, 1, 0 }, { 48000, 0,  1, 1, 0 },
    { 48000, 0, 30, 1, 1 }, { 48000, 0, 30, 1, 2 }, { 48000, 0, 30, 1, 3 },
    { 24000, 0, 30, 1, 0 }, { 24000, 1, 25, 1, 0 },
    // long blocks, wide geometry (coreEnd >= 384): bwStart 384, 2 x 256
    { 48000, 0, 31, 1, 0 }, { 48000, 0, 40, 1, 0 }, { 48000, 0, 48, 1, 0 },
    { 48000, 1, 35, 1, 0 }, { 48000, 3, 44, 1, 0 }, { 48000, 0, 49, 1, 0 },
    { 48000, 0, 40, 1, 2 }, { 48000, 0, 40, 1, 3 }, { 48000, 0, 31, 1, 1 },
    { 24000, 0, 40, 1, 0 }, { 24000, 0, 47, 1, 0 },
    // eight short, narrow (coreEnd < 384): bwStart 256, 4 x 16 per window
    { 48000, 2,  8, 0x00000008u, 0 },   // one group of eight
    { 48000, 2,  8, 0x11111111u, 0 },   // eight groups of one
    { 48000, 2,  8, 0x00000233u, 0 },   // 3 + 3 + 2
    { 48000, 2,  6, 0x00000044u, 0 },   // 4 + 4
    { 48000, 2,  4, 0x00001241u, 0 },   // 1 + 4 + 2 + 1
    { 48000, 2,  8, 0x00000008u, 1 },
    { 48000, 2,  8, 0x00000008u, 2 },
    { 48000, 2,  8, 0x11111111u, 3 },
    { 48000, 2,  2, 0x00000008u, 0 },
    { 24000, 2,  8, 0x00000233u, 0 },
    // eight short, wide (coreEnd >= 384): bwStart 384, 2 x 32 per window
    { 48000, 2,  9, 0x00000008u, 0 },
    { 48000, 2, 12, 0x11111111u, 0 },
    { 48000, 2, 14, 0x00000233u, 0 },
    { 48000, 2, 10, 0x00000044u, 0 },
    { 48000, 2, 11, 0x00001241u, 0 },
    { 48000, 2, 14, 0x00000008u, 2 },
    { 48000, 2,  9, 0x11111111u, 3 },
    { 48000, 2, 12, 0x00000008u, 1 },
    { 24000, 2, 12, 0x00000008u, 0 },
    { 24000, 2, 15, 0x11111111u, 0 },
    // maxSfb 0 - the second half of the `!active || maxSfb == 0` gate
    { 48000, 0,  0, 1, 0 },
    { 48000, 2,  0, 0x00000008u, 0 },
};

static inline void BweBuildFrameCases(BweFrameCase* c)
{
    BweRandom r;
    r.Seed(0xb1a5edULL);
    for (int i = 0; i < kBweFrameCases; ++i) {
        c[i].sampleRate = kBweFramePlan[i].rate;
        c[i].windowSequence = kBweFramePlan[i].seq;
        c[i].maxSfb = kBweFramePlan[i].sfb;
        c[i].windowGroupWord = kBweFramePlan[i].grp;
        c[i].env[0] = (uint16_t)r.Below(512);
        c[i].env[1] = (uint16_t)r.Below(512);
        for (int b = 0; b < kBweScaleBands; ++b)
            c[i].scale[b] = r.Below(64);
        // Spread the calls over channels so that a per-channel BWELPC (the
        // original) and one shared instance (the port) are distinguishable if
        // anything ever did carry across a call.
        c[i].channel = (uint32_t)(i % 6);
        BweFillSpectrum(c[i], kBweFramePlan[i].shape, (uint32_t)i);
    }
    // Four cases pinned onto the ends of the 6-bit gain table, which the sweep
    // above only samples: index 0 is 1.2e-04 and index 63 is 1.647e+02.
    for (int b = 0; b < kBweScaleBands; ++b) {
        c[0].scale[b] = 0;
        c[11].scale[b] = 63;
        c[22].scale[b] = (uint32_t)b * 9u;          // 0, 9, 18 ... 63
        c[32].scale[b] = 63u - (uint32_t)b * 9u;
    }
}

#endif  // APAC_BWE_CASES_H
