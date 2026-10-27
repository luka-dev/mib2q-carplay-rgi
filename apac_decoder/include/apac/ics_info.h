/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACICSInfo - the per-channel individual-channel-stream header: window
// sequence, maximum scalefactor band, and the window grouping.
//
// Original: APACICSInfo::Deserialize(BitstreamReader&, apac::CodingType) at
// 0x00099e60, decompiled in reference/decomp/APACICSInfo.c.  The object is 768
// bytes in the library but Deserialize touches only these six fields, at the
// offsets named below.
//
// maxSfbLong and maxSfbShort are INPUTS: the syntactic element fills them from
// apac::GetSFBOffsetTable before calling Deserialize, and Deserialize rejects a
// maxSfb larger than whichever one applies.  Wiring them up is stage f's job.
//
// Deserialize returns TRUE on error, which is the library's convention.

#ifndef APAC_ICS_INFO_H
#define APAC_ICS_INFO_H

#include <stdint.h>

#include "apac/bitstream.h"

namespace apac {

// The one value of apac::CodingType Deserialize treats specially.  Its other
// values all take the same path, so the enum's names are not recovered.
const int32_t kCodingTypeSpecial = 1;

struct APACICSInfo {
    uint8_t  maxSfbLong;        // +8   in, from the SFB table
    uint8_t  maxSfbShort;       // +9   in, from the SFB table
    uint8_t  windowSequence;    // +10  out, 2 bits; 2 means eight short windows
    uint8_t  maxSfb;            // +11  out
    uint8_t  numWindowGroups;   // +12  out
    uint32_t windowGroupWord;   // +16  out, one hex digit per group, see below

    void Clear();

    // Returns true on ERROR.
    bool Deserialize(BitstreamReader& bs, int32_t codingType);
};

} // namespace apac

#endif
