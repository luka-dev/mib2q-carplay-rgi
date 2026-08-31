/*
 * End-to-end check of the type-103 receive path, offline.
 *
 * This is the one test that runs the hook's own chain rather than the decoder
 * in isolation: framer -> spa_apap_open -> spa_apap_parse_extension ->
 * spa_apac_decode_s16 -> spa_reorder_5_1_decoder_to_declared_s16.  Everything
 * below it already has a differential test against Apple's decoder; what is
 * unproven until here is the plumbing between them.
 *
 * It needs a live session capture, which carries a session key and is therefore
 * never committed.  With no capture present the test SKIPS rather than fails,
 * so it is safe to run anywhere; point it elsewhere with APAC_CAPTURE_DIR.
 *
 * Build (from the repository root):
 *
 *   c++ -std=c++11 -O2 -fno-exceptions -fno-rtti -Iapac_decoder/include -Ihook \
 *       -Ihook/audio tests/apac_integration_test.c hook/audio/apap_crypto.c \
 *       hook/audio/spatial_downmix.c hook/audio/apac_bridge.cpp \
 *       apac_decoder/src/*.cpp -o /tmp/apac_integration_test
 */
#include "apac_bridge.h"
#include "apap_crypto.h"
#include "audio/spatial_downmix.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLAINTEXT   (1u << 20)
#define MAX_SAMPLES     (8u * 1024u)

static const char *capture_dir(void)
{
    const char *env = getenv("APAC_CAPTURE_DIR");
    return env && env[0] ? env : "captures/apac_5_1_live_20260831_0205";
}

static unsigned char *slurp(const char *dir, const char *name, size_t *lengthOut)
{
    char path[1024];
    unsigned char *buffer;
    long length;
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length <= 0) { fclose(f); return 0; }
    buffer = (unsigned char *)malloc((size_t)length);
    if (!buffer) { fclose(f); return 0; }
    if (fread(buffer, 1, (size_t)length, f) != (size_t)length) {
        free(buffer);
        fclose(f);
        return 0;
    }
    fclose(f);
    *lengthOut = (size_t)length;
    return buffer;
}

/* Channel 5 of the decoder's output is the LFE — measured on 74.6 s of this
 * capture and declared by the stream's own layout tag 0x007c0006.  The rail
 * below the reorder expects it in slot 3.  Comparing the two orderings on real
 * decoded audio is what catches a sink rewired to the wrong permutation, which
 * no amount of decoder testing would. */
static void check_reorder_moves_the_lfe(const int16_t *decoded, uint32_t frames)
{
    int16_t *permuted = (int16_t *)malloc((size_t)frames * 6 * sizeof(int16_t));
    uint32_t i;
    int moved = 0;

    assert(permuted);
    assert(spa_reorder_5_1_decoder_to_declared_s16(decoded, frames, permuted) ==
           (size_t)frames * 6);

    for (i = 0; i < frames; i++) {
        assert(permuted[i * 6 + 3] == decoded[i * 6 + 5]);   /* LFE  5 -> 3 */
        assert(permuted[i * 6 + 2] == decoded[i * 6 + 0]);   /* C    0 -> 2 */
        assert(permuted[i * 6 + 0] == decoded[i * 6 + 1]);   /* L    1 -> 0 */
        assert(permuted[i * 6 + 1] == decoded[i * 6 + 2]);   /* R    2 -> 1 */
        assert(permuted[i * 6 + 4] == decoded[i * 6 + 3]);   /* Ls   3 -> 4 */
        assert(permuted[i * 6 + 5] == decoded[i * 6 + 4]);   /* Rs   4 -> 5 */
        if (decoded[i * 6 + 5] != decoded[i * 6 + 3]) moved = 1;
    }
    /* A permutation asserted only on silence proves nothing. */
    assert(moved);
    free(permuted);
}

