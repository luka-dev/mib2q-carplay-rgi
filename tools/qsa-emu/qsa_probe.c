/* Drive the emulated mpl5_dio_ent exactly the way dio_manager drives the real one.
 *
 * The point is the CONVERSION: dio opens the device as S16 and lets QSA widen to
 * the device's native container, and that widening at six voices is the last
 * unexamined layer.  So this uses the plugin entry points (snd_pcm_plugin_params
 * / snd_pcm_plugin_write), not the raw channel ones -- the raw path would skip
 * the very code under suspicion.
 *
 * The open sequence follows retroarch-qnx/src/audio/drivers/qnx_qsa.c, which is
 * tuned on this same head unit: disable MMAP first, then ask the device for its
 * own geometry instead of asserting one.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/asoundlib.h>

static void dump_info(const snd_pcm_channel_info_t *ci)
{
    printf("  device geometry: voices %d..%d, rate %d..%d, frag %d..%d, formats %#x\n",
           ci->min_voices, ci->max_voices, ci->min_rate, ci->max_rate,
           ci->min_fragment_size, ci->max_fragment_size, (unsigned)ci->formats);
}

static int try_device(int card, int dev, int voices, int rate, int seconds)
{
    snd_pcm_t *pcm = 0;
    snd_pcm_channel_info_t info;
    snd_pcm_channel_params_t params;
    snd_pcm_channel_setup_t setup;
    snd_pcm_info_t pinfo;
    int rc, i, f, frag_frames, total;
    short *buf;

    rc = snd_pcm_open(&pcm, card, dev, SND_PCM_OPEN_PLAYBACK);
    if (rc < 0) return -1;

    memset(&pinfo, 0, sizeof(pinfo));
    if (snd_pcm_info(pcm, &pinfo) == 0)
        printf("card %d dev %d: '%s'\n", card, dev, pinfo.name);
    else
        printf("card %d dev %d: (no info)\n", card, dev);

    if (voices == 0) { snd_pcm_close(pcm); return 0; }   /* enumeration only */

    snd_pcm_plugin_set_disable(pcm, PLUGIN_DISABLE_MMAP);
    memset(&info, 0, sizeof(info));
    info.channel = SND_PCM_CHANNEL_PLAYBACK;
    if (snd_pcm_channel_info(pcm, &info) == 0) dump_info(&info);

    memset(&params, 0, sizeof(params));
    params.channel             = SND_PCM_CHANNEL_PLAYBACK;
    params.mode                = SND_PCM_MODE_BLOCK;
    params.start_mode          = SND_PCM_START_DATA;
    params.stop_mode           = SND_PCM_STOP_STOP;
    params.format.interleave   = 1;
    params.format.format       = SND_PCM_SFMT_S16_LE;
    params.format.rate         = rate;
    params.format.voices       = voices;
    params.buf.block.frag_size = voices * 2 * (rate / 1000) * 16;
    params.buf.block.frags_min = 1;
    params.buf.block.frags_max = 8;

    rc = snd_pcm_plugin_params(pcm, &params);
    printf("  plugin_params(S16,%dch,%dHz,frag=%d) -> %d (why=%#x)\n",
           voices, rate, params.buf.block.frag_size, rc, (unsigned)params.why_failed);
    if (rc < 0) { snd_pcm_close(pcm); return 1; }

    printf("  plugin_prepare -> %d\n", snd_pcm_plugin_prepare(pcm, SND_PCM_CHANNEL_PLAYBACK));
    memset(&setup, 0, sizeof(setup));
    setup.channel = SND_PCM_CHANNEL_PLAYBACK;
    if (snd_pcm_plugin_setup(pcm, &setup) == 0)
        printf("  setup readback: %d voices, %d Hz, format %d, frag %d, frags %d\n",
               setup.format.voices, setup.format.rate, setup.format.format,
               setup.buf.block.frag_size, setup.buf.block.frags);

    frag_frames = setup.buf.block.frag_size / (voices * 2);
    if (frag_frames <= 0) frag_frames = (rate / 1000) * 16;
    buf = (short *)malloc((size_t)frag_frames * voices * sizeof(short));
    for (f = 0; f < frag_frames; f++)
        for (i = 0; i < voices; i++)
            buf[f * voices + i] = (short)(0x1000 * (i + 1));

    total = (rate * seconds) / frag_frames;
    for (i = 0; i < total; i++) {
        ssize_t w = snd_pcm_plugin_write(pcm, buf, (size_t)frag_frames * voices * 2);
        if (w < 0) { printf("  write %d failed: %s\n", i, snd_strerror((int)w)); break; }
    }
    printf("  wrote %d fragments of %d frames\n", i, frag_frames);
    free(buf);
    snd_pcm_plugin_flush(pcm, SND_PCM_CHANNEL_PLAYBACK);
    snd_pcm_close(pcm);
    return 0;
}

int main(int argc, char **argv)
{
    int dev, target = argc > 1 ? atoi(argv[1]) : 3;   /* mpl5_dio_ent */

    printf("=== what does the library see? ===\n");
    printf("  snd_cards() = %d\n", snd_cards());
    {
        snd_pcm_t *p = 0;
        int card = -1, d = -1;
        int rc = snd_pcm_open_preferred(&p, &card, &d, SND_PCM_OPEN_PLAYBACK);
        printf("  open_preferred -> %d (card %d dev %d)\n", rc, card, d);
        if (rc == 0) snd_pcm_close(p);
    }
    printf("=== enumerating cards 0..1, devices 0..11 ===\n");
    for (dev = 0; dev < 12; dev++) { try_device(0, dev, 0, 0, 0); try_device(1, dev, 0, 0, 0); }

    if (target < 0) return 0;
    printf("=== six voices on device %d (the CarPlay configuration) ===\n", target);
    try_device(0, target, 6, 48000, 1);
    printf("=== two voices on device %d (the one that works on the car) ===\n", target);
    try_device(0, target, 2, 48000, 1);
    return 0;
}
