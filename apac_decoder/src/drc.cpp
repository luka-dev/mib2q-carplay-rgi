/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac::LoudnessDRCConfig / apac::drc::DynRangeCompressor / apac::APACDRCDecoder.
//
// Transcribed from reference/decomp/apac_LoudnessDRCConfig.c, apac_drc.c and
// apac_APACDRCDecoder.c.  See include/apac/drc.h for what these four functions
// are, what they do not do, and the measurement that says refusing at the
// mpddrc boundary is safe on the CarPlay 5.1 stream.

#include "apac/drc.h"

namespace apac {
namespace drc {

// 0x0007ea20  APACDRCExtensionPayload::Deserialize
//
//   if (mpddrc::UniDrc::Deserialize(this + 8, bs)) {
//       *(this->owner + 8) = 0;            // clear the compressor's enable
//       mpddrc::UniDrc::Reset(this + 8);
//   } else if (!*(this->owner + 8) && *((char*)this + 744)) {
//       *(this->owner + 8) = 1;
//   }
//   return 0;
//
// Both arms turn on what mpddrc::UniDrc::Deserialize did with the bitstream,
// and that is the function this decoder does not have.  It is the ONLY bit
// consumer anywhere in the DRC layer, so there is nothing to transcribe around
// it: either the parser is here or the position after the payload is unknown.
uint32_t DRCExtensionPayload::Deserialize(BitstreamReader& bs,
                                          HeaderPayloadType type) const
{
    (void)bs;
    (void)type;
    return kLimitNeedsLoudnessDRC;
}

// 0x0007ecc0  DynRangeCompressor::Deserialize
//
//   v3 = *((APACDRCExtensionPayload **)this + 5);
//   return v3 ? APACDRCExtensionPayload::Deserialize(v3, bs, n) : 1;
//
// The null-payload arm is the one that runs on content without DRC, and it
// consumes nothing.  The literal 1 is transcribed rather than translated: at
// its two call sites a 1 means "stop here", and the capture shows nothing
// downstream reacts to the difference between 0 and 1 - both were forced in
// the running library over all 3977 packets with byte-identical output.
uint32_t DynRangeCompressor::Deserialize(BitstreamReader& bs,
                                         HeaderPayloadType type) const
{
    if (hasPayload)
        return payload.Deserialize(bs, type);
    return kBadValue;   // the library's `return 1`
}

} // namespace drc

// 0x00072bb0  LoudnessDRCConfig::Deserialize
//
// The library: if no DynRangeCompressor has been built yet, `operator new`
// one, zero it, seed +56 = 1 and the pair at +60 with 0x3F800000C1F80000
// (float -31.0 then 1.0 - the default target loudness and boost), destroy any
// previous one, then DynRangeCompressor::Initialize(ParamList).  A non-zero
// Initialize makes the whole call return 1 WITHOUT reading a bit; otherwise it
// delegates to DynRangeCompressor::Deserialize.
//
// Initialize (0x0007ffb0) is what builds the APACDRCExtensionPayload out of
// the DRCConfiguration in the ParamList, and its 3796-byte constructor is what
// creates the mpddrc::UniDrc the payload then parses into.  Our decoder has
// neither, so a config that reaches this function is a config our decoder cannot
// follow - which is exactly what it reports.  It reads no bits on the way out,
// so the caller's bit position is still exact and the refusal is honest about
// where it stopped.
uint32_t LoudnessDRCConfig::Deserialize(BitstreamReader& bs,
                                        HeaderPayloadType type)
{
    if (!initialized) {
        compressor.Clear();
        initialized = 1;
        // DynRangeCompressor::Initialize(ParamList const&) would set
        // compressor.hasPayload here.  Not implemented; see above.
    }
    return compressor.Deserialize(bs, type);
}

// 0x00074980  APACDRCDecoder::Deserialize
//
//   return apac::drc::DynRangeCompressor::Deserialize(this->compressor, bs, 0);
//
// Only entered when AncillaryConfig set its flag bit 1, which only happens
// after LoudnessDRCConfig::Deserialize succeeded.  With no DRC in the config
// there is no compressor payload and this reads nothing, per frame, forever.
uint32_t APACDRCDecoder::Deserialize(BitstreamReader& bs) const
{
    return compressor.Deserialize(bs, kHeaderPayloadFrame);
}

// 0x000749d0  APACDRCDecoder::DecodeFrame
//
//   unsigned v3 = 0;
//   return apac::drc::DynRangeCompressor::ProcessUniDrc(this->compressor,
//                                                       buf, buf, &v3);
//
// ProcessUniDrc applies the gains in place.  It touches no bitstream, and the
// head unit does not want loudness normalisation applied twice, so our decoder
// stops here deliberately: the samples are left exactly as the core decoder
// produced them.  The library returns false on success.
bool APACDRCDecoder::DecodeFrame(float** buf) const
{
    (void)buf;
    return false;
}

} // namespace apac
