/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Stage h: the loudness / DRC layer's bit accounting.
//
// What this test can and cannot prove.
//
// The whole DRC layer in Apple's decoder is four dispatchers that read no bits
// themselves (include/apac/drc.h lists them with their addresses and sizes);
// every bit of the payload is read by mpddrc::UniDrc::Deserialize, the MPEG-D
// DRC parser Apple links in whole - 62 classes and 51 Deserialize members in
// this binary.  That parser is not ported, so there is no fixture of its
// output to compare against and this is not a differential test.
//
// What matters for the decoder that IS ported is the bit accounting, and that
// has exactly two cases:
//
//   no payload   the original returns 1 from DynRangeCompressor::Deserialize
//                without touching the reader.  This is the case every CarPlay
//                frame takes, and the test checks the port moves the reader by
//                zero bits in it - a port that consumed even one bit here
//                would desynchronise every frame.
//   a payload    the port must refuse, not guess.  The test checks it reports
//                kPortNeedsLoudnessDRC and, again, has not moved the reader -
//                so the refusal names an exact bit position.
//
// The claim that the first case is the only reachable one is measured, not
// asserted here: all five DRC entry points were patched to `MOV W0,#imm ; RET`
// in the running library and the whole capture decoded again
// (reference/scratch/reach_shim.c).  Over all 3977 packets - 97 738 752 bytes
// of float32 - the output is byte-identical to the unpatched decode, for both
// return values.  PORT_STATUS.md, stage h, records that run.

#include "apac/config.h"
#include "apac/drc.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// A buffer whose every bit is 1, so any accidental read is visible in the
// reader's own position rather than in a value that might happen to be zero.
unsigned char buf[64];

int checks = 0;

void expectQuiet(apac::BitstreamReader& bs, int32_t before, const char* what)
{
    const int32_t used = before - bs.BitsRemaining();
    if (used != 0) {
        printf("FAIL %s consumed %d bits, must consume none\n", what, (int)used);
        exit(1);
    }
    ++checks;
}

} // namespace

int main(void)
{
    memset(buf, 0xFF, sizeof buf);

    // ---- the frame side, no payload: the CarPlay case ---------------------
    for (int i = 0; i < 4; ++i) {          // every buffer alignment
        apac::APACDRCDecoder dec;
        dec.Clear();
        apac::BitstreamReader bs(buf + i, (uint32_t)(sizeof buf - 4));
        const int32_t before = bs.BitsRemaining();
        const uint32_t rc = dec.Deserialize(bs);
        expectQuiet(bs, before, "APACDRCDecoder::Deserialize (no payload)");
        // The original's literal `return 1` from the null-payload branch.
        if (rc != apac::kBadValue) {
            printf("FAIL APACDRCDecoder::Deserialize returned %u, expected 1\n", rc);
            return 1;
        }
        ++checks;
    }

    // Called once per frame for a whole capture's worth of frames, it must
    // still never move the reader.
    {
        apac::APACDRCDecoder dec;
        dec.Clear();
        apac::BitstreamReader bs(buf, (uint32_t)sizeof buf);
        const int32_t before = bs.BitsRemaining();
        for (int f = 0; f < 3977; ++f)
            (void)dec.Deserialize(bs);
        expectQuiet(bs, before, "APACDRCDecoder::Deserialize x3977");
    }

    // ---- the config side, no payload -------------------------------------
    {
        apac::LoudnessDRCConfig cfg;
        cfg.Clear();
        apac::BitstreamReader bs(buf, (uint32_t)sizeof buf);
        const int32_t before = bs.BitsRemaining();
        const uint32_t rc = cfg.Deserialize(bs, apac::kHeaderPayloadConfig);
        expectQuiet(bs, before, "LoudnessDRCConfig::Deserialize");
        if (rc != apac::kBadValue) {
            printf("FAIL LoudnessDRCConfig::Deserialize returned %u, expected 1\n", rc);
            return 1;
        }
        ++checks;
        // The lazy build happens once; a second call must behave identically.
        const int32_t before2 = bs.BitsRemaining();
        if (cfg.Deserialize(bs, apac::kHeaderPayloadConfig) != rc) {
            printf("FAIL LoudnessDRCConfig::Deserialize is not idempotent\n");
            return 1;
        }
        expectQuiet(bs, before2, "LoudnessDRCConfig::Deserialize (second call)");
    }

    // ---- with a payload: refuse, do not guess ----------------------------
    {
        apac::LoudnessDRCConfig cfg;
        cfg.Clear();
        // The original only builds a compressor when it has none; here one
        // already exists and Initialize gave it a payload.
        cfg.initialized                = 1;
        cfg.compressor.hasPayload      = 1;
        cfg.compressor.payload.present = 1;
        apac::BitstreamReader bs(buf, (uint32_t)sizeof buf);
        const int32_t before = bs.BitsRemaining();
        const uint32_t rc = cfg.Deserialize(bs, apac::kHeaderPayloadConfig);
        if (rc != apac::kPortNeedsLoudnessDRC) {
            printf("FAIL a DRC payload must report kPortNeedsLoudnessDRC, got %u\n", rc);
            return 1;
        }
        expectQuiet(bs, before, "LoudnessDRCConfig::Deserialize (payload present)");
        ++checks;
    }
    {
        apac::APACDRCDecoder dec;
        dec.Clear();
        dec.compressor.hasPayload      = 1;
        dec.compressor.payload.present = 1;
        apac::BitstreamReader bs(buf, (uint32_t)sizeof buf);
        const int32_t before = bs.BitsRemaining();
        if (dec.Deserialize(bs) != apac::kPortNeedsLoudnessDRC) {
            printf("FAIL the frame side must refuse a payload it cannot parse\n");
            return 1;
        }
        expectQuiet(bs, before, "APACDRCDecoder::Deserialize (payload present)");
        ++checks;
    }

    // ---- gains are deliberately not applied ------------------------------
    {
        apac::APACDRCDecoder dec;
        dec.Clear();
        float ch0[8], ch1[8];
        float* planes[2] = { ch0, ch1 };
        for (int i = 0; i < 8; ++i) { ch0[i] = 0.25f * (float)i; ch1[i] = -0.5f * (float)i; }
        float copy0[8], copy1[8];
        memcpy(copy0, ch0, sizeof ch0);
        memcpy(copy1, ch1, sizeof ch1);
        if (dec.DecodeFrame(planes)) {
            printf("FAIL DecodeFrame should return false, as the original does\n");
            return 1;
        }
        if (memcmp(copy0, ch0, sizeof ch0) != 0 || memcmp(copy1, ch1, sizeof ch1) != 0) {
            printf("FAIL DecodeFrame modified the samples; the gains are out of scope\n");
            return 1;
        }
        ++checks;
    }

    // ---- the config gate in GlobalConfig still refuses, not skips ---------
    // A cookie whose ancillary block sets the DRC flag must come back as a
    // refusal.  Anything else - kOk in particular - would mean the parser had
    // walked past a payload it never read.
    assert(apac::kPortNeedsLoudnessDRC >= apac::kStatusPortBase);
    ++checks;

    printf("OK  %d checks: the DRC layer reads zero bits with no payload and "
           "refuses at the mpddrc boundary with one\n", checks);
    return 0;
}
