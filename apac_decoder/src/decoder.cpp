#include "apac/decoder.h"

#include <string.h>

namespace apac {

// ---------------------------------------------------------------------------
// apac::TrimmingData::Deserialize, 0x00078040
// ---------------------------------------------------------------------------

void TrimmingData::Initialize(uint32_t frameLength)
{
    leading  = 0;
    trailing = 0;
    limit    = frameLength;
}

bool TrimmingData::Deserialize(BitstreamReader& bs)
{
    if (!bs.GetBit())
        return false;
    const uint32_t a = GetEscBits(bs, 11, 16, 20);
    const uint32_t b = GetEscBits(bs, 11, 16, 20);
    if (a + b > limit)
        return true;
    leading  = a;
    trailing = b;
    return false;
}

// ---------------------------------------------------------------------------
// APACCoreDecoder / APACCoreLBRDecoder
// ---------------------------------------------------------------------------

uint32_t CoreDecoder::Initialize(const CoreDecoderConfig& cfg,
                                 uint32_t sampleRate, uint32_t frameLength,
                                 LRVQDecoder* lrvq)
{
    mLRVQ = lrvq;

    // APACInstanceConfig::Create, 0x0009a870: the SFB record for the stream's
    // rate and frame length, or nothing at all.
    mTable = GetSFBOffsetTable(sampleRate, frameLength);
    if (mTable == 0)
        return kBadValue;

    if (cfg.numElements > kMaxDecoderElements)
        return kPortTooManyElementsRT;

    mFrameLength = frameLength;
    mNumElements = cfg.numElements;
    mNumChannels = 0;
    for (uint32_t i = 0; i < mNumElements; ++i) {
        const uint32_t rc = mElement[i].Configure(cfg.elementTypes[i], mTable, sampleRate);
        if (rc != kOk)
            return rc;
        mNumChannels += mElement[i].NumChannels();
    }
    if (mNumChannels > kMaxDecoderChannels)
        return kPortTooManyChannels;

    // APACChannelRemapper::Initialize, 0x0007bd80: the map is simply copied.
    // The remapper is only built when the config carries a non-empty one.
    mHasRemap = cfg.hasRemap;
    memset(mRemap, 0, sizeof mRemap);
    if (mHasRemap) {
        for (uint32_t i = 0; i < mNumChannels; ++i)
            mRemap[i] = cfg.remap[i];
    }

    mBWE.Initialize();
    mSink = 0;
    mSinkCtx = 0;

    const int32_t rc = mFilterBank.Initialize(mNumChannels, frameLength);
    if (rc != 0)
        return (uint32_t)rc;
    return kOk;
}

void CoreDecoder::Reset()
{
    mFilterBank.Reset();
}

// 0x0007e440  APACCoreLBRDecoder::Deserialize(BitstreamReader&, bool)
//
// Per element: one presence bit; the element itself when it is set, then the
// bandwidth-extension header for its channels; the element's Reset when it is
// clear.  After every element the reader must still have bits left.
//
// The channel counter is maintained exactly where the original maintains it -
// inside the bandwidth-extension branch for a present element, and by a bare
// ++ in the absent branch, which does NOT account for a channel pair.  That
// asymmetry is the original's; it only moves which channel index the BWE header
// is attributed to and it is unreachable on this content, where every presence
// bit is set.
uint32_t CoreDecoder::Deserialize(BitstreamReader& bs, bool align)
{
    uint32_t channel = 0;
    for (uint32_t i = 0; i < mNumElements; ++i) {
        SyntacticElement& e = mElement[i];
        if (bs.GetBit()) {
            const uint32_t rc = e.Deserialize(bs, mLRVQ);
            if (rc != kOk)
                return rc;
            if (e.Type() != kElementNone) {
                const uint32_t brc = mBWE.Deserialize(bs, channel, e);
                if (brc != kOk)
                    return brc;
                // GetActiveFlag is 0 for every channel on this content, so the
                // synthesis pass the original runs here does nothing.  If it
                // were ever set, BWE2Decoder::Deserialize has already refused.
                channel += (e.Type() == kElementCPE) ? 2u : 1u;
            }
        } else {
            ++channel;
            e.Reset();
        }
        if (bs.BitsRemaining() < 0)
            return kBadValue;
    }

    // APACCoreDecoder::DeserializeExtTools, 0x0007d8b0: an empty list on this
    // stream - the cookie's extension-config loop terminates on its first bit -
    // so there is nothing to walk.
    if (align)
        bs.AlignToNextByte();
    return kOk;
}

// 0x0007db30  APACCoreDecoder::DecodeAPACFrame
uint32_t CoreDecoder::DecodeAPACFrame(BitstreamReader& bs, uint32_t frameType,
                                      float** channels)
{
    (void)frameType;    // the original passes it on and never reads it here

    const uint32_t rc = Deserialize(bs, true);
    if (rc != kOk)
        return rc;

    uint32_t n = 0;
    uint32_t ch = 0;
    for (uint32_t i = 0; i < mNumElements; ++i) {
        SyntacticElement& e = mElement[i];
        if (e.Type() == kElementNone)
            continue;
        // The window SHAPE is a hard 0 here; only the window SEQUENCE varies.
        float* first = e.ChannelData(true);
        if (mSink)
            mSink(mSinkCtx, ch, e.GetICSInfo(true).windowSequence, first, mFrameLength);
        mFilterBank.FrequencyToTimeInPlace(ch, first, 0,
                                           e.GetICSInfo(true).windowSequence);
        channels[n++] = first;
        if (e.Type() == kElementCPE) {
            float* second = e.ChannelData(false);
            if (mSink)
                mSink(mSinkCtx, ch + 1, e.GetICSInfo(false).windowSequence, second,
                      mFrameLength);
            mFilterBank.FrequencyToTimeInPlace(ch + 1, second, 0,
                                               e.GetICSInfo(false).windowSequence);
            channels[n++] = second;
            ch += 2;
        } else {
            ch += 1;
        }
    }

    // 0x0007bcd0  APACChannelRemapper::Process - a cycle-following swap, not a
    // gather: entry i is chased through the map until it stops rising, and the
    // two slots are exchanged.
    //
    // COUNTED, where the original's walk is not.  `j = mRemap[j]` terminates
    // only if the map is a permutation: a fixed point above i, or a cycle among
    // entries above i, spins forever, and an entry >= mNumChannels indexes past
    // the port's fixed array.  The original grows its map from the bitstream and
    // never validates it, and Process is handed the map rather than the bits, so
    // the invariant is not enforced where it is relied on - the same shape of
    // risk as APACTNSData::Apply.  The port bounds the walk by the number of
    // channels, which is the length of any real cycle, and refuses an index it
    // cannot hold.  On a well-formed map this is the original, step for step.
    if (mHasRemap) {
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t j = mRemap[i];
            for (uint32_t hops = 0; j > i && hops < n; ++hops) {
                if (j >= kMaxDecoderChannels)
                    return kBadValue;
                j = mRemap[j];
            }
            if (j > i || j >= n)
                return kBadValue;       // not a permutation of 0 .. n-1
            if (j != i) {
                float* t = channels[i];
                channels[i] = channels[j];
                channels[j] = t;
            }
        }
    }
    return kOk;
}

