/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACScaleFactorData and APACSpectralData - the APAC entropy/spectrum core.
//
// Originals in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//   0x0009a140  APACScaleFactorData::Deserialize(TBitstreamReader<unsigned int>&,
//                                                APACICSInfo const&)
//   0x000ab3b0  APACSpectralData::Deserialize(TBitstreamReader<unsigned int>&,
//                                             APACICSInfo const&,
//                                             APACScaleFactorData const&,
//                                             apac::SFBOffsets const&)
// Decompiled in reference/decomp/APACScaleFactorData.c and APACSpectralData.c;
// the codebook semantics they depend on are in
// reference/decomp/LRVQ_SEMANTICS.md, sections 1 and 2.
//
// Neither function pulls single fields out of the bit reader.  Each opens a
// local 32-bit MSB-aligned window, tops it up with one GetBits when it runs
// low, and gives the surplus back with a single PutBack at the end - so the
// reader's final position is right even though the refills over-read.

#ifndef APAC_SPECTRUM_H
#define APAC_SPECTRUM_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/ics_info.h"

namespace apac {

// The `apac::SFBOffsets` the spectrum decoder is HANDED - not the 40-byte
// record apac::GetSFBOffsetTable returns.  The library's type is
// { uint64 numBands; const uint16* offset; }, and the table record is a sample
// rate followed by two of them (long, short); the syntactic element picks one.
// APACSpectralData::Deserialize reads numBands as a uint16.
struct SFBBandOffsets {
    uint32_t        numBands;
    const uint16_t* offset;     // numBands + 1 entries
};

// APACScaleFactorData, 728 bytes, laid out exactly as the original - the
// spectrum decoder walks it at these offsets.
struct ScaleFactorData {
    float    scf[120];          // +0    stride 15 per window group, linear gain
    uint8_t  numRecords[8];     // +480  (codebook, run) records per window group
    uint16_t records[120];      // +488  stride 15; low byte codebook, high byte run

    void Clear();

    // Returns TRUE on ERROR, the original's convention.  Errors are: a codebook
    // index above 11, a band run that would pass ics.maxSfb, and a zero run.
    bool Deserialize(BitstreamReader& bs, const APACICSInfo& ics);
};

// APACSpectralData - a window onto the channel's 1024-coefficient spectrum.
// The original is exactly these two pointers; the caller owns the storage.
struct SpectralData {
    float* begin;
    float* end;

    // Always returns false: the original's only exit sets W0 = 0.  Malformed
    // input is caught upstream in ScaleFactorData::Deserialize.
    bool Deserialize(BitstreamReader& bs, const APACICSInfo& ics,
                     const ScaleFactorData& sf, const SFBBandOffsets& sfb);
};

} // namespace apac

#endif
