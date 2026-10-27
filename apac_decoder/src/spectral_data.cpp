/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACSpectralData::Deserialize, 0x000ab3b0, 3820 bytes, 950 instructions.
//
// Transcribed from reference/decomp/APACSpectralData.c with the codebook
// semantics of reference/decomp/LRVQ_SEMANTICS.md sections 1 and 2.
//
// One pass per window group, one pass per (codebook, run) record the
// scalefactor stage left behind, one tuple at a time out of a local 32-bit
// window.  Five decode families, by codebook index:
//
//   1, 2   4 coefficients, signed 2-bit fields at r>>14/12/10/8, no sign bits
//   3, 4   4 coefficients, magnitude 0..2 at the same positions, one sign bit
//          from the stream after each NON-ZERO magnitude, in order
//   5, 6   2 coefficients, signed 4-bit fields at r>>11 and r>>6
//   7..10  2 coefficients, magnitude 0..12 at r>>11 and r>>6, one sign each
//   11     2 coefficients, magnitude 0..16 at r>>11 and r>>6, 16 = escape;
//          BOTH sign bits are read before EITHER escape
//
// The odd books 1, 3, 5, 7, 9 put a 1-bit flag in front of each tuple: a zero
// means the whole tuple is zero and costs exactly that one bit; a one is the
// first bit of the Huffman code.  It is the same `0` code that entries[0] of
// those books carries, hoisted into a fast path.  Book 11 has no such flag.
//
// Every coefficient is X[q] * scalefactor, where X is the i^(4/3) table at
// 0x003308c0.  Books 1..6 index it with NEGATIVE q; the library's table has a
// mirrored negative wing in front of it and our decoder copies that wing rather
// than branching on the sign.  Escapes above 1023 use pow(v, 4/3) instead.
//
// Eight-short-window frames are coded band-interleaved.  Groups longer than one
// window are staged into a scratch buffer and unpacked window-major at the end.

#include "apac/spectrum.h"

#include <math.h>
#include <string.h>

#include "apac/huffman.h"

namespace apac {

namespace {

// The largest spectrum the decoder has: offset[numBands] is 1024 for a long
// block, and a short block stages at most 8 x 128.  The library's scratch is
// 4104 bytes of stack; this is the same buffer, sized in floats.
const uint32_t kScratchFloats = 1026;

// The local bit window both this function and APACScaleFactorData open.  It
// over-reads on purpose and hands the surplus back with one PutBack at the end.
struct Window {
    BitstreamReader* bs;
    uint32_t         w;
    int32_t          avail;

    void need(int32_t k)
    {
        if (avail < k) {
            if (avail < 32)
                w |= bs->GetBits(32 - (uint32_t)avail);
            avail = 32;
        }
    }

    // True when the tuple's leading bit is a zero, i.e. the all-zero tuple.
    // Consumes that bit; the caller must already have called need().
    bool zeroTuple(bool flagged)
    {
        if (!flagged || (int32_t)w < 0)
            return false;
        w <<= 1;
        --avail;
        return true;
    }

    uint16_t code(const HuffTable* t, uint32_t clamp)
    {
        const int k = __builtin_clz(clamp | ~w);
        const HuffEntry& e = t->entries[k];
        const uint16_t r = e.payload[(uint32_t)(w << k) >> e.shift];
        avail -= (int32_t)(r & 0x3f);
        w <<= (r & 0x3f);
        return r;
    }

    bool sign()
    {
        const bool neg = (int32_t)w < 0;
        w <<= 1;
        --avail;
        return neg;
    }