int main(void)
{
    spa_apac_codec_api_t codecApi;
    const char *dir = capture_dir();
    unsigned char *key, *stream, *plaintext, *frameBuffer;
    size_t keyLength = 0, streamLength = 0, offset = 0;
    spa_apap_framer_t framer;
    spa_apac_decoder_t *decoder = 0;
    uint32_t channels = 0, frameLength = 0, sampleRate = 0;
    unsigned long frames = 0, opened = 0, audioPackets = 0, decodedPackets = 0;
    unsigned long matched = 0, aadAudio = 0, aadExtension = 0;
    int16_t *decoded;

    memset(&codecApi, 0, sizeof(codecApi));
    assert(spa_apac_codec_get_api(SPA_APAC_CODEC_ABI_VERSION, &codecApi,
                                  sizeof(codecApi)) == SPA_APAC_OK);
    assert(codecApi.structSize == sizeof(codecApi));
    assert(codecApi.abiVersion == SPA_APAC_CODEC_ABI_VERSION);
    assert(codecApi.create && codecApi.decodeS16 && codecApi.reset &&
           codecApi.destroy && codecApi.getInfo);

    key = slurp(dir, "shk.bin", &keyLength);
    stream = slurp(dir, "spatial_apap_5.bin", &streamLength);
    if (!key || !stream || keyLength != SPA_APAP_KEY_SIZE) {
        free(key);
        free(stream);
        printf("SKIP no capture under %s\n", dir);
        return 0;
    }

    plaintext = (unsigned char *)malloc(MAX_PLAINTEXT);
    decoded = (int16_t *)malloc(MAX_SAMPLES * sizeof(int16_t));
    frameBuffer = (unsigned char *)malloc(MAX_PLAINTEXT);
    assert(plaintext && decoded && frameBuffer);

    spa_apap_framer_init(&framer, frameBuffer, MAX_PLAINTEXT);

    while (offset < streamLength) {
        const unsigned char *frame = 0, *audio = 0;
        size_t consumed = 0, frameSize = 0, plaintextLength = 0, audioLength = 0;
        spa_apap_extension_t state;
        int aadVariant = -1;

        if (!spa_apap_framer_push(&framer, stream + offset, streamLength - offset,
                                  &consumed, &frame, &frameSize)) {
            offset += consumed;
            if (!consumed) break;          /* the truncated tail of the capture */
            continue;
        }
        offset += consumed;
        frames++;

        /* Every frame of this capture must authenticate.  A single failure means
         * the key, the AAD split or the body layout is wrong. */
        assert(spa_apap_open(key, frame, frameSize, plaintext, MAX_PLAINTEXT,
                             &plaintextLength, &aadVariant));
        opened++;
        if (aadVariant == SPA_APAP_AAD_AUDIO) aadAudio++;
        else aadExtension++;

        memset(&state, 0, sizeof(state));
        if (!spa_apap_parse_extension(plaintext, plaintextLength, &state,
                                      &audio, &audioLength) || !audioLength)
            continue;
        audioPackets++;

        /* The APAC packet the hook extracts must be byte-identical to the one
         * Apple's own decoder was driven with when this port was accepted.
         * tools/pioneer_apap_decrypt.py produced those independently of any code
         * under test here.
         *
         * Indexed by FRAME, not by audio packet: this capture has 3980 frames
         * but 3977 APAC packets, because three frames carry TLV state and no
         * audio at all.  That is also where the 3977 in every acceptance run
         * comes from. */
        {
            char name[64];
            size_t referenceLength = 0;
            unsigned char *reference;

            snprintf(name, sizeof(name), "apac_packets/p%05lu.bin", frames - 1);
            reference = slurp(dir, name, &referenceLength);
            if (reference) {
                assert(referenceLength == audioLength);
                assert(memcmp(reference, audio, audioLength) == 0);
                free(reference);
                matched++;
            }
        }

        if (!decoder) {
            size_t cookieLength = 0;
            unsigned char *cookie = slurp(dir,
                "spatial_magiccookie_5_60100d08f9d19abb.bin", &cookieLength);
            assert(cookie);
            assert(spa_apac_create(cookie, cookieLength, &decoder,
                                   &channels, &frameLength, &sampleRate) == SPA_APAC_OK);
            free(cookie);
            assert(channels == 6);
            assert(frameLength == 1024);
            assert(sampleRate == 48000);
        }

        if (decodedPackets < 64) {
            uint32_t got = 0;
            assert(spa_apac_decode_s16(decoder, audio, audioLength,
                                       decoded, MAX_SAMPLES, &got) == SPA_APAC_OK);
            assert(got == frameLength);
            decodedPackets++;
            if (decodedPackets == 64) check_reorder_moves_the_lfe(decoded, got);
        }
    }

    assert(frames > 3000);
    assert(opened == frames);
    assert(audioPackets > 3000);
    assert(matched > 3000);
    assert(decodedPackets == 64);

    spa_apac_destroy(decoder);
    free(plaintext);
    free(frameBuffer);
    free(decoded);
    free(stream);
    memset(key, 0, keyLength);
    free(key);

    printf("OK  %lu frames all authenticated (aad audio=%lu extension=%lu), "
           "%lu APAC packets, %lu byte-identical to the accepted reference, "
           "%lu decoded to %u ch x %u frames, reorder checked on real audio\n",
           frames, aadAudio, aadExtension, audioPackets, matched,
           decodedPackets, channels, frameLength);
    return 0;
}
