/* SPDX-License-Identifier: GPL-3.0-or-later
 * Steering-wheel roller -> the cockpit map's own input channel (owner 2026-09-29: "the scroll has to
 * correspond to the cockpit screen, then it acts as zoom").
 *
 * The hook's cluster input service (display_id 1, keycodes 23 + 65536 ROTARY_CONTROLLER) is bound by the
 * phone at every session (input.binding). Luka (AaRollerInput) writes a cumulative step counter to
 * /tmp/sq5_cluster_rotary ("SQ5R <total>", fixed 32-byte record written in place) while the cockpit shows
 * the cluster map; this thread polls it every 40 ms and sends the difference as an InputReport, built like
 * the receiver's own InputSource::reportRelative/sendInputReport (field numbers read from this
 * libautoreceiver): type 0x8001, {1: timestamp ns, 6: RelativeEvent {1: {1: keycode 65536, 2: delta}}},
 * queued with MessageRouter::queueOutgoing(router, channel) - thread-safe, the stock rotary path calls it
 * from the HMI/DSI thread too. SD flag sq5_cluster_rotary_off (checked in Luka) keeps the old path.
 */
#include "capture.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>

#define KEYCODE_ROTARY 65536u
static void *volatile rot_endpoint;
static volatile unsigned rot_channel;
/* Run 81-83: rotary reports on the cluster input channel arrived (rotary.sent) but Maps did not zoom.
 * Multi-display Android Auto tags input with the target display's video channel (InputReport field 2
 * disp_channel_id); the cockpit sink's channel is recorded here (live_rotary_video_channel, from the
 * sink frame callback). SD flag sq5_cluster_rotary_nodisp sends without it (the run 81 format). */
static volatile unsigned rot_video_channel;
void live_rotary_video_channel(unsigned ch) { rot_video_channel = ch; }
static pthread_mutex_t rot_lock = PTHREAD_MUTEX_INITIALIZER;
static int rot_started;

static unsigned varint64(unsigned char *b, uint64_t v)
{
    unsigned n = 0;
    while (v >= 0x80u) { b[n++] = (unsigned char)(v | 0x80u); v >>= 7; }
    b[n++] = (unsigned char)v;
    return n;
}

/* 0x8001 InputReport with one relative rotary event. int32 delta: negative values are sign-extended
 * to 64 bits (protobuf int32 wire format). */
unsigned live_rotary_message(unsigned char *out, unsigned max, uint64_t ts_ns, int delta, unsigned disp_ch)
{
    unsigned char rel[24], ev[32]; unsigned nr = 0, ne = 0, n = 0;
    if (max < 48u) return 0;
    rel[nr++] = 0x08; nr += varint64(rel + nr, KEYCODE_ROTARY);
    rel[nr++] = 0x10; nr += varint64(rel + nr, (uint64_t)(int64_t)delta);
    ev[ne++] = 0x0a; ev[ne++] = (unsigned char)nr; memcpy(ev + ne, rel, nr); ne += nr;
    out[n++] = 0x80; out[n++] = 0x01;
    out[n++] = 0x08; n += varint64(out + n, ts_ns);
    if (disp_ch) { out[n++] = 0x10; n += varint64(out + n, disp_ch); }
    out[n++] = 0x32; out[n++] = (unsigned char)ne; memcpy(out + n, ev, ne); n += ne;
    return n;
}

static int read_total(int *total)
{
    char b[40] = {0};
    FILE *f = fopen("/tmp/sq5_cluster_rotary", "r");
    int ok = 0;
    if (!f) return 0;
    if (fgets(b, sizeof(b), f) && sscanf(b, "SQ5R %d", total) == 1) ok = 1;
    fclose(f);
    return ok;
}

typedef void (*queue_fn)(void *, unsigned char, void *, unsigned);
static void *rotary_thread(void *arg)
{
    queue_fn queue = (queue_fn)dlsym(RTLD_DEFAULT, "_ZN13MessageRouter13queueOutgoingEhPvj");
    int last = 0, have = 0, total;
    unsigned sent = 0;
    (void)arg;
    if (!queue) { probe_log("rotary.off reason=no_queueOutgoing"); return NULL; }
    for (;;) {
        usleep(40000);
        if (!read_total(&total)) continue;
        if (!have) { last = total; have = 1; continue; }   /* a stale counter from before this session */
        if (total == last) continue;
        {
            int delta = total - last;
            unsigned char msg[64]; unsigned n;
            struct timespec ts;
            void *ep;
            last = total;
            if (delta > 20) delta = 20;
            if (delta < -20) delta = -20;
            {   /* 2026-09-30 map zoom (src/uiconfig.c): in "Roller zoom: Map" the step resizes the layout instead */
                int live_map_zoom(int);
                if (live_map_zoom(delta)) continue;
            }
            clock_gettime(CLOCK_MONOTONIC, &ts);
            n = live_rotary_message(msg, sizeof(msg), (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec, delta,
                                    access("/fs/sda0/sq5_cluster_rotary_nodisp", F_OK) == 0 ? 0u : rot_video_channel);
            pthread_mutex_lock(&rot_lock);
            ep = rot_endpoint;
            if (ep && n && ((const unsigned char *)ep)[4]) {
                uint32_t router; memcpy(&router, (const char *)ep + 8, 4);
                if (router) queue((void *)(uintptr_t)router, (unsigned char)rot_channel, msg, n);
                ++sent;
                if (sent <= 20 || sent % 50 == 0) probe_log("rotary.sent delta=%d channel=%u disp=%u n=%u", delta, rot_channel, rot_video_channel, sent);
            } else if (sent < 5) {
                probe_log("rotary.dropped delta=%d reason=input_channel_not_bound", delta);
            }
            pthread_mutex_unlock(&rot_lock);
        }
    }
    return NULL;
}

/* Called from the hook: bound after the phone's KeyBindingRequest on the cluster input channel, and with
 * NULL when the sink/endpoint is torn down (no use after free). */
void live_rotary_bind(void *endpoint, unsigned char channel)
{
    pthread_t t;
    pthread_mutex_lock(&rot_lock);
    rot_endpoint = endpoint;
    rot_channel = channel;
    if (endpoint && !rot_started) {
        rot_started = 1;
        if (pthread_create(&t, NULL, rotary_thread, NULL) == 0) pthread_detach(t);
        else probe_log("rotary.off reason=thread");
    }
    pthread_mutex_unlock(&rot_lock);
    probe_log("rotary.bind endpoint=%p channel=%u", endpoint, channel);
}
