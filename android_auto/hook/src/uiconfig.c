/* SPDX-License-Identifier: GPL-3.0-or-later
 * Cluster VideoConfiguration UiConfig (field 11): content insets (safe area for Google's cards) + UI theme.
 * Technique and object offsets from OneB1t/chopinwong01 video_sink_hook.c (same libautoreceiver build):
 * the payload goes into the VideoConfiguration's unknown-fields std::string (+0x08 buffer/pointer, size
 * +0x18, reserve +0x1c), on the sink after addSupportedConfiguration and again on the discovery-response
 * copy. UiConfig field 1 (margins) is NOT used: it makes the phone draw black borders.
 *
 * Google centres the car in the safe area and puts the turn card at its right edge (runs 49-54). Theme 2 =
 * dark. SD flag sq5_cluster_noui = no UiConfig. Per-view layout: live_relayout_tick() below.
 */
#include "capture.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

/* Full-view insets for the 1280x405 band, scaled by the requested resolution (1080p x1.5, 800x480 x0.625).
 * Owner reference photos 2026-09-29 (another Audi's working AA cluster): squeezing from BOTH sides puts the
 * car in the middle and the card just left of the Audi turn-arrow panel. Left 280 clears the rev counter;
 * right 280 ends the safe area at band 1000 = window 1095, the arrow panel starts ~1119. Owner-approved
 * after run 54 (player offset 0 0): top 60 keeps the card under the Audi top bar, bottom 40 puts the car
 * lower-middle with the card a little up. */
#define UI_TOP 210u   /* run 108: insets are FULL-frame rows (720p units, x1.5 at 1080p); visible band = rows 157..562 -> top = 157 + Audi top bar (~60 window px) */
#define UI_BOTTOM 255u /* run 108: bottom = 720 - (157 + street bar top ~340 window px): the car and card stay above the footer with no picture shift */
#define UI_LEFT 360u  /* run 108: symmetric - car at x 720; card edge (1280-360)x1.125 = 1035, before the stock-place tile frame (~1079) */
#define UI_RIGHT 360u
#define UI_THEME 2u
static uint32_t ui_top = UI_TOP, ui_bottom = UI_BOTTOM, ui_left = UI_LEFT, ui_right = UI_RIGHT;
/* Small view (Sport; classic reports the same view): the cockpit shows only a 480 px slice of window 99,
 * window x 476..956 with the player's small shift 0 (luka smallStageOffset 476; run 54 photos agree), i.e.
 * band 424..850. Telling the
 * phone this safe area makes Google lay out for the small window (compact card, car centred in it), as the
 * owner's reference car does. */
#define SM_TOP 197u    /* run 108: full-frame rows (x1.5): the Sport window shows frame rows ~284..732 -> area 295..725; narrow layout = compact card at the top, ETA pill at the bottom */
#define SM_BOTTOM 237u
#define SM_LEFT 510u   /* run 108: symmetric 510/510 (260 wide) centred on the Sport window, no shift (RoadKernel sport layout) */
#define SM_RIGHT 510u
static uint32_t sm_top = SM_TOP, sm_bottom = SM_BOTTOM, sm_left = SM_LEFT, sm_right = SM_RIGHT;
static uint32_t ui_num = 3u, ui_den = 2u;   /* band 1280 -> requested codec width (1080p default) */

/* SD file sq5_cluster_insets: "<top> <bottom> <left> <right> [<small top> <small bottom> <small left>
 * <small right>]" in 1280x405 band pixels (0..1000 each), read at phone connect. Zero fields are omitted. */
static void load_insets(void)
{
    unsigned a, b, c, d, e, g, h, k;
    FILE *f = fopen("/fs/sda0/sq5_cluster_insets", "r");
    if (!f) f = fopen("/mnt/app/root/sq5_android_auto/sq5_cluster_insets", "r");
    ui_top = UI_TOP; ui_bottom = UI_BOTTOM; ui_left = UI_LEFT; ui_right = UI_RIGHT;
    sm_top = SM_TOP; sm_bottom = SM_BOTTOM; sm_left = SM_LEFT; sm_right = SM_RIGHT;
    if (!f) return;
    if (fscanf(f, "%u %u %u %u", &a, &b, &c, &d) == 4 && a <= 1000u && b <= 1000u && c <= 1000u && d <= 1000u) {
        ui_top = a; ui_bottom = b; ui_left = c; ui_right = d;
        probe_log("uiconfig.insets file top=%u bottom=%u left=%u right=%u", a, b, c, d);
        if (fscanf(f, "%u %u %u %u", &e, &g, &h, &k) == 4 && e <= 1000u && g <= 1000u && h <= 1000u && k <= 1000u) {
            sm_top = e; sm_bottom = g; sm_left = h; sm_right = k;
            probe_log("uiconfig.insets file small top=%u bottom=%u left=%u right=%u", e, g, h, k);
        }
    } else probe_log("uiconfig.insets file ignored (need 4 numbers 0..1000)");
    fclose(f);
}

