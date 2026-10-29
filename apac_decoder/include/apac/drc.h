/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// apac::LoudnessDRCConfig, apac::drc::DynRangeCompressor and
// apac::APACDRCDecoder - the loudness / dynamic-range-control layer.
//
// Stage h.  Parsing only: the gains are never applied on this head unit, but
// the bit reader has to end up in the right place, so what the layer consumes
// matters and what it computes does not.
//
// WHAT IS ACTUALLY HERE
//
// All four of Apple's DRC entry points are dispatchers.  Between them they
// read no bits at all:
//
//   apac::LoudnessDRCConfig::Deserialize      0x00072bb0  272 bytes
//       lazily builds a DynRangeCompressor, Initialize()s it from the
//       ParamList, then delegates.
//   apac::drc::DynRangeCompressor::Deserialize            0x0007ecc0  100 bytes
//       delegates to its APACDRCExtensionPayload, or returns 1 immediately
//       when there is none - reading nothing.
//   APACDRCExtensionPayload::Deserialize                  0x0007ea20  160 bytes
//       calls mpddrc::UniDrc::Deserialize and, on failure, clears the owner's
//       enable byte and Reset()s the UniDrc.  Always returns 0.
//   apac::APACDRCDecoder::Deserialize         0x00074980   72 bytes
//       DynRangeCompressor::Deserialize(bs, HeaderPayloadType 0).
//
// Every bit of the payload is read by mpddrc::UniDrc::Deserialize (0x000b9b30)
// - the MPEG-D DRC (ISO/IEC 23003-4) parser Apple links in whole.  That is 62
// classes and 51 Deserialize members in this binary, an entire stage of its
// own, and it is NOT ported.  So this layer refuses with
// kPortNeedsLoudnessDRC the moment a payload exists, rather than guessing at a
// bit count and silently desynchronising everything after it.
//
// WHY REFUSING IS ENOUGH FOR THE CARPLAY TARGET
//
// Measured, not assumed.  All five DRC entry points - the two above plus
// APACDRCExtensionPayload::Deserialize, mpddrc::UniDrc::Deserialize and
// LoudnessDRCConfig::Deserialize - were patched in the running library to
// `MOV W0,#imm ; RET` and the whole capture decoded again
// (reference/scratch/reach_shim.c, the technique that settled LRVQ).  Over all
// 3977 packets, 97 738 752 bytes of float32 output, the result is BYTE
// IDENTICAL to the unpatched decode, and identical for both return values 0
// and 1.  The DRC path reads zero bits on this content and its return value is
// not even looked at.
//
// That follows from the structure: apac::AncillaryConfig::Deserialize only
// sets its flag bit 1 (`*(this+2) |= 2`) after LoudnessDRCConfig::Deserialize
// succeeds, and APACAncillaryFrameDecoder::Deserialize only calls the frame
// side when that same bit is set.  No DRC in the magic cookie means no DRC in
// any frame.  The live CarPlay cookie does not set the config's DRC flag - the
// port parses it end to end, 150 of its 152 bits, without reaching this file.
#ifndef APAC_DRC_H
#define APAC_DRC_H

#include "apac/bitstream.h"
#include "apac/config.h"

#include <stdint.h>

namespace apac {

// mpddrc::HeaderPayloadType, as the two call sites pass it.  AncillaryConfig
// hands LoudnessDRCConfig a 4; APACDRCDecoder hands the frame path a 0.
enum HeaderPayloadType {
    kHeaderPayloadFrame  = 0,
    kHeaderPayloadConfig = 4
};

namespace drc {

// apac::drc::DynRangeCompressor::APACDRCExtensionPayload.
//
// In the original this object embeds an mpddrc::UniDrc at +8 and a
// DRCConfiguration-derived state block; its 3796-byte constructor
// (0x0007ee70) builds all of it.  None of that is ported, because none of it
// is reachable here - see the file comment.  What the port keeps is the one
// property that governs bit accounting: whether a payload exists at all.
struct DRCExtensionPayload {
    // Set by the original's Initialize path when a DRCConfiguration produced a
    // payload.  The port never sets it, and says so rather than pretending.
    uint8_t present;

    void Clear() { present = 0; }

    // 0x0007ea20.  The original calls mpddrc::UniDrc::Deserialize and always
    // returns 0, folding a parse failure into "disable, reset, carry on".  The
    // port cannot reach that verdict without the parser, so it refuses.
    uint32_t Deserialize(BitstreamReader& bs, HeaderPayloadType type) const;
};

// apac::drc::DynRangeCompressor.  0x48 = 72 bytes in the original; the payload
// pointer lives at +40 (`*((_QWORD *)this + 5)`).
struct DynRangeCompressor {
    DRCExtensionPayload payload;
    uint8_t             hasPayload;   // the original's non-null pointer at +40

    void Clear()
    {
        payload.Clear();
        hasPayload = 0;
    }

    // 0x0007ecc0.  Reads nothing when there is no payload, exactly as the
    // original's `return 1` branch does.
    uint32_t Deserialize(BitstreamReader& bs, HeaderPayloadType type) const;
};

} // namespace drc

// apac::LoudnessDRCConfig - GlobalConfig's DRC branch, reached from
// apac::AncillaryConfig::Deserialize with HeaderPayloadType 4.
struct LoudnessDRCConfig {
    drc::DynRangeCompressor compressor;
    uint8_t                 initialized;   // the original's lazily-new'd pointer

    void Clear()
    {
        compressor.Clear();
        initialized = 0;
    }

    // 0x00072bb0.
    uint32_t Deserialize(BitstreamReader& bs, HeaderPayloadType type);
};

// apac::APACDRCDecoder - the frame side, owned by APACAncillaryFrameDecoder
// and only entered when AncillaryConfig set its flag bit 1.
struct APACDRCDecoder {
    drc::DynRangeCompressor compressor;

    void Clear() { compressor.Clear(); }

    // 0x00074980.
    uint32_t Deserialize(BitstreamReader& bs) const;

    // 0x000749d0.  Applying the gains is out of scope - it reads no bits, and
    // the head unit does not want them - so this is the original's call with
    // the processing left out.  It is a no-op, and says so.
    bool DecodeFrame(float** buf) const;
};

} // namespace apac

#endif
