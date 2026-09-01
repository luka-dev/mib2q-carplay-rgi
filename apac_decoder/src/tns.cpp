/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/tns.h"

#include "apac/transform.h"

#include <string.h>

namespace apac {

namespace {

// The all-pole synthesis filter at 0xaec20.  `lpc` has order+1 entries with
// lpc[0] unused, `stride` is +1 (upwards) or -1 (downwards), and the state is
// carried in transposed direct form:
//
//     y[n]      = x[n] - state[0]
//     state[j]  = state[j+1] + lpc[j+1] * y[n]      j = 0 .. order-2
//     state[order-1] = lpc[order] * y[n]
//
// which unrolls to y[n] = x[n] - sum_{k=1..order} lpc[k] * y[n-k].
void SynthesisFilter(float* p, int stride, uint32_t count,
                     const float* lpc, uint32_t order)
{
    float state[kTNSMaxOrder + 1];
    for (uint32_t i = 0; i <= kTNSMaxOrder; ++i)
        state[i] = 0.0f;
    if (!count)
        return;
    for (uint32_t n = 0; n < count; ++n) {
        const float y = *p - state[0];
        *p = y;
        p += stride;
        for (uint32_t j = 1; j < order; ++j)
            state[j - 1] = state[j] + lpc[j] * y;
        state[order - 1] = lpc[order] * y;
    }
}

// Parcor (reflection) coefficients to direct-form LPC, the recursion the
// original spells out inline in Apply with two interleaved stack arrays:
//
//     a[0] = 1, a[1] = k[0]
//     for m = 1 .. order-1:
//         t[i]   = a[i] + k[m] * a[m+1-i]     i = 1 .. m
//         t[m+1] = k[m]
//         a[1..m+1] = t[1..m+1]
//
// `a` ends up with order+1 entries.
void ParcorToLPC(const float* k, uint32_t order, float* a)
{
    float t[kTNSMaxOrder + 2];
    uint32_t i;
    for (i = 0; i <= kTNSMaxOrder + 1; ++i) { a[i] = 0.0f; t[i] = 0.0f; }
    a[0] = 1.0f;
    if (!order)
        return;
    a[1] = k[0];
    for (uint32_t m = 1; m < order; ++m) {
        for (i = 1; i <= m; ++i)
            t[i] = a[i] + k[m] * a[m + 1 - i];
        t[m + 1] = k[m];
        for (i = 1; i <= m + 1; ++i)
            a[i] = t[i];
    }
}

}  // namespace

// APACTNSData::APACTNSData, 0xaed10.  The record table is 12 {rate, limits}
// pairs; the limits word carries the long band cap in its low byte and the
// short one in the next.  A rate that is not in the table gives 0 / 0, and the
// original does not treat that as an error - it just means no band ever passes
// the cap.
void TNSData::Configure(uint32_t sampleRate)
{
    present = 0;
    nFilt = 0;
    maxOrderLong = 12;      // the 1804 = 0x070C the original stores at +186
    maxOrderShort = 7;
    maxBandsLong = 0;
    maxBandsShort = 0;
    for (uint32_t i = 0; i < 12; ++i) {
        if (tables::kTNSMaxBandRecords[2 * i] == sampleRate) {
            const uint32_t limits = tables::kTNSMaxBandRecords[2 * i + 1];
            maxBandsLong  = (uint8_t)(limits & 0xff);
            maxBandsShort = (uint8_t)((limits >> 8) & 0xff);
            break;
        }
    }
}

// APACTNSData::ParseTNSData, 0xaedc0.  The original inlines the bit reader;
// every read below is one of its GetBits calls, in the same order.
//
// Long blocks: one window, a 2-bit filter count, up to three filters, each
// 11 bits of {length:6, order:5}.  Short blocks: eight windows, a 1-bit
// "has a filter" flag each accumulated MSB-first into `nFilt`, and 7 bits of
// {length:4, order:3} per filter.  The order is clamped to the block's cap
// rather than rejected.
bool TNSData::ParseTNSData(BitstreamReader& bs, const APACICSInfo& ics)
{
    const bool eightShort = (ics.windowSequence == 2);
    const uint32_t numWindows = eightShort ? 8 : 1;
    nFilt = 0;

    for (uint32_t w = 0; w < numWindows; ++w) {
        const uint32_t count = bs.GetBits(eightShort ? 1 : 2);
        nFilt = (uint8_t)(count | (nFilt << 1));
        if (count == 0)
            continue;

        const uint32_t coefRes = bs.GetBit();
        const uint8_t resBit = (uint8_t)(16 * (coefRes & 0xf));

        for (uint32_t fi = 0; fi < count; ++fi) {
            // Short blocks keep one filter per window, at record index w;
            // long blocks keep `count` filters at record indices 0..count-1.
            TNSFilter& rec = filter[eightShort ? w : fi];
            uint32_t length, order;
            if (eightShort) {
                const uint32_t v = bs.GetBits(7);
                order  = v & 7;
                length = (v >> 3) & 0xf;
                if (order > maxOrderShort) order = maxOrderShort;
            } else {
                const uint32_t v = bs.GetBits(11);
                order  = v & 0x1f;
                length = (v >> 5) & 0x3f;
                if (order > maxOrderLong) order = maxOrderLong;
            }
            if (length == 0)
                return true;                       // the original's -1

            rec.length = (uint8_t)length;
            rec.order  = (uint8_t)order;
            if (order) {
                const uint32_t hdr = bs.GetBits(2);
                rec.direction = (uint8_t)((hdr & 2) != 0);
                // 3 + coefRes - coefCompress bits, sign-extended, biased by 8,
                // then merged with the resolution bit into a table index.
                const uint32_t nbits = 3 + coefRes - (hdr & 1);
                const uint32_t shift = 32 - nbits;
                for (uint32_t j = 0; j < order; ++j) {
                    const int32_t raw =
                        ((int32_t)(bs.GetBits(nbits) << shift)) >> shift;
                    rec.coef[j] = (uint8_t)(resBit | (uint8_t)(raw + 8));
                }
            }
        }
    }
    return false;
}

// APACTNSData::Apply, 0xaf240.
//
// Filters run from the top of the spectrum downwards: each one covers
// `length` scalefactor bands ending where the previous one started, and both
// its start and its end band are clamped to the rate's TNS band cap (which is
// itself clamped to ics.maxSfb).  A filter whose start band would go negative
// is clamped to band 0 and ends the window.
void TNSData::Apply(const APACICSInfo& ics, const SFBBandOffsets& sfb,
                    SpectralData& spectrum) const
{
    if (!present)
        return;

    const bool eightShort = (ics.windowSequence == 2);
    const uint32_t numWindows = eightShort ? 8 : 1;
    const uint32_t numBands = (uint16_t)sfb.numBands;
    const uint16_t* offset = sfb.offset;
    const uint32_t windowLength = offset[numBands];

    uint32_t maxBand = eightShort ? maxBandsShort : maxBandsLong;
    if (maxBand > ics.maxSfb)
        maxBand = ics.maxSfb;

    float* base = spectrum.begin;

    for (uint32_t w = 0; w < numWindows; ++w, base += windowLength) {
        uint32_t count = eightShort ? ((nFilt >> (7 - w)) & 1) : nFilt;
        if (count == 0)
            continue;
        // ParseTNSData reads the long-block count from two bits so it is at
        // most 3, but Apply is handed the object rather than the bitstream and
        // `filter` is a fixed array.  Clamp instead of trusting the field.
        if (count > kTNSMaxFilters)
            count = kTNSMaxFilters;

        uint32_t top = numBands;
        for (uint32_t fi = 0; fi < count; ++fi) {
            const TNSFilter& rec = filter[eightShort ? w : fi];
            uint32_t order = rec.order;
            if (order > kTNSMaxOrder)
                order = kTNSMaxOrder;      // same reasoning as `count` above
            const int32_t start = (int32_t)top - (int32_t)rec.length;

            uint32_t startBand;
            if (start < 0) {
                if (order == 0) { top = 0; continue; }
                startBand = 0;
            } else {
                if (order == 0) { top = (uint32_t)start; continue; }
                startBand = ((uint32_t)start > maxBand) ? maxBand : (uint32_t)start;
            }
            const uint32_t startBin = offset[startBand];
            const uint32_t topBand = (top > maxBand) ? maxBand : top;
            const uint32_t endBin = offset[topBand];

            if (startBin < endBin) {
                float parcor[kTNSMaxOrder];
                float lpc[kTNSMaxOrder + 2];
                for (uint32_t j = 0; j < order; ++j)
                    parcor[j] = tables::kTNSCoefficients[rec.coef[j]].f;
                ParcorToLPC(parcor, order, lpc);
                const uint32_t n = endBin - startBin;
                if (rec.direction)
                    SynthesisFilter(base + endBin - 1, -1, n, lpc, order);
                else
                    SynthesisFilter(base + startBin, 1, n, lpc, order);
            }
            top = (start < 0) ? 0 : (uint32_t)start;
        }
    }
}

}  // namespace apac