static uint32_t rd(const void *p, unsigned off) { uint32_t v; memcpy(&v, (const char *)p + off, 4); return v; }
static void wr(void *p, unsigned off, uint32_t v) { memcpy((char *)p + off, &v, 4); }
static unsigned varint(unsigned char *b, uint32_t v)
{
    unsigned n = 0;
    while (v >= 0x80u) { b[n++] = (unsigned char)(v | 0x80u); v >>= 7; }
    b[n++] = (unsigned char)v;
    return n;
}

/* Insets message fields {0x08 top, 0x10 bottom, 0x18 left, 0x20 right}; zero fields omitted. */
static unsigned insets(unsigned char *in, uint32_t t, uint32_t b, uint32_t l, uint32_t r)
{
    unsigned ni = 0;
    if (t) { in[ni++] = 0x08; ni += varint(in + ni, t); }
    if (b) { in[ni++] = 0x10; ni += varint(in + ni, b); }
    if (l) { in[ni++] = 0x18; ni += varint(in + ni, l); }
    if (r) { in[ni++] = 0x20; ni += varint(in + ni, r); }
    return ni;
}

static int ui_theme_sent;
/* UiConfig ui_theme: always Dark. Runs 150 and 162 (owner): Automatic (0) and Light (1) on the cluster display
 * both crashed the phone's Android Auto - after a live switch to Light the phone never acked the 0x8009 and the
 * whole link restarted 10 s later. The menu Map theme item and the day/night reader were removed; a saved
 * mapTheme= value is ignored. */
static const int ui_theme = UI_THEME;

/* UiConfig body: 0x12 len {content insets}, 0x20 theme (theme dropped when it does not fit in max). */
static unsigned uiconfig_body(unsigned char *ui, unsigned max, uint32_t t, uint32_t b, uint32_t l, uint32_t r)
{
    unsigned char in[20]; unsigned ni, nu = 0;
    ni = insets(in, t, b, l, r);
    if (ni > 12u || max < ni + 2u) return 0;
    ui[nu++] = 0x12; nu += varint(ui + nu, ni); memcpy(ui + nu, in, ni); nu += ni;
    ui_theme_sent = 0;
    if (nu + 2u <= max) { ui[nu++] = 0x20; ui[nu++] = (unsigned char)ui_theme; ui_theme_sent = 1; }
    return nu;
}

/* VideoConfiguration field 11: 0x5a len [UiConfig body] -> 18 bytes with the defaults (heap string). */
static unsigned payload(unsigned char *out, unsigned max)
{
    unsigned char ui[48]; unsigned nu, n = 0;
    nu = uiconfig_body(ui, max >= 2u ? max - 2u : 0u, ui_top, ui_bottom, ui_left, ui_right);
    if (!nu) return 0;
    out[n++] = 0x5a; n += varint(out + n, nu);
    if (n + nu > max) return 0;
    memcpy(out + n, ui, nu);
    return n + nu;
}

/* Four insets + theme need up to ~20 bytes, more than the 15-byte SSO buffer. The unknown-fields string is
 * the Dinkumware std::string of this libautoreceiver (buffer/pointer at +0x08, size +0x18, reserve +0x1c;
 * heap mode when reserve >= 16, freed with operator delete), so a longer payload goes into a buffer from
 * the library's own operator new (confirmed on the car, run 53). SD flag sq5_cluster_ui_sso keeps the
 * 15-byte path (theme dropped). */
typedef void *(*opnew_fn)(unsigned);
static char *heap_copy(const unsigned char *p, unsigned n)
{
    static opnew_fn opnew;
    char *b;
    if (!opnew) opnew = (opnew_fn)dlsym(RTLD_DEFAULT, "_Znwj");
    if (!opnew) return NULL;
    b = (char *)opnew(n + 1u);
    if (b) { memcpy(b, p, n); b[n] = 0; }
    return b;
}

