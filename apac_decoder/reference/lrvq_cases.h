// The LRVQ differential-test plan, included by BOTH the oracle
// (reference/apac_lrvq_ref.cpp, which calls the real library) and the host test
// (tests/test_lrvq.cpp).  The committed fixtures therefore carry only the
// library's ANSWERS; the inputs are regenerated identically on both sides.
//
// Same arrangement as reference/dsp_cases.h for stage d, and for the same
// reason: a case here is a whole bitstream plus the buffer state the call
// starts from, and shipping those would bury the expectations.
//
// C++98-compatible, no STL: the oracle is built with the container's g++ and
// the host test with the port's -fno-exceptions -fno-rtti flags.

#ifndef APAC_LRVQ_CASES_H
#define APAC_LRVQ_CASES_H

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------- writer ----
struct LrvqBitWriter {
    uint8_t* buf;
    uint32_t cap;
    uint32_t pos;          // in bits

    void Init(uint8_t* b, uint32_t n)
    {
        buf = b; cap = n; pos = 0; memset(b, 0, n);
    }
    void Put(uint32_t v, uint32_t n)
    {
        for (int i = (int)n - 1; i >= 0; --i) {
            if ((v >> i) & 1u)
                buf[pos >> 3] |= (uint8_t)(0x80u >> (pos & 7));
            ++pos;
        }
    }
};

// xorshift64*, so the plan is identical on every host and compiler.
struct LrvqRandom {
    uint64_t s;
    void Seed(uint64_t v) { s = v ? v : 0x9e3779b97f4a7c15ULL; }
    uint32_t Next()
    {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return (uint32_t)(s >> 32);
    }
    uint32_t Below(uint32_t n) { return Next() % n; }
};

// ------------------------------------------------------------- mode table ----
// bits0/bits1 for the 16 VQ mode rows, duplicated here so the plan can build a
// syntactically valid stream without depending on either side's copy of the
// table.  Checked against apac::kLrvqModes by the host test.
struct LrvqCaseMode { int bits0, n0, bits1, n1; };
static const LrvqCaseMode kLrvqCaseModes[16] = {
    { 6,  64, 0,   0 }, { 7, 128, 0,   0 }, { 8, 256, 0,   0 }, { 9, 512, 0,   0 },
    { 6,  64, 5,  32 }, { 7, 128, 5,  32 }, { 7, 128, 6,  64 }, { 8, 256, 6,  64 },
    { 8, 256, 7, 128 }, { 9, 512, 7, 128 }, { 9, 512, 8, 256 }, { 0,   0, 0,   0 },
    { 0,   0, 0,   0 }, { 0,   0, 0,   0 }, { 0,   0, 0,   0 }, { 0,   0, 0,   0 },
};

// ----------------------------------------------------------------- a case ----
enum { kLrvqMaxBytes = 256, kLrvqMaxCalls = 2 };

struct LrvqCall {
    int isShort;
    int firstChannel;
};

struct LrvqCase {
    const char* name;
    int         nCalls;
    LrvqCall    call[kLrvqMaxCalls];
    uint8_t     bits[kLrvqMaxBytes];
    uint32_t    nbytes;
    // The state the first call starts from.  The library reads all of it.
    float       env[1024];
    float       env2[1024];
    float       work[256];
    int16_t     qidx[64];
    uint16_t    prevMode;
    // Bookkeeping the test prints, not input.
    int         vqMode[kLrvqMaxCalls];
    int         shortLsfBooks;
    int         ltp;
    int         usesEnvelopeBuilder;   // 1 if any call runs the LSF/LTP FFTs
    int         layout;                // -1 none, 0 interleaved, 1 reversed
};