    // Codebook 11's inlined Elias-gamma-like escape.  NOT apac::GetEscBits,
    // which is a different, three-stage escape used by the config parsers.
    uint32_t escape()
    {
        if (avail <= 20) {
            if (avail < 32)
                w |= bs->GetBits(32 - (uint32_t)avail);
            avail = 32;
        }
        const uint32_t k  = (uint32_t)__builtin_clz(~w | 0x800000u);
        const uint32_t nb = k + 4;
        const uint32_t t  = w << (k + 1);
        avail -= (int32_t)(k + 1 + nb);
        const uint32_t v = (1u << nb) + (t >> ((32 - nb) & 31));
        w = t << nb;
        return v;
    }
};

// The escape magnitude.  The exponent is the double nearest 4/3 - the library
// materialises 0x3FF5555555555555 - and 1.33333333 is NOT the same thing: it
// differs by one float ULP above 1023.  See LRVQ_SEMANTICS.md 1.4.
inline float escapeMagnitude(uint32_t v)
{
    if (v > kInverseQuantMax)
        return (float)pow((double)v, 4.0 / 3.0);
    return kInverseQuant[v];
}

} // namespace

bool SpectralData::Deserialize(BitstreamReader& bs, const APACICSInfo& ics,
                               const ScaleFactorData& sf,
                               const SFBBandOffsets& sfb)
{
    // maxSfb == 0: the whole spectrum is zero and not one bit is read.
    if (ics.maxSfb == 0) {
        for (float* p = begin; p != end; ++p)
            *p = 0.0f;
        return false;
    }

    const uint32_t  numBands      = sfb.numBands & 0xFFFFu;   // read as a u16
    const uint16_t* const off     = sfb.offset;
    const uint32_t  numGroups     = ics.numWindowGroups;
    const uint32_t  coefPerWindow = off[numBands];
    const float* const X          = kInverseQuant;

    Window win;
    win.bs = &bs;
    win.w = 0;
    win.avail = 0;

    if (numGroups == 0) {
        bs.PutBack(0);
        return false;
    }

    float scratch[kScratchFloats];
    uint32_t windowIdx = 0;

    for (uint32_t g = 0; g < numGroups; ++g) {
        const uint32_t base    = coefPerWindow * windowIdx;
        const uint32_t nrec    = sf.numRecords[g];
        uint32_t       groupLen;
        float*         dst;

        if (g <= 7) {
            groupLen = (ics.windowGroupWord >> (4 * g)) & 0xF;
            if (groupLen == 1) {
                // one window: straight into the output, no staging
                dst = begin + base;
                ++windowIdx;
            } else {
                windowIdx += groupLen;
                dst = scratch + base;
            }
        } else {
            groupLen = 0;
            dst = scratch + base;
        }

        // Defensive, and the one place this decoder does not reproduce the
        // library: a staged group that does not fit the scratch buffer would
        // smash the library's stack.  Unreachable from APACICSInfo, which only
        // ever groups eight-short frames, where coefPerWindow is 128.
        if (groupLen != 1 && (uint64_t)base + (uint64_t)coefPerWindow * groupLen
                                 > kScratchFloats)
            return true;

        uint32_t coef     = 0;      // floats written into dst so far
        uint32_t lastBand = nrec;   // the library seeds this with nrec

        if (nrec != 0) {
            lastBand = 0;
            uint32_t band = 0;

            for (uint32_t r = 0; r < nrec; ++r) {
                const uint16_t rec      = sf.records[15 * g + r];
                const uint32_t codebook = rec & 0xff;
                band += (uint32_t)(rec >> 8);

                if (codebook == 0) {
                    const uint32_t stop = (uint32_t)off[band] * groupLen;
                    for (uint32_t i = coef; i < stop; ++i)
                        *dst++ = 0.0f;
                    coef     = stop;
                    lastBand = band;
                    continue;
                }

                const HuffTable* const t = GetHuffmanTable(codebook);
                const uint32_t clamp  = (1u << (32 - t->n)) - 1u;
                const bool     flagged = (codebook & 1) != 0;

                for (uint32_t b = lastBand + 1;; ++b) {
                    const float    sfv  = sf.scf[15 * g + b - 1];
                    const uint32_t prev = coef;
                    coef = (uint32_t)off[b] * groupLen;
                    const uint32_t n = coef - prev;

                    if (codebook <= 2) {
                        for (uint32_t i = 0; i < n; i += 4) {
                            win.need((int32_t)t->n);
                            if (win.zeroTuple(flagged)) {
                                dst[0] = dst[1] = dst[2] = dst[3] = 0.0f;
                            } else {
                                const uint16_t r16 = win.code(t, clamp);
                                dst[0] = X[(int16_t)r16 >> 14] * sfv;
                                dst[1] = X[(int32_t)(int16_t)(r16 << 2) >> 14] * sfv;
                                dst[2] = X[(int32_t)(int16_t)(r16 << 4) >> 14] * sfv;
                                dst[3] = X[(int32_t)(int16_t)(r16 << 6) >> 14] * sfv;
                            }
                            dst += 4;
                        }
                    } else if (codebook <= 4) {
                        for (uint32_t i = 0; i < n; i += 4) {
                            win.need((int32_t)t->n + 4);
                            if (win.zeroTuple(flagged)) {
                                dst[0] = dst[1] = dst[2] = dst[3] = 0.0f;
                            } else {
                                const uint16_t r16 = win.code(t, clamp);
                                // The library leaves the zero lanes at 0.0f
                                // and multiplies all four by the scalefactor,
                                // so a negative scalefactor yields -0.0 there.
                                float q[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                                for (int c = 0; c < 4; ++c) {
                                    int32_t m = (int32_t)((r16 >> (14 - 2 * c)) & 3);
                                    if (m == 0)
                                        continue;
                                    if (win.sign())
                                        m = -m;
                                    q[c] = X[m];
                                }
                                dst[0] = q[0] * sfv;
                                dst[1] = q[1] * sfv;
                                dst[2] = q[2] * sfv;
                                dst[3] = q[3] * sfv;
                            }
                            dst += 4;
                        }
                    } else if (codebook <= 6) {
                        for (uint32_t i = 0; i < n; i += 2) {
                            win.need((int32_t)t->n);
                            if (win.zeroTuple(flagged)) {
                                dst[0] = dst[1] = 0.0f;
                            } else {
                                const uint16_t r16 = win.code(t, clamp);
                                dst[0] = X[(int32_t)(int16_t)(r16 << 1) >> 12] * sfv;
                                dst[1] = X[(int32_t)(int16_t)(r16 << 6) >> 12] * sfv;
                            }
                            dst += 2;
                        }
                    } else if (codebook <= 10) {
                        for (uint32_t i = 0; i < n; i += 2) {
                            win.need((int32_t)t->n + 2);
                            if (win.zeroTuple(flagged)) {
                                dst[0] = dst[1] = 0.0f;
                            } else {
                                const uint16_t r16 = win.code(t, clamp);
                                const uint32_t m0 = (uint32_t)(r16 >> 11) & 0xF;
                                const uint32_t m1 = (uint32_t)(r16 >> 6) & 0xF;
                                if (m0) {
                                    float v = X[m0];
                                    if (win.sign())
                                        v = -v;
                                    dst[0] = v * sfv;
                                } else {
                                    dst[0] = 0.0f;
                                }
                                if (m1) {
                                    float v = X[m1];
                                    if (win.sign())
                                        v = -v;
                                    dst[1] = v * sfv;
                                } else {
                                    dst[1] = 0.0f;
                                }
                            }
                            dst += 2;
                        }
                    } else if (codebook == 11) {
                        for (uint32_t i = 0; i < n; i += 2) {
                            win.need((int32_t)t->n + 2);
                            const uint16_t r16 = win.code(t, clamp);
                            const uint32_t m0 = (uint32_t)(r16 >> 11);
                            const uint32_t m1 = (uint32_t)(r16 >> 6) & 0x1F;
                            // Both signs first, then both escapes.
                            const bool s0 = m0 ? win.sign() : false;
                            const bool s1 = m1 ? win.sign() : false;
                            float v0 = (m0 == 16) ? escapeMagnitude(win.escape())
                                                  : X[m0];
                            if (s0)
                                v0 = -v0;
                            dst[0] = v0 * sfv;
                            float v1 = (m1 == 16) ? escapeMagnitude(win.escape())
                                                  : X[m1];
                            if (s1)
                                v1 = -v1;
                            dst[1] = v1 * sfv;
                            dst += 2;
                        }
                    }
                    // codebook 12..15 cannot occur: ScaleFactorData rejects
                    // them, and the library falls straight through as well.

                    lastBand = b;
                    if (b >= band)
                        break;
                }
            }
        }

        if (numBands > lastBand)
            memset(dst, 0, 4 * (size_t)(coefPerWindow * groupLen - coef));
    }

    // Eight short windows: the staged groups are band-interleaved, unpack them
    // window-major.  Groups of one window went straight out and are skipped.
    if (ics.windowSequence == 2) {
        uint32_t wIdx = 0;
        for (uint32_t g = 0; g < numGroups; ++g) {
            if (g > 7)
                continue;
            const uint32_t groupLen = (ics.windowGroupWord >> (4 * g)) & 0xF;
            if ((groupLen & 0xE) == 0) {
                wIdx += groupLen;
                continue;
            }
            const uint32_t base = coefPerWindow * wIdx;
            float* out = begin + base;
            for (uint32_t j = 0; j < groupLen; ++j) {
                for (uint32_t b = 0; b < numBands; ++b) {
                    const uint32_t o     = off[b];
                    const uint32_t width = (uint32_t)off[b + 1] - o;
                    const float* src = scratch + base + o * groupLen + j * width;
                    for (uint32_t i = 0; i < width; ++i)
                        *out++ = src[i];
                }
            }
            wIdx += groupLen;
        }
    }

    bs.PutBack((uint32_t)win.avail);
    return false;
}

} // namespace apac
