/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACScaleFactorData::Deserialize, 0x0009a140, 1224 bytes, 304 instructions.
//
// Transcribed from reference/decomp/APACScaleFactorData.c with the codebook
// semantics of reference/decomp/LRVQ_SEMANTICS.md sections 1.1 and 2.4.
//
// Syntax, per window group, once ics.maxSfb bands are covered:
//
//     startValue          u(8)          ONCE, before the first group
//     repeat {
//         codebook        u(4)              > 11 is an error
//         run             u(runBits)+       escape-coded on all-ones
//         if (codebook)   run x scalefactor delta, codebook 0 of the Huffman set
//     }
//
// runBits is 3 for eight-short-window frames and 5 otherwise.  A zero run and
// a run that would pass maxSfb are both errors.
//
// The scalefactors come out already in linear form: 2^((v - 100) / 4), built
// without a pow as bits(2^(k/4)) + (((v - 100) >> 2) << 23) - an exponent add
// on the float's bit pattern.  v is the running 8-bit start value plus the
// deltas, clamped to [-256, 255]; the two out-of-range results are the
// precomputed constants 0x13000000 (2^-89) and 0x52D744FD (2^38.75).
//
// Codebook 0 carries a 1-bit code `0` meaning "delta 0", and the original
// hoists it into a fast path: if the top bit of the window is clear, one bit is
// consumed and the previous exponent is re-used verbatim.

#include "apac/spectrum.h"

#include <string.h>

#include "apac/huffman.h"

namespace apac {

namespace {

inline float bitsToFloat(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, sizeof f);
    return f;
}

} // namespace

void ScaleFactorData::Clear()
{
    memset(this, 0, sizeof *this);
}

bool ScaleFactorData::Deserialize(BitstreamReader& bs, const APACICSInfo& ics)
{
    const HuffTable* const table = GetSCFHuffmanTable();

    // `n` is a cap on the unary prefix, not an element count: clz(clamp | ~w)
    // is min(leadingOnes(w), n).  This function spells the clamp
    // 0x80000000u >> n; APACSpectralData spells the same clz as
    // (1u << (32 - n)) - 1.
    const uint32_t clamp   = 0x80000000u >> table->n;
    const uint32_t runBits = (ics.windowSequence == 2) ? 3u : 5u;
    const uint32_t runEsc  = (1u << runBits) - 1u;

    const uint32_t* const twoPowQuarterBits =
        reinterpret_cast<const uint32_t*>(kTwoPowQuarter);

    int32_t value = (int32_t)bs.GetBits(8);

    if (ics.numWindowGroups == 0)
        return false;

    for (uint32_t g = 0; g < ics.numWindowGroups; ++g) {
        uint32_t numRec = 0;   // records written for this group

        if (ics.maxSfb != 0) {
            uint32_t band = 0;      // scalefactors written so far in this group
            uint32_t done = 0;      // bands covered by the records so far

            for (;;) {
                const uint32_t codebook = bs.GetBits(4);
                if (codebook > 11)
                    return true;

                uint32_t run = 0, field;
                do {
                    field = bs.GetBits(runBits);
                    run += field;
                    if (run + band > ics.maxSfb)
                        return true;
                } while (field == runEsc);
                if (run == 0)
                    return true;

                done += run;
                records[15 * g + numRec] =
                    (uint16_t)(codebook | (run << 8));

                if (codebook != 0) {
                    uint32_t w     = 0;     // MSB-aligned bit window
                    int32_t  avail = 0;     // valid bits in w
                    int32_t  exp   = value - 100;

                    for (;;) {
                        if ((int32_t)table->n > avail) {
                            if (avail < 32)
                                w |= bs.GetBits(32 - (uint32_t)avail);
                            avail = 32;
                        }

                        uint32_t bits;
                        if ((int32_t)w < 0) {
                            const int k = __builtin_clz(clamp | ~w);
                            const HuffEntry& e = table->entries[k];
                            const uint16_t r =
                                e.payload[(uint32_t)(w << k) >> e.shift];
                            avail -= (int32_t)(r & 0x3f);
                            w <<= (r & 0x3f);
                            value += (int32_t)(int16_t)r >> 6;

                            // abs(value) <= 255, written as the original's
                            // value ^ (value >> 31), which admits -256 too.
                            if ((value ^ (value >> 31)) <= 255) {
                                exp  = value - 100;
                                bits = twoPowQuarterBits[(uint32_t)value & 3] +
                                       ((uint32_t)(exp >> 2) << 23);
                            } else if (value <= 0) {
                                bits  = 0x13000000u;    // 2^-89
                                value = -256;
                                exp   = -356;
                            } else {
                                bits  = 0x52D744FDu;    // 2^38.75
                                value = 255;
                                exp   = 155;
                            }
                        } else {
                            // the 1-bit `0` code: delta 0, exponent unchanged
                            w <<= 1;
                            --avail;
                            bits = twoPowQuarterBits[(uint32_t)exp & 3] +
                                   ((uint32_t)(exp >> 2) << 23);
                        }

                        scf[15 * g + band] = bitsToFloat(bits);
                        ++band;
                        if (band >= done)
                            break;
                    }

                    bs.PutBack((uint32_t)avail);
                } else {
                    for (uint32_t i = 0; i < run; ++i)
                        scf[15 * g + band + i] = 0.0f;
                    band += run;
                }

                ++numRec;
                if (done >= ics.maxSfb)
                    break;
            }
        }

        numRecords[g] = (uint8_t)numRec;
    }

    return false;
}

} // namespace apac
