#include "apac/bwe.h"

#include <string.h>

#include "apac/config.h"

namespace apac {

void BWE2Decoder::Initialize()
{
    mActive = 0;
    memset(mEnvelope, 0, sizeof mEnvelope);
    memset(mScale, 0, sizeof mScale);
}

bool BWE2Decoder::GetActiveFlag(uint32_t channel) const
{
    // 0x00079730: (1 << ch) & bits[ch >> 6].  The port caps at 16 channels.
    return channel < kBWEMaxChannels && ((mActive >> channel) & 1u) != 0;
}

// The payload of one active channel: two 9-bit envelope fields and one 6-bit
// scale per window group.  `groups` is always the FIRST channel's group count,
// even for the second half of a channel pair - that is what the original reads.
void BWE2Decoder::ReadChannel(BitstreamReader& bs, uint32_t channel, uint32_t groups)
{
    mEnvelope[channel][0] = bs.GetBits(9);
    mEnvelope[channel][1] = bs.GetBits(9);
    for (uint32_t i = 0; i < groups; ++i) {
        const uint32_t v = bs.GetBits(6);
        if (i < kBWEMaxBands)
            mScale[channel][i] = v;
    }
}

// 0x00078a60  APACBWE2Decoder::Deserialize(BitstreamReader&, unsigned,
//                                          APACSyntacticElement&)
uint32_t BWE2Decoder::Deserialize(BitstreamReader& bs, uint32_t channel,
                                  SyntacticElement& element)
{
    if (channel >= kBWEMaxChannels)
        return kPortTooManyChannels;

    const uint32_t type = element.Type();
    mActive &= ~(1u << channel);

    if (type == kElementSCE) {
        if (bs.GetBit()) {
            mActive |= 1u << channel;
            ReadChannel(bs, channel, element.GetICSInfo(true).numWindowGroups);
            return kPortNeedsBWE;
        }
        return kOk;
    }

    // Every other element type but the channel pair reads nothing.  The LFE
    // (type 3) and the empty element (type 6) fall out here.
    if (type != kElementCPE)
        return kOk;

    const uint32_t second = channel + 1;
    if (second >= kBWEMaxChannels)
        return kPortTooManyChannels;
    mActive &= ~(1u << second);

    const uint32_t flag0 = bs.GetBit();
    const uint32_t flag1 = bs.GetBit();

    const APACICSInfo& ics0 = element.GetICSInfo(true);
    const APACICSInfo& ics1 = element.GetICSInfo(false);

    bool readSecond = (flag1 != 0);

    if (flag0 && ics0.maxSfb) {
        mActive |= 1u << channel;
        ReadChannel(bs, channel, ics0.numWindowGroups);
        if (!flag1) {
            // The second channel copies the first channel's payload instead of
            // carrying its own.
            if (ics1.maxSfb) {
                mActive |= 1u << second;
                mEnvelope[second][0] = mEnvelope[channel][0];
                mEnvelope[second][1] = mEnvelope[channel][1];
                for (uint32_t i = 0; i < ics0.numWindowGroups && i < kBWEMaxBands; ++i)
                    mScale[second][i] = mScale[channel][i];
            }
            readSecond = false;
        }
    }

    if (readSecond && ics1.maxSfb) {
        mActive |= 1u << second;
        ReadChannel(bs, second, ics0.numWindowGroups);
    }

    if (mActive & ((1u << channel) | (1u << second)))
        return kPortNeedsBWE;
    return kOk;
}

}  // namespace apac
