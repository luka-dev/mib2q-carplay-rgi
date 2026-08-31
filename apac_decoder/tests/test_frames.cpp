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

    const float* expected = (const float*)(g_blob + pcmAt);
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

        const float* want = expected + (size_t)i * channels * frameLength;
        for (uint32_t k = 0; k < channels * frameLength; ++k) {
            const double a = want[k], b = pcm[k];
            const double d = fabs(a - b);
            if (fabs(a) > peak) peak = fabs(a);
            if (d > worst) worst = d;
            if (d > worstCh[k % channels]) worstCh[k % channels] = d;
            // Compare bit patterns, not values: signed zeros are meaningful in
            // this codec and a float compare hides them.
            uint32_t ua, ub;
            memcpy(&ua, &want[k], 4);
            memcpy(&ub, &pcm[k], 4);
            if (ua == ub) ++identical;
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

    const double rel = worst / peak;
    if (!(rel < kTolerance)) {
        printf("FAIL worst %.4g of peak %.4g = %.3g, tolerance %.3g\n",
               worst, peak, rel, kTolerance);
        for (uint32_t c = 0; c < channels; ++c)
            printf("   ch%u max |diff| %.4g\n", c, worstCh[c]);
        return 1;
    }

    printf("OK  %u packets x %u ch, seq %d/%d/%d/%d, worst %.2g of full scale, "
           "%.1f%% bit-identical\n",
           numPackets, channels, g_seq[0], g_seq[1], g_seq[2], g_seq[3], rel,
           100.0 * identical / total);
    free(pcm);
    free(g_blob);
    return 0;
}
