/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac/config.h"
#include "apac/drc.h"
#include "apac/sfb_offsets.h"

#include <math.h>
#include <string.h>

#if defined(__QNX__) && !defined(log2f)
// QNX 6.5 hides every C99 function in <math.h> behind `#if _HAS_C9X`, which the
// headers set only for C compiled as C99 - so in C++ the declaration is simply
// absent, even though libm.so.2 exports the symbol (`nm -D` finds log2f, exp2f,
// pow, ceilf).  Declaring it is the whole fix.  Defining _HAS_C9X globally
// instead would also switch on QNX's own C++ <math.h> overloads, which then
// collide with GCC's <cmath> wherever a translation unit includes both.
extern "C" float log2f(float);
#endif

namespace apac {

namespace {

uint32_t popcount32(uint32_t v)
{
    v = v - ((v >> 1) & 0x55555555u);
    v = (v & 0x33333333u) + ((v >> 2) & 0x33333333u);
    v = (v + (v >> 4)) & 0x0F0F0F0Fu;
    return (v * 0x01010101u) >> 24;
}

// apac::ASCChannelMap, as far as GlobalConfig::Deserialize uses it: a list of
// [start, end) channel ranges kept sorted by start, with an insert that fails
// when the new range overlaps a neighbour.  0x000af5b0 / 0x000af770.
struct ChannelMap {
    struct Range { uint16_t start, end; uint32_t type; };
    Range r[kMaxCodecConfigs + kMaxASCConfigs];
    uint32_t n;

    void Clear() { n = 0; }

    bool AddMap(uint32_t type, uint16_t start, uint16_t end)
    {
        if (n >= sizeof r / sizeof r[0])
            return false;
        // upper_bound by start, which is what the original's binary search finds
        uint32_t i = 0;
        while (i < n && !(start < r[i].start))
            ++i;
        if (i != n) {
            // The original tests the entry at the insertion point and the one
            // after it, not the one before.  Transcribed as written.
            if ((r[i].start < end && r[i].end > start) ||
                (i + 1 != n && r[i + 1].start < end && r[i + 1].end > start))
                return false;
        }
        for (uint32_t k = n; k > i; --k)
            r[k] = r[k - 1];
        r[i].start = start;
        r[i].end = end;
        r[i].type = type;
        ++n;
        return true;
    }
};

// 0x000b0d20  apac::ConvertToAudioDataType(apac::ASCType): 6 becomes 8, every
// other value passes through.
uint32_t ConvertToAudioDataType(uint32_t t) { return t == 6 ? 8u : t; }

} // namespace

// ---------------------------------------------------------------------------
bool ProfileLevelConfig::IsDecodable(uint16_t profile, uint16_t streamProfile)
{
    if (profile > 31)
        return false;
    return streamProfile <= 31 && profile <= streamProfile;
}

void ChannelLayout::Clear() { memset(this, 0, sizeof *this); }

// 0x0007c080 / 0x000975f0 - one body, reached through two classes.
uint32_t ChannelLayout::NumChannels() const
{
    if (tag == 0)
        return numDescriptions;
    if (tag == 0x10000u)
        return popcount32(bitmap);
    return tag & 0xFFFFu;
}

void CoreDecoderConfig::Clear() { memset(this, 0, sizeof *this); }
void BaseASCConfig::Clear()     { memset(this, 0, sizeof *this); }

// 0x0007e160 apac::lbr::DecoderConfig::Deserialize, which reads one bit and
// then falls into 0x0007c100 apac::chan::CoreDecoderConfig::Deserialize.
// GlobalConfig always builds the lbr subclass, whatever the stream.
uint32_t CoreDecoderConfig::Deserialize(BitstreamReader& bs)
{
    lbrFlag = (uint8_t)(bs.GetBit() != 0);

    // Element count, escape coded by hand as 5 / +10 / +16 rather than through
    // GetEscBits; the two escape steps add 31 and 1054.
    uint32_t count = bs.GetBits(5);
    if (count == 31) {
        uint32_t e1 = bs.GetBits(10);
        count = (e1 == 1023) ? bs.GetBits(16) + 1054 : e1 + 31;
    }
    if (count > kMaxElements)
        return kPortTooManyElements;
    numElements = count;

    uint32_t nch = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t t = bs.GetBits(3);
        elementTypes[i] = (uint8_t)t;
        if (t == 1)
            nch += 2;                 // channel pair
        else if (t == 3 || t == 0)
            nch += 1;                 // LFE, single channel
        // every other element type contributes no channels
    }
    if (nch > kMaxChannels)
        return kPortTooManyChannels;

