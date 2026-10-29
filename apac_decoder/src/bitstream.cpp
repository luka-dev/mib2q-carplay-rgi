/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/bitstream.h"

namespace apac {

namespace {

// The AArch64 original shifts by a register and LSL/LSR on a W register take
// the shift amount modulo 32.  Writing the mask out keeps the C defined and
// keeps the two implementations bit-identical for n == 32 and for the negative
// shift amounts the decompilation shows as `-(char)n`.
inline uint32_t shl32(uint32_t v, uint32_t s) { return v << (s & 31); }
inline uint32_t shr32(uint32_t v, uint32_t s) { return v >> (s & 31); }

} // namespace

// 0x000b1430  TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int)
BitstreamReader::BitstreamReader(const uint8_t* buf, uint32_t len)
{
    cur = buf;
    end = buf + len;
    cache = 0;
    bitsLeft = 0;

    // Pre-align: consume the leading bytes until `cur` is 4-byte aligned (or
    // the buffer runs out), so every FillCacheFrom after this loads an aligned
    // word.  `p` is tested BEFORE it is incremented, exactly as in the original;
    // that is what makes `cur` end up at the first aligned byte rather than one
    // past it.
    if (((uintptr_t)buf & 3) != 0 && end > buf) {
        const uint8_t* p = buf + 1;
        uint32_t nbits = 8;
        uint32_t acc = 0;
        for (;;) {
            bool aligned = (((uintptr_t)p & 3) == 0);
            bool atEnd   = (end <= p);
            uint32_t took = nbits;
            acc = (uint32_t)p[-1] | (acc << 8);
            const uint8_t* here = p;
            nbits += 8;
            ++p;
            if (aligned || atEnd) {
                cur = here;
                cache = shl32(acc, 0u - took);
                bitsLeft = (int32_t)took;
                break;
            }
        }
    }
}

// 0x000b1600  FillCacheFrom(unsigned char const*)
void BitstreamReader::FillCacheFrom(const uint8_t* p)
{
    // The original truncates the pointer difference to 32 bits before the
    // comparison; on the ARMv7 target that is the natural width anyway.
    int32_t avail = (int32_t)(uint32_t)(uintptr_t)(end - p);
    if (avail > 3) {
        cache = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
        return;
    }
    uint32_t c = cache;
    int32_t n = (avail < 0) ? 0 : avail;
    for (int32_t i = 0; i < n; ++i)
        c = (c << 8) | p[i];
    // Pad the rest of the word with 0xFF, NOT with zero.
    for (; n != 4; ++n)
        c = (c << 8) | 0xFFu;
    cache = c;
}

// The refill tail shared by GetBits, GetBit, SkipBits and AlignToNextByte:
// pull a fresh word in, splice the bits that were already taken, and advance.
void BitstreamReader::Refill()
{
    FillCacheFrom(cur);
    int32_t bl = bitsLeft;
    uint32_t c = cache;
    uint32_t rest = shl32(c, 0u - (uint32_t)bl);
    if (bl == -32)
        rest = 0;
    cur += 4;
    cache = rest;
    bitsLeft = bl + 32;
}

// 0x000b14e0  GetBits(unsigned int)
uint32_t BitstreamReader::GetBits(uint32_t n)
{
    if (n - 1 > 0x1Fu)          // n == 0 or n > 32: no state change, returns 0
        return 0;
    uint32_t c = cache;
    int32_t bl = bitsLeft - (int32_t)n;
    bitsLeft = bl;
    uint32_t v = shr32(c, 0u - n);
    if (bl < 0) {
        FillCacheFrom(cur);
        uint32_t nc = cache;
        int32_t nbl = bitsLeft;
        v |= shr32(nc, (uint32_t)(nbl + 32));
        uint32_t rest = shl32(nc, 0u - (uint32_t)nbl);
        if (nbl == -32)
            rest = 0;
        cur += 4;
        cache = rest;
        bitsLeft = nbl + 32;
    } else {
        cache = shl32(c, n);
    }
    return v;
}

// 0x000b16b0  GetBit()
uint32_t BitstreamReader::GetBit()
{
    uint32_t c = cache;
    int32_t bl = bitsLeft - 1;
    bitsLeft = bl;
    uint32_t v = c >> 31;
    if (bl < 0) {
        FillCacheFrom(cur);
        uint32_t nc = cache;
        int32_t nbl = bitsLeft;
        v |= shr32(nc, (uint32_t)(nbl + 32));
        uint32_t rest = shl32(nc, 0u - (uint32_t)nbl);
        if (nbl == -32)
            rest = 0;
        cur += 4;
        cache = rest;
        bitsLeft = nbl + 32;
    } else {
        cache = c << 1;
    }
    return v;
}

// 0x000b1760  SkipBits(unsigned int)
void BitstreamReader::SkipBits(uint32_t n)
{
    int32_t bl = bitsLeft;
    if (bl < (int32_t)n) {
        // One 64-bit store clears cache AND bitsLeft (they are adjacent).
        cache = 0;
        bitsLeft = 0;
        n -= (uint32_t)bl;
        if (n > 0x1Fu) {
            uint32_t words = n >> 5;
            n &= 0x1Fu;
            cur += 4 * words;
        }
    }
    if (n - 1 > 0x1Fu)
        return;
    uint32_t c = cache;
    int32_t nbl = bitsLeft - (int32_t)n;
    bitsLeft = nbl;
    if (nbl < 0)
        Refill();
    else
        cache = shl32(c, n);
}

// 0x000b1850  AlignToNextByte()
void BitstreamReader::AlignToNextByte()
{
    int32_t bl = bitsLeft;
    int32_t frac = bl & 7;
    if (frac == 0)
        return;
    // The original also tests (frac - 1) <= 31, which is always true here.
    int32_t nbl = bl - frac;
    bitsLeft = nbl;
    if (nbl < 0)
        Refill();
    else
        cache = shl32(cache, (uint32_t)frac);
}

// 0x000b1910  PutBack(unsigned int)
void BitstreamReader::PutBack(uint32_t n)
{
    if (n == 0)
        return;
    const uint8_t* c = cur;
    uint32_t total = n + (uint32_t)bitsLeft;
    int32_t nbl = (int32_t)(total & 0x1Fu);
    bitsLeft = nbl;
    const uint8_t* p = c - 4 * (total >> 5);
    cur = p;
    if (nbl) {
        FillCacheFrom(p - 4);
        cache = shl32(cache, 0u - (uint32_t)bitsLeft);
    } else {
        cache = 0;
    }
}

// 0x000b15b0  BitsRemaining() const
int32_t BitstreamReader::BitsRemaining() const
{
    // The pointer difference is truncated to 32 bits BEFORE the multiply.  That
    // is what the code does; do not "fix" it.
    uint32_t bytes = (uint32_t)(uintptr_t)(end - cur);
    return (int32_t)((uint32_t)bitsLeft + (bytes << 3));
}

// 0x00099220 (<6,12>), 0x00096630 (<8,16>), 0x00078150 (<16,20>)
uint32_t GetEscBits(BitstreamReader& bs, uint32_t numBits,
                    uint32_t escBits, uint32_t esc2Bits)
{
    // The original asserts numBits <= 32 (APACStructs.h:198) and then takes a
    // path for numBits outside 1..32 that reads nothing: numBits == 32 returns
    // 0 outright, numBits == 0 falls through into the escape read with v == 0.
    uint32_t all = (uint32_t)((1ull << numBits) - 1ull);
    uint32_t v;
    if (numBits - 1 > 0x1Fu) {
        v = 0;
        if ((uint32_t)(1ull << numBits) != 1u)
            return v;
    } else {
        v = bs.GetBits(numBits);
        if (v != all)
            return v;
    }
    uint32_t e1 = bs.GetBits(escBits);
    v += e1;
    if (e1 != (uint32_t)((1ull << escBits) - 1ull))
        return v;
    v += bs.GetBits(esc2Bits);
    return v;
}

} // namespace apac
