#include "apac/element.h"

#include <string.h>

#include "apac/config.h"
#include "apac/lrvq.h"

namespace apac {

namespace {

// The half of the 40-byte SFB record the spectrum decoder and TNS are handed.
// APACIndividualChannelStream::Deserialize picks `record + 24` for an
// eight-short frame and `record + 8` otherwise; those are the short and long
// SFBOffsets sub-objects.
SFBBandOffsets BandsFor(const SFBOffsets& table, const APACICSInfo& ics)
{
    SFBBandOffsets b;
    if (ics.windowSequence == 2) {
        b.numBands = table.numSfbShort;
        b.offset   = table.sfbOffsetShort;
    } else {
        b.numBands = table.numSfbLong;
        b.offset   = table.sfbOffsetLong;
    }
    return b;
}

}  // namespace

void IndividualChannelStream::Clear()
{
    memset(spectrum, 0, sizeof spectrum);
}

// 0x0009a610  APACIndividualChannelStream::Deserialize(BitstreamReader&,
//                                                      APACICSInfo&, bool)
// The third argument is unused in the original; every caller passes it and it
// is never read, so the port does not carry it.
bool IndividualChannelStream::Deserialize(BitstreamReader& bs,
                                          const APACICSInfo& ics,
                                          const SFBOffsets& table)
{
    if (scf.Deserialize(bs, ics))
        return true;
    const SFBBandOffsets bands = BandsFor(table, ics);
    SpectralData view;
    view.begin = spectrum;
    view.end   = spectrum + kFrameLength;
    return view.Deserialize(bs, ics, scf, bands);
}

// 0x000acd60  APACSyntacticElement::CreateList, for one element.  The original
// builds the ICSInfo's two band limits out of the SFB table record it is handed
// - the u16 at +8 for a long block and the u16 at +24 for a short one - and
// hands every APACTNSData the same instance config.
uint32_t SyntacticElement::Configure(uint32_t type, const SFBOffsets* table,
                                     uint32_t sampleRate)
{
    if (type != kElementSCE && type != kElementCPE && type != kElementLFE)
        return kPortBadElementType;

    mType  = type;
    mTable = table;

    for (uint32_t i = 0; i < 2; ++i) {
        mICS[i].Clear();
        mICS[i].maxSfbLong  = (uint8_t)table->numSfbLong;
        mICS[i].maxSfbShort = (uint8_t)table->numSfbShort;
        // APACICSInfo::Reset leaves the grouping word at 1, not 0.
        mICS[i].windowGroupWord = 1;
        memset(&mTNS[i], 0, sizeof mTNS[i]);
        mTNS[i].Configure(sampleRate);
        mStream[i].scf.Clear();
        mStream[i].Clear();
    }
    mCAC.Initialize();
    return kOk;
}

// 0x000accd0  APACSyntacticElement::NumberOfChannels
uint32_t SyntacticElement::NumChannels() const
{
    if (mType == kElementNone)
        return 0;
    return (mType == kElementCPE) ? 2u : 1u;
}

float* SyntacticElement::ChannelData(bool first)
{
    return mStream[first ? 0 : 1].spectrum;
}

const APACICSInfo& SyntacticElement::GetICSInfo(bool first) const
{
    return mICS[first ? 0 : 1];
}

APACICSInfo& SyntacticElement::GetICSInfo(bool first)
{
    return mICS[first ? 0 : 1];
}

// 0x000ad520 / 0x000ad5d0 / 0x000ad680 - the three Reset bodies are the same
// work per channel: APACICSInfo::Reset, then zero the coefficients.
void SyntacticElement::Reset()
{
    const uint32_t n = (mType == kElementCPE) ? 2u : 1u;
    for (uint32_t i = 0; i < n; ++i) {
        // APACICSInfo::Reset: windowSequence = maxSfb = numWindowGroups = 0,
        // grouping word = 1.  It does NOT touch the two band limits.
        mICS[i].windowSequence  = 0;
        mICS[i].maxSfb          = 0;
        mICS[i].numWindowGroups = 0;
        mICS[i].windowGroupWord = 1;
        mStream[i].Clear();
    }
}

SpectralData SyntacticElement::View(uint32_t channel)
{
    SpectralData view;
    view.begin = mStream[channel].spectrum;
    view.end   = mStream[channel].spectrum + kFrameLength;
    return view;
}

uint32_t SyntacticElement::ApplyTNS(uint32_t channel)
{
    const APACICSInfo& ics = mICS[channel];
    const SFBBandOffsets bands = BandsFor(*mTable, ics);
    SpectralData view = View(channel);
    mTNS[channel].Apply(ics, bands, view);
    return kOk;
}

// LRVQ, wired.  The stage-h1 port supplies the frame body
// (apac::DeserializeLrvqFrame) and the three spectrum entry points;
// LrvqFrameContext just names the seven things off `this` the original touches,
// and the offsets in its declaration are the ones this class reproduces.
//
// None of it is reachable on the CarPlay 5.1 content: patching each of
// APACLRVQDecoder::SpectrumDeserialize{SCE,CPE,LFE} in the running library to
// return 0 - to consume no bits - leaves the decoded float32 output of the whole
// capture bit identical, which none of them could do if it read a single bit.
// So this path is checked against the oracle only indirectly, by
// tests/test_frames.cpp driving the stage-h1 test's own library-checked frame
// cases through this element and requiring the same spectra, the same TNS
// records and one extra bit - the frame-mode bit this layer adds.
//
// 0x000ac3f0  APACSingleChannelElement::Deserialize
// 0x000ac2a0  APACLFEElement::Deserialize
// 0x000adea0  APACChannelPairElement::Deserialize
uint32_t SyntacticElement::Deserialize(BitstreamReader& bs, LRVQDecoder* lrvq)
{
    if (mType == kElementCPE) {
        // The CPE reads its frame-mode bit first and dispatches.  The LRVQ body
        // reads BOTH ICS headers itself, at coding type 1.
        if (bs.GetBit()) {
            if (lrvq == 0)
                return kPortNeedsLRVQ;
            LrvqFrameContext ctx;
            SpectralData sp0 = View(0), sp1 = View(1);
            ctx.ics[0]  = &mICS[0];   ctx.ics[1]  = &mICS[1];
            ctx.spec[0] = &sp0;       ctx.spec[1] = &sp1;
            ctx.tns[0]  = &mTNS[0];   ctx.tns[1]  = &mTNS[1];
            ctx.sfbLong.numBands  = mTable->numSfbLong;
            ctx.sfbLong.offset    = mTable->sfbOffsetLong;
            ctx.sfbShort.numBands = mTable->numSfbShort;
            ctx.sfbShort.offset   = mTable->sfbOffsetShort;
            ctx.lrvq = lrvq;
            return DeserializeLrvqFrame(bs, ctx) ? kBadValue : kOk;
        }
        return DeserializeSQFrame(bs);
    }

    // SCE and LFE: one bit, which is BOTH the LRVQ selector and the coding type
    // handed to APACICSInfo::Deserialize.
    const uint32_t codingType = bs.GetBit();
    if (mICS[0].Deserialize(bs, (int32_t)codingType))
        return kBadValue;
    if (codingType == 1) {
        if (lrvq == 0)
            return kPortNeedsLRVQ;
        SpectralData sp = View(0);
        const int32_t err = (mType == kElementLFE)
            ? lrvq->SpectrumDeserializeLFE(bs, mICS[0], sp)
            : lrvq->SpectrumDeserializeSCE(bs, mICS[0], sp);
        if (err)
            return kBadValue;
        // The LFE stops here; the SCE falls through to the shared TNS tail,
        // which is what APACSingleChannelElement::Deserialize does after either
        // spectrum path.
        if (mType == kElementLFE)
            return kOk;
    } else {
        if (mStream[0].Deserialize(bs, mICS[0], *mTable))
            return kBadValue;
        if (mType == kElementLFE)
            return kOk;                 // the LFE carries no TNS
    }

    mTNS[0].present = (uint8_t)(bs.GetBit() != 0);
    if (mTNS[0].present) {
        if (mTNS[0].ParseTNSData(bs, mICS[0]))
            return kBadValue;
    } else {
        mTNS[0].nFilt = 0;
    }
    return ApplyTNS(0);
}

// 0x000ac8d0  APACChannelPairElement::DeserializeSQFrame
uint32_t SyntacticElement::DeserializeSQFrame(BitstreamReader& bs)
{
    if (mICS[0].Deserialize(bs, 0))
        return kBadValue;
    if (mStream[0].Deserialize(bs, mICS[0], *mTable))
        return kBadValue;

    // One bit selects the joint-coded pair.  When it is set the second channel
    // inherits the first channel's window layout verbatim - the original copies
    // +10, +11, +12 and +16 across - is read against the FIRST channel's
    // ICSInfo, and the pair is then rotated by the CAC tool.
    const uint32_t joint = bs.GetBit();
    if (joint) {
        mICS[1].windowSequence  = mICS[0].windowSequence;
        mICS[1].maxSfb          = mICS[0].maxSfb;
        mICS[1].numWindowGroups = mICS[0].numWindowGroups;
        mICS[1].windowGroupWord = mICS[0].windowGroupWord;

        if (mStream[1].Deserialize(bs, mICS[0], *mTable))
            return kBadValue;
        if (mCAC.Deserialize(bs, mICS[0]))
            return kBadValue;
        if (mCAC.ProcessCac(mICS[0], *mTable,
                            mStream[0].spectrum, mStream[1].spectrum))
            return kBadValue;
    } else {
        if (mICS[1].Deserialize(bs, 0))
            return kBadValue;
        if (mStream[1].Deserialize(bs, mICS[1], *mTable))
            return kBadValue;
    }

    for (uint32_t c = 0; c < 2; ++c) {
        mTNS[c].present = (uint8_t)(bs.GetBit() != 0);
        if (mTNS[c].present) {
            if (mTNS[c].ParseTNSData(bs, mICS[c]))
                return kBadValue;
        } else {
            mTNS[c].nFilt = 0;
        }
    }
    ApplyTNS(0);
    ApplyTNS(1);
    return kOk;
}

}  // namespace apac
