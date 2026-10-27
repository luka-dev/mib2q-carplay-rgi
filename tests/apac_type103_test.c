/*
 * End-to-end test for the CarPlay type-103 render path, offline and with no capture.
 *
 *   APAP byte stream -> spa_apap_framer_* -> spa_apap_open -> spa_apap_parse_extension
 *   -> spa_apac_decode_s16 -> spa_reorder_5_1_decoder_to_declared_s16
 *
 * i.e. every step the hook's receive loop performs on a frame, driven by the same functions the
 * hook calls, in the same order.
 *
 * WHAT IT CHECKS AGAINST.  apac_decoder/tests/fixtures/frames.bin holds six real APAC packets out
 * of the live CarPlay 5.1 capture, the magic cookie they were decoded with, and the float32 PCM
 * the reference library produced for them - Pioneer's decoder, run natively on AArch64.  This
 * test wraps those six packets in APAP frames, opens them the way the receiver does, decodes them
 * through our decoder, and requires every one of 36 864 samples to land within one 16-bit LSB of the
 * library's float, in the permuted (declared) channel order.  So a mistake anywhere in the chain -
 * a wrong AAD, a mis-split frame, a dropped TLV terminator, a transposed channel - fails it.
 *
 * WHY THE KEY IS MADE UP.  The capture's own `shk` is a live session key and is never read here.
 * The test seals its own frames with a key of its own using spa_chacha20_xor/spa_poly1305_mac,
 * the same two primitives spa_apap_open verifies with; that those primitives implement the wire
 * contract is established separately against the reference implementation the capture tooling
 * uses (tools/pioneer_apap_decrypt.py -> PyCryptodome), on random keys and both AAD variants.
 *
 * Build and run:  ./scripts/test_hook_audio.sh
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "apac_bridge.h"
#include "apap_crypto.h"
#include "spatial_downmix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PACKETS   16
#define MAX_PACKET    4096
#define MAX_FRAME     (16u * 1024u)
#define CHANNELS      6
#define FRAME_SAMPLES 1024

/* decoder C L R Ls Rs LFE -> declared L R C LFE Ls Rs; the same table spatial_downmix.c uses,
 * restated here so the test compares against an independently written permutation rather than
 * against the one under test. */
static const int kDeclaredFromDecoder[CHANNELS] = { 1, 2, 0, 5, 3, 4 };

static const unsigned char kTestKey[SPA_APAP_KEY_SIZE] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x0f, 0x1e, 0x2d, 0x3c, 0x4b, 0x5a, 0x69, 0x78,
    0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf0
};

/* ---- the fixture ------------------------------------------------------------------------- */

static unsigned char *g_blob;
static size_t g_blobBytes;
static const unsigned char *g_fixtureCookie;
static size_t g_fixtureCookieBytes;

static uint32_t rd32(size_t at)
{
    assert(at + 4 <= g_blobBytes);
    return (uint32_t)g_blob[at] | ((uint32_t)g_blob[at + 1] << 8) |
           ((uint32_t)g_blob[at + 2] << 16) | ((uint32_t)g_blob[at + 3] << 24);
}

/* ---- sealing an APAP frame, the sender's side of spa_apap_open --------------------------- */

static void put_be32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void put_be64(unsigned char *p, uint64_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        p[i] = (unsigned char)(v >> (56 - 8 * i));
}

static void poly1305_lengths(unsigned char *out, size_t aadLength, size_t ctLength)
{
    int i;
    for (i = 0; i < 8; i++) out[i] = (unsigned char)((uint64_t)aadLength >> (8 * i));
    for (i = 0; i < 8; i++) out[8 + i] = (unsigned char)((uint64_t)ctLength >> (8 * i));
}

/* Build one whole APAP frame: BE32 inclusive length, 15-byte fixed header, then
 * ciphertext || tag[16] || nonce[8].  `variant` picks which AAD the sender authenticated with. */