/* 1080p viewport mode (2026-09-30, the MIBSI geometry): the cluster stream is 1920x1080 with margins 480 x 540,
 * so the phone lays its UI out in the centred 1440x540 viewport = the cockpit terminal, shown 1:1 (no scaling).
 * Insets are then in viewport pixels, one preset per cockpit state:
 *   L large map (centred wide), C classic small (two large dials, map centred), S sport small (map on the left). */
/* Run 149 (owner): the map-zoom step -1 framing is the new default in every view, baked into the presets (the
 * one-dimension rule of mz_apply: L/C height x223/256 around its centre = top +20 / bottom +21; S width x223/256
 * around its centre = left/right +24). Was L {77,146,350,370}, C {77,146,510,510}, S {78,146,580,490}. */
/* Run 153 (owner): the roller "zoom" only resizes Google's layout area (card size / layout), not the map scale
 * or text - the owner liked Sport at +4 from that base (= the original preset at step +3, width 370 -> 562 around
 * its centre) -> Sport default. */
static const uint32_t vp_preset[3][4] = { { 97, 167, 350, 370 } /* right +20: gap before our tile frame at 1081 */, { 97, 167, 510, 510 }, { 78, 146, 484, 394 } };   /* run 125: S +476 = the B9Sport small-stage offset (the Sport panel shows terminal x >= 476) */
/* Run 117 (owner photos): the phone measures the insets from the FULL 1920x1080 frame, not from the 1440x540
 * viewport (Large card edge at cockpit ~1309 = 1550 - 240) -> add the viewport offset to every preset. */
#define VP_OX 240u
#define VP_OY 270u
static int vp_mode;                         /* set by apply() when the cluster sink asks for 1080p */
static int conn_state;                      /* L/C/S layout sent at connect (apply) */

/* Cockpit state from Luka: /tmp/sq5_cluster_view (full|small) + /tmp/sq5_cluster_skin (sport|classic).
 * 0 = L, 1 = C, 2 = S; -1 = unknown (a record mid-write). */
int live_view_state(void)
{
    char v[8] = {0}, k[8] = {0};
    FILE *f = fopen("/tmp/sq5_cluster_view", "r");
    if (!f) return 0;
    if (!fgets(v, sizeof(v), f)) v[0] = 0;
    fclose(f);
    if (v[0] == 'f') return 0;
    if (v[0] != 's') return -1;
    if ((f = fopen("/tmp/sq5_cluster_skin", "r")) != NULL) { if (!fgets(k, sizeof(k), f)) k[0] = 0; fclose(f); }
    return k[0] == 's' ? 2 : 1;
}

static void apply(void *vconf, const char *where)
{
    unsigned char p[40]; unsigned n; char *heap = NULL;
    if (!vconf) { probe_log("uiconfig.skipped where=%s reason=no_vconf", where); return; }
    if (access("/fs/sda0/sq5_cluster_noui", F_OK) == 0) { probe_log("uiconfig.off where=%s (sq5_cluster_noui)", where); return; }
    if (rd(vconf, 0x28) < 1u || rd(vconf, 0x28) > 3u) {
        probe_log("uiconfig.skipped where=%s reason=resolution res=%u", where, rd(vconf, 0x28)); return;
    }
    {   uint32_t num = rd(vconf, 0x28) == 3u ? 3u : rd(vconf, 0x28) == 1u ? 5u : 1u;
        uint32_t den = rd(vconf, 0x28) == 3u ? 2u : rd(vconf, 0x28) == 1u ? 8u : 1u;
        load_insets();
        ui_num = num; ui_den = den;
        ui_top = ui_top * num / den; ui_bottom = ui_bottom * num / den; ui_left = ui_left * num / den; ui_right = ui_right * num / den;
        vp_mode = rd(vconf, 0x28) == 3u && access("/fs/sda0/sq5_cluster_band", F_OK) != 0;   /* SD flag = old band path */
        if (vp_mode) {
            int s = live_view_state(); if (s < 0) s = 0;
            conn_state = s;
            ui_num = ui_den = 1;
            ui_top = vp_preset[s][0] + VP_OY; ui_bottom = vp_preset[s][1] + VP_OY; ui_left = vp_preset[s][2] + VP_OX; ui_right = vp_preset[s][3] + VP_OX;
        } }
    if (rd(vconf, 0x1c) > 15u || rd(vconf, 0x18) != 0u) {
        probe_log("uiconfig.skipped where=%s reason=unknown_fields_in_use len=%u cap=%u", where, rd(vconf, 0x18), rd(vconf, 0x1c)); return;
    }
    n = payload(p, 32u);
    if (n > 15u && (access("/fs/sda0/sq5_cluster_ui_sso", F_OK) == 0 || !(heap = heap_copy(p, n)))) {
        probe_log("uiconfig.heap unavailable where=%s bytes=%u; 15-byte payload", where, n);
        n = payload(p, 15u);
    }
    if (!n) { probe_log("uiconfig.skipped where=%s reason=payload_too_long", where); return; }
    if (heap) {
        wr(vconf, 0x08, (uint32_t)(uintptr_t)heap);
        wr(vconf, 0x18, n);
        wr(vconf, 0x1c, n);
    } else {
        memcpy((char *)vconf + 0x08, p, n);
        ((char *)vconf)[0x08 + n] = 0;
        wr(vconf, 0x18, n);
    }
    probe_log("uiconfig.set where=%s top=%u bottom=%u left=%u right=%u theme=%u bytes=%u%s", where,
              ui_top, ui_bottom, ui_left, ui_right, ui_theme_sent ? (unsigned)ui_theme : 0u, n, heap ? " heap" : "");
}

