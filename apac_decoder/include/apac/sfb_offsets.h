/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac::SFBOffsets - the scalefactor-band layout for one sample rate.
//
// Original: apac::GetSFBOffsetTable(uint32 sampleRate, uint32 frameLength) at
// 0x0009d9d0 in libSPF_CP_PlugInWrapper.so.  It returns NULL unless
// frameLength == 1024 - the decoder has no other frame length - and then
// linearly searches a 12-record table at 0x004258f8 for a matching rate.
// Records are 40 bytes: {u64 rate, u64 numLong, const u16* long, u64 numShort,
// const u16* short}, with numSfb+1 entries in each array.
// See reference/decomp/RODATA.md, "The scalefactor-band offset table".
//
// The offset arrays themselves are copied byte-for-byte out of the binary;
// src/sfb_tables.cpp names the .rodata address each one came from.

#ifndef APAC_SFB_OFFSETS_H
#define APAC_SFB_OFFSETS_H

#include <stdint.h>

namespace apac {

struct SFBOffsets {
    uint32_t        sampleRate;
    uint32_t        numSfbLong;
    const uint16_t* sfbOffsetLong;    // numSfbLong + 1 entries
    uint32_t        numSfbShort;
    const uint16_t* sfbOffsetShort;   // numSfbShort + 1 entries
};

// NULL for frameLength != 1024 and for any rate not in the 12-record table.
// 7350 Hz is in the decoder's sample-rate table but has no SFB record.
const SFBOffsets* GetSFBOffsetTable(uint32_t sampleRate, uint32_t frameLength);

// The decoder's sample-rate table at 0x003483a8, 13 entries, indexed by the
// 6-bit rate index in GlobalConfig.  Returns 0 for an index >= 13.
uint32_t SampleRateForIndex(uint32_t index);

const uint32_t kNumSampleRates   = 13;
const uint32_t kMaxSfbLong       = 51;   // 32000 Hz
const uint32_t kMaxSfbShort      = 15;   // 24000 Hz and below

} // namespace apac

#endif