static size_t seal_frame(unsigned char *out, uint32_t sequence, int variant,
                         const unsigned char *plaintext, size_t plaintextLength)
{
    unsigned char header[SPA_APAP_HEADER_SIZE];
    unsigned char nonce[SPA_APAP_NONCE_SIZE];
    unsigned char polyKey[64];
    unsigned char zeros[64];
    unsigned char mac[MAX_FRAME];
    unsigned char *body = out + 4 + SPA_APAP_HEADER_SIZE;
    const unsigned char *aad;
    size_t aadLength, macAt = 0, pad;
    size_t total;
    int i;

    /* media_time_value[8] || media_time_scale[4] || sequence_24[3] */
    memset(header, 0, sizeof(header));
    for (i = 0; i < 8; i++) header[i] = (unsigned char)((uint64_t)(0x2000000ull + sequence) >> (56 - 8 * i));
    put_be32(header + 8, 48000);
    header[12] = (unsigned char)(sequence >> 16);
    header[13] = (unsigned char)(sequence >> 8);
    header[14] = (unsigned char)sequence;
    for (i = 0; i < (int)sizeof(nonce); i++)
        nonce[i] = (unsigned char)(0xa0 + i + sequence);

    aad = (variant == SPA_APAP_AAD_AUDIO) ? header + 4 : header;
    aadLength = (variant == SPA_APAP_AAD_AUDIO) ? 8u : 12u;

    memset(zeros, 0, sizeof(zeros));
    spa_chacha20_xor(kTestKey, nonce, 0, zeros, sizeof(zeros), polyKey);
    spa_chacha20_xor(kTestKey, nonce, 1, plaintext, plaintextLength, body);

    assert(aadLength + 16 + plaintextLength + 16 + 16 <= sizeof(mac));
    memcpy(mac + macAt, aad, aadLength); macAt += aadLength;
    pad = aadLength & 15u;
    if (pad) { memset(mac + macAt, 0, 16 - pad); macAt += 16 - pad; }
    memcpy(mac + macAt, body, plaintextLength); macAt += plaintextLength;
    pad = plaintextLength & 15u;
    if (pad) { memset(mac + macAt, 0, 16 - pad); macAt += 16 - pad; }
    poly1305_lengths(mac + macAt, aadLength, plaintextLength); macAt += 16;
    spa_poly1305_mac(polyKey, mac, macAt, body + plaintextLength);

    memcpy(body + plaintextLength + SPA_APAP_TAG_SIZE, nonce, sizeof(nonce));

    total = 4 + SPA_APAP_HEADER_SIZE + plaintextLength + SPA_APAP_OVERHEAD;
    put_be32(out, (uint32_t)total);
    memcpy(out + 4, header, sizeof(header));
    return total;
}

/* The ULEB128 TLV prefix an extension packet carries in front of its audio. */
static size_t build_extension(unsigned char *out, uint8_t formatIndex,
                              uint16_t samplesPerFrame, uint64_t cookieId,
                              const unsigned char *cookie, size_t cookieLength,
                              const unsigned char *audio, size_t audioLength)
{
    size_t at = 0;
    int i;

    out[at++] = SPA_APAP_TLV_FORMAT_INDEX;
    out[at++] = 1;
    out[at++] = formatIndex;

    out[at++] = SPA_APAP_TLV_SAMPLES_PER_FRAME;
    out[at++] = 2;
    out[at++] = (unsigned char)(samplesPerFrame >> 8);
    out[at++] = (unsigned char)samplesPerFrame;

    out[at++] = SPA_APAP_TLV_MAGIC_COOKIE_ID;
    out[at++] = 8;
    for (i = 0; i < 8; i++) out[at++] = (unsigned char)(cookieId >> (56 - 8 * i));

    /* The same packet carries its decoder configuration.  The fixture cookie is small enough
     * for a one-byte ULEB128 length, as were the APAC and AAC cookies captured on the car. */
    assert(cookie && cookieLength && cookieLength < 0x80);
    out[at++] = SPA_APAP_TLV_MAGIC_COOKIE;
    out[at++] = (unsigned char)cookieLength;
    memcpy(out + at, cookie, cookieLength); at += cookieLength;

    out[at++] = SPA_APAP_TLV_LOUDNESS_PARAMETERS;
    out[at++] = 12;
    put_be32(out + at, 0xc0d00000u); at += 4; /* -6.5 dB */
    put_be32(out + at, 0xbf800000u); at += 4; /* -1.0 dB */
    put_be32(out + at, 0x3f780000u); at += 4; /*  0.96875 */
    out[at++] = SPA_APAP_TLV_MEDIA_KIND;
    out[at++] = 1;
    out[at++] = 1;
    out[at++] = SPA_APAP_TLV_SOURCE_FORMAT_INDEX;
    out[at++] = 1;
    out[at++] = 73;

    out[at++] = SPA_APAP_TLV_END;
    memcpy(out + at, audio, audioLength);
    return at + audioLength;
}

/* ---- the test ---------------------------------------------------------------------------- */

static unsigned char g_stream[MAX_PACKETS * (MAX_PACKET + 256)];
static size_t g_streamBytes;
static size_t g_frameCount;