void live_uiconfig_sink(void *sink)
{
    uint32_t vec = sink ? rd(sink, 0x40) : 0;
    apply(vec ? (void *)(uintptr_t)rd((void *)(uintptr_t)vec, 4) : NULL, "sink");
}

void live_uiconfig_media(void *media)
{
    void **arr = media ? (void **)(uintptr_t)rd(media, 0x40) : NULL;
    apply(media && rd(media, 0x44) > 0u && arr ? arr[0] : NULL, "discovery");
}

/* Per-view layout (owner 2026-09-29: "tell it the map got smaller, serve smaller elements"). AAP message
 * 0x8009 UpdateUiConfigRequest (HU -> phone, video channel; open-android-auto docs/channels/video.md) carries
 * field 1 = the same UiConfig as VideoConfiguration field 11. The 2016 receiver has no sender for it, so it
 * is built here and queued like the receiver's own VideoFocusNotification (VideoSink::setVideoFocus): 2-byte
 * big-endian type + protobuf, MessageRouter::queueOutgoing(router, channel, data, len) copies it; the sink
 * (ProtocolEndpointBase) holds open flag +4, channel +5, router +8. Called from the cockpit sink's frame
 * callback (receiver thread, the thread the receiver itself sends from). Reads /tmp/sq5_cluster_view (Luka)
 * every 15 frames; on a change sends the full or small safe area. SD flag sq5_cluster_relayout_off. */
/* Map zoom (owner 2026-09-30: "we zoom the video, can't we zoom the map?"). Google ignores zoom input for the
 * cluster display but picks its camera zoom from the size of the layout area (run 117: Classic's narrow area
 * came out zoomed out, Large zoomed in). Menu "rollerZoom=1" (Roller zoom: Map, default): each roller step scales the
 * layout area around its centre by x1.15 (bigger area = closer camera) and resends it (0x8009); the player
 * then keeps the video 1:1 and pins the card (/tmp/sq5_cluster_mapzoom "SQ5Z <mode> <step>"). */