// ---------------------------------------------------------------------------
// APACDecoder
// ---------------------------------------------------------------------------

uint32_t Decoder::Initialize(const GlobalConfig& cfg)
{
    mConfigured = 0;
    mNumChannels = 0;
    mFrameType = 0;
    mFrameLength = cfg.frameLength;

    if (cfg.numCodecConfigs == 0 || cfg.numCodecConfigs > kMaxCodecConfigs)
        return kPortTooManyConfigs;
    mNumCores = cfg.numCodecConfigs;

    for (uint32_t i = 0; i < mNumCores; ++i) {
        const uint32_t rc = mCore[i].Initialize(cfg.codecConfigs[i], cfg.sampleRate,
                                                cfg.frameLength, &mLRVQ);
        if (rc != kOk)
            return rc;
        mNumChannels += mCore[i].GetNumChannels();
    }
    if (mNumChannels > kMaxDecoderChannels)
        return kPortTooManyChannels;

    mTrimming.Initialize(cfg.frameLength);
    mConfigured = 1;
    return kOk;
}

void Decoder::SetSpectrumSink(CoreDecoder::SpectrumSink fn, void* ctx)
{
    for (uint32_t i = 0; i < mNumCores; ++i)
        mCore[i].SetSpectrumSink(fn, ctx);
}