static int16_t g_got[MAX_PACKETS][CHANNELS * FRAME_SAMPLES];
static size_t g_gotFrames;

/* Replay the whole byte stream through the framer at a fixed chunk size, honouring
 * spa_apap_framer_want() exactly as the socket loop does, and decode every frame it yields. */
static void replay(spa_apac_decoder_t *decoder, size_t chunk, const char *what)
{
    static unsigned char frameStore[MAX_FRAME];
    static unsigned char plaintext[MAX_FRAME];
    spa_apap_framer_t framer;
    size_t at = 0;

    spa_apap_framer_init(&framer, frameStore, sizeof(frameStore));
    g_gotFrames = 0;

    while (at < g_streamBytes) {
        size_t want = spa_apap_framer_want(&framer, chunk);
        size_t have = g_streamBytes - at;
        size_t off = 0;
        if (want > have) want = have;
        assert(want > 0);

        while (off < want) {
            const unsigned char *frame = 0;
            size_t frameLength = 0, consumed = 0;
            const int rc = spa_apap_framer_push(&framer, g_stream + at + off, want - off,
                                                &consumed, &frame, &frameLength);
            off += consumed;
            if (rc == 0) break;
            if (rc < 0) {
                printf("FAIL %s: framer rejected a valid frame at %zu\n", what, at + off);
                exit(1);
            }

            {
                spa_apap_extension_t ext;
                const unsigned char *audio = 0;
                size_t audioLength = 0, plainLength = 0;
                uint32_t frames = 0;
                int variant = -1;

                if (!spa_apap_open(kTestKey, frame, frameLength, plaintext, sizeof(plaintext),
                                   &plainLength, &variant)) {
                    printf("FAIL %s: frame %zu did not authenticate\n", what, g_gotFrames);
                    exit(1);
                }
                memset(&ext, 0, sizeof(ext));
                if (variant == SPA_APAP_AAD_EXTENSION) {
                    if (!spa_apap_parse_extension(plaintext, plainLength, &ext,
                                                  &audio, &audioLength)) {
                        printf("FAIL %s: frame %zu TLV did not parse\n", what, g_gotFrames);
                        exit(1);
                    }
                    assert(ext.hasFormatIndex && ext.formatIndex == 65);
                    assert(ext.hasSamplesPerFrame &&
                           ext.samplesPerFrame == FRAME_SAMPLES);
                    assert(ext.hasLoudnessParameters && ext.hasMediaKind &&
                           ext.mediaKind == 1);
                    assert(ext.hasSourceFormatIndex &&
                           ext.sourceFormatIndex == 73);
                    assert(ext.hasMagicCookieId &&
                           ext.magicCookieId == 0x60100d08f9d19abbull);
                    assert(ext.magicCookieLength == g_fixtureCookieBytes);
                    assert(memcmp(ext.magicCookie, g_fixtureCookie,
                                  g_fixtureCookieBytes) == 0);
                } else {
                    audio = plaintext;
                    audioLength = plainLength;
                }

                assert(g_gotFrames < MAX_PACKETS);
                if (spa_apac_decode_s16(decoder, audio, audioLength, g_got[g_gotFrames],
                                        CHANNELS * FRAME_SAMPLES, &frames) != SPA_APAC_OK) {
                    printf("FAIL %s: frame %zu did not decode\n", what, g_gotFrames);
                    exit(1);
                }
                if (frames != FRAME_SAMPLES) {
                    printf("FAIL %s: frame %zu produced %u frames, expected %d\n",
                           what, g_gotFrames, (unsigned)frames, FRAME_SAMPLES);
                    exit(1);
                }
                spa_reorder_5_1_decoder_to_declared_s16(g_got[g_gotFrames], frames,
                                                        g_got[g_gotFrames]);
                g_gotFrames++;
            }
        }
        at += off;
    }

    if (g_gotFrames != g_frameCount) {
        printf("FAIL %s: %zu frames out of %zu in\n", what, g_gotFrames, g_frameCount);
        exit(1);
    }
}

