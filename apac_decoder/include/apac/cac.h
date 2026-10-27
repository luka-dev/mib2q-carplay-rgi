/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACCACDecoder - the channel-pair "CAC" tool: a per-band Givens rotation
// between the two channels of a channel-pair element, with the rotation angle
// carried as a run-length-coded index into a quantised gain table.
//
// Reference functions in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x0007b5a0  APACCACDecoder::Deserialize(TBitstreamReader<unsigned>&,
//                                           APACICSInfo const&)
//   0x0007b8c0  APACCACDecoder::ProcessCac(APACICSInfo const&,
//                                          APACIndividualChannelStream&,
//                                          APACIndividualChannelStream&) const
//   0x000b2110  CacQuantizer::GainTableEntry(unsigned) const
//   0x000b21d0  CacQuantizer::Initialize()
//   0x000b2070  CacDecoder::RepeatCodeTableDec() const
//   0x000b20c0  CacDecoder::GainIndexCodeTableDec() const
//   0x0007b070  the SIMD rotation kernel ProcessCac calls (sub_7B070)
//
// Decompiled in reference/decomp/APACCACDecoder.c, CacQuantizer.c,
// CacDecoder.c and _free_functions.c.
//
// This tool IS live on the CarPlay 5.1 capture: patching
// APACCACDecoder::Deserialize's entry to `return 0` - i.e. letting it consume
// no bits - desynchronises the bitstream and the library then produces no
// output at all, and patching ProcessCac the same way changes the decoded PCM.
// See STATUS.md, "What the stream actually uses".
//
// The two code tables are copied byte for byte out of the library's .rodata
// (`CacDecoder::kGainIndexCodeTableDec` at 0x331b28, 35 records, and
// `kRepeatCodeTableDec` at 0x331c00, 44 records) by tools/gen_cac_tables.py.
// A record is six bytes: value, pad, u16 code, code length, pad; a code matches
// when it equals the top `length` bits of a 16-bit look-ahead window.

#ifndef APAC_CAC_H
#define APAC_CAC_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/ics_info.h"
#include "apac/sfb_offsets.h"
#include "apac/spectrum.h"

namespace apac {

// One record of either variable-length code table.
struct CacCode {
    uint8_t  value;
    uint16_t code;
    uint8_t  length;
};

const uint32_t kCacGainCodes   = 35;
const uint32_t kCacRepeatCodes = 44;

// The run-length terminator carried in the repeat table.
const uint8_t kCacRepeatEnd = 43;

extern const CacCode kCacGainCodeTable[kCacGainCodes];
extern const CacCode kCacRepeatCodeTable[kCacRepeatCodes];

// CacQuantizer's 18-entry gain table, built by CacQuantizer::Initialize.
// `gain` is 10^((1.5*(k-1) - 12)/20) for k >= 1 and 0 for k == 0; `cosine` and
// `sine` are the normalised rotation pair built from g = gain, or from 1/gain
// once gain reaches 0.999.  Indices 18..34 are the same entries with `gain` and
// `sine` negated, and anything above 34 reads entry 0.
struct CacGain {
    float gain;     // .a in the library's 12-byte record
    float sine;     // .b = 1 / sqrt(1 + g*g)
    float cosine;   // .c = g / sqrt(1 + g*g)
};

// APACScaleFactorData's stride: 15 bands x 8 window groups.
const uint32_t kCacMaxBands = 120;

class CACDecoder {
public:
    void Initialize();          // CacQuantizer::Initialize

    // Returns TRUE on ERROR, the library's convention.  Reads nothing when
    // ics.maxSfb is zero.  Leaves the reader exactly where the library does:
    // it decodes out of a 16-bit look-ahead window and gives the surplus back
    // with a single PutBack(16).
    bool Deserialize(BitstreamReader& bs, const APACICSInfo& ics);

    // Rotates the two spectra in place.  Returns TRUE on ERROR.
    bool ProcessCac(const APACICSInfo& ics, const SFBOffsets& table,
                    float* left, float* right) const;

private:
    CacGain        mTable[18];
    uint8_t        mGainIndex[kCacMaxBands];
    uint8_t        mRepeat[kCacMaxBands];

    void  Entry(uint32_t index, CacGain& out) const;
};

}  // namespace apac

#endif  // APAC_CAC_H
