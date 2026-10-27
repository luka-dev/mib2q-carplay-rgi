/* Decode a whole live APAP capture and describe what is actually in each of the
 * six channels.  The integration test only inspects one packet; a car complaint
 * about voices in the wrong place needs the statistics of the whole stream, plus
 * a file you can listen to. */
#include "apac_bridge.h"
#include "apap_crypto.h"
#include "spatial_downmix.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLAINTEXT (1u << 20)
#define MAX_SAMPLES   (8u * 1024u)

static unsigned char *slurp(const char *path, size_t *lengthOut)
{
    FILE *f = fopen(path, "rb");
    long n; unsigned char *b;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    b = (unsigned char *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return 0; }
    fclose(f); *lengthOut = (size_t)n; return b;
}

int main(int argc, char **argv)
{
    const char *streamPath = argc > 1 ? argv[1] : 0;
    const char *keyPath    = argc > 2 ? argv[2] : 0;
    const char *cookiePath = argc > 3 ? argv[3] : 0;
    const char *outPath    = argc > 4 ? argv[4] : 0;
    unsigned char *key, *stream, *plaintext, *frameBuffer, *cookie;
    size_t keyLength = 0, streamLength = 0, cookieLength = 0, offset = 0;
    spa_apap_framer_t framer;
    spa_apac_decoder_t *decoder = 0;
    uint32_t channels = 0, frameLength = 0, sampleRate = 0;
    unsigned long frames = 0, audioPackets = 0, decodedPackets = 0;
    int16_t *decoded;
    double sum[8] = {0}; long long peak[8] = {0}; unsigned long long total = 0;
    FILE *out = 0;
    int c;

    if (!streamPath || !keyPath || !cookiePath) {
        fprintf(stderr, "usage: apac_probe stream.bin shk.bin cookie.bin [out.pcm]\n");
        return 2;
    }
    key = slurp(keyPath, &keyLength);
    stream = slurp(streamPath, &streamLength);
    cookie = slurp(cookiePath, &cookieLength);
    if (!key || !stream || !cookie || keyLength != SPA_APAP_KEY_SIZE) {
        fprintf(stderr, "missing or bad inputs\n");
        return 1;
    }
    if (outPath) out = fopen(outPath, "wb");

    plaintext = (unsigned char *)malloc(MAX_PLAINTEXT);
    decoded = (int16_t *)malloc(MAX_SAMPLES * sizeof(int16_t));
    frameBuffer = (unsigned char *)malloc(MAX_PLAINTEXT);
    spa_apap_framer_init(&framer, frameBuffer, MAX_PLAINTEXT);

    while (offset < streamLength) {
        const unsigned char *frame = 0, *audio = 0;
        size_t consumed = 0, frameSize = 0, plaintextLength = 0, audioLength = 0;
        spa_apap_extension_t state;
        int aadVariant = -1;
        uint32_t got = 0, i;

        if (!spa_apap_framer_push(&framer, stream + offset, streamLength - offset,
                                  &consumed, &frame, &frameSize)) {
            offset += consumed;
            if (!consumed) break;
            continue;
        }
        offset += consumed;
        frames++;
        if (!spa_apap_open(key, frame, frameSize, plaintext, MAX_PLAINTEXT,
                           &plaintextLength, &aadVariant)) {
            fprintf(stderr, "frame %lu FAILED to authenticate\n", frames);
            continue;
        }
        memset(&state, 0, sizeof(state));
        if (!spa_apap_parse_extension(plaintext, plaintextLength, &state,
                                      &audio, &audioLength) || !audioLength)
            continue;
        audioPackets++;

        if (!decoder) {
            if (spa_apac_create(cookie, cookieLength, &decoder, &channels,
                                &frameLength, &sampleRate) != SPA_APAC_OK) {
                fprintf(stderr, "decoder create FAILED\n");
                return 1;
            }
            printf("decoder: %u ch, %u frames/packet, %u Hz\n",
                   channels, frameLength, sampleRate);
        }
        if (spa_apac_decode_s16(decoder, audio, audioLength, decoded,
                                MAX_SAMPLES, &got) != SPA_APAC_OK) {
            fprintf(stderr, "packet %lu decode FAILED\n", audioPackets);
            continue;
        }
        decodedPackets++;
        for (i = 0; i < got; i++) {
            for (c = 0; c < (int)channels; c++) {
                long long v = decoded[i * channels + c];
                sum[c] += (double)v * (double)v;
                if (llabs(v) > peak[c]) peak[c] = llabs(v);
            }
        }
        total += got;
        if (out) fwrite(decoded, sizeof(int16_t), (size_t)got * channels, out);
    }
    if (out) fclose(out);

    printf("frames=%lu audioPackets=%lu decoded=%lu totalFrames=%llu (%.2f s)\n",
           frames, audioPackets, decodedPackets, total,
           sampleRate ? (double)total / sampleRate : 0.0);
    printf("\n  ch  decoder-order   RMS      peak   dBFS\n");
    {
        static const char *names[6] = { "C", "L", "R", "Ls", "Rs", "LFE" };
        for (c = 0; c < (int)channels; c++) {
            double rms = total ? sqrt(sum[c] / (double)total) : 0.0;
            double db = rms > 0 ? 20.0 * log10(rms / 32768.0) : -999.0;
            printf("  %2d  %-13s %8.1f %7lld  %6.1f\n",
                   c, names[c], rms, peak[c], db);
        }
    }
    return 0;
}
