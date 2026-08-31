// Whole-frame differential test for stages f and g: six real packets out of the
// live CarPlay 5.1 capture, decoded by the port and compared against the float32
// PCM the ORIGINAL library produced for exactly the same six.
//
// FIXTURE: tests/fixtures/frames.bin, produced by
//
//   apac_decoder/tools/gen_frame_fixture.sh      (needs Docker)
//
// which runs Pioneer's copy of Apple's decoder natively in a linux/arm64
// container.  Every expectation in it is a value the library actually produced.
//
// WHICH SIX PACKETS, AND WHY.  429..434, contiguous, decoded from a fresh
// decoder.  Instrumenting the port and replaying all 3977 packets of the
// capture shows what each one exercises; that window is the shortest run that
// reaches every feature this content ever uses but one:
//
//   long blocks, LONG_START, EIGHT_SHORT with one window group and with three,
//   LONG_STOP, TNS present and absent, a joint (CAC) channel pair, and an
//   element whose presence bit is clear so the core decoder resets it instead
//   of reading it.
//
// Ten more packets would add only a second short-window grouping, which is not
// worth 2.7x the fixture.  Three things the capture never does anywhere, so the
// fixture cannot either: a channel pair WITHOUT the CAC tool (all 3977 packets
// are joint), an LRVQ frame, and an active bandwidth-extension channel.
//
// WHY THE TOLERANCE IS NOT ZERO.  The library's FFT is a wrapper around Apple's
// vDSP_DFT_Execute, which is closed and is not on the head unit, so the port has
// its own and everything downstream of it agrees to float noise rather than bit
// exactly.  Everything ABOVE the transform is bit exact and is checked that way
// by `reference/run.sh port`, which compares the spectra the frame layer hands
// the filter bank - 23 862 of them over the whole capture, byte for byte.
//
// The whole-capture run, which is too big to commit:
//
//   ./apac_decoder/reference/run.sh port

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apac/config.h"
#include "apac/decoder.h"
#include "apac/element.h"
#include "apac/lrvq.h"

#include "../reference/lrvq_cases.h"