static volatile int mz_step, mz_dirty, mz_mode = -1;
static const int mz_k256[8] = { 168, 194, 223, 256, 294, 338, 389, 447 };   /* steps -3..+4 */
static int menu_setting(const char *key, int dflt);
static void mz_publish(void)
{
    char rec[24];
    FILE *f = fopen("/tmp/sq5_cluster_mapzoom", "r+");
    if (!f) f = fopen("/tmp/sq5_cluster_mapzoom", "w");
    if (!f) return;
    snprintf(rec, sizeof(rec), "SQ5Z %d %+d          ", mz_mode > 0, mz_step);
    rec[15] = '\n'; rec[16] = 0;
    fwrite(rec, 1, 16, f);
    fclose(f);
}
/* Roller thread: 1 = map mode (the delta is taken here, nothing is sent as input). Mode re-read every call. */
int live_map_zoom(int delta)
{
    /* run 137 (owner): Digital zoom removed (blurry, ~7 fps) - the roller always zooms the map */
    int mode = 1, s;
    if (mode != mz_mode) { mz_mode = mode; if (!mode && mz_step) { mz_step = 0; mz_dirty = 1; } mz_publish(); }
    if (!mode) return 0;
    s = mz_step + (delta > 0 ? 1 : delta < 0 ? -1 : 0);
    if (s < -3) s = -3;
    if (s > 4) s = 4;
    if (s != mz_step) { mz_step = s; mz_dirty = 1; mz_publish(); probe_log("mapzoom step=%d", s); }
    return 1;
}
/* Scale the content rect (frame coords) for the current map-zoom step so neither the car nor the card moves
 * (owner: "zoom in without the car drifting"). Run 127 frames: wide layouts (L, C) put card and car both
 * vertically centred (card flush right, car centred left of it) -> only the height changes, around its centre;
 * narrow Sport puts card (top) and car both horizontally centred -> only the width changes, around its centre.
 * The player then needs no cut-out (the filler smear looked blurry); its pin stays as a fallback. */
static void mz_apply(uint32_t *t, uint32_t *b, uint32_t *l, uint32_t *r, int view)
{
    int k = mz_k256[mz_step + 3], x0 = (int)*l, x1 = 1920 - (int)*r, y0 = (int)*t, y1 = 1080 - (int)*b;
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, w = (x1 - x0) * k / 256, h = (y1 - y0) * k / 256;
    /* SD flag sq5_mapzoom_both: the run-125 method (both sides around the centre: zooms in both views, car
     * still, card moves and is pasted back by the player) for side-by-side comparison. */
    int both = access("/fs/sda0/sq5_mapzoom_both", F_OK) == 0;   /* per step: no restart needed */
    if (both || view != 2) { y0 = cy - h / 2; y1 = cy + h / 2; }
    if (both || view == 2) { x0 = cx - w / 2; x1 = cx + w / 2; }
    if (x0 < 0) x0 = 0;
    if (x1 > 1920) x1 = 1920;
    if (y0 < 0) y0 = 0;
    if (y1 > 1080) y1 = 1080;
    *l = (uint32_t)x0; *r = (uint32_t)(1920 - x1); *t = (uint32_t)y0; *b = (uint32_t)(1080 - y1);
}

/* Run 127 (owner): in Sport the Map roller moves the car instead of zooming -> log the insets actually sent,
 * so the step where the car sits right can be baked in as the Sport preset. */
static uint32_t last_ins[4];
unsigned live_relayout_message(unsigned char *out, unsigned max, int small)
{
    unsigned char ui[48]; unsigned nu, n = 0;
    uint32_t t = ui_top, b = ui_bottom, l = ui_left, r = ui_right;   /* already scaled by apply() */
    /* small = cockpit state: 0 L, 1 C, 2 S (legacy band path: any non-zero = the one small layout) */
    if (vp_mode && small >= 0 && small <= 2) {
        t = vp_preset[small][0] + VP_OY; b = vp_preset[small][1] + VP_OY; l = vp_preset[small][2] + VP_OX; r = vp_preset[small][3] + VP_OX;
        if (mz_step) mz_apply(&t, &b, &l, &r, small);
    } else if (small) {
        t = sm_top * ui_num / ui_den; b = sm_bottom * ui_num / ui_den;
        l = sm_left * ui_num / ui_den; r = sm_right * ui_num / ui_den;
    }
    last_ins[0] = t; last_ins[1] = b; last_ins[2] = l; last_ins[3] = r;
    nu = uiconfig_body(ui, 40u, t, b, l, r);
    if (!nu || max < nu + 5u) return 0;
    out[n++] = 0x80; out[n++] = 0x09;
    out[n++] = 0x0a; n += varint(out + n, nu);
    memcpy(out + n, ui, nu);
    return n + nu;
}

typedef void (*queue_fn)(void *, unsigned char, void *, unsigned);
typedef void (*focus_fn)(void *, int, int);

