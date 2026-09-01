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

static snd_pcm_t *g_bundle;      /* the mixer's outlet, kept open and read below */
static int g_bundle_frag;

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
    /* Real music, not the flat pattern.  A constant per channel is identical in
     * every frame, so it cannot catch a duplicated, dropped or reordered frame --
     * and a periodic tick is exactly that kind of fault.  One second of the car
     * capture, in the declared order the sink hands to QSA, has none of that
     * symmetry: any frame-level damage shows up as a mismatch. */
    {
        FILE *mf = fopen("/proc/boot/music6.s16", "rb");
        size_t want = (size_t)frag_frames * voices * sizeof(short);
        size_t got = 0;
        if (mf) { got = fread(buf, 1, want, mf); fclose(mf); }
        if (got != want) {
            printf("  no music file (%u of %u bytes) -- falling back to the flat pattern\n",
                   (unsigned)got, (unsigned)want);
            for (f = 0; f < frag_frames; f++)
                for (i = 0; i < voices; i++)
                    buf[f * voices + i] = (short)(0x1000 * (i + 1));
        } else {
            printf("  first fragment is real music: %u frames from the car capture\n",
                   (unsigned)frag_frames);
        }
    }

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
    printf("=== playback devices ===\n");
    for (dev = 0; dev < 32; dev++) try_device(0, dev, 0, 0, 0);
    /* Capture devices carry their own numbering, which is why the playback sweep
     * stops at 'sse' and never shows the bundle. */
    printf("=== capture devices ===\n");
    for (dev = 0; dev < 32; dev++) {
        snd_pcm_t *c = 0;
        snd_pcm_info_t ci;
        if (snd_pcm_open(&c, 0, dev, SND_PCM_OPEN_CAPTURE) < 0) continue;
        memset(&ci, 0, sizeof(ci));
        if (snd_pcm_info(c, &ci) == 0) printf("  cap dev %d: '%s'\n", dev, ci.name);
        snd_pcm_close(c);
    }

    if (target < 0) return 0;
    /* mpl5 plays into the software mixer; it is sse_bundle_1's CAPTURE timer that
     * pulls the mix out and hands it to CSD (the slog names the two timers
     * deva_timer_play_mpl5_dio_ent and deva_timer_cap_sse_bundle_1).  On the head
     * unit the audio manager keeps that side running.  Nothing does here, so the
     * played audio has nowhere to go -- open the bundle for capture first. */
    if (argc > 2) {
        int bundle = atoi(argv[2]);
        snd_pcm_t *cap = 0;
        int rc = snd_pcm_open(&cap, 0, bundle, SND_PCM_OPEN_CAPTURE);
        printf("=== opening device %d for capture (the mixer's outlet) -> %d ===\n",
               bundle, rc);
        if (rc == 0) {
            snd_pcm_channel_info_t bi;
            snd_pcm_channel_params_t cp;
            /* Ask the bundle for its own geometry instead of guessing one: it is
             * the mixer's outlet, not an ordinary stereo capture, and a guessed
             * format is simply refused. */
            memset(&bi, 0, sizeof(bi));
            bi.channel = SND_PCM_CHANNEL_CAPTURE;
            if (snd_pcm_channel_info(cap, &bi) == 0) {
                printf("  bundle geometry: voices %d..%d, rate %d..%d, frag %d..%d, formats %#x\n",
                       bi.min_voices, bi.max_voices, bi.min_rate, bi.max_rate,
                       bi.min_fragment_size, bi.max_fragment_size, (unsigned)bi.formats);
                memset(&cp, 0, sizeof(cp));
                cp.channel = SND_PCM_CHANNEL_CAPTURE;
                cp.mode = SND_PCM_MODE_BLOCK;
                cp.start_mode = SND_PCM_START_DATA;
                cp.stop_mode = SND_PCM_STOP_STOP;
                cp.format.interleave = 1;
                cp.format.format = SND_PCM_SFMT_S16_LE;
                cp.format.rate = bi.min_rate ? bi.min_rate : 48000;
                cp.format.voices = bi.min_voices ? bi.min_voices : 2;
                cp.buf.block.frag_size = bi.min_fragment_size;
                cp.buf.block.frags_min = 1;
                cp.buf.block.frags_max = 8;
                /* The requested geometry already equals the device's own, so there
                 * is nothing for the plugin layer to convert -- and it refuses the
                 * open anyway (ENOENT, why_failed 0). Take the raw channel path,
                 * which is what the RetroArch QNX driver uses on this unit. */
                rc = snd_pcm_plugin_params(cap, &cp);
                printf("  capture plugin_params -> %d (why=%#x)\n", rc, (unsigned)cp.why_failed);
                if (rc < 0) {
                    snd_pcm_plugin_set_disable(cap, PLUGIN_DISABLE_MMAP);
                    cp.why_failed = 0;
                    rc = snd_pcm_channel_params(cap, &cp);
                    printf("  capture channel_params -> %d (why=%#x)\n",
                           rc, (unsigned)cp.why_failed);
                    if (rc == 0)
                        printf("  capture channel_prepare -> %d\n",
                               snd_pcm_channel_prepare(cap, SND_PCM_CHANNEL_CAPTURE));
                } else {
                    printf("  capture prepare -> %d\n",
                           snd_pcm_plugin_prepare(cap, SND_PCM_CHANNEL_CAPTURE));
                }
                if (rc == 0) {
                    g_bundle = cap;
                    g_bundle_frag = cp.buf.block.frag_size;
                    /* Blocking reads hang the whole probe when the capture side
                     * never produces, and that also costs the slog: sloginfo runs
                     * after this program returns. Non-blocking keeps the driver's
                     * own account of what happened. */
                    printf("  capture nonblock -> %d\n", snd_pcm_nonblock_mode(cap, 1));
                    printf("  capture go -> %d\n",
                           snd_pcm_channel_go(cap, SND_PCM_CHANNEL_CAPTURE));
                }
                cap = 0;   /* deliberately not closed: releasing it stops the outlet */
            }
        }
    }

    printf("=== six voices on device %d (the CarPlay configuration) ===\n", target);
    try_device(0, target, 6, 48000, 1);
    /* A capture channel with START_DATA begins on the first read, so the mix only
     * moves once somebody pulls it.  This is the step that makes the played audio
     * travel mpl5 -> mixer -> bundle -> CSD, where the stub can see it. */
    if (g_bundle && g_bundle_frag > 0) {
        char *rb = (char *)malloc((size_t)g_bundle_frag);
        int k, ok = 0;
        printf("=== draining the mixer outlet (%d-byte fragments) ===\n", g_bundle_frag);
        for (k = 0; k < 200 && rb; k++) {
            ssize_t r = snd_pcm_read(g_bundle, rb, (size_t)g_bundle_frag);
            if (r > 0) { ok++; continue; }
            if (k < 3 || (k % 50) == 0) printf("  read %d -> %d\n", k, (int)r);
        }
        printf("  drained %d fragments\n", ok);
        free(rb);
    }

    printf("=== two voices on device %d (the one that works on the car) ===\n", target);
    try_device(0, target, 2, 48000, 1);
    return 0;
}