// Fill env/env2/work/qidx/prevMode with something the library will actually
// divide by: a spread of magnitudes, both signs, never zero.
inline void LrvqSeedBuffers(LrvqCase& c, uint64_t seed)
{
    LrvqRandom r; r.Seed(seed);
    for (int i = 0; i < 1024; ++i) {
        c.env[i] = 0.05f + (float)(r.Next() & 0xffff) / 8192.0f;
        c.env2[i] = 0.25f + (float)(r.Next() & 0x3fff) / 16384.0f;
    }
    for (int i = 0; i < 256; ++i)
        c.work[i] = ((float)(int32_t)(r.Next() & 0xffff) - 32768.0f) / 256.0f;
    for (int i = 0; i < 64; ++i)
        c.qidx[i] = (int16_t)(r.Next() & 31);
    c.prevMode = 0;
}

// Emit one channel's payload.  `sameMode` says whether the decoder will treat
// this channel's mode as matching the previous one - the reuse flags only exist
// then, and the writer has to agree with the decoder about that or the stream
// desynchronises.
inline void LrvqEmitChannel(LrvqBitWriter& w, LrvqRandom& r, LrvqCase& c,
                            int callIdx, int isShort, int firstChannel,
                            int reuseEnv, int noLtpSection, int reuseResidual,
                            int shortLsfBooks, int ltpPresent, int vqMode,
                            int sameMode)
{
    w.Put(r.Next() & 1, 1);            // gainMode
    w.Put(r.Below(512), 9);            // gainIndex
    if (!firstChannel) {
        w.Put((uint32_t)reuseEnv, 1);
        if (!isShort)
            w.Put((uint32_t)noLtpSection, 1);
        w.Put((uint32_t)reuseResidual, 1);
    }
    if (firstChannel || !reuseEnv) {
        for (int i = 0; i < 5; ++i)
            w.Put(r.Below(512), 9);
        if (isShort)
            w.Put((uint32_t)shortLsfBooks, 1);
        c.usesEnvelopeBuilder = 1;
        if (isShort)
            c.layout = shortLsfBooks;
        c.shortLsfBooks = shortLsfBooks;
    } else if (isShort) {
        c.layout = 0;
    }
    int runVQ = 1;
    if (!isShort && !noLtpSection) {
        w.Put((uint32_t)ltpPresent, 1);
        if (ltpPresent) {
            w.Put(r.Below(128), 7);
            w.Put(r.Below(1024), 10);
            c.usesEnvelopeBuilder = 1;
            c.ltp = 1;
            if (reuseResidual)
                runVQ = 0;
        } else if (reuseResidual) {
            runVQ = 0;
        }
    } else if (reuseResidual) {
        runVQ = 0;
    }
    c.vqMode[callIdx] = runVQ ? vqMode : -1;
    if (!runVQ)
        return;
    w.Put((uint32_t)vqMode, 4);
    const LrvqCaseMode& m = kLrvqCaseModes[vqMode];
    if (!m.bits0 && !m.bits1)
        return;                        // an invalid row: the decoder returns 1
    for (int k = 0; k < 32; ++k) {
        if (sameMode) {
            if (r.Next() & 1) { w.Put(1, 1); }
            else { w.Put(0, 1); w.Put(r.Below((uint32_t)m.n0), (uint32_t)m.bits0); }
        } else {
            w.Put(r.Below((uint32_t)m.n0), (uint32_t)m.bits0);
        }
        if (m.bits1) {
            if (sameMode) {
                if (r.Next() & 1) { w.Put(1, 1); }
                else { w.Put(0, 1); w.Put(r.Below((uint32_t)m.n1), (uint32_t)m.bits1); }
            } else {
                w.Put(r.Below((uint32_t)m.n1), (uint32_t)m.bits1);
            }
        }
    }
}

