/* Capture both sides of QSA's conversion, inside the client process.
 *
 * snd_pcm_plugin_write runs the plugin chain and hands the result to the device
 * through snd_pcm_write_internal, and libasound reaches that through its own PLT,
 * so interposing it sees the bytes AFTER conversion.  Preloading this into the
 * probe therefore answers the question the head unit could not: what does QSA's
 * S16 widening actually produce at six voices.
 *
 * Deliberately not the shipping hook -- that one keys off snd_pcm_open_name and
 * carries a lot of CarPlay machinery irrelevant here.  This is the same seam,
 * nothing else.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/asoundlib.h>

static void preview(const char *tag, const void *buf, size_t len, int words)
{
    const unsigned char *b = (const unsigned char *)buf;
    size_t n = len < (size_t)(words * 4) ? len : (size_t)(words * 4);
    size_t i;
    fprintf(stderr, "TAP %s len=%u:", tag, (unsigned)len);
    for (i = 0; i < n; i++) {
        if ((i & 3) == 0) fprintf(stderr, " ");
        fprintf(stderr, "%02x", b[i]);
    }
    fprintf(stderr, "\n");
}

ssize_t snd_pcm_plugin_write(snd_pcm_t *pcm, const void *buf, size_t len)
{
    static ssize_t (*real)(snd_pcm_t *, const void *, size_t);
    static int shown;
    if (!real) real = dlsym(RTLD_NEXT, "snd_pcm_plugin_write");
    if (shown < 2) { shown++; preview("in ", buf, len, 8); }
    return real(pcm, buf, len);
}

ssize_t snd_pcm_write_internal(snd_pcm_t *pcm, const void *buf, size_t len)
{
    static ssize_t (*real)(snd_pcm_t *, const void *, size_t);
    static int shown;
    if (!real) real = dlsym(RTLD_NEXT, "snd_pcm_write_internal");
    if (shown < 2) { shown++; preview("out", buf, len, 8); }
    return real(pcm, buf, len);
}
