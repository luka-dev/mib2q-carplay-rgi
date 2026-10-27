/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// The APAC decoder's top two layers: the per-ASC core decoder and the frame
// decoder that drives it.
//
// Reference functions in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x0007d1e0  APACCoreDecoder::Initialize(apac::ParamList const&,
//                                           observer_ptr<apac::IASCConfig>)
//   0x0007e7f0  APACCoreLBRDecoder::Initialize(...)
//   0x0007d950  APACCoreDecoder::Deserialize(BitstreamReader&, bool)
//   0x0007e440  APACCoreLBRDecoder::Deserialize(BitstreamReader&, bool)
//   0x0007d8b0  APACCoreDecoder::DeserializeExtTools(BitstreamReader&)
//   0x0007db30  APACCoreDecoder::DecodeAPACFrame(BitstreamReader&,
//                                                apac::FrameType,
//                                                std::vector<float*>&)
//   0x0007cf50  APACCoreDecoder::Reset()
//   0x0007bcd0  APACChannelRemapper::Process(std::vector<float*>&)
//   0x00093e50  APACDecoder::DecodeFrame(unsigned char const*, unsigned,
//                                        apac::FrameOutRecord&)
//   0x00093cc0  APACDecoder::Reset()
//   0x00072000  APACASPDecoder::DecodeFrame(...)
//   0x00072130  APACASPDecoder::DecodeOneFrame(BitstreamReader&)
//   0x00078040  apac::TrimmingData::Deserialize(BitstreamReader&)
//   0x00096070  APACExtFrameDecoder::DecodeFrame(BitstreamReader&)
//   0x00074d10  apac::APACAncillaryFrameDecoder::DecodeFrame(BitstreamReader&,
//                            std::vector<apac::ASCAudioOutData>&, apac::FrameType)
//
// WHICH core decoder.  `apac::ChanDecElement::Initialize` allocates 0x110 bytes,
// plants the APACCoreLBRDecoder vtable and calls its Initialize, so the object
// the C API ends up driving is the LBR subclass and the Deserialize on the
// frame path is APACCoreLBRDecoder's, not APACCoreDecoder's.  The difference is
// the bandwidth-extension header per element.  Established by patching
// APACCoreLBRDecoder::Deserialize's entry to `return 0` in the running library,
// which changes the decoded output.
//
// WHAT THE TAIL OF A FRAME DOES.  After the core decoder byte-aligns, the
// ancillary frame decoder runs and then the extension-frame decoder.  On the
// CarPlay cookie the ancillary config carries neither an audio scene nor a
// loudness/DRC payload, so the only thing that reads bits there is
// apac::TrimmingData::Deserialize - one bit, and two escape-coded 11-bit fields
// if it is set - and the extension-frame decoder has no elements at all.  Both
// were checked by patching their entries in the running library; because
// nothing after them reads bits, neither can change the output on this content.
// Our decoder implements the trimming parser anyway and the extension list as the
// empty list it is.
//
// WHICH TOP-LEVEL DECODER THE CARPLAY STREAM NEEDS: the ASP one.
// `APACDecoder_Create` at 0x00106800 allocates the object, runs
// APACDecoder's constructor, zeroes the three extra words at +464 / +472 /
// +480 and then overwrites the vptr with `APACASPDecoder`'s - so every packet
// the C API decodes goes through APACASPDecoder::DecodeFrame, whose frame-type
// field is TWO bits, not the one bit APACDecoder::DecodeFrame reads.  Reading
// the capture with one bit puts every later field one place early and the very
// first element then looks like an LRVQ frame.
//
// Measured, not assumed: hooking the exported vtables in the running library
// (reference/scratch/trace_shim.c replaces a slot, which works where
// LD_PRELOAD cannot because the library is DT_SYMBOLIC) shows the reader
// arriving at APACCoreLBRDecoder::Deserialize with two bits gone and a cache of
// `word << 2`.  The field is 1 on all 3977 packets of the capture, which is
// what Apple's specification describes: an Audio Synchronization Packet is
// independently decodable and is the only legal entry point into a running
// stream, so a stream that has to survive a flush sends nothing else.
//
// FLOAT vs S16.  This decoder is float32 throughout.  The library's own S16
// path truncates and scales positives by 32767 while Apple's converter rounds
// and scales by 32768, which is why every comparison here is made in float32.

