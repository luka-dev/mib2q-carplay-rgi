/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// APACBWE2Decoder - Apple's bandwidth extension: the header parser and the
// synthesis.
//
// Original: libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010)
//
//   0x00078a60  APACBWE2Decoder::Deserialize(BitstreamReader&, unsigned,
//                                            APACSyntacticElement&)
//   0x00079730  APACBWE2Decoder::GetActiveFlag(unsigned) const
//   0x00078490  APACBWE2Decoder::DecodeFrame(APACICSInfo const&,
//                                            APACSpectralData&, unsigned)
//   0x000791d0  APACBWE2Decoder::InitInterFrameMemory(unsigned)
//   0x00079670  APACBWE2Decoder::Initialize(unsigned)
//
// Decompiled in reference/decomp/APACBWE2Decoder.c; the LPC half is BWELPC,
// see include/apac/bwe_lpc.h.
//
// NEVER ACTIVE ON THIS CONTENT, and that is measured rather than assumed.
// `APACCoreLBRDecoder::Deserialize` calls BWE2's Deserialize for every element
// of every frame, so the header is ON the bit path: patching its entry to
// `return 0` in the running library - letting it consume no bits - truncates
// the decode.  But patching `GetActiveFlag` to return 0, and patching
// `DecodeFrame` to return 0, each leave the decoded float32 output of the whole
// capture BIT IDENTICAL.  So on the CarPlay 5.1 stream BWE is signalled and
// always off.  The synthesis below is therefore driven directly against the
// library by tests/test_bwe.cpp, the way stage h1 drove LRVQ, and not from the
// capture, which cannot reach it.
//
// WHAT THE SYNTHESIS DOES.  The core coder stops at the scalefactor band
// `maxSfb` names.  Let `coreEnd` be that band's first coefficient - the long
// table's entry, or eight times the short table's.  Everything below is then
// keyed off one comparison, `coreEnd >= 384`:
//
//                       coreEnd >= 384      coreEnd < 384
//     bwStart               384                 256      first extended bin
//     patches                 2                   4      copies made
//     patch width (long)    256                 128
//     patch width (short)    32                  16      per 128-bin window
//
// and the extension always covers exactly 512 long bins (64 short ones per
// window).  The top `patch width` bins of the core are copied up once per
// patch, flattened by the core's own LP envelope and re-shaped by an envelope
// decoded from two 9-bit LSF codebook indices, then scaled by a 6-bit gain -
// one gain for a long block, one per window GROUP for a short one.
//
// The reason the two envelopes are divided rather than the signal whitened is
// that both come out of apac::LPMDCTFilter as magnitude spectra: `spec[bwStart
// + j] = spec[bwStart - w + j] * coreEnv[bwStart - w + j] / bweEnv[bwStart +
// j]`, with the division done in place over the BWE envelope buffer.

#ifndef APAC_BWE_H
#define APAC_BWE_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/bwe_lpc.h"
#include "apac/element.h"
#include "apac/ics_info.h"
#include "apac/sfb_offsets.h"

namespace apac {

const uint32_t kBWEMaxChannels = 16;
const uint32_t kBWEMaxBands    = 8;

class BWE2Decoder {
public:
    // `table` is the stream's scalefactor-band record, which the original
    // reaches through the APACInstanceConfig it holds at +80; `lpc` is the
    // BWELPC the synthesis runs on.  The original builds ONE BWELPC PER
    // CHANNEL in InitInterFrameMemory - a vector at +88, indexed by channel -
    // but nothing in it survives a call: MDCTCoefficientsToLPC and
    // DecodeLSFsToLPCs both overwrite every buffer they read, and its filters
    // are pure scratch.  The per-channel state that DOES persist is the
    // envelope and scale arrays below, which are this object's.  So one shared
    // instance is faithful, and it is what apac::Decoder hands down - the same
    // argument, and the same ownership, as LRVQDecoder.
    //
    // A null `lpc` means "this build has no BWE synthesis": DecodeFrame then
    // returns kPortNeedsBWE rather than silently leaving the band empty.
    void Initialize(const SFBOffsets* table, BWELPC* lpc);

    // `channel` is the index of the element's FIRST channel.  Returns 0 on
    // success.  Reads nothing at all for an LFE.
    uint32_t Deserialize(BitstreamReader& bs, uint32_t channel,
                         SyntacticElement& element);

    bool GetActiveFlag(uint32_t channel) const;

    // 0x00078490.  Runs the synthesis for one channel over its 1024
    // coefficients, in place.  `channel` is that channel's own index, not the
    // element's first - APACCoreLBRDecoder::Deserialize calls this once per
    // channel of a present element, after the element's own Deserialize (so
    // after TNS) and after this object's Deserialize.  Returns 0 when there is
    // nothing to do, which includes an inactive channel and maxSfb == 0.
    uint32_t DecodeFrame(const APACICSInfo& ics, float* spectrum,
                         uint32_t channel);

    // The envelope scratch as DecodeFrame left it: the core's LP magnitude
    // spectrum below bwStart and, above it, the extension envelope AFTER the
    // in-place division - i.e. the exact factor each replicated coefficient was
    // multiplied by.  Exposed only so tests/test_bwe.cpp can attribute a
    // difference in the output band to a null of |A(w)|, which is where this
    // reconstruction is ill-conditioned by construction.  Nothing on the decode
    // path reads it.
    const float* Envelope() const { return mEnv; }

private:
    void ReadChannel(BitstreamReader& bs, uint32_t channel, uint32_t groups);

    uint32_t DecodeLong(const APACICSInfo& ics, float* spec, uint32_t channel,
                        uint32_t bwStart, uint32_t patches, uint32_t width);
    uint32_t DecodeShort(const APACICSInfo& ics, float* spec, uint32_t channel,
                         uint32_t bwStart, uint32_t patches, uint32_t width);

    const SFBOffsets* mTable;
    BWELPC*           mLPC;
    uint32_t mActive;                                   // one bit per channel
    uint32_t mEnvelope[kBWEMaxChannels][2];             // the two 9-bit fields
    uint32_t mScale[kBWEMaxChannels][kBWEMaxBands];     // the 6-bit fields
    // The original's `float v87[1026]` stack buffer, filled with 1.0 and then
    // written with the core envelope below bwStart and the extension envelope
    // above it.  The port cannot put a kilobyte of floats on the decode path's
    // stack, so it is a member; nothing carries across calls, the fill is the
    // first thing DecodeFrame does.
    float    mEnv[kFrameLength];
};

}  // namespace apac

#endif  // APAC_BWE_H
