/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/bwe.h"

#include <string.h>

#include "apac/config.h"

namespace apac {

// APACBWE2Decoder::Initialize(unsigned) at 0x79670 sizes the active bitmap and
// then InitInterFrameMemory (0x791d0) allocates and ZEROES the envelope and
// scale arrays and builds one BWELPC per channel.  Both arrays are per channel
// and both persist across frames - a channel pair whose second half copies the
// first, and a `flag0 == 0, flag1 == 1` pair that leaves the first channel's
// record untouched, both depend on that.
void BWE2Decoder::Initialize(const SFBOffsets* table, BWELPC* lpc)
{
    mTable = table;
    mLPC = lpc;
    mActive = 0;
    memset(mEnvelope, 0, sizeof mEnvelope);
    memset(mScale, 0, sizeof mScale);
    memset(mEnv, 0, sizeof mEnv);
}

bool BWE2Decoder::GetActiveFlag(uint32_t channel) const
{
    // 0x00079730: (1 << ch) & bits[ch >> 6].  Our decoder caps at 16 channels.
    return channel < kBWEMaxChannels && ((mActive >> channel) & 1u) != 0;
}

// The payload of one active channel: two 9-bit envelope fields and one 6-bit
// scale per window group.  `groups` is always the FIRST channel's group count,
// even for the second half of a channel pair - that is what the library reads.
//
// The number of 6-bit reads is NOT clamped, deliberately: it is what keeps the
// bitstream in step, and shortening it would desynchronise every later field
// rather than protect anything.  What is clamped is the STORE - Deserialize is
// handed the element rather than the bitstream, so nothing here enforces the
// cap of 8 that APACICSInfo::Deserialize puts on numWindowGroups, and mScale is
// a fixed array.  A count past the cap therefore mis-parses (and is caught by
// the caller running out of bits) instead of writing off the end.
void BWE2Decoder::ReadChannel(BitstreamReader& bs, uint32_t channel, uint32_t groups)
{
    mEnvelope[channel][0] = bs.GetBits(9);
    mEnvelope[channel][1] = bs.GetBits(9);
    for (uint32_t i = 0; i < groups; ++i) {
        const uint32_t v = bs.GetBits(6);
        if (i < kBWEMaxBands)
            mScale[channel][i] = v;
    }
}

// 0x00078a60  APACBWE2Decoder::Deserialize(BitstreamReader&, unsigned,
//                                          APACSyntacticElement&)
uint32_t BWE2Decoder::Deserialize(BitstreamReader& bs, uint32_t channel,
                                  SyntacticElement& element)
{
    if (channel >= kBWEMaxChannels)
        return kLimitTooManyChannels;

    const uint32_t type = element.Type();
    mActive &= ~(1u << channel);

    if (type == kElementSCE) {
        if (bs.GetBit()) {
            mActive |= 1u << channel;
            ReadChannel(bs, channel, element.GetICSInfo(true).numWindowGroups);
        }
        return kOk;
    }

    // Every other element type but the channel pair reads nothing.  The LFE
    // (type 3) and the empty element (type 6) fall out here.
    if (type != kElementCPE)
        return kOk;

    const uint32_t second = channel + 1;
    if (second >= kBWEMaxChannels)
        return kLimitTooManyChannels;
    mActive &= ~(1u << second);

    const uint32_t flag0 = bs.GetBit();
    const uint32_t flag1 = bs.GetBit();

    const APACICSInfo& ics0 = element.GetICSInfo(true);
    const APACICSInfo& ics1 = element.GetICSInfo(false);

    bool readSecond = (flag1 != 0);

    if (flag0 && ics0.maxSfb) {
        mActive |= 1u << channel;
        ReadChannel(bs, channel, ics0.numWindowGroups);
        if (!flag1) {
            // The second channel copies the first channel's payload instead of
            // carrying its own.
            if (ics1.maxSfb) {
                mActive |= 1u << second;
                mEnvelope[second][0] = mEnvelope[channel][0];
                mEnvelope[second][1] = mEnvelope[channel][1];
                for (uint32_t i = 0; i < ics0.numWindowGroups && i < kBWEMaxBands; ++i)
                    mScale[second][i] = mScale[channel][i];
            }
            readSecond = false;
        }
    }

    if (readSecond && ics1.maxSfb) {
        mActive |= 1u << second;
        ReadChannel(bs, second, ics0.numWindowGroups);
    }

    return kOk;
}

// sub_78370, 0x78370: `std::find_if(first, last, != 0.0f)`, unrolled by four.
// DecodeFrame uses it as a guard - an all-zero core has no content to
// replicate, and the LPC analysis of it would divide by the 1.0001 ridge - so
// the whole synthesis is skipped rather than run on silence.
static bool AnyNonZero(const float* v, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i)
        if (v[i] != 0.0f)
            return true;
    return false;
}

