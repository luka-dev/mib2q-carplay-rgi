/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// TBitstreamReader<unsigned int> - the APAC bit reader, bit-exact against the library.
//
// Reference: libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010), out-of-line
// copies at 0x000b1430 .. 0x000b19a8, decompiled in
// reference/decomp/TBitstreamReader.c.  Apple's own source calls this typedef
// `BitstreamReader` (see the __PRETTY_FUNCTION__ string at 0x30ea90).
//
// This is a transcription, not a reimplementation.  Three behaviours are not
// what a fresh implementation would do and are all deliberate:
//
//   * the refill is a BIG-ENDIAN 32-bit load;
//   * a short tail pads with 0xFF, not with zero;
//   * the constructor pre-loads the leading 1-3 bytes of an unaligned buffer so
//     that every later refill can assume `cur` is 4-byte aligned.
//
// Every shift is written with an explicit `& 31`, because the AArch64 original
// relies on LSL/LSR taking their shift amount modulo 32 and C would make those
// cases undefined.  In particular `GetBits(32)` on a full cache leaves the
// cache unchanged rather than zeroed; that is what the hardware does and our
// decoder keeps it.  See reference/decomp/NOTES.md, "open questions", item 4.

#ifndef APAC_BITSTREAM_H
#define APAC_BITSTREAM_H

#include <stdint.h>

namespace apac {

class BitstreamReader {
public:
    // buf may be unaligned; len is in bytes.
    BitstreamReader(const uint8_t* buf, uint32_t len);

    uint32_t GetBits(uint32_t n);       // n outside 1..32 reads nothing, returns 0
    uint32_t GetBit();
    void     SkipBits(uint32_t n);
    void     AlignToNextByte();
    void     PutBack(uint32_t n);
    int32_t  BitsRemaining() const;

    // Public because GetEscBits and the config parsers are transcribed from
    // code that inlined the reader and touched these directly.
    void FillCacheFrom(const uint8_t* p);

    const uint8_t* cur;       // +0   next 32-bit word to pull into the cache
    const uint8_t* end;       // +8   one past the last byte of the buffer
    uint32_t       cache;     // +16  MSB-aligned; bits are taken from the top
    int32_t        bitsLeft;  // +20  signed; < 0 means the cache must be refilled

private:
    void Refill();            // the shared tail of GetBits/GetBit/SkipBits/Align
};

// apac::GetEscBits<ESCBits, ESC2Bits>(BitstreamReader&, uint32_t numBits).
// Instantiated exactly three times in the library - <16,20> at 0x78150,
// <8,16> at 0x96630, <6,12> at 0x99220 - so the bitstream has three
// escape-coded field widths and no others.  Decompiled in decomp/apac.c.
uint32_t GetEscBits(BitstreamReader& bs, uint32_t numBits,
                    uint32_t escBits, uint32_t esc2Bits);

} // namespace apac

#endif