void Decoder::Reset()
{
    for (uint32_t i = 0; i < mNumCores; ++i)
        mCore[i].Reset();
}

uint32_t Decoder::DecodeBody(BitstreamReader& bs, float* out)
{
    float* channels[kMaxDecoderChannels];
    uint32_t n = 0;

    for (uint32_t i = 0; i < mNumCores; ++i) {
        const uint32_t rc = mCore[i].DecodeAPACFrame(bs, mFrameType, channels + n);
        if (rc != kOk)
            return rc;
        n += mCore[i].GetNumChannels();
    }

    // apac::APACAncillaryFrameDecoder::DecodeFrame with an ancillary config
    // that declares neither an audio scene nor loudness/DRC: the trimming
    // record is the whole of it.  Then APACExtFrameDecoder::DecodeFrame over an
    // empty element list, which reads nothing.
    if (mTrimming.Deserialize(bs))
        return kBadValue;

    // ContentOutBuffer::GetAudioData: the per-ASC channel pointers concatenated
    // in order, then InterleaveConvert (0x000b1d90), which multiplies by the
    // scale the decoder object keeps at +2064.
    //
    // THAT SCALE IS 1/32768 AND IT APPLIES TO THE FLOAT PATH TOO.  The decoder
    // is internally S16-scaled: the filter bank's output runs to +-32768, not to
    // +-1.  Measured, not assumed - dumping the buffer the library hands its own
    // filter bank and dividing the library's float32 output by it gives exactly
    // 3.0517578e-05 on every sample of the capture, and the first sample of the
    // first non-silent channel is -0.000161116 inside the decoder against
    // -4.9168745e-09 out of it.  Being a power of two the multiply is exact, so
    // it costs no accuracy.
    const uint32_t frames = mFrameLength;
    for (uint32_t c = 0; c < n; ++c) {
        const float* src = channels[c];
        float* dst = out + c;
        for (uint32_t k = 0; k < frames; ++k) {
            *dst = src[k] * kOutputScale;
            dst += n;
        }
    }
    return kOk;
}

uint32_t Decoder::DecodeFrame(const uint8_t* data, uint32_t bytes,
                              float* out, uint32_t* framesOut)
{
    if (framesOut)
        *framesOut = 0;
    if (!mConfigured)
        return kBadValue;
    if (bytes == 0) {
        // APACDecoder_DecodeFrame's flush path: reset, no frames, no error.
        Reset();
        return kOk;
    }
    if (mNumCores == 0)
        return kBadValue;

    BitstreamReader bs(data, bytes);
    mFrameType = bs.GetBit();
    const uint32_t rc = DecodeBody(bs, out);
    if (rc == kOk && framesOut)
        *framesOut = mFrameLength;
    return rc;
}

// ---------------------------------------------------------------------------
// APACASPDecoder, 0x00072000
// ---------------------------------------------------------------------------

uint32_t ASPDecoder::DecodeFrame(const uint8_t* data, uint32_t bytes,
                                 float* out, uint32_t* framesOut)
{
    if (framesOut)
        *framesOut = 0;
    if (!mConfigured)
        return kBadValue;
    if (bytes == 0) {
        Reset();
        return kOk;
    }
    if (mNumCores == 0)
        return kBadValue;

    BitstreamReader bs(data, bytes);
    mFrameType = bs.GetBits(2);
    if (mFrameType == 2)
        return kBadValue;       // the original's only rejection here
    const uint32_t rc = DecodeBody(bs, out);
    if (rc == kOk && framesOut)
        *framesOut = mFrameLength;
    return rc;
}

}  // namespace apac