// ----------------------------------------------------------------- the plan --
//
// Two families.
//
//   * "exact" cases never enter lsfsToMdctSpectralEnvelope or
//     ltpToMdctSpectralEnvelope, because every call sets reuseEnvelope and
//     (for a long block) noLtpSection.  There is no FFT anywhere under them,
//     so the port has to match the library BIT FOR BIT - including the
//     envelope division, which runs against the seeded env, not against 1.0.
//   * "full" cases run the envelope builders, so they agree only to float
//     noise.  They are what covers the LSF chain, LTP, and the reversed short
//     layout, which is only reachable from a channel that read its own LSFs.
//
inline int LrvqBuildCases(LrvqCase* out, int max)
{
    int n = 0;
    LrvqRandom r;

    // -- exact: every valid VQ mode, long, channel 1, envelope reused --------
    for (int mode = 0; mode < 16 && n < max; ++mode) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "exact-long-mode";
        c.layout = -1;
        LrvqSeedBuffers(c, 0x100 + mode);
        r.Seed(0x2000 + mode);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        c.nCalls = 1;
        c.call[0].isShort = 0;
        c.call[0].firstChannel = 0;
        LrvqEmitChannel(w, r, c, 0, 0, 0, /*reuseEnv*/1, /*noLtpSection*/1,
                        /*reuseResidual*/0, 0, 0, mode, /*sameMode*/0);
        c.prevMode = (uint16_t)((mode + 1) & 15);   // deliberately NOT sameMode
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- exact: every valid VQ mode, eight short windows, layout 0 -----------
    for (int mode = 0; mode < 11 && n < max; ++mode) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "exact-short-mode";
        c.layout = -1;
        LrvqSeedBuffers(c, 0x300 + mode);
        r.Seed(0x4000 + mode);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        c.nCalls = 1;
        c.call[0].isShort = 1;
        c.call[0].firstChannel = 0;
        LrvqEmitChannel(w, r, c, 0, 1, 0, 1, 0, 0, 0, 0, mode, 0);
        c.prevMode = (uint16_t)((mode + 1) & 15);
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- exact: sameMode inter-channel reuse, every two-stage mode -----------
    for (int mode = 0; mode < 11 && n < max; ++mode) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "exact-samemode-reuse";
        c.layout = -1;
        LrvqSeedBuffers(c, 0x500 + mode);
        r.Seed(0x6000 + mode);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        c.nCalls = 1;
        c.call[0].isShort = mode & 1;
        c.call[0].firstChannel = 0;
        c.prevMode = (uint16_t)mode;                // sameMode holds
        LrvqEmitChannel(w, r, c, 0, c.call[0].isShort, 0, 1,
                        c.call[0].isShort ? 0 : 1, 0, 0, 0, mode,
                        /*sameMode*/1);
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- exact: reuseResidual, both block shapes -----------------------------
    for (int i = 0; i < 2 && n < max; ++i) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "exact-reuse-residual";
        c.layout = -1;
        LrvqSeedBuffers(c, 0x700 + i);
        r.Seed(0x8000 + i);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        c.nCalls = 1;
        c.call[0].isShort = i;
        c.call[0].firstChannel = 0;
        LrvqEmitChannel(w, r, c, 0, i, 0, 1, i ? 0 : 1, /*reuseResidual*/1,
                        0, 0, 4, 0);
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- full: channel 0, long, with and without LTP -------------------------
    for (int i = 0; i < 8 && n < max; ++i) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "full-long";
        c.layout = -1;
        LrvqSeedBuffers(c, 0x900 + i);
        r.Seed(0xa000 + i);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        c.nCalls = 1;
        c.call[0].isShort = 0;
        c.call[0].firstChannel = 1;
        LrvqEmitChannel(w, r, c, 0, 0, 1, 0, 0, 0, 0, /*ltpPresent*/i & 1,
                        i % 11, 0);
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- full: channel 0, eight short windows, BOTH output layouts -----------
    for (int i = 0; i < 8 && n < max; ++i) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "full-short";
        c.layout = -1;
        LrvqSeedBuffers(c, 0xb00 + i);
        r.Seed(0xc000 + i);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        c.nCalls = 1;
        c.call[0].isShort = 1;
        c.call[0].firstChannel = 1;
        LrvqEmitChannel(w, r, c, 0, 1, 1, 0, 0, 0, /*shortLsfBooks*/i & 1, 0,
                        i % 11, 0);
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- full: a whole channel pair, both channels, every reuse combination --
    for (int i = 0; i < 24 && n < max; ++i) {
        LrvqCase& c = out[n];
        memset(&c, 0, sizeof c);
        c.name = "full-pair";
        c.layout = -1;
        LrvqSeedBuffers(c, 0xd00 + i);
        r.Seed(0xe000 + i);
        LrvqBitWriter w; w.Init(c.bits, kLrvqMaxBytes);
        const int isShort = (i >> 4) & 1;
        const int reuseEnv = i & 1;
        const int reuseResidual = (i >> 1) & 1;
        const int noLtpSection = (i >> 2) & 1;
        const int same = (i >> 3) & 1;
        const int mode0 = i % 11;
        const int mode1 = same ? mode0 : (mode0 + 3) % 11;
        c.nCalls = 2;
        c.call[0].isShort = isShort;
        c.call[0].firstChannel = 1;
        c.call[1].isShort = isShort;
        c.call[1].firstChannel = 0;
        LrvqEmitChannel(w, r, c, 0, isShort, 1, 0, 0, 0, i & 1, (i >> 2) & 1,
                        mode0, 0);
        LrvqEmitChannel(w, r, c, 1, isShort, 0, reuseEnv, noLtpSection,
                        reuseResidual, (i >> 1) & 1, (i >> 3) & 1, mode1,
                        /*sameMode*/mode1 == mode0);
        c.nbytes = kLrvqMaxBytes;
        ++n;
    }

    return n;
}

