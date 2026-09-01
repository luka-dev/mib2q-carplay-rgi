/* Play a raw interleaved S16 file straight into an mpl5_dio_ent-style QSA device.
 *
 * Purpose: split "the head unit's 6-voice path is broken" from "the hook feeds it
 * badly".  CarPlay, stock libairplay, the hook, the APAC ring and the type-100
 * cadence are all out of the picture here -- this opens the device itself, sets
 * the same geometry the hook negotiates, and writes a file at its own pace.
 * It also reports the achieved frame rate, so the clock is measured rather than
 * inferred from how it sounds.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/asoundlib.h>

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

int main(int argc, char **argv)
{
    const char *path   = argc > 1 ? argv[1] : 0;
    int voices         = argc > 2 ? atoi(argv[2]) : 6;
    int rate           = argc > 3 ? atoi(argv[3]) : 48000;
    const char *pcmName= argc > 4 ? argv[4] : "ent1p";      /* /dev/snd node */
    const char *devName= argc > 5 ? argv[5] : "";           /* sw mixer subchannel, optional */
    snd_pcm_t *pcm = 0;
    snd_pcm_channel_params_t params;
    snd_pcm_channel_setup_t setup;
    FILE *f;
    unsigned char *buf;
    size_t frameBytes, chunkFrames, chunkBytes, totalFrames = 0;
    uint64_t t0, t1;
    int card = -1, dev = -1, rc;

    if (!path) {
        fprintf(stderr, "usage: qsa_play file.s16 [voices] [rate] [device]\n");
        return 2;
    }
    /* "mpl5_dio_ent" is a software-mixer SUBCHANNEL name, not a PCM device name:
     * it belongs in params.sw_mixer_subchn_name.  Open the card's preferred
     * playback device and route to that subchannel, which is how the stock
     * CarPlay path reaches it. */
    rc = snd_pcm_open_name(&pcm, (char *)pcmName, SND_PCM_OPEN_PLAYBACK);
    if (rc < 0) {
        fprintf(stderr, "open '%s' failed: %s\n", pcmName, snd_strerror(rc));
        return 1;
    }
    printf("opened '%s'%s%s\n", pcmName,
           devName[0] ? ", subchannel " : "", devName[0] ? devName : "");
    (void)card; (void)dev;

    memset(&params, 0, sizeof(params));
    params.channel = SND_PCM_CHANNEL_PLAYBACK;
    params.mode = SND_PCM_MODE_BLOCK;
    params.start_mode = SND_PCM_START_FULL;
    params.stop_mode = SND_PCM_STOP_STOP;
    params.format.interleave = 1;
    params.format.format = SND_PCM_SFMT_S16_LE;
    params.format.rate = rate;
    params.format.voices = voices;
    params.buf.block.frag_size = voices * 2 * (rate / 1000) * 16;  /* 16 ms */
    params.buf.block.frags_min = 1;
    params.buf.block.frags_max = 8;
    if (devName[0])
        strncpy(params.sw_mixer_subchn_name, devName,
                sizeof(params.sw_mixer_subchn_name) - 1);

    rc = snd_pcm_plugin_params(pcm, &params);
    if (rc < 0) {
        fprintf(stderr, "params failed: %s (why_failed=%u)\n",
                snd_strerror(rc), (unsigned)params.why_failed);
        return 1;
    }
    printf("params ok: %d voices, %d Hz, frag request %d\n",
           voices, rate, params.buf.block.frag_size);

    if ((rc = snd_pcm_plugin_prepare(pcm, SND_PCM_CHANNEL_PLAYBACK)) < 0) {
        fprintf(stderr, "prepare failed: %s\n", snd_strerror(rc));
        return 1;
    }
    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    if (snd_pcm_plugin_setup(pcm, &setup) == 0) {
        printf("device: %d voices, %d Hz, frag %d, frags %d\n",
               setup.format.voices, setup.format.rate,
               setup.buf.block.frag_size, setup.buf.block.frags);
    }

    frameBytes = (size_t)voices * 2u;
    chunkFrames = (size_t)(rate / 1000) * 16u;            /* 16 ms */
    chunkBytes = chunkFrames * frameBytes;
    buf = (unsigned char *)malloc(chunkBytes);
    f = fopen(path, "rb");
    if (!buf || !f) { fprintf(stderr, "cannot read %s\n", path); return 1; }

    t0 = now_ms();
    for (;;) {
        size_t got = fread(buf, 1, chunkBytes, f);
        ssize_t w;
        if (got < chunkBytes) {
            if (got == 0) break;
            memset(buf + got, 0, chunkBytes - got);
        }
        w = snd_pcm_plugin_write(pcm, buf, chunkBytes);
        if (w < (ssize_t)chunkBytes) {
            fprintf(stderr, "short write %d of %u\n", (int)w, (unsigned)chunkBytes);
            if (w <= 0) break;
        }
        totalFrames += chunkFrames;
        if ((totalFrames % (chunkFrames * 62u)) == 0) {
            uint64_t ms = now_ms() - t0;
            printf("  %6u frames in %6llu ms -> %7.0f frames/s\n",
                   (unsigned)totalFrames, (unsigned long long)ms,
                   ms ? (double)totalFrames * 1000.0 / (double)ms : 0.0);
            fflush(stdout);
        }
    }
    t1 = now_ms();
    printf("TOTAL %u frames in %llu ms -> %.0f frames/s (expected %d)\n",
           (unsigned)totalFrames, (unsigned long long)(t1 - t0),
           (t1 > t0) ? (double)totalFrames * 1000.0 / (double)(t1 - t0) : 0.0, rate);
    snd_pcm_plugin_flush(pcm, SND_PCM_CHANNEL_PLAYBACK);
    snd_pcm_close(pcm);
    fclose(f);
    free(buf);
    return 0;
}
