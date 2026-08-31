// APACBWE2Decoder - Apple's bandwidth extension, PARSER ONLY.
//
// Original: libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010)
//
//   0x00078a60  APACBWE2Decoder::Deserialize(BitstreamReader&, unsigned,
//                                            APACSyntacticElement&)
//   0x00079730  APACBWE2Decoder::GetActiveFlag(unsigned) const
//   0x00078490  APACBWE2Decoder::DecodeFrame(APACICSInfo const&,
//                                            APACSpectralData&, unsigned)
//
// Decompiled in reference/decomp/APACBWE2Decoder.c.
//
// Why the parser is here and the synthesis is not.  `APACCoreLBRDecoder::
// Deserialize` calls BWE2's Deserialize for every element of every frame, so it
// is ON the bit path: patching its entry to `return 0` in the running library -
// letting it consume no bits - truncates the decode.  But patching
// `GetActiveFlag` to return 0, and patching `DecodeFrame` to return 0, each
// leave the decoded float32 output of the whole capture BIT IDENTICAL.  So on
// this content BWE is signalled, always off, and never synthesises anything.
//
// The port therefore reads the header exactly - which is all the bit path needs
// - and refuses with kPortNeedsBWE if a channel ever comes up active, which is
// stage e's job.  That refusal propagates as a frame error, the same way the
// original's own non-zero returns do.

#ifndef APAC_BWE_H
#define APAC_BWE_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/element.h"

namespace apac {

const uint32_t kBWEMaxChannels = 16;
const uint32_t kBWEMaxBands    = 8;

class BWE2Decoder {
public:
    void Initialize();

    // `channel` is the index of the element's FIRST channel.  Returns 0 on
    // success.  Reads nothing at all for an LFE.
    uint32_t Deserialize(BitstreamReader& bs, uint32_t channel,
                         SyntacticElement& element);

    bool GetActiveFlag(uint32_t channel) const;

private:
    void ReadChannel(BitstreamReader& bs, uint32_t channel, uint32_t groups);

    uint32_t mActive;                                   // one bit per channel
    uint32_t mEnvelope[kBWEMaxChannels][2];             // the two 9-bit fields
    uint32_t mScale[kBWEMaxChannels][kBWEMaxBands];     // the 6-bit fields
};

}  // namespace apac

#endif  // APAC_BWE_H