// 0x00078490  APACBWE2Decoder::DecodeFrame(const APACICSInfo&,
//                                          APACSpectralData&, unsigned)
uint32_t BWE2Decoder::DecodeFrame(const APACICSInfo& ics, float* spec,
                                  uint32_t channel)
{
    // `v19 = active == 0 || maxSfb == 0` - the whole body is under `!v19`.
    if (!GetActiveFlag(channel) || ics.maxSfb == 0)
        return kOk;
    if (mLPC == 0 || mTable == 0)
        return kLimitNeedsBWE;

    // The core's width in coefficients: the offset of the band maxSfb names,
    // times eight for a short block because that table is per window.
    //
    // CLAMPED, and the library does not clamp it.  DecodeFrame is handed the
    // PARSED APACICSInfo rather than the bitstream, so nothing here enforces
    // the bound APACICSInfo::Deserialize applies to maxSfb - and this indexes a
    // .rodata table with it, which is exactly the shape of the ProcessCac
    // read-past-the-table this decoder already found.  A maxSfb past the table is
    // impossible from any bitstream and is a hard error here rather than a
    // silent out-of-bounds load.
    uint32_t coreEnd;
    if (ics.windowSequence == 2) {
        if (ics.maxSfb > mTable->numSfbShort)
            return kBadValue;
        coreEnd = 8u * (uint32_t)mTable->sfbOffsetShort[ics.maxSfb];
    } else {
        if (ics.maxSfb > mTable->numSfbLong)
            return kBadValue;
        coreEnd = (uint32_t)mTable->sfbOffsetLong[ics.maxSfb];
    }

    // One comparison sets the whole geometry; see the table in the header.
    const bool wide = (coreEnd >= 384);
    const uint32_t bwStart = wide ? 384u : 256u;
    const uint32_t patches = wide ? 2u : 4u;
    const uint32_t longW   = wide ? 256u : 128u;
    const uint32_t shortW  = wide ? 32u : 16u;

    // `v83 = 1.0; vDSP_vfill(&v83, v87, 1, 1024)`.  Only the bins the two
    // envelope calls write are ever read back, so the fill only matters as the
    // library's own initial state; it is kept because it is cheap and because
    // a divisor of 1.0 is the one value that cannot turn a missing envelope
    // into an infinity.
    for (uint32_t i = 0; i < kFrameLength; ++i)
        mEnv[i] = 1.0f;

    if (ics.windowSequence == 2)
        return DecodeShort(ics, spec, channel, bwStart, patches, shortW);
    return DecodeLong(ics, spec, channel, bwStart, patches, longW);
}

// The `windowSequence != 2` half of DecodeFrame.  One block of 1024
// coefficients: 512 extended bins in `patches` copies of `width`.
uint32_t BWE2Decoder::DecodeLong(const APACICSInfo& ics, float* spec,
                                 uint32_t channel, uint32_t bwStart,
                                 uint32_t patches, uint32_t width)
{
    (void)ics;
    if (!AnyNonZero(spec, bwStart))
        return kOk;

    // One spectrum, `bwStart` bins of it, hop = the whole block.
    mLPC->MDCTCoefficientsToLPC(spec, kFrameLength, bwStart, 1);
    mLPC->LPCToMDCTSpectrum(mLPC->CoreLPC(), mEnv, bwStart);

    // The two 9-bit header fields, which the library reads back out of its
    // envelope array as a pair of uint16 (`v85`, `v86`) and hands to
    // DecodeLSFsToLPCs as a `const unsigned short*`.
    const uint16_t idx[2] = { (uint16_t)mEnvelope[channel][0],
                              (uint16_t)mEnvelope[channel][1] };
    if (!mLPC->DecodeLSFsToLPCs(idx))
        return kBadValue;
    mLPC->LPCToMDCTSpectrum(mLPC->LsfLPC(), mEnv + bwStart, 512);

    // The scale is a 6-bit field and the table has exactly 64 entries, so the
    // fold is a no-op on anything the parser can produce; it is here because
    // mScale is an array this function is handed rather than one it filled.
    const float  gain = kBWEGain[mScale[channel][0] % kBWEGainSize];
    const float* src  = mEnv + (bwStart - width);       // fixed across patches
    const float* from = spec + (bwStart - width);       // and so is this
    uint32_t pos = bwStart;
    for (uint32_t p = 0; p < patches; ++p) {
        float* dst = mEnv + pos;
        // vDSP_vdiv(A, 1, B, 1, C, 1, N) is C = B / A: the BWE envelope is the
        // DIVISOR and the result lands back on top of it.
        for (uint32_t j = 0; j < width; ++j)
            dst[j] = src[j] / dst[j];
        for (uint32_t j = 0; j < width; ++j)
            spec[pos + j] = from[j] * dst[j];
        pos += width;
    }
    for (uint32_t j = 0; j < 512; ++j)
        spec[bwStart + j] = spec[bwStart + j] * gain;
    return kOk;
}