    layout.Clear();
    uint32_t tag16 = bs.GetBits(16);
    if (tag16 == 0) {
        layout.numDescriptions = nch;
        for (uint32_t i = 0; i < nch; ++i)
            layout.labels[i] = (uint8_t)bs.GetBits(7);
    } else if (tag16 != 1) {
        layout.tag = nch | (tag16 << 16);
    } else {
        // Transcribed as written: for tag16 == 1 the original stores the bitmap
        // in word 1 and never sets word 0 to 0x10000, so GetNumChannels then
        // reads the layout as "use descriptions" with a count of zero.  That
        // looks like a bug in Apple's source.  It is unreachable on the CarPlay
        // streams, which carry tag16 121 or 124, and is NOT corrected here.
        layout.bitmap = bs.GetBits(27);
    }

    hasRemap = (uint8_t)(bs.GetBit() != 0);
    if (!hasRemap)
        return kOk;

    uint32_t n = NumChannels();
    if (n > kMaxChannels)
        return kPortTooManyChannels;
    // vcvtps_u32_f32: float to unsigned, rounding toward +infinity.  A negative
    // result (n == 1; n == 0 gives -inf) saturates to zero.
    float f = log2f((float)n) - 0.0001f;
    remapBits = (f <= 0.0f) ? 0u : (uint32_t)ceilf(f);
    for (uint32_t i = 0; i < NumChannels(); ++i) {
        uint32_t idx = 0;
        if (remapBits - 1 <= 0x1Fu)
            idx = bs.GetBits(remapBits);
        remap[i] = (uint8_t)idx;
        if (remap[i] >= NumChannels())
            return kBadValue;
    }
    return kOk;
}

// ---------------------------------------------------------------------------
// 0x000978c0  apac::GlobalConfig::Reset()
void GlobalConfig::Reset()
{
    memset(this, 0, sizeof *this);
}

// 0x000979a0  apac::GlobalConfig::SetChannelLayout()
//
// Only the single-codec-config path is ported, which is the path both the live
// CarPlay cookie and the afconvert-generated one take: copy that config's
// layout up into the GlobalConfig and accept it if it already names a channel
// count.  The original's other two paths are NOT here -
//   * more than one config, where it merges the layouts and renumbers them;
//   * a layout with no channel count, where it derives a tag from
//     apac::GetProfileConfig's table in .data.rel.ro.
// Neither reads any bits, so neither can move the bitstream position.
uint32_t GlobalConfig::SetChannelLayout()
{
    if (numCodecConfigs != 1)
        return kPortTooManyConfigs;
    layout = codecConfigs[0].layout;
    if (layout.tag == 0) {
        if (layout.numDescriptions)
            return kOk;
    } else if (layout.tag == 0x10000u) {
        if (popcount32(layout.bitmap))
            return kOk;
    } else if ((layout.tag & 0xFFFFu) != 0) {
        return kOk;
    }
    return kPortNeedsProfileTable;
}