#ifndef APAC_DECODER_H
#define APAC_DECODER_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/bwe.h"
#include "apac/config.h"
#include "apac/element.h"
#include "apac/filterbank.h"
#include "apac/lrvq.h"
#include "apac/sfb_offsets.h"

namespace apac {

// Limits of this decoder.  Eight elements is sixteen channels, twice what the
// CarPlay 5.1.2 profile needs.
const uint32_t kMaxDecoderElements = 8;
const uint32_t kMaxDecoderChannels = 16;

// The decoder object's +2064: the scale InterleaveConvert applies on the way
// out.  The decode path is S16-scaled internally, so this is what turns the
// filter bank's +-32768 into the +-1 the float32 output carries.  Exact.
const float kOutputScale = 1.0f / 32768.0f;

// apac::TrimmingData, 0x00078040.  The encoder-delay / end-padding record that
// closes every frame.  `limit` is the object's own cap on leading + trailing.
struct TrimmingData {
    uint32_t leading;
    uint32_t trailing;
    uint32_t limit;

    void Initialize(uint32_t frameLength);
    bool Deserialize(BitstreamReader& bs);      // TRUE on ERROR
};

// One apac::chan::CoreDecoderConfig's worth of decoding: the element list, the
// filter bank they share, the bandwidth-extension header parser, and the
// channel remapper.
//
// 90 KiB of filter bank and ~10 KiB per element channel live inside this
// object.  It MUST NOT be put on a stack.
class CoreDecoder {
public:
    // `lrvq` may be null, in which case an LRVQ frame is refused with
    // kLimitNeedsLRVQ.  One instance is shared by the whole decoder: every
    // APACLRVQDecoder member is static in the library and its prediction state
    // is a stack local of the wrapper, so nothing survives a call and our decoder
    // resets it on entry to each of the three spectrum entry points.
    // `bwe` may be null too, in which case an ACTIVE bandwidth-extension
    // channel is refused with kLimitNeedsBWE.  Shared for the same reason and
    // on the same evidence as `lrvq`: BWELPC carries nothing across a call.
    uint32_t Initialize(const CoreDecoderConfig& cfg, uint32_t sampleRate,
                        uint32_t frameLength, LRVQDecoder* lrvq, BWELPC* bwe);

    void Reset();                               // filter-bank overlap only
    uint32_t GetNumChannels() const { return mNumChannels; }
    uint32_t GetBlockSize() const { return mFrameLength; }

    // Reads one frame and hands back GetNumChannels() pointers to
    // GetBlockSize() time-domain samples each, in the remapped order.
    uint32_t DecodeAPACFrame(BitstreamReader& bs, uint32_t frameType,
                             float** channels);

    // Differential-test hook, null by default: called with every channel's
    // spectrum just before the filter bank consumes it.  That is the only
    // observable output the frame layer has of its own - everything below it is
    // the transform, which cannot be bit exact - so this is what
    // reference/spectrum_dump_shim.cpp compares against.  One predictable
    // branch per channel per frame; nothing else on the decode path sees it.
    typedef void (*SpectrumSink)(void* ctx, uint32_t channel,
                                 uint32_t windowSequence, const float* spectrum,
                                 uint32_t count);
    void SetSpectrumSink(SpectrumSink fn, void* ctx) { mSink = fn; mSinkCtx = ctx; }

private:
    uint32_t Deserialize(BitstreamReader& bs, bool align);

    SynthesisFilterBank mFilterBank;
    SyntacticElement    mElement[kMaxDecoderElements];
    BWE2Decoder         mBWE;
    const SFBOffsets*   mTable;
    uint32_t            mNumElements;
    uint32_t            mNumChannels;
    uint32_t            mFrameLength;
    uint8_t             mHasRemap;
    uint8_t             mRemap[kMaxDecoderChannels];
    SpectrumSink        mSink;
    void*               mSinkCtx;
    LRVQDecoder*        mLRVQ;
};

// APACDecoder.  One core decoder per codec config; the channels of the configs
// are concatenated in config order, which is what ContentOutBuffer::GetAudioData
// does with the per-ASC records it holds.
//
// Big: it contains the core decoders.  Heap-allocate it.
class Decoder {
public:
    uint32_t Initialize(const GlobalConfig& cfg);

