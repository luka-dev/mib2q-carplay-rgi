/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// The APAC syntactic elements: the objects that own one or two channels of
// spectrum and know how to read them out of the bitstream.
//
// Reference functions in libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010):
//
//   0x0009a610  APACIndividualChannelStream::Deserialize(BitstreamReader&,
//                                                        APACICSInfo&, bool)
//   0x000ac3f0  APACSingleChannelElement::Deserialize(BitstreamReader&)
//   0x000ac2a0  APACLFEElement::Deserialize(BitstreamReader&)
//   0x000adea0  APACChannelPairElement::Deserialize(BitstreamReader&)
//   0x000ac8d0  APACChannelPairElement::DeserializeSQFrame(BitstreamReader&)
//   0x000ac600  APACChannelPairElement::DeserializeLrvqFrame(BitstreamReader&)
//   0x000ad520  APACSingleChannelElement::Reset() and the LFE / CPE copies
//   0x000ad3c0  APACSyntacticElement::ChannelData(bool)
//   0x000ad410  APACSyntacticElement::GetICSInfo(bool)
//   0x000acd60  APACSyntacticElement::CreateList(...)
//   0x000accd0  APACSyntacticElement::NumberOfChannels(...)
//
// Decompiled in reference/decomp/APACSyntacticElement.c,
// APACSingleChannelElement.c, APACLFEElement.c, APACChannelPairElement.c and
// APACIndividualChannelStream.c.
//
// The library has a class per element type behind a six-slot vtable.  Our decoder
// keeps one struct with a type tag, because the target is an LD_PRELOADed .so
// on QNX 6.5 with no RTTI and the dispatch is over three cases.  The element
// TYPE codes are the library's, read from apac::chan::CoreDecoderConfig:
// 0 SCE, 1 CPE, 3 LFE, 6 "no channels" - CreateList accepts 0, 1 and 3 and
// rejects everything else, while NumberOfChannels counts 2 for a CPE, 0 for
// type 6 and 1 otherwise.
//
// ChannelData(true) and GetICSInfo(true) name the FIRST channel; the library's
// bool reads that way round, and `false` is only meaningful on a CPE.
//
// LRVQ.  A leading 1 bit in an SCE, an LFE or a CPE selects Apple's LRVQ frame
// mode.  Deserialize takes the decoder for it as an argument rather than owning
// one, for two reasons the stage-h1 work established from the binary: every
// APACLRVQDecoder member is STATIC, so there is no decoder object in the
// library at all, and the prediction state its wrappers use lives on their own
// stack, so nothing survives a call.  One instance shared by the whole decoder
// therefore reproduces the library exactly - our
// SpectrumDeserialize{SCE,CPE,LFE} each call ResetPrediction on entry - and
// costs 56 KiB once instead of 56 KiB per element.
//
// A null pointer means "this build has no LRVQ decoder": the element then
// returns kLimitNeedsLRVQ, which propagates as a hard error exactly as
// APACLRVQDecoder's own non-zero return does.  LRVQ is never entered on the
// CarPlay 5.1 content - proved three times, see STATUS.md - so on that
// content the argument is never dereferenced either way.

#ifndef APAC_ELEMENT_H
#define APAC_ELEMENT_H

#include <stdint.h>

#include "apac/bitstream.h"
#include "apac/cac.h"
#include "apac/ics_info.h"
#include "apac/sfb_offsets.h"
#include "apac/spectrum.h"
#include "apac/tns.h"

namespace apac {

class LRVQDecoder;

// Element type codes, as they appear in apac::chan::CoreDecoderConfig.
enum ElementType {
    kElementSCE = 0,
    kElementCPE = 1,
    kElementLFE = 3,
    kElementNone = 6
};

// The frame length is fixed at 1024 - APACSynthesisFilterBank only works there
// and every SFB table is built for it - so the spectrum is a fixed array rather
// than the library's vector sized from the SFB table's last offset.
const uint32_t kFrameLength = 1024;

// APACIndividualChannelStream, 776 bytes in the library: an
// APACScaleFactorData at +24 and the coefficient vector at +752.
struct IndividualChannelStream {
    ScaleFactorData scf;
    float           spectrum[kFrameLength];

    void Clear();                   // what Reset() does to the coefficients

    // Returns TRUE on ERROR.  `table` is the 40-byte SFB record; the long or
    // short half is chosen by ics.windowSequence, exactly as the library does
    // from the InstanceConfig it holds at +16.
    bool Deserialize(BitstreamReader& bs, const APACICSInfo& ics,
                     const SFBOffsets& table);
};

class SyntacticElement {
public:
    // type is 0, 1 or 3; anything else is rejected, as CreateList does.
    // Returns 0 on success.
    uint32_t Configure(uint32_t type, const SFBOffsets* table, uint32_t sampleRate);

    uint32_t Type() const { return mType; }
    uint32_t NumChannels() const;   // 2 for a CPE, 0 for type 6, else 1

    // Returns 0 on success.  Non-zero propagates out of the core decoder as a
    // frame error, which is what every original return path does.  `lrvq` may
    // be null; see the note on the LRVQ frame mode above.
    uint32_t Deserialize(BitstreamReader& bs, LRVQDecoder* lrvq);

    void Reset();

    float*             ChannelData(bool first);
    const APACICSInfo& GetICSInfo(bool first) const;
    APACICSInfo&       GetICSInfo(bool first);

    // The 190-byte APACTNSData record, for differential tests: TNS is the last
    // thing an element does to its spectrum and the record is the only part of
    // that the spectrum itself does not show.  The library has no such
    // accessor - it reaches the record at a fixed offset off `this`.
    const TNSData&     TNSRecord(bool first) const { return mTNS[first ? 0 : 1]; }

private:
    uint32_t DeserializeSQFrame(BitstreamReader& bs);
    uint32_t ApplyTNS(uint32_t channel);
    SpectralData View(uint32_t channel);

    uint32_t                mType;
    const SFBOffsets*       mTable;
    APACICSInfo             mICS[2];
    TNSData                 mTNS[2];
    CACDecoder              mCAC;
    IndividualChannelStream mStream[2];
};

}  // namespace apac

#endif  // APAC_ELEMENT_H
