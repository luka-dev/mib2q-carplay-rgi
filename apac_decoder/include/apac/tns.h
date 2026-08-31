// APACTNSData - temporal noise shaping: the per-filter parcor coefficients in
// the bitstream, and the all-pole filter they drive along the spectrum.
//
// Original: libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010)
//
//   0x000aed10  APACTNSData::APACTNSData(APACInstanceConfig&)
//   0x000aedc0  APACTNSData::ParseTNSData(TBitstreamReader<unsigned>&, const APACICSInfo&)
//   0x000af240  APACTNSData::Apply(const APACICSInfo&, const apac::SFBOffsets&,
//                                  APACSpectralData&) const
//   0x000aec20  the lattice-free direct-form synthesis filter Apply calls
//
// Decompiled in reference/decomp/APACTNSData.c and _free_functions.c.
//
// The object is 190 bytes and this struct reproduces it byte for byte, because
// every offset in the decompilation is a raw one:
//
//   +0    present            set by the syntactic element, not by the parser
//   +1    filter count (long) or an 8-bit per-window present mask (short),
//         window 0 in bit 7
//   +2    eight 23-byte filter records
//   +186  maximum filter order, long blocks   (12)
//   +187  maximum filter order, short blocks  (7)
//   +188  maximum TNS band, long blocks       (40 at 48 kHz)
//   +189  maximum TNS band, short blocks      (14 at 48 kHz)
//
// Eight records is exactly enough for both layouts: a long block's count is two
// bits so at most 3, and a short block gets one filter per window.

#ifndef APAC_TNS_H
#define APAC_TNS_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/ics_info.h"
#include "apac/spectrum.h"

namespace apac {

// 23 bytes.  `coef` holds INDICES into apac::tables::kTNSCoefficients, already
// combined as (coefRes << 4) | (value + 8) by the parser - the same byte the
// original stores, so a filter record can be handed straight to Apply.
struct TNSFilter {
    uint8_t direction;      // +0  1 = filter downwards, from the top band
    uint8_t length;         // +1  extent in scalefactor bands
    uint8_t order;          // +2
    uint8_t coef[20];       // +3
};

const uint32_t kTNSMaxFilters = 8;
const uint32_t kTNSMaxOrder   = 20;   // the record's capacity; the real cap is 12

struct TNSData {
    uint8_t   present;                        // +0
    uint8_t   nFilt;                          // +1
    TNSFilter filter[kTNSMaxFilters];         // +2
    uint8_t   maxOrderLong;                   // +186
    uint8_t   maxOrderShort;                  // +187
    uint8_t   maxBandsLong;                   // +188
    uint8_t   maxBandsShort;                  // +189

    // APACTNSData::APACTNSData: sets the order caps to 12 / 7 and looks the
    // band caps up in the 12-record rate table at 0x331970.  An unknown rate
    // gives 0 / 0, which switches TNS off for every band.
    void Configure(uint32_t sampleRate);

    // Returns TRUE on ERROR, the original's convention.  The only error is a
    // filter whose band length is zero.  Does not touch `present`.
    bool ParseTNSData(BitstreamReader& bs, const APACICSInfo& ics);

    // No-op unless `present`.  Filters the spectrum in place.
    void Apply(const APACICSInfo& ics, const SFBBandOffsets& sfb,
               SpectralData& spectrum) const;
};

}  // namespace apac

#endif  // APAC_TNS_H
