/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac::GlobalConfig and the configuration objects it deserialises.
//
// Originals in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x00097d50  apac::GlobalConfig::Deserialize(BitstreamReader&)
//   0x000978c0  apac::GlobalConfig::Reset()
//   0x000979a0  apac::GlobalConfig::SetChannelLayout()
//   0x0007e160  apac::lbr::DecoderConfig::Deserialize(BitstreamReader&)
//   0x0007c100  apac::chan::CoreDecoderConfig::Deserialize(BitstreamReader&)
//   0x0007c080  apac::chan::CoreDecoderConfig::GetNumChannels() const
//   0x00098f00  apac::BaseASCConfig::Deserialize(BitstreamReader&)   - returns 5, reads nothing
//   0x0009d8e0  apac::ProfileLevelConfig::ValidateProfileLevelData(...)
//   0x0009d870  apac::ProfileLevelConfig::IsDecodable(Profile, Profile)
//   0x000af770  apac::ASCChannelMap::AddMap(AudioDataType, unsigned short, unsigned short)
//
// No STL and no dynamic allocation: the original's std::vectors become
// fixed-capacity arrays and an over-long stream is rejected with a port-specific
// status instead of growing.  The capacities are far above anything the CarPlay
// 5.1 / 5.1.2 profile can produce.
//
// The channel layout is CoreAudio's AudioChannelLayout, reached through a
// vector<char> in the original: word 0 is the tag, word 1 the bitmap, word 2 the
// description count, then 20-byte AudioChannelDescriptions.  Tag 0 means "use
// the descriptions", 0x10000 means "use the bitmap", anything else encodes
// (constant << 16) | channelCount - so the 5.1 streams here carry 0x007c0006,
// kAudioChannelLayoutTag_MPEG_5_1_D, C L R Ls Rs LFE.

#ifndef APAC_CONFIG_H
#define APAC_CONFIG_H

#include <stdint.h>

#include "apac/bitstream.h"

namespace apac {

// Capacities.  Not limits of the format: limits of this port.
const uint32_t kMaxCodecConfigs = 8;
const uint32_t kMaxASCConfigs   = 16;
const uint32_t kMaxElements     = 64;
const uint32_t kMaxChannels     = 64;

// Status codes.  0, 1, 4 and 5 are the original's own return values.  Anything
// >= kStatusPortBase is this port saying it will not guess.
enum Status {
    kOk                  = 0,
    kBadValue            = 1,   // the original's `return 1`
    kUnsupportedValue    = 4,   // the original's `return 4`
    kNotDeserialized     = 5,   // BaseASCConfig::Deserialize's constant

    kStatusPortBase      = 0x100,
    kPortNeedsAudioScene   = 0x100,  // apac::AudioSceneConfig, out of scope here
    kPortNeedsLoudnessDRC  = 0x101,  // apac::LoudnessDRCConfig -> mpddrc, stage h
    kPortTooManyElements   = 0x102,
    kPortTooManyChannels   = 0x103,
    kPortTooManyConfigs    = 0x104,
    kPortNeedsProfileTable = 0x105,  // SetChannelLayout's profile-derived layout

    // Frame layer.  The first two are refusals: each stands in for a tool this
    // port does not implement, each is proved unreachable on the CarPlay 5.1
    // capture, and each propagates as a frame error exactly as the original's
    // own non-zero return from that tool does.  The last two are this port's
    // fixed capacities, which the original grows instead.
    kPortNeedsLRVQ         = 0x106,  // APACLRVQDecoder, stage h1
    kPortNeedsBWE          = 0x107,  // APACBWE2Decoder::DecodeFrame, stage e
    kPortTooManyElementsRT = 0x108,  // more elements than the decoder can hold
    kPortBadElementType    = 0x109   // an element type CreateList would reject
};

// apac::ProfileLevelConfig - GlobalConfig + 32, three bytes.
struct ProfileLevelConfig {
    uint16_t profile;   // +0
    uint8_t  level;     // +2

    // 0x0009d870.  Both sides must be <= 31 and `profile` must not exceed the
    // stream's own.
    static bool IsDecodable(uint16_t profile, uint16_t streamProfile);
};

// The one entry of apac::kSupportedProfileLevels at 0x00330080: `1f 00 06 00`.
const uint16_t kSupportedProfile    = 31;
const uint16_t kSupportedMaxLevel   = 6;

struct ChannelLayout {
    uint32_t tag;              // word 0
    uint32_t bitmap;           // word 1
    uint32_t numDescriptions;  // word 2
    uint8_t  labels[kMaxChannels];

    void Clear();
    uint32_t NumChannels() const;   // 0x0007c080 / 0x000975f0, same body
};

// apac::chan::CoreDecoderConfig, plus the one bit apac::lbr::DecoderConfig
// prepends to it.  GlobalConfig always builds the lbr subclass.
struct CoreDecoderConfig {
    uint8_t       lbrFlag;         // +96 in the original
    uint16_t      channelOffset;   // +42, written by GlobalConfig after Deserialize
    uint32_t      numElements;
    uint8_t       elementTypes[kMaxElements];
    ChannelLayout layout;
    uint8_t       hasRemap;
    uint32_t      remapBits;       // +44
    uint8_t       remap[kMaxChannels];

    void Clear();
    uint32_t NumChannels() const { return layout.NumChannels(); }
    uint32_t Deserialize(BitstreamReader& bs);      // the lbr entry point
};

// apac::BaseASCConfig.  Its Deserialize reads nothing and returns 5; the two
// fields are written by GlobalConfig from the bits it reads itself.
struct BaseASCConfig {
    uint32_t      type;            // +8, the 3-bit field
    uint16_t      channelOffset;   // +42, the 8-bit field
    ChannelLayout layout;

    void Clear();
    uint32_t NumChannels() const { return layout.NumChannels(); }
    static uint32_t Deserialize(BitstreamReader& bs) { (void)bs; return kNotDeserialized; }
};

struct GlobalConfig {
    ProfileLevelConfig profileLevel;   // +32 .. +34
    uint32_t sampleRate;               // +36
    uint32_t sampleRateIndex;
    uint32_t frameLengthIndex;         // +41; must be 0
    uint32_t frameLength;              // 1024 when frameLengthIndex == 0
    uint32_t numChannels;              // +43
    uint32_t reservedAfterChannels;    // 8 bits the original reads and drops

    uint32_t numCodecConfigs;
    CoreDecoderConfig codecConfigs[kMaxCodecConfigs];

    uint8_t  hasASCConfigData;         // +249
    uint32_t numASCConfigs;
    BaseASCConfig ascConfigs[kMaxASCConfigs];

    ChannelLayout layout;              // the layout at +256, from SetChannelLayout
    uint8_t  deserialized;             // +248, set only after the whole config parses

    // Where the reader stood, in bits from the start of the config, when
    // Deserialize returned.  Meaningful for a port-specific status too, which is
    // how the tests pin the point the port stops at.
    int32_t  bitsConsumed;

    void Reset();                                   // 0x000978c0
    uint32_t Deserialize(BitstreamReader& bs);      // 0x00097d50
    uint32_t SetChannelLayout();                    // 0x000979a0, single-config path
};

} // namespace apac

#endif