/* Queue one UpdateUiConfigRequest on the sink's channel. 1 = queued. */
static int relayout_send(void *sink, int small, const char *why)
{
    static queue_fn queue;
    static focus_fn focus;
    unsigned char msg[64]; unsigned n;
    int dance;
    if (!queue) queue = (queue_fn)dlsym(RTLD_DEFAULT, "_ZN13MessageRouter13queueOutgoingEhPvj");
    n = live_relayout_message(msg, sizeof(msg), small);
    if (!queue || !n || !rd(sink, 8) || !((const unsigned char *)sink)[4]) {
        probe_log("relayout.skipped %s view=%d queue=%p bytes=%u open=%u", why, small, (void *)queue,
                  n, ((const unsigned char *)sink)[4]);
        return 0;
    }
    /* Run 96 (owner 2026-09-29): open-android-auto docs/channels/video.md documents 0x8009 as fire-and-forget
     * with no focus handling -> plain send is the default; SD flag sq5_cluster_relayout_focus = the old
     * native/projected focus cycle around it (runs 64-67). Plain sends also failed once (run 63, older phone
     * Android Auto), hence the staged test in live_relayout_tick. */
    dance = access("/fs/sda0/sq5_cluster_relayout_focus", F_OK) == 0;
    if (!focus) focus = (focus_fn)dlsym(RTLD_DEFAULT, "_ZN9VideoSink13setVideoFocusEib");
    if (dance && focus) focus(sink, 2, 1);
    { void live_msg_watch_start(void); live_msg_watch_start(); }
    queue((void *)(uintptr_t)rd(sink, 8), ((const unsigned char *)sink)[5], msg, n);
    if (dance && focus) focus(sink, 1, 1);
    probe_log("relayout.sent %s view=%c step=%d top=%u bottom=%u left=%u right=%u channel=%u bytes=%u focus_cycle=%d mode=%s", why,
              small >= 0 && small <= 2 ? "LCS"[small] : '?', mz_step, last_ins[0], last_ins[1], last_ins[2], last_ins[3],
              ((const unsigned char *)sink)[5], n, dance && focus,
              vp_mode ? "viewport1440x540" : "band");
    return 1;
}

/* Called at the top of the cockpit sink's frame callback (receiver thread, before that frame's ack). Staged
 * test (review of runs 63-67). Relayout is enabled by default; sq5_cluster_relayout_off disables it:
 *   1. ~20 s into the session: resend the UNCHANGED connect-time layout once (relayout.sent noop). If the
 *      link drops here, the send path / this receiver's protocol version is the problem, not the geometry.
 *   2. ~30 s after that, and only if the session survived: view changes send the full or Sport layout.
 * A change is picked up on one frame and sent on the NEXT callback, i.e. after the previous frame's ack.
 * State is per session (a new sink or a channel reopen starts over); a view counts as sent only once queued. */
void live_relayout_tick(void *sink)
{
    static void *cur_sink;
    static unsigned ticks, stage_tick;
    static int stage;               /* 0 = waiting for the no-op, 1 = no-op sent, 2 = view changes live */
    static int sent_view;           /* 0 = full (the connect-time UiConfig), 1 = small */
    static int pending = -1;        /* view to send on the next callback */
    static int off = 1;
    char v[8] = {0};
    FILE *f;
    int small;
    { void live_rotary_video_channel(unsigned); if (sink && ((const unsigned char *)sink)[4]) live_rotary_video_channel(((const unsigned char *)sink)[5]); }
    if (!sink) return;
    if (sink != cur_sink || !((const unsigned char *)sink)[4]) {   /* new session or channel closed */
        if (cur_sink == sink && stage) probe_log("relayout.session_reset (channel closed)");
        cur_sink = ((const unsigned char *)sink)[4] ? sink : NULL;
        ticks = 0; stage_tick = 0; sent_view = conn_state; pending = -1; off = 1;
        if (mz_step) { mz_step = 0; mz_dirty = 0; mz_publish(); }   /* connect layout is unzoomed */
        /* 2026-09-30: the layout update is proven at GAL 4.3 (runs 102-116) -> view changes go out at once;
         * SD flag sq5_cluster_relayout_staged = the old no-op-first test (~50 s before the first change) */
        stage = access("/fs/sda0/sq5_cluster_relayout_staged", F_OK) == 0 ? 0 : 2;
        if (!cur_sink) return;
    }
    ticks++;
    if (ticks == 15u || ticks % 900u == 0u) off = access("/fs/sda0/sq5_cluster_relayout_off", F_OK) == 0;
    if (off) return;
    if (pending >= 0) {             /* decided on the previous frame, sent after its ack */
        int want = pending;
        pending = -1;
        if (stage == 0) {
            if (relayout_send(sink, 0, "noop")) { stage = 1; stage_tick = ticks; }
        } else if (relayout_send(sink, want, "view")) sent_view = want;
        return;
    }
    if (stage == 0) {
        if (ticks >= 600u) pending = 0;              /* ~20 s at 30 fps */
        return;
    }
    if (stage == 1) {
        if (ticks - stage_tick >= 900u) { stage = 2; probe_log("relayout.noop_survived frames=%u", ticks - stage_tick); }
        return;
    }
    if (mz_dirty && stage == 2) { mz_dirty = 0; pending = sent_view; return; }   /* map zoom: resend the current layout */
    if (ticks % 15u) return;
    /* 0 L / 1 C / 2 S (Luka view + skin); legacy band path: classic and sport share the one small layout */
    small = live_view_state();
    if (small < 0) return;                        /* mid-write: keep the last state */
    if (!vp_mode && small) small = 1;
    if (small != sent_view) pending = small;
    (void)f; (void)v;
}