// 0x00097d50  apac::GlobalConfig::Deserialize(TBitstreamReader<unsigned int>&)
uint32_t GlobalConfig::Deserialize(BitstreamReader& bs)
{
    const int32_t startBits = bs.BitsRemaining();
    Reset();

#define APAC_RETURN(rc) do { bitsConsumed = startBits - bs.BitsRemaining(); return (rc); } while (0)

    profileLevel.profile = (uint16_t)bs.GetBits(6);
    profileLevel.level   = (uint8_t) bs.GetBits(4);
    bs.GetBit();                        // one reserved bit; the value is dropped

    sampleRateIndex = bs.GetBits(6);
    if (sampleRateIndex > 12)
        APAC_RETURN(kBadValue);
    sampleRate = SampleRateForIndex(sampleRateIndex);

    // A 6-bit frame-length index that must be zero.  APACDecoder_FrameLength
    // says 1024 exactly when these six bits - mask 0x7e00 of the big-endian
    // config word - are clear, which is the same statement.
    frameLengthIndex = bs.GetBits(6);
    if (frameLengthIndex != 0)
        APAC_RETURN(kUnsupportedValue);
    frameLength = 1024;

    numChannels = bs.GetBits(8);
    reservedAfterChannels = bs.GetBits(8);   // read and dropped by the original
    if (bs.GetBit())
        APAC_RETURN(kBadValue);

    // ---- codec configs --------------------------------------------------
    uint32_t n = GetEscBits(bs, 3, 6, 12);
    if (n == 0)
        APAC_RETURN(kBadValue);
    if (n > kMaxCodecConfigs)
        APAC_RETURN(kPortTooManyConfigs);
    numCodecConfigs = n;

    ChannelMap map;
    map.Clear();
    uint32_t total = 0;
    for (uint32_t i = 0; i < n; ++i) {
        CoreDecoderConfig& cfg = codecConfigs[i];
        cfg.Clear();
        uint32_t chOff = bs.GetBits(8);
        uint32_t type  = bs.GetBits(3);
        if (type != 0)
            APAC_RETURN(kUnsupportedValue);
        uint32_t rc = cfg.Deserialize(bs);
        if (rc != kOk)
            APAC_RETURN(rc);
        uint32_t got = cfg.NumChannels();
        total += got;
        if (got == 0 || total <= chOff ||
            !map.AddMap(ConvertToAudioDataType(0), (uint16_t)chOff,
                        (uint16_t)(got + chOff)))
            APAC_RETURN(kBadValue);
        cfg.channelOffset = (uint16_t)chOff;
    }

    // ValidateProfileLevelData, 0x0009d8e0.  Only its first gate is ported.  The
    // profile it infers from the codec configs (0x0009d2e0) and the level it
    // derives from them (0x0009d0d0) both walk pointer tables in .data.rel.ro
    // that were not dumped, so a stream this accepts might still be rejected by
    // the original on those grounds.  Neither reads any bits.
    if (profileLevel.profile != kSupportedProfile ||
        profileLevel.level > kSupportedMaxLevel)
        APAC_RETURN(kUnsupportedValue);
    {
        uint32_t rc = SetChannelLayout();
        if (rc != kOk)
            APAC_RETURN(rc);
    }

    // ---- optional ASC config list ---------------------------------------
    hasASCConfigData = (uint8_t)(bs.GetBit() != 0);
    if (hasASCConfigData) {
        uint32_t m = GetEscBits(bs, 3, 6, 12);
        if (m == 0)
            APAC_RETURN(kBadValue);
        if (m > kMaxASCConfigs)
            APAC_RETURN(kPortTooManyConfigs);
        numASCConfigs = m;
        for (uint32_t i = 0; i < m; ++i) {
            BaseASCConfig& cfg = ascConfigs[i];
            cfg.Clear();
            uint32_t chOff = bs.GetBits(8);
            if (chOff >= layout.NumChannels())
                APAC_RETURN(kBadValue);
            cfg.type = bs.GetBits(3);
            cfg.channelOffset = (uint16_t)chOff;
        }
        // The original then sorts these by channel offset and hands each one a
        // slice of the global layout.  That is bookkeeping over data already
        // read - it consumes no bits - and is not ported.
    }

    // ---- per-config trailer ---------------------------------------------
    // Once per config in whichever list is active: a 3 / +6 / +9 escape-coded
    // field and then a 2 / +8 / +32 one, both discarded.
    uint32_t trailers = hasASCConfigData ? numASCConfigs : numCodecConfigs;
    for (uint32_t i = 0; i < trailers; ++i) {
        if (bs.GetBits(3) == 7 && bs.GetBits(6) == 63)
            bs.GetBits(9);
        if (bs.GetBits(2) == 3 && bs.GetBits(8) == 255)
            bs.GetBits(32);
    }

    // ---- AncillaryConfig, 0x00072e60 ------------------------------------
    if (bs.GetBit())
        APAC_RETURN(kOk);   // the original also returns 0 here, without setting +248
    if (bs.GetBit())
        // apac::AudioSceneConfig::Deserialize - the Ambisonics scene
        // description, explicitly out of scope for the CarPlay 5.1 target.
        APAC_RETURN(kPortNeedsAudioScene);
    if (bs.GetBit()) {
        // apac::LoudnessDRCConfig::Deserialize, 0x00072bb0, with
        // HeaderPayloadType 4 - the value AncillaryConfig passes.  Ported in
        // src/drc.cpp, where it is three dispatchers deep and then stops at
        // mpddrc::UniDrc::Deserialize, the only bit consumer in the DRC layer
        // and not part of this port.  It consumes nothing on the way out, so
        // the position reported here is still exact.
        LoudnessDRCConfig drcCfg;
        drcCfg.Clear();
        APAC_RETURN(drcCfg.Deserialize(bs, kHeaderPayloadConfig));
    }
    if (bs.GetBits(2) != 0)
        APAC_RETURN(kOk);

    // ---- ExtensionConfig, 0x000964a0 ------------------------------------
    for (;;) {
        if (!bs.GetBit())
            break;
        GetEscBits(bs, 4, 8, 16);                       // extension type
        int32_t len = (int32_t)(8u * (GetEscBits(bs, 4, 8, 16) + 1u));
        int32_t before = bs.BitsRemaining();
        if (before < len)
            APAC_RETURN(kBadValue);
        bs.SkipBits((uint32_t)len);
        int32_t extra = len - (before - bs.BitsRemaining());
        if (extra < 0)
            APAC_RETURN(kBadValue);
        bs.SkipBits((uint32_t)extra);
    }

    deserialized = 1;
    APAC_RETURN(kOk);
#undef APAC_RETURN
}

} // namespace apac
