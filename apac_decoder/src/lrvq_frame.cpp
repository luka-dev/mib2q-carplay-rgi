// APACChannelPairElement::DeserializeLrvqFrame, 0xac600 - 712 bytes,
// 178 instructions.
//
// The element class itself belongs to stage f; this translation unit holds only
// the LRVQ frame body, so that the two can land independently.  When
// APACChannelPairElement exists its method is
//
//     int32_t APACChannelPairElement::DeserializeLrvqFrame(BitstreamReader& bs)
//     { LrvqFrameContext ctx = { ... }; return apac::DeserializeLrvqFrame(bs, ctx); }
//
// with the members named in the comments on LrvqFrameContext.
//
// Reconstructed from the disassembly (this function has no `#if 0` block in
// reference/decomp/APACChannelPairElement.c; it was read out of the ELF).

#include "apac/lrvq.h"

namespace apac {

int32_t DeserializeLrvqFrame(BitstreamReader& bs, LrvqFrameContext& ctx)
{
    // Both ICS headers first, and both with coding type 1 - which is what makes
    // an LRVQ frame's band layout fixed (maxSfb 27 long, 7 short) and reads no
    // maxSfb or grouping field.  The SQ frame passes 0 for the first channel.
    if (ctx.ics[0]->Deserialize(bs, kCodingTypeSpecial))
        return 1;
    if (ctx.ics[1]->Deserialize(bs, kCodingTypeSpecial))
        return 1;

    int32_t err = ctx.lrvq->SpectrumDeserializeCPE(bs, *ctx.ics[0], *ctx.ics[1],
                                                   *ctx.spec[0], *ctx.spec[1]);
    if (err)
        return err;

    const bool tns0 = bs.GetBits(1) != 0;
    ctx.tns[0]->present = tns0 ? 1 : 0;
    if (tns0) {
        if (ctx.tns[0]->ParseTNSData(bs, *ctx.ics[0]))
            return 1;
    } else {
        ctx.tns[0]->nFilt = 0;
    }

    const bool tns1 = bs.GetBits(1) != 0;
    ctx.tns[1]->present = tns1 ? 1 : 0;
    if (tns1) {
        if (ctx.tns[1]->ParseTNSData(bs, *ctx.ics[1]))
            return 1;
    } else {
        ctx.tns[1]->nFilt = 0;
        // An asymmetry, not a transcription slip: when the second channel has
        // no TNS block AND the first one did, one more bit is read and thrown
        // away (0xac7d8..0xac7f4).  Absent from DeserializeSQFrame.  Dropping it
        // desynchronises everything after this point in the frame.
        if (tns0)
            bs.GetBits(1);
    }

    // Each channel picks its own band table from its own window sequence.
    ctx.tns[0]->Apply(*ctx.ics[0],
                      ctx.ics[0]->windowSequence == 2 ? ctx.sfbShort : ctx.sfbLong,
                      *ctx.spec[0]);
    ctx.tns[1]->Apply(*ctx.ics[1],
                      ctx.ics[1]->windowSequence == 2 ? ctx.sfbShort : ctx.sfbLong,
                      *ctx.spec[1]);
    return 0;
}

}  // namespace apac
