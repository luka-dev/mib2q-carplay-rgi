/* SPDX-License-Identifier: GPL-3.0-or-later
 * GAL 4.3 request (run 102 test, owner 2026-09-29). Run 101: the unchanged-layout 0x8009 UpdateUiConfigRequest
 * dropped the link ~6 s later in 3/3 sessions at our requested 1.7. open-android-auto
 * docs/interactions/02-version-ssl-auth.md: Android Auto 17.3 gates "AdditionalVideoConfig UI/resize policy"
 * on the HU's REQUESTED version >= 4.3, and answers any request above 1.7 with 6.1. So with SD flag
 * The hook requests 4.3 by default (sq5_cluster_aap17 restores the older request), and here the
 * phone's response is rewritten to 1.7 before the 2016 receiver reads it, so gal carries on as at 1.7.
 * Response layout (Controller::handleVersionResponse disassembly): u16 BE major, u16 BE minor, u16 BE status.
 * The same watch logs every non-media message for 2 minutes after a relayout send (live_msg_watch).
 */
#include "capture.h"
#include <dlfcn.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

typedef int (*version_fn)(void *, void *, unsigned);

int _ZN10Controller21handleVersionResponseEPvj(void *self, void *data, unsigned len)
{
    static version_fn real;
    unsigned char *p = (unsigned char *)data;
    if (!real) real = (version_fn)dlsym(RTLD_NEXT, "_ZN10Controller21handleVersionResponseEPvj");
    if (p && len >= 4) {
        unsigned major = (unsigned)p[0] << 8 | p[1], minor = (unsigned)p[2] << 8 | p[3];
        unsigned status = len >= 6 ? ((unsigned)p[4] << 8 | p[5]) : 0xffffu;
        if (major > 1u && access("/fs/sda0/sq5_cluster_aap17", F_OK) != 0) {
            probe_log("version.response major=%u minor=%u status=%u len=%u -> told gal 1.7", major, minor, status, len);
            p[0] = 0; p[1] = 1; p[2] = 0; p[3] = 7;
        } else probe_log("version.response major=%u minor=%u status=%u len=%u", major, minor, status, len);
    }
    return real ? real(self, data, len) : 0;
}

static unsigned watch_until_ms, watch_lines;
static unsigned now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

/* Called by the relayout sender: log incoming non-media messages for the next 2 minutes. */
void live_msg_watch_start(void)
{
    watch_until_ms = now_ms() + 120000u;
    if (!watch_until_ms) watch_until_ms = 1u;
    watch_lines = 0;
}

/* Every routed incoming message (probe routeMessage hook). Skips media data (0x0000/0x0001) and pings. */
void live_msg_watch(unsigned char ch, const unsigned char *p, unsigned n)
{
    unsigned id;
    if (!p || n < 2) return;
    id = (unsigned)p[0] << 8 | p[1];
    if (id == 0x800au) { probe_log("msg.watch channel=%u id=0x800a bytes=%u (phone UpdateUiConfig)", ch, n); return; }
    if (!watch_until_ms || (int)(now_ms() - watch_until_ms) > 0 || watch_lines >= 200u) return;
    if (id <= 0x0001u || id == 0x000bu || id == 0x000cu) return;
    watch_lines++;
    probe_log("msg.watch channel=%u id=0x%04x bytes=%u b=%02x%02x%02x%02x%02x%02x", ch, id, n,
              n > 2 ? p[2] : 0, n > 3 ? p[3] : 0, n > 4 ? p[4] : 0, n > 5 ? p[5] : 0, n > 6 ? p[6] : 0, n > 7 ? p[7] : 0);
}