namespace {

// What the port is allowed to differ by, as a fraction of the fixture's own
// peak.  The whole 3977-packet capture comes in at 3.5e-07 of each channel's
// peak; this leaves an order of magnitude of headroom and would still catch any
// real decode difference, which is never subtle.
const double kTolerance = 5e-06;

unsigned char* g_blob;
size_t g_blobBytes;

uint32_t Rd32(size_t at)
{
    assert(at + 4 <= g_blobBytes);
    return (uint32_t)g_blob[at] | ((uint32_t)g_blob[at + 1] << 8) |
           ((uint32_t)g_blob[at + 2] << 16) | ((uint32_t)g_blob[at + 3] << 24);
}

// The expected PCM starts wherever the packet payloads happen to end, so its
// offset in the blob is arbitrary and casting to `const float*` there is
// undefined - and not harmlessly so.  On ARMv7 the VFP load faults: built for
// armle-v7 and run on QNX this test died with SIGBUS at an odd address
// (`ref=00300725`) under both GCC 4.9.4 and 8.5.0, while x86 and arm64 hide it
// completely.  Read the bit pattern instead; everything downstream compares
// bit patterns anyway.
uint32_t RdF32Bits(size_t at)
{
    assert(at + 4 <= g_blobBytes);
    uint32_t b;
    memcpy(&b, g_blob + at, 4);
    return b;
}

float RdF32(size_t at)
{
    const uint32_t b = RdF32Bits(at);
    float f;
    memcpy(&f, &b, 4);
    return f;
}

// Window sequences seen, counted through the decoder's spectrum sink.  This is
// the fixture's coverage claim, checked rather than asserted in a comment.
int g_seq[4];
void CountSequence(void* ctx, uint32_t channel, uint32_t windowSequence,
                   const float* spectrum, uint32_t count)
{
    (void)ctx; (void)channel; (void)spectrum; (void)count;
    if (windowSequence < 4)
        ++g_seq[windowSequence];
}

// ---------------------------------------------------------- the LRVQ seam ---
//
// LRVQ is unreachable on this content - patching each of
// APACLRVQDecoder::SpectrumDeserialize{SCE,CPE,LFE} in the running library to
// consume no bits leaves the whole capture bit identical - so the element
// layer's LRVQ branch cannot be exercised by the capture at all.  What it CAN
// be checked against is the stage-h1 test's own frame cases, whose expected
// spectra, TNS records and bit counts came from the library:
// tests/test_lrvq.cpp drives apac::DeserializeLrvqFrame with a hand-built
// context and compares against reference/.../lrvq_cpe_frame.fixture.
//
// This drives the SAME cases through APACChannelPairElement instead, and
// requires that the element reaches exactly the same state.  Anything the seam
// can plausibly get wrong - a context pointer crossed, the long and short band
// tables swapped, the frame-mode bit forgotten or read twice, the wrong ICSInfo
// handed to TNS - moves the spectra, the TNS bytes or the bit count.  The
// element must consume exactly ONE bit more than the frame body: the frame-mode
// bit this layer adds in front.
int g_lrvqCases, g_lrvqShort, g_lrvqFiltered, g_lrvqSce, g_lrvqLfe;

// What this cannot see, and why.  LrvqBuildFrameCases sets ws1 = ws0 on every
// case and both LRVQ ICS headers are read at coding type 1, which fixes maxSfb
// at 27 long / 7 short and reads no grouping field - so the two APACICSInfo
// objects come out identical and crossing ctx.ics[0] with ctx.ics[1] is
// unobservable here.  It would still be a defect on a frame whose two channels
// have different window sequences, because CoreDecoder::DecodeAPACFrame windows
// channel 0 with GetICSInfo(true).  Crossing ctx.spec or ctx.tns, swapping the
// long and short band tables, or losing the frame-mode bit all ARE caught.

// Prepend a single 1 bit - the CPE's frame-mode selector - to a bit buffer.
void PrependOneBit(const uint8_t* src, uint32_t n, uint8_t* dst)
{
    dst[0] = (uint8_t)(0x80u | (src[0] >> 1));
    for (uint32_t i = 1; i < n; ++i)
        dst[i] = (uint8_t)((src[i - 1] << 7) | (src[i] >> 1));
    dst[n] = (uint8_t)(src[n - 1] << 7);
}

void CheckLrvqSeam()
{
    static LrvqFrameCase cases[64];
    const int n = LrvqBuildFrameCases(cases, 64);
    assert(n > 0);

    const apac::SFBOffsets* tab = apac::GetSFBOffsetTable(48000, 1024);
    assert(tab != 0);

    static apac::LRVQDecoder lrvqA, lrvqB;
    static apac::SyntacticElement element;
    static float spec0[1024], spec1[1024];
    static uint8_t tnsA[190], tnsB[190];
    static uint8_t shifted[kLrvqMaxBytes + 1];

    for (int i = 0; i < n; ++i) {
        const LrvqFrameCase& c = cases[i];

        // Reference side: the frame body on its own, exactly as the stage-h1
        // test drives it - which is the side the library checked.
        apac::APACICSInfo ics0, ics1;
        ics0.Clear(); ics1.Clear();
        ics0.maxSfbLong = (uint8_t)tab->numSfbLong;
        ics0.maxSfbShort = (uint8_t)tab->numSfbShort;
        ics1.maxSfbLong = ics0.maxSfbLong;
        ics1.maxSfbShort = ics0.maxSfbShort;
        apac::TNSData tns0, tns1;
        memset(&tns0, 0, sizeof tns0);
        memset(&tns1, 0, sizeof tns1);
        tns0.Configure(48000);
        tns1.Configure(48000);
        for (int k = 0; k < 1024; ++k)
            spec0[k] = spec1[k] = -777.0f;
        apac::SpectralData sp0, sp1;
        sp0.begin = spec0; sp0.end = spec0 + 1024;
        sp1.begin = spec1; sp1.end = spec1 + 1024;

        apac::LrvqFrameContext ctx;
        ctx.ics[0] = &ics0;   ctx.ics[1] = &ics1;
        ctx.spec[0] = &sp0;   ctx.spec[1] = &sp1;
        ctx.tns[0] = &tns0;   ctx.tns[1] = &tns1;
        ctx.sfbLong.numBands  = tab->numSfbLong;
        ctx.sfbLong.offset    = tab->sfbOffsetLong;
        ctx.sfbShort.numBands = tab->numSfbShort;
        ctx.sfbShort.offset   = tab->sfbOffsetShort;
        ctx.lrvq = &lrvqA;

        apac::BitstreamReader bsA(c.bits, c.nbytes);
        const int32_t beforeA = bsA.BitsRemaining();
        const int32_t rcA = apac::DeserializeLrvqFrame(bsA, ctx);
        const int32_t usedA = beforeA - bsA.BitsRemaining();

        // Seam side: the same bits behind a set frame-mode bit, through the
        // element.
        PrependOneBit(c.bits, c.nbytes, shifted);
        assert(element.Configure(apac::kElementCPE, tab, 48000) == apac::kOk);
        apac::BitstreamReader bsB(shifted, c.nbytes + 1);
        const int32_t beforeB = bsB.BitsRemaining();
        const uint32_t rcB = element.Deserialize(bsB, &lrvqB);
        const int32_t usedB = beforeB - bsB.BitsRemaining();

        if ((rcA != 0) != (rcB != apac::kOk)) {
            printf("FAIL lrvq seam case %d (%s): body rc %d, element rc %u\n",
                   i, c.name, (int)rcA, rcB);
            exit(1);
        }
        if (usedB != usedA + 1) {
            printf("FAIL lrvq seam case %d (%s): body used %d bits, element %d "
                   "- expected exactly one more, the frame-mode bit\n",
                   i, c.name, (int)usedA, (int)usedB);
            exit(1);
        }
        if (rcA != 0)
            continue;

        // Bit patterns, not values: LRVQ writes signed zeros too.
        if (memcmp(spec0, element.ChannelData(true), sizeof spec0) != 0 ||
            memcmp(spec1, element.ChannelData(false), sizeof spec1) != 0) {
            printf("FAIL lrvq seam case %d (%s): spectra differ\n", i, c.name);
            exit(1);
        }
        memcpy(tnsA, &tns0, 190);
        memcpy(tnsB, &element.TNSRecord(true), 190);
        if (memcmp(tnsA, tnsB, 190) != 0) {
            printf("FAIL lrvq seam case %d (%s): channel 0 TNS record differs\n",
                   i, c.name);
            exit(1);
        }
        memcpy(tnsA, &tns1, 190);
        memcpy(tnsB, &element.TNSRecord(false), 190);
        if (memcmp(tnsA, tnsB, 190) != 0) {
            printf("FAIL lrvq seam case %d (%s): channel 1 TNS record differs\n",
                   i, c.name);
            exit(1);
        }
        const apac::APACICSInfo& e0 = element.GetICSInfo(true);
        const apac::APACICSInfo& e1 = element.GetICSInfo(false);
        assert(e0.windowSequence == ics0.windowSequence && e0.maxSfb == ics0.maxSfb);
        assert(e1.windowSequence == ics1.windowSequence && e1.maxSfb == ics1.maxSfb);
        if (c.ws0 == 2) ++g_lrvqShort;
        if (c.tnsFilter) ++g_lrvqFiltered;
        ++g_lrvqCases;
    }

    // And with no decoder supplied the same frame must be refused, not
    // mis-parsed: that is the build this port shipped before stage h1 landed.
    {
        PrependOneBit(cases[0].bits, cases[0].nbytes, shifted);
        assert(element.Configure(apac::kElementCPE, tab, 48000) == apac::kOk);
        apac::BitstreamReader bs(shifted, cases[0].nbytes + 1);
        assert(element.Deserialize(bs, 0) == apac::kPortNeedsLRVQ);
    }
}

// The SCE and LFE halves of the seam.  Those two do not go through
// DeserializeLrvqFrame: the element reads the coding-type bit and the ICS
// header itself - two bits at coding type 1 - and then calls
// SpectrumDeserialize{SCE,LFE} directly, with the SCE carrying on into the
// shared TNS tail and the LFE stopping.  The stage-h1 test's element cases are
// exactly those two spectrum payloads, so this frames one and requires the
// element to reach the same state as the entry point called on its own.
void CheckLrvqSceLfeSeam()
{
    static LrvqElemCase cases[64];
    const int n = LrvqBuildElemCases(cases, 64);
    assert(n > 0);

    const apac::SFBOffsets* tab = apac::GetSFBOffsetTable(48000, 1024);
    assert(tab != 0);

    static apac::LRVQDecoder lrvqA, lrvqB;
    static apac::SyntacticElement element;
    static float spec[1024];
    static uint8_t framed[kLrvqMaxBytes + 4];

    for (int i = 0; i < n; ++i) {
        const LrvqElemCase& c = cases[i];
        const bool isLfe = (c.kind == kLrvqElemLFE);
        if (!isLfe && c.kind != kLrvqElemSCE)
            continue;                       // the CPE cases go through the frame body

        // [windowSequence:2][the case's own spectrum payload][tnsPresent:0 for
        // the SCE].  The window sequence is the only field an LRVQ ICS header
        // carries; 2 selects eight short windows.
        const uint32_t ws = c.isShort0 ? 2u : 0u;
        LrvqBitWriter w;
        w.Init(framed, sizeof framed);
        w.Put(ws, 2);
        for (uint32_t bit = 0; bit < c.nbytes * 8u; ++bit)
            w.Put((c.bits[bit >> 3] >> (7 - (bit & 7))) & 1u, 1);
        if (!isLfe)
            w.Put(0, 1);                    // no TNS block
        const uint32_t framedBytes = (uint32_t)sizeof framed;

        // Reference: the ICS header and the entry point, called directly.
        apac::APACICSInfo ics;
        ics.Clear();
        ics.maxSfbLong  = (uint8_t)tab->numSfbLong;
        ics.maxSfbShort = (uint8_t)tab->numSfbShort;
        for (int k = 0; k < 1024; ++k)
            spec[k] = -333.0f;
        apac::SpectralData sp;
        sp.begin = spec; sp.end = spec + 1024;

        apac::TNSData tns;
        memset(&tns, 0, sizeof tns);
        tns.Configure(48000);

        apac::BitstreamReader bsA(framed, framedBytes);
        const int32_t beforeA = bsA.BitsRemaining();
        if (ics.Deserialize(bsA, apac::kCodingTypeSpecial))
            continue;                       // the header itself was rejected
        int32_t rcA = isLfe ? lrvqA.SpectrumDeserializeLFE(bsA, ics, sp)
                            : lrvqA.SpectrumDeserializeSCE(bsA, ics, sp);
        if (!isLfe && rcA == 0) {
            // The SCE's shared tail, spelled out here rather than borrowed from
            // the element under test: the TNS-present bit, the block if it is
            // set, and the filter over the band table the window sequence picks.
            tns.present = (uint8_t)(bsA.GetBit() != 0);
            if (tns.present) {
                if (tns.ParseTNSData(bsA, ics))
                    rcA = 1;
            } else {
                tns.nFilt = 0;
            }
            if (rcA == 0) {
                apac::SFBBandOffsets bands;
                if (ics.windowSequence == 2) {
                    bands.numBands = tab->numSfbShort;
                    bands.offset   = tab->sfbOffsetShort;
                } else {
                    bands.numBands = tab->numSfbLong;
                    bands.offset   = tab->sfbOffsetLong;
                }
                tns.Apply(ics, bands, sp);
            }
        }
        const int32_t usedA = beforeA - bsA.BitsRemaining();

        // Seam: the same bits behind the coding-type bit, through the element.
        static uint8_t shifted[sizeof framed + 1];
        PrependOneBit(framed, framedBytes, shifted);
        assert(element.Configure(isLfe ? apac::kElementLFE : apac::kElementSCE,
                                 tab, 48000) == apac::kOk);
        apac::BitstreamReader bsB(shifted, framedBytes + 1);
        const int32_t beforeB = bsB.BitsRemaining();
        const uint32_t rcB = element.Deserialize(bsB, &lrvqB);
        const int32_t usedB = beforeB - bsB.BitsRemaining();

        if ((rcA != 0) != (rcB != apac::kOk)) {
            printf("FAIL lrvq %s seam case %d: entry rc %d, element rc %u\n",
                   isLfe ? "LFE" : "SCE", i, (int)rcA, rcB);
            exit(1);
        }
        if (rcA != 0)
            continue;
        if (usedB != usedA + 1) {
            printf("FAIL lrvq %s seam case %d: entry used %d bits, element %d "
                   "- expected exactly one more, the coding-type bit\n",
                   isLfe ? "LFE" : "SCE", i, (int)usedA, (int)usedB);
            exit(1);
        }
        if (memcmp(spec, element.ChannelData(true), sizeof spec) != 0) {
            printf("FAIL lrvq %s seam case %d: spectrum differs\n",
                   isLfe ? "LFE" : "SCE", i);
            exit(1);
        }
        assert(element.GetICSInfo(true).windowSequence == ics.windowSequence);
        if (!isLfe && memcmp(&tns, &element.TNSRecord(true), 190) != 0) {
            printf("FAIL lrvq SCE seam case %d: TNS record differs\n", i);
            exit(1);
        }
        if (isLfe) ++g_lrvqLfe; else ++g_lrvqSce;
    }
}

// Is ONE shared LRVQDecoder the same as one per element?
//
// The port shares a single instance across the whole decoder because the
// original has no decoder object at all - APACLRVQDecoder's members are static,
// and env/env2/work/qidx/prevMode are stack locals of the wrapper (0x9bdc0 and
// 0x9bf60 both open with `sub sp, sp, #0x2500`, 9472 bytes).  Sharing hoists
// those off the stack, which is the one thing that could carry state between
// elements where the original could not.
//
// So drive every case twice - once with an instance reused across all of them,
// once with a fresh one per case - and require identical output.  Including a
// CPE whose FIRST channel is skipped, which NO generated case covers (they all
// set skip0 = 0) and which is precisely where the original reads uninitialised
// stack: if the port's carried-over envelope were ever observable, it would be
// there.
int g_shareChecked;

void CheckLrvqSharing()
{
    static LrvqFrameCase frames[64];
    static LrvqElemCase elems[64];
    const int nf = LrvqBuildFrameCases(frames, 64);
    const int ne = LrvqBuildElemCases(elems, 64);
    const apac::SFBOffsets* tab = apac::GetSFBOffsetTable(48000, 1024);
    assert(tab != 0);

    static apac::SyntacticElement element;
    static apac::LRVQDecoder shared;
    static float keep0[64][1024], keep1[64][1024];
    static uint8_t framed[kLrvqMaxBytes + 8];
    static uint8_t shifted[kLrvqMaxBytes + 9];
    static bool decoded[64];

    for (int forced = 0; forced < 2; ++forced) {
        memset(decoded, 0, sizeof decoded);
        for (int pass = 0; pass < 2; ++pass) {
            shared = apac::LRVQDecoder();
            for (int i = 0; i < (forced ? ne : nf); ++i) {
                if (forced && elems[i].kind != kLrvqElemCPE)
                    continue;
                apac::LRVQDecoder fresh;
                apac::LRVQDecoder* dec = pass ? &fresh : &shared;
                uint32_t nbytes;
                if (!forced) {
                    memcpy(framed, frames[i].bits, frames[i].nbytes);
                    nbytes = frames[i].nbytes;
                } else {
                    // [ws:2][skip0 = 1][the rest of the case][no TNS][no TNS]
                    LrvqBitWriter w;
                    w.Init(framed, sizeof framed);
                    w.Put(0, 2);
                    w.Put(1, 1);
                    for (uint32_t b = 1; b < elems[i].nbytes * 8u; ++b)
                        w.Put((elems[i].bits[b >> 3] >> (7 - (b & 7))) & 1u, 1);
                    w.Put(0, 1);
                    w.Put(0, 1);
                    nbytes = (uint32_t)sizeof framed;
                }
                PrependOneBit(framed, nbytes, shifted);
                assert(element.Configure(apac::kElementCPE, tab, 48000) == apac::kOk);
                apac::BitstreamReader bs(shifted, nbytes + 1);
                if (element.Deserialize(bs, dec) != apac::kOk)
                    continue;
                if (pass == 0) {
                    memcpy(keep0[i], element.ChannelData(true), sizeof keep0[i]);
                    memcpy(keep1[i], element.ChannelData(false), sizeof keep1[i]);
                    decoded[i] = true;
                    ++g_shareChecked;
                } else if (decoded[i]) {
                    if (memcmp(keep0[i], element.ChannelData(true), sizeof keep0[i]) != 0 ||
                        memcmp(keep1[i], element.ChannelData(false), sizeof keep1[i]) != 0) {
                        printf("FAIL lrvq sharing: %scase %d differs between a "
                               "shared decoder and a fresh one - the port's "
                               "hoisted envelope is carrying state the original "
                               "could not\n", forced ? "forced-skip0 " : "", i);
                        exit(1);
                    }
                }
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/frames.bin";
    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("FAIL cannot open %s\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    g_blobBytes = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    g_blob = (unsigned char*)malloc(g_blobBytes);
    assert(fread(g_blob, 1, g_blobBytes, f) == g_blobBytes);
    fclose(f);

    assert(g_blobBytes > 24 && memcmp(g_blob, "APACFRM1", 8) == 0);
    const uint32_t numPackets  = Rd32(8);
    const uint32_t channels    = Rd32(12);
    const uint32_t frameLength = Rd32(16);
    const uint32_t cookieBytes = Rd32(20);
    const unsigned char* cookie = g_blob + 24;

    size_t at = 24 + cookieBytes;
    const size_t lengthsAt = at;
    size_t payload = 0;
    for (uint32_t i = 0; i < numPackets; ++i)
        payload += Rd32(lengthsAt + 4 * i);
    const size_t packetsAt = lengthsAt + 4 * numPackets;
    const size_t pcmAt = packetsAt + payload;
    const size_t pcmBytes = (size_t)numPackets * channels * frameLength * 4;
    assert(pcmAt + pcmBytes == g_blobBytes);

    // The config comes out of the cookie the library was configured with, not
    // out of a transcription of it.  The `dapa` payload starts with the 0x0800
    // sync word and apac::GlobalConfig starts two bytes further on.
    uint32_t off = 0, payloadBytes = 0;
    for (uint32_t p = 0; p + 8 <= cookieBytes;) {
        const uint32_t size = ((uint32_t)cookie[p] << 24) | ((uint32_t)cookie[p + 1] << 16) |
                              ((uint32_t)cookie[p + 2] << 8) | cookie[p + 3];
        const uint32_t type = ((uint32_t)cookie[p + 4] << 24) | ((uint32_t)cookie[p + 5] << 16) |
                              ((uint32_t)cookie[p + 6] << 8) | cookie[p + 7];
        if (size < 8 || p + size > cookieBytes)
            break;
        if (type == 0x64617061u) { off = p + 12; payloadBytes = size - 12; break; }
        p += (type == 0x66726d61u || type == 0x61706163u) ? 8 : size;
    }
    assert(off != 0 && payloadBytes > 2);

    static apac::GlobalConfig gc;
    gc.Reset();
    {
        apac::BitstreamReader bs(cookie + off + 2, payloadBytes - 2);
        const uint32_t rc = gc.Deserialize(bs);
        assert(rc == apac::kOk);
    }
    assert(gc.sampleRate == 48000);
    assert(gc.frameLength == frameLength);

    // APACDecoder_Create builds an APACASPDecoder, so the frame-type field is
    // two bits.  Reading it as one - which is what APACDecoder::DecodeFrame
    // does - puts every later field one place early and the first element then
    // looks like an LRVQ frame.
    static apac::ASPDecoder decoder;
    assert(decoder.Initialize(gc) == apac::kOk);
    assert(decoder.NumChannels() == channels);
    assert(decoder.FrameLength() == frameLength);
    decoder.SetSpectrumSink(&CountSequence, 0);

    float* pcm = (float*)malloc(sizeof(float) * channels * frameLength);

    double worst = 0.0, worstCh[16];
    double peak = 0.0;
    for (uint32_t c = 0; c < 16; ++c) worstCh[c] = 0.0;
    uint32_t identical = 0, total = 0;

    size_t pkt = packetsAt;
    for (uint32_t i = 0; i < numPackets; ++i) {
        const uint32_t len = Rd32(lengthsAt + 4 * i);
        uint32_t frames = 0;
        const uint32_t rc = decoder.DecodeFrame(g_blob + pkt, len, pcm, &frames);
        if (rc != apac::kOk) {
            printf("FAIL packet %u: status %u\n", i, rc);
            return 1;
        }
        assert(frames == frameLength);
        assert(decoder.FrameType() == 1);       // every packet in this capture
        pkt += len;

        const size_t wantAt = pcmAt + (size_t)i * channels * frameLength * 4;
        for (uint32_t k = 0; k < channels * frameLength; ++k) {
            const double a = RdF32(wantAt + 4 * (size_t)k), b = pcm[k];
            const double d = fabs(a - b);
            if (fabs(a) > peak) peak = fabs(a);
            if (d > worst) worst = d;
            if (d > worstCh[k % channels]) worstCh[k % channels] = d;
            // Compare bit patterns, not values: signed zeros are meaningful in
            // this codec and a float compare hides them.
            uint32_t ub;
            memcpy(&ub, &pcm[k], 4);
            if (RdF32Bits(wantAt + 4 * (size_t)k) == ub) ++identical;
            ++total;
        }
    }

    // The flush path: a zero-length packet is not an error, it resets and
    // produces nothing.  That is what APACDecoder_DecodeFrame does with
    // inBytes == 0, and it is the only legal way back into a running stream.
    {
        uint32_t frames = 123;
        assert(decoder.DecodeFrame(g_blob + packetsAt, 0, pcm, &frames) == apac::kOk);
        assert(frames == 0);
    }

    // Coverage, checked rather than claimed: all four window sequences, and the
    // fixture's six packets times six channels of spectrum.
    assert(g_seq[0] > 0 && g_seq[1] > 0 && g_seq[2] > 0 && g_seq[3] > 0);
    assert(g_seq[0] + g_seq[1] + g_seq[2] + g_seq[3] ==
           (int)(numPackets * channels));

    CheckLrvqSeam();
    CheckLrvqSceLfeSeam();
    CheckLrvqSharing();

    const double rel = worst / peak;
    if (!(rel < kTolerance)) {
        printf("FAIL worst %.4g of peak %.4g = %.3g, tolerance %.3g\n",
               worst, peak, rel, kTolerance);
        for (uint32_t c = 0; c < channels; ++c)
            printf("   ch%u max |diff| %.4g\n", c, worstCh[c]);
        return 1;
    }

    printf("OK  %u packets x %u ch, seq %d/%d/%d/%d, worst %.2g of full scale, "
           "%.1f%% bit-identical; LRVQ seam %d CPE frames (%d short, %d "
           "filtered), %d SCE, %d LFE, %d share-invariant\n",
           numPackets, channels, g_seq[0], g_seq[1], g_seq[2], g_seq[3], rel,
           100.0 * identical / total, g_lrvqCases, g_lrvqShort, g_lrvqFiltered,
           g_lrvqSce, g_lrvqLfe, g_shareChecked);
    free(pcm);
    free(g_blob);
    return 0;
}