// ------------------------------------------------------- the element wrappers --
//
// SpectrumDeserialize{SCE,CPE,LFE} each read one skip bit per channel before
// that channel's payload.  These build the whole element stream so the wrapper,
// not just LrvqDeserialize, is what is compared.
//
// One combination is deliberately absent: a CPE whose FIRST channel carries the
// skip bit and whose second does not.  The library never initialises the qidx
// and prevMode it puts on its own stack (0x9bf60 has no clear loop and no
// memset - only the stack-clash probes), so channel 1 would then predict from
// whatever the stack held.  That is not reproducible and not something to
// encode in a fixture; the port closes it by zeroing, which is what
// SpectrumDeserializeSCE itself does.  See ResetPrediction in src/lrvq.cpp.

enum { kLrvqElemSCE = 0, kLrvqElemCPE = 1, kLrvqElemLFE = 2 };

struct LrvqElemCase {
    const char* name;
    int         kind;
    int         skip0;
    int         skip1;
    int         isShort0;
    int         isShort1;
    uint8_t     bits[kLrvqMaxBytes];
    uint32_t    nbytes;
};

inline int LrvqBuildElemCases(LrvqElemCase* out, int max)
{
    int n = 0;
    LrvqRandom r;
    LrvqCase scratch;

    // -- SCE: skipped and not, both block shapes, several modes --------------
    for (int i = 0; i < 12 && n < max; ++i) {
        LrvqElemCase& e = out[n];
        memset(&e, 0, sizeof e);
        e.name = "sce";
        e.kind = kLrvqElemSCE;
        e.skip0 = i & 1;
        e.isShort0 = (i >> 1) & 1;
        memset(&scratch, 0, sizeof scratch);
        r.Seed(0x11000 + i);
        LrvqBitWriter w; w.Init(e.bits, kLrvqMaxBytes);
        w.Put((uint32_t)e.skip0, 1);
        LrvqEmitChannel(w, r, scratch, 0, e.isShort0, 1, 0, 0, 0,
                        (i >> 2) & 1, (i >> 2) & 1, i % 12, 0);
        e.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- CPE: channel 0 always present, channel 1 skipped and not ------------
    for (int i = 0; i < 16 && n < max; ++i) {
        LrvqElemCase& e = out[n];
        memset(&e, 0, sizeof e);
        e.name = "cpe";
        e.kind = kLrvqElemCPE;
        e.skip0 = 0;
        e.skip1 = i & 1;
        e.isShort0 = (i >> 1) & 1;
        e.isShort1 = e.isShort0;
        const int mode0 = i % 11;
        const int mode1 = (i & 4) ? mode0 : (mode0 + 5) % 11;
        memset(&scratch, 0, sizeof scratch);
        r.Seed(0x12000 + i);
        LrvqBitWriter w; w.Init(e.bits, kLrvqMaxBytes);
        w.Put(0, 1);
        LrvqEmitChannel(w, r, scratch, 0, e.isShort0, 1, 0, 0, 0,
                        (i >> 3) & 1, (i >> 3) & 1, mode0, 0);
        w.Put((uint32_t)e.skip1, 1);
        if (!e.skip1)
            LrvqEmitChannel(w, r, scratch, 1, e.isShort1, 0, (i >> 2) & 1,
                            e.isShort1 ? 0 : ((i >> 3) & 1), 0, 0, (i >> 3) & 1,
                            mode1, mode1 == mode0);
        e.nbytes = kLrvqMaxBytes;
        ++n;
    }

    // -- LFE: every mode row, including the five invalid ones ----------------
    for (int i = 0; i < 32 && n < max; ++i) {
        LrvqElemCase& e = out[n];
        memset(&e, 0, sizeof e);
        e.name = "lfe";
        e.kind = kLrvqElemLFE;
        e.skip0 = i >> 4;
        const int mode = i & 15;
        r.Seed(0x13000 + i);
        LrvqBitWriter w; w.Init(e.bits, kLrvqMaxBytes);
        w.Put((uint32_t)e.skip0, 1);
        w.Put(r.Next() & 1, 1);              // gainMode
        w.Put(r.Below(512), 9);              // gainIndex
        w.Put((uint32_t)mode, 4);
        const LrvqCaseMode& m = kLrvqCaseModes[mode];
        if (m.bits0 || m.bits1) {
            for (int k = 0; k < 4; ++k) {
                w.Put(r.Below((uint32_t)m.n0), (uint32_t)m.bits0);
                if (m.bits1)
                    w.Put(r.Below((uint32_t)m.n1), (uint32_t)m.bits1);
            }
        }
        e.nbytes = kLrvqMaxBytes;
        ++n;
    }
    return n;
}

// ---------------------------------------------------------- the LRVQ frame ----
//
// APACChannelPairElement::DeserializeLrvqFrame: two ICS headers at coding type
// 1, the CPE spectrum, then one TNS block per channel and the odd extra bit
// between them.  An LRVQ ICS header is exactly two bits - coding type 1 fixes
// maxSfb at 27 long / 7 short and reads no grouping field - so the whole frame
// is those four bits, the element payload, and the TNS section.

struct LrvqFrameCase {
    const char* name;
    int         ws0;          // window sequence, 2 = eight short windows
    int         ws1;
    int         tns0;         // TNS block present
    int         tns1;
    int         tnsFilter;    // 1 = a real filter rather than a zero count
    uint8_t     bits[kLrvqMaxBytes];
    uint32_t    nbytes;
};

// One TNS block, long or short.  `filter` picks between a zero filter count -
// the shortest thing that parses - and one real order-4 filter.
inline void LrvqEmitTNS(LrvqBitWriter& w, int isShort, int filter)
{
    if (isShort) {
        for (int win = 0; win < 8; ++win) {
            w.Put(filter ? 1u : 0u, 1);
            if (!filter)
                continue;
            w.Put(1, 1);                 // coefRes
            w.Put((5u << 3) | 3u, 7);    // length 5, order 3
            w.Put(0, 2);                 // direction 0, coefCompress 0
            for (int j = 0; j < 3; ++j)
                w.Put((uint32_t)(j + 1) & 15u, 4);   // 3 + coefRes - 0 bits
        }
        return;
    }
    if (!filter) {
        w.Put(0, 2);                     // zero filters
        return;
    }
    w.Put(1, 2);                         // one filter
    w.Put(1, 1);                         // coefRes
    w.Put((8u << 5) | 4u, 11);           // length 8, order 4
    w.Put(0, 2);                         // direction 0, coefCompress 0
    for (int j = 0; j < 4; ++j)
        w.Put((uint32_t)(j + 1) & 15u, 4);
}

inline int LrvqBuildFrameCases(LrvqFrameCase* out, int max)
{
    int n = 0;
    LrvqRandom r;
    LrvqCase scratch;
    for (int i = 0; i < 24 && n < max; ++i) {
        LrvqFrameCase& e = out[n];
        memset(&e, 0, sizeof e);
        e.name = "frame";
        const int isShort = (i >> 2) & 1;
        e.ws0 = isShort ? 2 : (i & 1);          // 0 long start, 1 long, 2 short
        e.ws1 = e.ws0;
        e.tns0 = i & 1;
        e.tns1 = (i >> 1) & 1;
        e.tnsFilter = (i >> 3) & 1;
        const int mode0 = i % 11;
        const int mode1 = (i & 4) ? mode0 : (mode0 + 6) % 11;
        memset(&scratch, 0, sizeof scratch);
        r.Seed(0x21000 + i);
        LrvqBitWriter w; w.Init(e.bits, kLrvqMaxBytes);
        w.Put((uint32_t)e.ws0, 2);
        w.Put((uint32_t)e.ws1, 2);
        w.Put(0, 1);                             // channel 0 is present
        LrvqEmitChannel(w, r, scratch, 0, isShort, 1, 0, 0, 0, (i >> 3) & 1,
                        (i >> 3) & 1, mode0, 0);
        w.Put(0, 1);                             // channel 1 is present
        LrvqEmitChannel(w, r, scratch, 1, isShort, 0, (i >> 2) & 1,
                        isShort ? 0 : ((i >> 4) & 1), 0, 0, (i >> 4) & 1, mode1,
                        mode1 == mode0);
        w.Put((uint32_t)e.tns0, 1);
        if (e.tns0)
            LrvqEmitTNS(w, isShort, e.tnsFilter);
        w.Put((uint32_t)e.tns1, 1);
        if (e.tns1)
            LrvqEmitTNS(w, isShort, e.tnsFilter);
        // If channel 1 has no TNS block and channel 0 did, the original reads
        // one more bit and throws it away.  Nothing has to be written for it -
        // the padding is already there - but the case exists to catch a port
        // that does not read it.
        e.nbytes = kLrvqMaxBytes;
        ++n;
    }
    return n;
}

// ------------------------------------------------- the two envelope builders --
// dequantizeLrvqGain: every input there is.
// ltpToMdctSpectralEnvelope: a sweep that INCLUDES lag 0, which overwrites the
// impulse's leading 1.0 and which the earlier probe never tried.
// lsfsToMdctSpectralEnvelope: both codebook sets, corners and random rows.
inline void LrvqLsfCase(int i, uint16_t idx[5], int* shortBooks)
{
    *shortBooks = i & 1;
    if (i < 4) {
        const uint16_t v = (i < 2) ? 0 : 511;
        for (int k = 0; k < 5; ++k) idx[k] = v;
        return;
    }
    LrvqRandom r; r.Seed(0x51f0000ull + (uint64_t)i);
    for (int k = 0; k < 5; ++k) idx[k] = (uint16_t)r.Below(512);
}
enum { kLrvqLsfCases = 64 };

inline void LrvqLtpCase(int i, uint16_t* gainIdx, uint16_t* lag)
{
    static const uint16_t kLags[8] = { 0, 1, 2, 3, 127, 512, 1022, 1023 };
    if (i < 16) { *gainIdx = (uint16_t)((i >> 3) ? 127 : 0); *lag = kLags[i & 7]; return; }
    LrvqRandom r; r.Seed(0x17a0000ull + (uint64_t)i);
    *gainIdx = (uint16_t)r.Below(128);
    *lag = (uint16_t)r.Below(1024);
}
enum { kLrvqLtpCases = 96 };

#endif  // APAC_LRVQ_CASES_H