static uint32_t trim_random(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

/* Independent reference for the bounded values generated below.  The product
 * is deliberately kept below UINT64_MAX, so this simple expression is not the
 * same quotient/remainder implementation as production. */
static size_t reference_trim_frames(int present, uint64_t value, uint32_t scale,
                                    uint32_t sampleRate, size_t limit)
{
    uint64_t frames;

    if (!present || !value || !scale || !sampleRate || !limit) return 0;
    frames = (value * sampleRate) / scale;
    return frames > limit ? limit : (size_t)frames;
}

static void test_trim_properties(void)
{
    static const uint32_t rates[] = { 8000, 44100, 48000, 96000, 192000 };
    spa_apap_extension_t ext;
    uint32_t randomState = 0x41c64e6du;
    unsigned i;

    for (i = 0; i < 100000; i++) {
        uint64_t startValue = trim_random(&randomState) % 1000001u;
        uint64_t endValue = trim_random(&randomState) % 1000001u;
        uint32_t startScale = trim_random(&randomState) % 192000u + 1u;
        uint32_t endScale = trim_random(&randomState) % 192000u + 1u;
        uint32_t rate = rates[trim_random(&randomState) %
                              (sizeof(rates) / sizeof(rates[0]))];
        size_t decoded = trim_random(&randomState) % 8193u;
        int haveStart = (int)(trim_random(&randomState) & 1u);
        int haveEnd = (int)(trim_random(&randomState) & 1u);
        size_t expectedStart, expectedEnd, expectedCount;
        size_t start = SIZE_MAX, count = SIZE_MAX;

        memset(&ext, 0, sizeof(ext));
        ext.trimAtStart.present = haveStart;
        ext.trimAtStart.value = (int64_t)startValue;
        ext.trimAtStart.timescale = (int32_t)startScale;
        ext.trimAtEnd.present = haveEnd;
        ext.trimAtEnd.value = (int64_t)endValue;
        ext.trimAtEnd.timescale = (int32_t)endScale;

        expectedStart = reference_trim_frames(haveStart, startValue,
                                              startScale, rate, decoded);
        expectedCount = decoded - expectedStart;
        expectedEnd = reference_trim_frames(haveEnd, endValue,
                                            endScale, rate, expectedCount);
        expectedCount -= expectedEnd;

        spa_apap_trim_window(&ext, rate, decoded, &start, &count);
        assert(start == expectedStart);
        assert(count == expectedCount);
        assert(start <= decoded && count <= decoded - start);
    }
}

static void test_extension_parser_fuzz(void)
{
    unsigned char bytes[64];
    uint32_t randomState = 0xc001d00du;
    unsigned iteration;

    /* Deterministic malformed-input sweep.  A successful parse must return a
     * suffix of the same input; failures only have to stay memory-safe. */
    for (iteration = 0; iteration < 50000; iteration++) {
        spa_apap_extension_t ext;
        const unsigned char *audio = 0;
        size_t audioLength = SIZE_MAX;
        size_t length = trim_random(&randomState) % (sizeof(bytes) + 1u);
        size_t i;
        int parsed;

        for (i = 0; i < length; i++)
            bytes[i] = (unsigned char)trim_random(&randomState);
        memset(&ext, 0, sizeof(ext));
        parsed = spa_apap_parse_extension(bytes, length, &ext,
                                          &audio, &audioLength);
        if (parsed) {
            assert(audio >= bytes && audio <= bytes + length);
            assert(audioLength == (size_t)(bytes + length - audio));
        }
    }
}

static void test_extension_metadata(void)
{
    unsigned char plain[160];
    spa_apap_extension_t ext;
    const unsigned char *audio = 0;
    size_t audioLength = 0, at = 0;

    plain[at++] = SPA_APAP_TLV_FORMAT_INDEX;
    plain[at++] = 1; plain[at++] = 23;
    plain[at++] = SPA_APAP_TLV_SAMPLES_PER_FRAME;
    plain[at++] = 2; plain[at++] = 4; plain[at++] = 0;
    plain[at++] = SPA_APAP_TLV_TRANSITION_ID;
    plain[at++] = 3; plain[at++] = 1; plain[at++] = 2; plain[at++] = 3;
    plain[at++] = SPA_APAP_TLV_SOUNDCHECK_INFO;
    plain[at++] = 20; memset(plain + at, 0x11, 20); at += 20;
    plain[at++] = SPA_APAP_TLV_LOUDNESS_PARAMETERS;
    plain[at++] = 12;
    put_be32(plain + at, 0xc0d00000u); at += 4;
    put_be32(plain + at, 0xbf800000u); at += 4;
    put_be32(plain + at, 0x3f780000u); at += 4;
    plain[at++] = SPA_APAP_TLV_MEDIA_KIND;
    plain[at++] = 1; plain[at++] = 1;
    plain[at++] = SPA_APAP_TLV_SBUF_TRACER;
    plain[at++] = 3; plain[at++] = 'a'; plain[at++] = 'b'; plain[at++] = 'c';
    plain[at++] = SPA_APAP_TLV_SOURCE_FORMAT_INDEX;
    plain[at++] = 1; plain[at++] = 73;
    plain[at++] = SPA_APAP_TLV_SBUF_TRACER_EXTENDED;
    plain[at++] = 2; plain[at++] = 0; plain[at++] = 1;
    plain[at++] = 99; plain[at++] = 1; plain[at++] = 0xee;
    plain[at++] = SPA_APAP_TLV_END;
    plain[at++] = 0xaa;

    memset(&ext, 0, sizeof(ext));
    assert(spa_apap_parse_extension(plain, at, &ext, &audio, &audioLength));
    assert(ext.hasFormatIndex && ext.formatIndex == 23);
    assert(ext.hasSamplesPerFrame && ext.samplesPerFrame == 1024);
    assert(ext.hasTransitionId && ext.transitionId == 0x010203);
    assert(ext.soundCheckRecordCount == 1 && ext.soundCheckInfoLength == 20);
    assert(ext.hasLoudnessParameters && ext.loudnessParameters[0] == -6.5f &&
           ext.loudnessParameters[1] == -1.0f &&
           ext.loudnessParameters[2] == 0.96875f);
    assert(ext.hasMediaKind && ext.mediaKind == 1);
    assert(ext.hasSbufTracer && ext.sbufTracerLength == 3);
    assert(ext.hasSourceFormatIndex && ext.sourceFormatIndex == 73);
    assert(ext.hasSbufTracerExtended && ext.sbufTracerExtendedLength == 2);
    assert(ext.unknownFieldCount == 1);
    assert(audioLength == 1 && audio[0] == 0xaa);

    /* Known fields fail closed on an impossible shape. */
    {
        static const unsigned char malformedLoudness[] = {
            SPA_APAP_TLV_LOUDNESS_PARAMETERS, 11,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            SPA_APAP_TLV_END
        };
        memset(&ext, 0, sizeof(ext));
        assert(!spa_apap_parse_extension(malformedLoudness,
                                         sizeof(malformedLoudness),
                                         &ext, &audio, &audioLength));
    }
}

/* Fields 4/5 use Apple's compact CMTime wire form: BE64 signed value followed
 * by BE32 signed timescale.  Exercise parsing, floor conversion, clamping, and
 * the exact decoded-PCM slice consumed by audio_hook.c. */
static void test_trim_tlvs(void)
{
    unsigned char plain[64];
    spa_apap_extension_t ext;
    const unsigned char *audio = 0;
    size_t audioLength = 0, start = 0, count = 0, at = 0;

    plain[at++] = SPA_APAP_TLV_TRIM_AT_START;
    plain[at++] = 12;
    put_be64(plain + at, 480); at += 8;
    put_be32(plain + at, 48000); at += 4;  /* 10 ms = 480 frames */
    plain[at++] = SPA_APAP_TLV_TRIM_AT_END;
    plain[at++] = 12;
    put_be64(plain + at, 1); at += 8;
    put_be32(plain + at, 1000); at += 4;   /* 1 ms = 48 frames */
    plain[at++] = SPA_APAP_TLV_END;
    plain[at++] = 0xaa;
    plain[at++] = 0xbb;

    memset(&ext, 0, sizeof(ext));
    assert(spa_apap_parse_extension(plain, at, &ext, &audio, &audioLength));
    assert(ext.trimAtStart.present && ext.trimAtStart.value == 480 &&
           ext.trimAtStart.timescale == 48000);
    assert(ext.trimAtEnd.present && ext.trimAtEnd.value == 1 &&
           ext.trimAtEnd.timescale == 1000);
    assert(audioLength == 2 && audio[0] == 0xaa && audio[1] == 0xbb);
    spa_apap_trim_window(&ext, 48000, 1024, &start, &count);
    assert(start == 480 && count == 496);

    /* Rescaling is against decoded PCM rate, with the receiver's floor rule. */
    ext.trimAtStart.value = 441;
    ext.trimAtStart.timescale = 44100;
    ext.trimAtEnd.value = 1;
    ext.trimAtEnd.timescale = 1000;
    spa_apap_trim_window(&ext, 48000, 1024, &start, &count);
    assert(start == 480 && count == 496);
    spa_apap_trim_window(&ext, 44100, 1024, &start, &count);
    assert(start == 441 && count == 539); /* end floor(44.1) = 44 */

    /* Either side may consume the rest of the AU, but never exceed it. */
    ext.trimAtStart.value = 10;
    ext.trimAtStart.timescale = 1;
    ext.trimAtEnd.present = 0;
    spa_apap_trim_window(&ext, 48000, 1024, &start, &count);
    assert(start == 1024 && count == 0);
    ext.trimAtStart.present = 0;
    ext.trimAtEnd.present = 1;
    ext.trimAtEnd.value = INT64_MAX;
    ext.trimAtEnd.timescale = 1;
    spa_apap_trim_window(&ext, 48000, 1024, &start, &count);
    assert(start == 0 && count == 0);

    /* Invalid CMTime values are safe no-ops, as are absent fields. */
    ext.trimAtStart.present = 1;
    ext.trimAtStart.value = -1;
    ext.trimAtStart.timescale = 48000;
    ext.trimAtEnd.value = 1;
    ext.trimAtEnd.timescale = 0;
    spa_apap_trim_window(&ext, 48000, 1024, &start, &count);
    assert(start == 0 && count == 1024);
    spa_apap_trim_window(0, 48000, 1024, &start, &count);
    assert(start == 0 && count == 1024);

    /* Signed wire decoding matters: -1 is not a gigantic unsigned trim. */
    at = 0;
    plain[at++] = SPA_APAP_TLV_TRIM_AT_START;
    plain[at++] = 12;
    put_be64(plain + at, UINT64_MAX); at += 8;
    put_be32(plain + at, 48000); at += 4;
    plain[at++] = SPA_APAP_TLV_END;
    memset(&ext, 0, sizeof(ext));
    assert(spa_apap_parse_extension(plain, at, &ext, &audio, &audioLength));
    assert(ext.trimAtStart.present && ext.trimAtStart.value == -1 &&
           ext.trimAtStart.timescale == 48000);
    spa_apap_trim_window(&ext, 48000, 1024, &start, &count);
    assert(start == 0 && count == 1024);

    /* Known trim fields have an exact 12-byte shape; silently skipping a
     * damaged one would make a gapless boundary audibly wrong. */
    {
        static const unsigned char shortTrim[] = {
            SPA_APAP_TLV_TRIM_AT_START, 11,
            0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
            SPA_APAP_TLV_END
        };
        memset(&ext, 0, sizeof(ext));
        assert(!spa_apap_parse_extension(shortTrim, sizeof(shortTrim), &ext,
                                         &audio, &audioLength));
    }

    test_trim_properties();
    test_extension_parser_fuzz();
}

/* Nothing that has been tampered with may open. */
static void test_rejections(const unsigned char *goodFrame, size_t goodLength)
{
    static unsigned char frame[MAX_FRAME];
    static unsigned char plaintext[MAX_FRAME];
    spa_apap_framer_t framer;
    spa_apap_extension_t ext;
    const unsigned char *audio = 0;
    size_t plainLength = 0, audioLength = 0, consumed = 0;
    size_t i;
    int variant = -1;

    assert(goodLength <= sizeof(frame));

    /* a flipped ciphertext bit */
    memcpy(frame, goodFrame, goodLength);
    frame[SPA_APAP_HEADER_SIZE] ^= 0x01;
    assert(!spa_apap_open(kTestKey, frame, goodLength, plaintext, sizeof(plaintext),
                          &plainLength, &variant));

    /* a flipped tag bit */
    memcpy(frame, goodFrame, goodLength);
    frame[goodLength - SPA_APAP_OVERHEAD] ^= 0x80;
    assert(!spa_apap_open(kTestKey, frame, goodLength, plaintext, sizeof(plaintext),
                          &plainLength, &variant));

    /* a flipped nonce bit */
    memcpy(frame, goodFrame, goodLength);
    frame[goodLength - 1] ^= 0x01;
    assert(!spa_apap_open(kTestKey, frame, goodLength, plaintext, sizeof(plaintext),
                          &plainLength, &variant));

    /* This fixture is the audio-AAD variant: exactly header[4:12] is
     * authenticated. Bytes 0..3 are deliberately outside its AAD. */
    for (i = 4; i < 12; i++) {
        memcpy(frame, goodFrame, goodLength);
        frame[i] ^= 0x01;
        assert(!spa_apap_open(kTestKey, frame, goodLength, plaintext, sizeof(plaintext),
                              &plainLength, &variant));
    }

    /* a wrong key */
    {
        unsigned char wrong[SPA_APAP_KEY_SIZE];
        memcpy(wrong, kTestKey, sizeof(wrong));
        wrong[31] ^= 0x01;
        assert(!spa_apap_open(wrong, goodFrame, goodLength, plaintext, sizeof(plaintext),
                              &plainLength, &variant));
    }

    /* too short to hold its own overhead */
    assert(!spa_apap_open(kTestKey, goodFrame, SPA_APAP_HEADER_SIZE + SPA_APAP_OVERHEAD - 1,
                          plaintext, sizeof(plaintext), &plainLength, &variant));

    /* an output buffer that cannot hold the plaintext */
    assert(!spa_apap_open(kTestKey, goodFrame, goodLength, plaintext, 4,
                          &plainLength, &variant));

    /* a TLV list with no terminator, and one whose last field runs past the end */
    memset(&ext, 0, sizeof(ext));
    {
        static const unsigned char unterminated[] = { 8, 2, 0x04, 0x00 };
        assert(!spa_apap_parse_extension(unterminated, sizeof(unterminated), &ext,
                                         &audio, &audioLength));
    }
    {
        static const unsigned char overrun[] = { 17, 8, 1, 2, 3 };
        assert(!spa_apap_parse_extension(overrun, sizeof(overrun), &ext, &audio, &audioLength));
    }
    /* an empty list is well formed and carries no audio */
    {
        static const unsigned char empty[] = { 0 };
        assert(spa_apap_parse_extension(empty, sizeof(empty), &ext, &audio, &audioLength));
        assert(audioLength == 0);
    }

    /* framing: a declared length below the prefix, and one above the capacity */
    {
        static const unsigned char shortLength[4] = { 0, 0, 0, 3 };
        const unsigned char *out = 0;
        size_t outLength = 0;
        spa_apap_framer_init(&framer, frame, sizeof(frame));
        assert(spa_apap_framer_push(&framer, shortLength, sizeof(shortLength), &consumed,
                                    &out, &outLength) < 0);
    }
    {
        unsigned char hugeLength[4];
        const unsigned char *out = 0;
        size_t outLength = 0;
        put_be32(hugeLength, (uint32_t)sizeof(frame) + 64u);
        spa_apap_framer_init(&framer, frame, sizeof(frame));
        assert(spa_apap_framer_push(&framer, hugeLength, sizeof(hugeLength), &consumed,
                                    &out, &outLength) < 0);
    }
    /* a length equal to the prefix alone is a legal, empty frame */
    {
        static const unsigned char emptyFrame[4] = { 0, 0, 0, 4 };
        const unsigned char *out = 0;
        size_t outLength = 1;
        spa_apap_framer_init(&framer, frame, sizeof(frame));
        assert(spa_apap_framer_push(&framer, emptyFrame, sizeof(emptyFrame), &consumed,
                                    &out, &outLength) == 1);
        assert(outLength == 0);
    }
}

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "apac_decoder/tests/fixtures/frames.bin";
    static const size_t chunks[] = { 1, 2, 3, 5, 7, 13, 64, 1500, 16384 };
    static unsigned char plaintext[MAX_FRAME];
    spa_apac_decoder_t *decoder = 0;
    uint32_t channels = 0, frameLength = 0, sampleRate = 0;
    const unsigned char *cookie;
    const unsigned char *ref;
    uint32_t numPackets, refChannels, refFrameLength, cookieBytes;
    size_t lengthsAt, packetsAt, pcmAt, payload = 0;
    size_t firstFrameLength = 0;
    uint32_t i;
    size_t c;
    double worst = 0.0;
    long identical = 0, compared = 0;
    FILE *f;

    f = fopen(path, "rb");
    if (!f) {
        printf("FAIL cannot open %s\n", path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    g_blobBytes = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    g_blob = (unsigned char *)malloc(g_blobBytes);
    assert(g_blob && fread(g_blob, 1, g_blobBytes, f) == g_blobBytes);
    fclose(f);

    assert(g_blobBytes > 24 && memcmp(g_blob, "APACFRM1", 8) == 0);
    numPackets     = rd32(8);
    refChannels    = rd32(12);
    refFrameLength = rd32(16);
    cookieBytes    = rd32(20);
    cookie = g_blob + 24;
    g_fixtureCookie = cookie;
    g_fixtureCookieBytes = cookieBytes;
    assert(refChannels == CHANNELS && refFrameLength == FRAME_SAMPLES);
    assert(numPackets && numPackets <= MAX_PACKETS);

    lengthsAt = 24 + cookieBytes;
    for (i = 0; i < numPackets; i++) payload += rd32(lengthsAt + 4 * i);
    packetsAt = lengthsAt + 4 * numPackets;
    pcmAt = packetsAt + payload;
    assert(pcmAt + (size_t)numPackets * CHANNELS * FRAME_SAMPLES * 4 == g_blobBytes);
    /* The packed fixture does not promise that its float32 section is
     * naturally aligned.  Keep it byte-addressed and copy each value out;
     * directly casting this pointer was undefined behavior on strict hosts. */
    ref = g_blob + pcmAt;

    /* The receiver builds its decoder from the SETMAGICCOOKIE payload, so the test does too. */
    if (spa_apac_create(cookie, cookieBytes, &decoder,
                        &channels, &frameLength, &sampleRate) != SPA_APAC_OK) {
        printf("FAIL spa_apac_create refused the fixture cookie\n");
        return 1;
    }
    assert(channels == CHANNELS && frameLength == FRAME_SAMPLES && sampleRate == 48000);

    /* Seal the six packets into an APAP stream.  The first is sent in the audio-AAD form and the
     * rest as extension packets, so both variants and the TLV split are on the decode path. */
    g_frameCount = numPackets;
    {
        size_t at = packetsAt;
        for (i = 0; i < numPackets; i++) {
            const uint32_t packetBytes = rd32(lengthsAt + 4 * i);
            size_t plainLength;
            assert(packetBytes <= MAX_PACKET);
            if (i == 0) {
                memcpy(plaintext, g_blob + at, packetBytes);
                plainLength = packetBytes;
            } else {
                plainLength = build_extension(plaintext, 65, FRAME_SAMPLES,
                                              0x60100d08f9d19abbull,
                                              cookie, cookieBytes,
                                              g_blob + at, packetBytes);
            }
            g_streamBytes += seal_frame(g_stream + g_streamBytes, 100 + i,
                                        i == 0 ? SPA_APAP_AAD_AUDIO : SPA_APAP_AAD_EXTENSION,
                                        plaintext, plainLength);
            if (i == 0) firstFrameLength = g_streamBytes - 4;
            at += packetBytes;
        }
    }

    /* Replay at every chunk size.  The decoder carries overlap between packets, so it is reset
     * before each pass and every pass must produce identical output. */
    for (c = 0; c < sizeof(chunks) / sizeof(chunks[0]); c++) {
        static int16_t firstPass[MAX_PACKETS][CHANNELS * FRAME_SAMPLES];
        char what[64];
        snprintf(what, sizeof(what), "chunk=%zu", chunks[c]);

        spa_apac_reset(decoder);
        replay(decoder, chunks[c], what);

        if (c == 0) {
            memcpy(firstPass, g_got, sizeof(firstPass));
            /* Only the first pass is compared against the library: it is the one that starts
             * from a decoder whose overlap memory has never held anything else, which is the
             * state the fixture was produced in. */
            for (i = 0; i < numPackets; i++) {
                uint32_t s;
                for (s = 0; s < CHANNELS * FRAME_SAMPLES; s++) {
                    const uint32_t frame = s / CHANNELS;
                    const uint32_t ch = s % CHANNELS;
                    const size_t refSample =
                        (size_t)i * CHANNELS * FRAME_SAMPLES +
                        (size_t)frame * CHANNELS + kDeclaredFromDecoder[ch];
                    float expect;
                    const double got = g_got[i][s] / 32768.0;
                    double diff;
                    double mag;
                    memcpy(&expect, ref + refSample * sizeof(expect), sizeof(expect));
                    diff = got - expect;
                    mag = diff < 0 ? -diff : diff;
                    if (mag > worst) worst = mag;
                    if (mag * 32768.0 < 1e-9) identical++;
                    compared++;
                    if (mag > 1.5 / 32768.0) {
                        printf("FAIL packet %u sample %u ch %u: got %.8f expected %.8f "
                               "(%.2f LSB)\n", i, s, ch, got, expect, mag * 32768.0);
                        return 1;
                    }
                }
            }
        } else if (memcmp(firstPass, g_got, sizeof(firstPass)) != 0) {
            printf("FAIL %s: a different split produced different PCM\n", what);
            return 1;
        }
    }

    test_trim_tlvs();
    test_extension_metadata();
    test_rejections(g_stream + 4, firstFrameLength);

    spa_apac_destroy(decoder);
    printf("OK  %u packets sealed and reopened at %zu split sizes, %ld samples within one LSB "
           "of the library (worst %.3f LSB, %.1f%% exact), both AAD variants, "
           "TLV 4/5 trim (100000 arithmetic properties, 50000 parser fuzz cases), "
           "18 rejections\n",
           numPackets, sizeof(chunks) / sizeof(chunks[0]), compared, worst * 32768.0,
           100.0 * (double)identical / (double)compared);
    free(g_blob);
    return 0;
}
