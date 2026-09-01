/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Scalefactor-band offset tables, copied byte-for-byte out of
// libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010).  Nothing here is
// retyped from a specification or regenerated from a formula: each array is the
// little-endian uint16 view of the bytes dumped in
// reference/decomp/rodata/apac_rodata_<addr>.bin, truncated to the numSfb+1
// entries that reference/decomp/RODATA.md pins by arithmetic.
//
// The 12-record dispatch table and the record layout are
// apac::GetSFBOffsetTable at 0x0009d9d0; several rates share an array, exactly
// as the original's records share a pointer.

#include "apac/sfb_offsets.h"

namespace apac {
namespace {
// 0x00330088, short, 15+1 entries
const uint16_t kSfb_00330088[16] = {
    0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 60, 72, 88, 108, 128,
};
// 0x003300a8, long, 40+1 entries
const uint16_t kSfb_003300a8[41] = {
    0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 120, 132, 144, 156, 172, 188,
    204, 220, 236, 252, 268, 288, 308, 328, 348, 372, 396, 420, 448, 476,
    508, 544, 580, 620, 664, 712, 764, 820, 880, 944, 1024,
};
// 0x00330100, short, 15+1 entries
const uint16_t kSfb_00330100[16] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 60, 72, 88, 108, 128,
};
// 0x00330120, long, 43+1 entries
const uint16_t kSfb_00330120[44] = {
    0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 100, 112, 124, 136, 148,
    160, 172, 184, 196, 212, 228, 244, 260, 280, 300, 320, 344, 368, 396,
    424, 456, 492, 532, 572, 616, 664, 716, 772, 832, 896, 960, 1024,
};
// 0x00330178, short, 15+1 entries
const uint16_t kSfb_00330178[16] = {
    0, 4, 8, 12, 16, 20, 24, 28, 36, 44, 52, 64, 76, 92, 108, 128,
};
// 0x00330198, long, 47+1 entries
const uint16_t kSfb_00330198[48] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 52, 60, 68, 76, 84, 92, 100,
    108, 116, 124, 136, 148, 160, 172, 188, 204, 220, 240, 260, 284, 308,
    336, 364, 396, 432, 468, 508, 552, 600, 652, 704, 768, 832, 896, 960,
    1024,
};
// 0x003301f8, long, 51+1 entries
const uint16_t kSfb_003301f8[52] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 88, 96, 108,
    120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320, 352, 384, 416,
    448, 480, 512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864,
    896, 928, 960, 992, 1024,
};
// 0x00330260, short, 14+1 entries
const uint16_t kSfb_00330260[15] = {
    0, 4, 8, 12, 16, 20, 28, 36, 44, 56, 68, 80, 96, 112, 128,
};
// 0x00330280, long, 49+1 entries
const uint16_t kSfb_00330280[50] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 88, 96, 108,
    120, 132, 144, 160, 176, 196, 216, 240, 264, 292, 320, 352, 384, 416,
    448, 480, 512, 544, 576, 608, 640, 672, 704, 736, 768, 800, 832, 864,
    896, 928, 1024,
};
// 0x003302e8, short, 12+1 entries
const uint16_t kSfb_003302e8[13] = {
    0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128,
};
// 0x00330308, long, 47+1 entries
const uint16_t kSfb_00330308[48] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88,
    100, 112, 124, 140, 156, 172, 192, 216, 240, 268, 304, 344, 384, 424,
    464, 504, 544, 584, 624, 664, 704, 744, 784, 824, 864, 904, 944, 984,
    1024,
};
// 0x00330368, short, 12+1 entries
const uint16_t kSfb_00330368[13] = {
    0, 4, 8, 12, 16, 20, 24, 32, 40, 48, 64, 92, 128,
};
// 0x00330388, long, 41+1 entries
const uint16_t kSfb_00330388[42] = {
    0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88,
    96, 108, 120, 132, 144, 156, 172, 188, 212, 240, 276, 320, 384, 448, 512,
    576, 640, 704, 768, 832, 896, 960, 1024,
};
// The 12 records at 0x004258f8, in the order the original searches them.
const SFBOffsets kRecords[12] = {
    { 96000, 41, kSfb_00330388, 12, kSfb_00330368 },
    { 88200, 41, kSfb_00330388, 12, kSfb_00330368 },
    { 64000, 47, kSfb_00330308, 12, kSfb_003302e8 },
    { 48000, 49, kSfb_00330280, 14, kSfb_00330260 },
    { 44100, 49, kSfb_00330280, 14, kSfb_00330260 },
    { 32000, 51, kSfb_003301f8, 14, kSfb_00330260 },
    { 24000, 47, kSfb_00330198, 15, kSfb_00330178 },
    { 22050, 47, kSfb_00330198, 15, kSfb_00330178 },
    { 16000, 43, kSfb_00330120, 15, kSfb_00330100 },
    { 12000, 43, kSfb_00330120, 15, kSfb_00330100 },
    { 11025, 43, kSfb_00330120, 15, kSfb_00330100 },
    {  8000, 40, kSfb_003300a8, 15, kSfb_00330088 },
};

// 0x003483a8 - the decoder's sample-rate table, indexed by the 6-bit rate index.
const uint32_t kSampleRates[kNumSampleRates] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

} // namespace

// 0x0009d9d0  apac::GetSFBOffsetTable(unsigned int, unsigned int)
const SFBOffsets* GetSFBOffsetTable(uint32_t sampleRate, uint32_t frameLength)
{
    if (frameLength != 1024)
        return 0;
    for (uint32_t i = 0; i < 12; ++i)
        if (kRecords[i].sampleRate == sampleRate)
            return &kRecords[i];
    return 0;
}

uint32_t SampleRateForIndex(uint32_t index)
{
    if (index >= kNumSampleRates)
        return 0;
    return kSampleRates[index];
}

} // namespace apac
