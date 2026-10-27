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
    static unsigned calls;
    ssize_t rc;
    if (!real) real = dlsym(RTLD_NEXT, "snd_pcm_plugin_write");
    calls++;
    if (calls <= 2) preview("in ", buf, len, 8);
    rc = real(pcm, buf, len);
    if (calls <= 2 || calls == 10 || calls == 20 || calls == 40 || calls == 62)
        fprintf(stderr, "TAP plugin_write calls=%u len=%u rc=%d\n",
                calls, (unsigned)len, (int)rc);
    return rc;
}

ssize_t snd_pcm_write_internal(snd_pcm_t *pcm, const void *buf, size_t len)
{
    static ssize_t (*real)(snd_pcm_t *, const void *, size_t);
    static unsigned calls;
    ssize_t rc;
    if (!real) real = dlsym(RTLD_NEXT, "snd_pcm_write_internal");
    calls++;
    if (calls <= 2) preview("out", buf, len, 8);
    rc = real(pcm, buf, len);
    if (calls <= 2 || calls == 10 || calls == 20 || calls == 40 || calls == 62)
        fprintf(stderr, "TAP write_internal calls=%u len=%u rc=%d\n",
                calls, (unsigned)len, (int)rc);
    return rc;
}
