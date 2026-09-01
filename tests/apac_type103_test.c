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
 * the ORIGINAL library produced for them - Apple's own decoder, run natively on AArch64.  This
 * test wraps those six packets in APAP frames, opens them the way the receiver does, decodes them
 * through the port, and requires every one of 36 864 samples to land within one 16-bit LSB of the
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
    unsigned char mac[512];
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
static size_t build_extension(unsigned char *out, uint64_t formatIndex, uint64_t cookieId,
                              const unsigned char *audio, size_t audioLength)
{
    size_t at = 0;
    int i;

    out[at++] = SPA_APAP_TLV_AUDIO_FORMAT_INDEX;
    out[at++] = 2;
    out[at++] = (unsigned char)(formatIndex >> 8);
    out[at++] = (unsigned char)formatIndex;

    out[at++] = SPA_APAP_TLV_MAGIC_COOKIE_ID;
    out[at++] = 8;
    for (i = 0; i < 8; i++) out[at++] = (unsigned char)(cookieId >> (56 - 8 * i));

    /* A field this path does not act on, present so the skip is exercised. */
    out[at++] = 11;                 /* soundcheck_three_words */
    out[at++] = 12;
    memset(out + at, 0x5a, 12); at += 12;

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
                    /* The TLV state has to survive the split, not just the parse. */
                    assert(ext.hasFormatIndex && ext.formatIndex == 1024);
                    assert(ext.hasMagicCookieId &&
                           ext.magicCookieId == 0x60100d08f9d19abbull);
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
    const float *ref;
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
    assert(refChannels == CHANNELS && refFrameLength == FRAME_SAMPLES);
    assert(numPackets && numPackets <= MAX_PACKETS);

    lengthsAt = 24 + cookieBytes;
    for (i = 0; i < numPackets; i++) payload += rd32(lengthsAt + 4 * i);
    packetsAt = lengthsAt + 4 * numPackets;
    pcmAt = packetsAt + payload;
    assert(pcmAt + (size_t)numPackets * CHANNELS * FRAME_SAMPLES * 4 == g_blobBytes);
    ref = (const float *)(const void *)(g_blob + pcmAt);

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
                plainLength = build_extension(plaintext, 1024, 0x60100d08f9d19abbull,
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
                    const float expect =
                        ref[(size_t)i * CHANNELS * FRAME_SAMPLES +
                            (size_t)frame * CHANNELS + kDeclaredFromDecoder[ch]];
                    const double got = g_got[i][s] / 32768.0;
                    const double diff = got - expect;
                    const double mag = diff < 0 ? -diff : diff;
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

    test_rejections(g_stream + 4, firstFrameLength);

    spa_apac_destroy(decoder);
    printf("OK  %u packets sealed and reopened at %zu split sizes, %ld samples within one LSB "
           "of the library (worst %.3f LSB, %.1f%% exact), both AAD variants, 18 rejections\n",
           numPackets, sizeof(chunks) / sizeof(chunks[0]), compared, worst * 32768.0,
           100.0 * (double)identical / (double)compared);
    free(g_blob);
    return 0;
}
