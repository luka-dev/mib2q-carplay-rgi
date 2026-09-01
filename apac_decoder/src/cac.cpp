/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/cac.h"

#include <math.h>
#include <string.h>

namespace apac {

// 0x000b21d0  CacQuantizer::Initialize()
//
// Entry 0 is {0, 1, 0}.  Entry k, 1 <= k <= 17, carries the gain
// 10^((1.5*(k-1) - 12)/20) and the normalised rotation pair built from it, or
// from its reciprocal once the gain reaches 0.999.  The original writes the raw
// powf result into `.a` and uses the possibly-reciprocated value for the pair,
// which is why `.a` alone tells ProcessCac which way round to apply the result.
void CACDecoder::Initialize()
{
    memset(mGainIndex, 0, sizeof mGainIndex);
    memset(mRepeat, 0, sizeof mRepeat);

    mTable[0].gain = 0.0f;
    mTable[0].sine = 1.0f;
    mTable[0].cosine = 0.0f;

    for (uint32_t k = 1; k <= 17; ++k) {
        const float a = powf(10.0f, (((float)(int32_t)(k - 1) * 1.5f) - 12.0f) * 0.05f);
        const float g = (a >= 0.999f) ? (1.0f / a) : a;
        const float d = sqrtf(g * g + 1.0f);
        mTable[k].gain = a;
        mTable[k].sine = 1.0f / d;
        mTable[k].cosine = mTable[k].sine * g;
    }
}

// 0x000b2110  CacQuantizer::GainTableEntry(unsigned) const
void CACDecoder::Entry(uint32_t index, CacGain& out) const
{
    if (index <= 17) {
        out = mTable[index];
    } else if (index <= 34) {
        const CacGain& e = mTable[index - 17];
        out.gain = -e.gain;
        out.sine = e.sine;
        out.cosine = -e.cosine;
    } else {
        out = mTable[0];
    }
}

namespace {

// 0x0007b070, the kernel ProcessCac calls.  A Givens rotation of the two
// spectra by (cosine, sine); when |gain| has reached 0.999 the table holds the
// reciprocal gain instead, and the original compensates by swapping which
// output each half lands in.  The library's copy is NEON, unrolled 16/8/4/1;
// the arithmetic is identical lane for lane.
void Rotate(bool reciprocal, float* x, float* y, uint32_t n, float s, float c)
{
    if (reciprocal) {
        for (uint32_t i = 0; i < n; ++i) {
            const float a = x[i], b = y[i];
            x[i] = c * a - s * b;
            y[i] = s * a + c * b;
        }
    } else {
        for (uint32_t i = 0; i < n; ++i) {
            const float a = x[i], b = y[i];
            x[i] = s * a + c * b;
            y[i] = c * a - s * b;
        }
    }
}

}  // namespace

// 0x0007b5a0  APACCACDecoder::Deserialize(BitstreamReader&, APACICSInfo const&)
//
// Reads (gainIndex, repeat) pairs out of a 16-bit look-ahead window until the
// repeat table yields its terminator, 43.  A run of `r` means r + 1 bands.  The
// window is topped up after every symbol and the surplus given back with one
// PutBack(16) at the end, so the reader's final position is exact even though
// the decode over-reads.
bool CACDecoder::Deserialize(BitstreamReader& bs, const APACICSInfo& ics)
{
    if (ics.maxSfb == 0)
        return false;

    uint32_t w = bs.GetBits(16);

    for (uint32_t i = 0; i < kCacMaxBands; ++i) {
        uint32_t k;

        for (k = 0; k < kCacGainCodes; ++k) {
            const CacCode& r = kCacGainCodeTable[k];
            if (r.code == (w >> (16 - r.length)))
                break;
        }
        if (k == kCacGainCodes)
            return true;
        mGainIndex[i] = kCacGainCodeTable[k].value;
        {
            const uint32_t len = kCacGainCodeTable[k].length;
            w = ((w << len) | bs.GetBits(len)) & 0xffffu;
        }

        for (k = 0; k < kCacRepeatCodes; ++k) {
            const CacCode& r = kCacRepeatCodeTable[k];
            if (r.code == (w >> (16 - r.length)))
                break;
        }
        if (k == kCacRepeatCodes)
            return true;
        mRepeat[i] = kCacRepeatCodeTable[k].value;
        {
            const uint32_t len = kCacRepeatCodeTable[k].length;
            w = ((w << len) | bs.GetBits(len)) & 0xffffu;
        }

        if (mRepeat[i] == kCacRepeatEnd) {
            bs.PutBack(16);
            return false;
        }
    }
    // 120 pairs without a terminator: the original's `if (++v17 != 120)` falls
    // out of the loop into its error exit.
    return true;
}

// 0x0007b8c0  APACCACDecoder::ProcessCac(...) const
bool CACDecoder::ProcessCac(const APACICSInfo& ics, const SFBOffsets& table,
                            float* left, float* right) const
{
    const uint32_t maxSfb = ics.maxSfb;
    if (maxSfb == 0)
        return false;

    const bool eightShort = (ics.windowSequence == 2);
    const uint16_t* offs = eightShort ? table.sfbOffsetShort : table.sfbOffsetLong;
    const uint32_t numBands = eightShort ? table.numSfbShort : table.numSfbLong;

    // COUNTED, where the original's band walks are not.  Every band loop below
    // reads offs[band + 1], and the offset array holds numBands + 1 entries, so
    // a maxSfb above numBands walks off the end of a .rodata table.
    // APACICSInfo::Deserialize cannot produce one - it rejects a maxSfb past the
    // limit the SFB table gave it - but ProcessCac is handed the ICSInfo rather
    // than the bitstream, so the invariant is not enforced where it is relied
    // on.  Same reasoning as APACTNSData::Apply.
    if (maxSfb > numBands)
        return true;
    // Eight nibbles is all the window-grouping word carries, and 8 x 15 is the
    // gain map's whole capacity; the RLE expansion below asserts the product
    // again per group.
    const uint32_t groups = ics.numWindowGroups;
    if (groups > 8)
        return true;

    // The run-length expansion, one slot per (window group, band).
    uint8_t gains[kCacMaxBands];
    memset(gains, 0, sizeof gains);

    if (groups == 0) {
        // The original returns immediately for an eight-short frame with no
        // groups and otherwise runs the band loop over an all-zero gain map.
        if (eightShort)
            return false;
    } else {
        uint32_t pair = 0;
        int32_t  left_in_run = -1;
        uint8_t  current = 0;
        uint32_t out = 0;
        for (uint32_t g = 0; g < groups; ++g) {
            const uint32_t end = (g + 1) * maxSfb;
            if (end > kCacMaxBands)
                return true;        // not reachable: 8 groups x 15 bands is the cap
            while (out != end) {
                if (left_in_run == -1) {
                    if (pair >= kCacMaxBands)
                        return true;
                    current = mGainIndex[pair];
                    left_in_run = (int32_t)mRepeat[pair];
                    ++pair;
                }
                gains[out++] = current;
                --left_in_run;
            }
        }
    }

    CacGain e;

    if (!eightShort) {
        uint32_t prev = 0;
        for (uint32_t band = 0; band < maxSfb; ++band) {
            const uint32_t next = offs[band + 1];
            const uint32_t width = next - prev;
            if (gains[band]) {
                Entry(gains[band], e);
                Rotate(fabsf(e.gain) >= 0.999f, left, right, width, e.sine, e.cosine);
            }
            left += width;
            right += width;
            prev = next;
        }
        return false;
    }

    // Eight short windows.  The coefficients of a window group are w_g
    // consecutive short blocks of `blockLen` coefficients; the rotation is
    // applied to the same band of every window in the group.  w_g is the g-th
    // nibble of the window-grouping word, which only carries eight of them.
    const uint32_t blockLen = offs[numBands];
    uint32_t gainIdx = 0;
    for (uint32_t g = 0; g < groups; ++g) {
        const uint32_t windows = (g <= 7) ? ((ics.windowGroupWord >> (4 * g)) & 0xfu) : 0;
        uint32_t prev = 0;
        uint32_t band;
        for (band = 0; band < maxSfb; ++band) {
            const uint32_t next = offs[band + 1];
            const uint32_t width = next - prev;
            if (gainIdx >= kCacMaxBands)
                return true;
            if (gains[gainIdx]) {
                Entry(gains[gainIdx], e);
                const bool recip = fabsf(e.gain) >= 0.999f;
                for (uint32_t win = 0; win < windows; ++win)
                    Rotate(recip, left + win * blockLen, right + win * blockLen,
                           width, e.sine, e.cosine);
            }
            ++gainIdx;
            left += width;
            right += width;
            prev = next;
        }
        // Step over the rest of the group: w_g whole short blocks, less the
        // `prev` coefficients the band loop already walked.
        const uint32_t advance = windows * blockLen - prev;
        left += advance;
        right += advance;
    }
    return false;
}

}  // namespace apac
