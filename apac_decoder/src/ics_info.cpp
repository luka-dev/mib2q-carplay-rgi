/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/ics_info.h"

#include <string.h>

namespace apac {

void APACICSInfo::Clear() { memset(this, 0, sizeof *this); }

// 0x00099e60  APACICSInfo::Deserialize(TBitstreamReader<unsigned int>&, apac::CodingType)
bool APACICSInfo::Deserialize(BitstreamReader& bs, int32_t codingType)
{
    windowSequence = (uint8_t)bs.GetBits(2);
    // One 16-bit store in the original: maxSfb = 0, numWindowGroups = 1.
    maxSfb = 0;
    numWindowGroups = 1;
    windowGroupWord = 1;

    uint32_t sfbLimit;

    if (windowSequence == 2) {
        // Eight short windows.
        if (maxSfbShort == 0)
            return true;
        uint32_t grouping;      // seven grouping bits, left aligned in a word
        if (codingType == kCodingTypeSpecial) {
            maxSfb = 7;
            if (maxSfbShort <= 6)
                return true;
            sfbLimit = 7;
            grouping = 0;       // no bits read: every window starts a new group
        } else {
            uint32_t m = bs.GetBits(4);
            maxSfb = (uint8_t)m;
            if (maxSfbShort < (uint8_t)m)
                return true;
            grouping = bs.GetBits(7) << 25;
            sfbLimit = maxSfb;
        }
        // One nibble per window group, counting the windows in it: a zero bit
        // opens a new group, which shifts the running weight up by 16.  Seven
        // bits for the seven windows after the first, so an all-ones grouping
        // gives 1 + 7 = 8, one group of eight windows, and an all-zero one
        // gives 0x11111111 and eight groups of one.
        uint32_t word = windowGroupWord;
        uint32_t weight = 1;
        for (int i = 0; i < 7; ++i) {
            if ((grouping & 0x80000000u) == 0) {
                weight *= 16;
                ++numWindowGroups;
            }
            word += weight;
            grouping <<= 1;
        }
        windowGroupWord = word;
    } else if (codingType == kCodingTypeSpecial) {
        maxSfb = 27;
        return maxSfbLong <= 0x1A;
    } else {
        uint32_t m = bs.GetBits(6);
        maxSfb = (uint8_t)m;
        if (maxSfbLong < m)
            return true;
        sfbLimit = m;
    }

    if (sfbLimit == 0)
        numWindowGroups = 0;
    return false;
}

} // namespace apac