    // APACDecoder::Reset: the filter-bank overlap of every core decoder.  The
    // element state is deliberately NOT cleared - that is what the library
    // does, and a frame decode overwrites all of it anyway.
    void Reset();

    uint32_t NumChannels() const { return mNumChannels; }
    uint32_t FrameLength() const { return mFrameLength; }

    // Forwards to every core decoder; see CoreDecoder::SetSpectrumSink.
    void SetSpectrumSink(CoreDecoder::SpectrumSink fn, void* ctx);

    // The frame-type field off the front of the last frame: one bit here, two
    // in the ASP subclass below.  Nothing downstream of the read acts on it -
    // the core decoder takes it as an argument and ignores it, and so does the
    // ancillary frame decoder.
    uint32_t FrameType() const { return mFrameType; }

    // Decodes one packet into `out`, INTERLEAVED float32, NumChannels() *
    // FrameLength() samples, in the decoder's own channel order.  `bytes == 0`
    // is the flush path, exactly as APACDecoder_DecodeFrame treats it: reset and
    // produce no frames.  Returns 0 on success.
    uint32_t DecodeFrame(const uint8_t* data, uint32_t bytes,
                         float* out, uint32_t* framesOut);

protected:
    // The shared body: everything after the frame-type field has been read.
    uint32_t DecodeBody(BitstreamReader& bs, float* out);

    CoreDecoder  mCore[kMaxCodecConfigs];
    // One for the whole decoder, not one per core: 56 KiB once.  See
    // CoreDecoder::Initialize for why a single instance is faithful.
    LRVQDecoder  mLRVQ;
    // And one BWELPC, for the same reason.  The library has one per CHANNEL;
    // nothing in it survives a call, so this costs ~100 KiB once instead of
    // ~100 KiB per channel.  See include/apac/bwe.h.
    BWELPC       mBWELPC;
    TrimmingData mTrimming;
    uint32_t     mNumCores;
    uint32_t     mNumChannels;
    uint32_t     mFrameLength;
    uint32_t     mFrameType;
    uint8_t      mConfigured;
};

// APACASPDecoder.  THIS is the decoder the CarPlay stream needs, and the one
// APACDecoder_Create hands back: identical to APACDecoder except that the frame
// type is a TWO-bit field and the value 2 is rejected outright.  Apple's
// specification calls a packet in this framing an Audio Synchronization Packet:
// independently decodable, and the only legal entry point into a running
// stream, which is what a flush has to resume on.  Every packet of the capture
// carries frame type 1.
//
// Start, flush and resume, as the library defines them:
//
//   * start   - Initialize, then decode.  There is no priming to drop: the
//               trim-remaining field APACDecoder_Create leaves at zero means
//               every packet yields a full 1024 frames.
//   * flush   - APACDecoder_DecodeFrame with a zero byte count is NOT an error;
//               it resets and returns no frames.  Reset clears the filter bank's
//               overlap-add memory and nothing else, so the first frame after it
//               is windowed against silence rather than against the frame that
//               came before.
//   * resume  - only legal on an ASP.  Every packet here is one, so resuming
//               anywhere is safe on this content; on a stream that mixes frame
//               types it would not be, and our decoder exposes FrameType() rather
//               than deciding for the caller.
//
// Two things this decoder will not guess at, because the binary does not say them:
// which of frame types 0, 1 and 3 means what, and what the 2048-bytes-per-
// channel buffer APACASPDecoder::Initialize sizes at +464 is for.  Neither is
// read anywhere on the decode path.
class ASPDecoder : public Decoder {
public:
    uint32_t DecodeFrame(const uint8_t* data, uint32_t bytes,
                         float* out, uint32_t* framesOut);
};

}  // namespace apac

#endif  // APAC_DECODER_H