/* Density sent in the cluster VideoConfiguration (SD file sq5_cluster_dpi: one number, 80..400). Owner runs:
 * 720p at 160 = too zoomed in; 1080p 160 "a lil too zoomed out", 190 "a little too zoomed in" -> 175 (run 54).
 * Read when the phone connects (addSupportedConfiguration). */
/* Cockpit menu settings (Luka AaClusterMenu, /mnt/persist/var/app/sq5_cluster/menu.properties): "density=<n>"
 * (0 = automatic) and "res=<3|2>". Owner 2026-09-29: density and resolution from the menu, on the car. */
static int menu_setting(const char *key, int dflt)
{
    char b[256] = {0}, *p;
    size_t n;
    int v;
    FILE *f = fopen("/mnt/persist/var/app/sq5_cluster/menu.properties", "r");
    if (!f) return dflt;
    n = fread(b, 1, sizeof(b) - 1, f);
    fclose(f);
    b[n] = 0;
    for (p = b; (p = strstr(p, key)) != NULL; p++)
        if ((p == b || p[-1] == '\n') && sscanf(p + strlen(key), "%d", &v) == 1) return v;
    return dflt;
}

/* Requested codec resolution: 2 = 720p band (default since run 108: the owner-approved size), 3 = 1080p band.
 * Menu "stream=2" or SD flag sq5_cluster_720 = 720p (keys renamed 2026-09-30: old saved res/dpi ignored). */
int live_res(void)
{
    /* Owner 2026-09-30: target 1080p (the default again); menu "stream=2" or SD flag sq5_cluster_720 = 720p. */
    int r = menu_setting("stream=", 3) == 2 || access("/fs/sda0/sq5_cluster_720", F_OK) == 0 ? 2 : 3;
    probe_log("res value=%d (%s)", r, r == 3 ? "1080p" : "720p");
    return r;
}

int live_dpi(int res)
{
    /* Owner 2026-09-30: cockpit menu Size (menu.properties "size=1|2|3", default 2 Medium) mapped per resolution.
     * Run 117: 144 at the 1:1 viewport was too big -> Medium 110 (the owner's old 140 look x0.75 = ~105).
     * An SD / unit sq5_cluster_dpi file (one number) still overrides everything. */
    /* run 149: Size 1..5 = Small, Medium, Large, X-Large, XX-Large (owner wants the bigger zoomed-in look) */
    static const int map1080[6] = { 110, 95, 110, 125, 140, 160 }, map720[6] = { 120, 105, 120, 140, 160, 180 };
    int size = menu_setting("size=", 3), v, d;
    const char *src = "menu size";
    FILE *f;
    if (size < 1 || size > 5) size = 3;   /* run 154: Large (125 dpi at 1080p = the panel's 125 PPI) */
    d = res == 3 ? map1080[size] : res == 2 ? map720[size] : 160;
    f = fopen("/fs/sda0/sq5_cluster_dpi", "r");
    if (!f) f = fopen("/mnt/app/root/sq5_android_auto/sq5_cluster_dpi", "r");
    if (f) { if (fscanf(f, "%d", &v) == 1 && v >= 80 && v <= 400) { d = v; src = "file"; } fclose(f); }
    probe_log("dpi res=%d size=%d value=%d source=%s", res, size, d, src);
    return d;
}