// The `windowSequence == 2` half.  Everything is per 128-bin window: the core
// is `bwStart / 8` wide, the extension 64, and the gain comes from the window
// GROUP the window belongs to.
uint32_t BWE2Decoder::DecodeShort(const APACICSInfo& ics, float* spec,
                                  uint32_t channel, uint32_t bwStart,
                                  uint32_t patches, uint32_t width)
{
    const uint32_t kWindows = 8;
    const uint32_t kWin = kFrameLength / kWindows;      // 128
    const uint32_t core = bwStart >> 3;                 // 48 or 32

    // The library scans window by window and stops at the first non-zero; if
    // all eight cores are silent there is nothing to replicate.
    bool any = false;
    for (uint32_t w = 0; w < kWindows && !any; ++w)
        any = AnyNonZero(spec + w * kWin, core);
    if (!any)
        return kOk;

    // Eight spectra, 128 floats apart, `core` bins of each, summed into one
    // autocorrelation.
    mLPC->MDCTCoefficientsToLPC(spec, kWin, core, kWindows);
    mLPC->LPCToMDCTSpectrum(mLPC->CoreLPC(), mEnv, core);

    const uint16_t idx[2] = { (uint16_t)mEnvelope[channel][0],
                              (uint16_t)mEnvelope[channel][1] };
    if (!mLPC->DecodeLSFsToLPCs(idx))
        return kBadValue;
    mLPC->LPCToMDCTSpectrum(mLPC->LsfLPC(), mEnv + core, 64);

    // `core - width` is 16 either way - 48 - 32 and 32 - 16 - so the divisor
    // slice is the same bins of the core envelope in both geometries.
    const float* src = mEnv + (core - width);
    for (uint32_t p = 0; p < patches; ++p) {
        float* dst = mEnv + core + p * width;
        for (uint32_t j = 0; j < width; ++j)
            dst[j] = src[j] / dst[j];
    }

    // The window-group walk.  `windowGroupWord` is one hex digit per group
    // counting its windows; a group index past 7 leaves a count of zero, which
    // makes every later window start its own group.  The gain is read at the
    // TOP of each window, so the last index this can reach is 7 - within
    // mScale's eight slots, which is why nothing here needs a bound.
    uint32_t inGroup = 0;
    uint32_t group = 0;
    uint32_t groupSize = ics.windowGroupWord & 0xF;
    for (uint32_t w = 0; w < kWindows; ++w) {
        float* s = spec + w * kWin;
        const float  gain = kBWEGain[mScale[channel][group] % kBWEGainSize];
        const float* from = s + (core - width);
        for (uint32_t p = 0; p < patches; ++p) {
            const uint32_t at = core + p * width;
            for (uint32_t j = 0; j < width; ++j)
                s[at + j] = from[j] * mEnv[at + j];
        }
        for (uint32_t j = 0; j < 64; ++j)
            s[core + j] = s[core + j] * gain;

        ++inGroup;
        if (inGroup >= groupSize) {
            ++group;
            inGroup = 0;
            groupSize = (group > 7) ? 0u : ((ics.windowGroupWord >> (4 * group)) & 0xF);
        }
    }
    return kOk;
}

}  // namespace apac
