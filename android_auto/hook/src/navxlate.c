/* SPDX-License-Identifier: GPL-3.0-or-later
 * Navigation status: new format -> legacy, inside gal (live run 8, 2026-09-29).
 *
 * With AAP 1.7 + a cluster display, current Google Maps sends the NavigationStatusService (channel 12 on
 * MU0918) 0x8006 NavigationState and 0x8007 NavigationCurrentPosition (~1 Hz each) instead of the legacy
 * 0x8004 NavigationNextTurnEvent / 0x8005 NavigationNextTurnDistanceEvent. The 2016 libautoreceiver only
 * knows the legacy pair, so the DSI turn/distance events stopped and the luka arrows + HUD got nothing.
 * Here each new message is rewritten IN PLACE (the legacy encoding is always shorter) into its legacy
 * equivalent before gal routes it, so everything downstream is unchanged.
 *
 * Schemas: aa-proxy-rs src/protos/protos.proto (commit 0e71880). Maneuver mapping: headunit-revived
 * AapNavigationHelper.maneuverTypeToLegacyNextEvent. Roundabout side (clockwise = left-hand traffic) is
 * inferred. Cue text, later steps and destinations are dropped (not in the legacy format).
 * Lanes of steps[0] (not in the legacy format either) go to Java beside the message: /tmp/sq5_aa_lanes,
 * a fixed-length text record rewritten in place when it changes (nav_lanes_record; reader AaLaneFeed).
 * SD flag sq5_cluster_navxlate_off = leave the new messages untouched (and no lanes record).
 */
#include "capture.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define NAV_CHANNEL 12u   /* MU0918 service/channel 12 = NavigationStatusService (hook logs, live run 8) */

typedef struct { const unsigned char *p, *e; } pb;
static int pb_varint(pb *b, uint64_t *v)
{
    unsigned s = 0; *v = 0;
    while (b->p < b->e && s < 64) { unsigned char c = *b->p++; *v |= (uint64_t)(c & 0x7f) << s; if (!(c & 0x80)) return 1; s += 7; }
    return 0;
}
/* next field: tag + either varint (vv) or length-delimited (sub) */
static int pb_next(pb *b, unsigned *field, unsigned *wire, uint64_t *vv, pb *sub)
{
    uint64_t k, len;
    if (b->p >= b->e || !pb_varint(b, &k)) return 0;
    *field = (unsigned)(k >> 3); *wire = (unsigned)(k & 7);
    if (*wire == 0) return pb_varint(b, vv);
    if (*wire == 2) {
        if (!pb_varint(b, &len) || len > (uint64_t)(b->e - b->p)) return 0;
        sub->p = b->p; sub->e = b->p + len; b->p += len; return 1;
    }
    if (*wire == 5) { if (b->e - b->p < 4) return 0; b->p += 4; return 1; }
    if (*wire == 1) { if (b->e - b->p < 8) return 0; b->p += 8; return 1; }
    return 0;
}
static unsigned put_varint(unsigned char *o, uint64_t v)
{
    unsigned n = 0;
    while (v >= 0x80) { o[n++] = (unsigned char)(v | 0x80); v >>= 7; }
    o[n++] = (unsigned char)v; return n;
}

/* new ManeuverType (0..42) -> legacy NextTurnEnum */
static int legacy_event(unsigned t)
{
    if (t <= 2) return (int)t;                               /* UNKNOWN, DEPART, NAME_CHANGE */
    if (t == 3 || t == 4 || t == 36) return 14;              /* KEEP_L/R, STRAIGHT -> STRAIGHT */
    if (t == 5 || t == 6) return 3;                          /* SLIGHT_TURN */
    if (t == 7 || t == 8) return 4;                          /* TURN */
    if (t == 9 || t == 10) return 5;                         /* SHARP_TURN */
    if (t == 11 || t == 12 || t == 19 || t == 20) return 6;  /* U_TURN */
    if (t >= 13 && t <= 18) return 7;                        /* ON_RAMP */
    if (t >= 21 && t <= 24) return 8;                        /* OFF_RAMP */
    if (t == 25 || t == 26) return 9;                        /* FORK */
    if (t >= 27 && t <= 29) return 10;                       /* MERGE */
    if (t == 30) return 11;                                  /* ROUNDABOUT_ENTER */
    if (t == 31) return 12;                                  /* ROUNDABOUT_EXIT */
    if (t >= 32 && t <= 35) return 13;                       /* ROUNDABOUT_ENTER_AND_EXIT */
    if (t == 37) return 16;                                  /* FERRY_BOAT */
    if (t == 38) return 17;                                  /* FERRY_TRAIN */
    if (t >= 39 && t <= 42) return 19;                       /* DESTINATION */
    return 0;
}
/* legacy turn_side: LEFT=1 RIGHT=2 UNSPECIFIED=3 (new types: odd/even pairs are _LEFT/_RIGHT) */
static int legacy_side(unsigned t)
{
    static const unsigned char side[43] = {
        3,3,3, 1,2, 1,2, 1,2, 1,2, 1,2, 1,2,1,2,1,2, 1,2, 1,2,1,2, 1,2, 1,2,3, 3,3, 1,1,2,2, 3, 3,3, 3,3,1,2 };
    return t <= 42 ? side[t] : 3;
}

/* 0x8006 NavigationState body -> 0x8004 NavigationNextTurnEvent body. Returns length or 0. */
int nav_state_to_legacy(const unsigned char *in, unsigned n, unsigned char *out, unsigned max, const char *fallback_road)
{
    pb b = { in, in + n }, s, x, y; unsigned f, w; uint64_t v;
    unsigned type = 0, exitn = 0, angle = 0; int have_step = 0, have_exit = 0, have_angle = 0;
    const unsigned char *road = NULL; unsigned road_n = 0, o = 0; int ev, side;
    while (pb_next(&b, &f, &w, &v, &s)) {
        if (f != 1 || w != 2) continue;                       /* steps: first one only */
        have_step = 1;
        while (pb_next(&s, &f, &w, &v, &x)) {
            if (f == 1 && w == 2) {                           /* maneuver */
                while (pb_next(&x, &f, &w, &v, &y)) {
                    if (w != 0) continue;
                    if (f == 1) type = (unsigned)v;
                    else if (f == 2) { exitn = (unsigned)v; have_exit = 1; }
                    else if (f == 3) { angle = (unsigned)v; have_angle = 1; }
                }
            } else if (f == 2 && w == 2) {                    /* road { name = 1 } */
                while (pb_next(&x, &f, &w, &v, &y)) if (f == 1 && w == 2) { road = y.p; road_n = (unsigned)(y.e - y.p); }
            }
        }
        break;
    }
    if (!have_step) return 0;
    if (!road && fallback_road) { road = (const unsigned char *)fallback_road; road_n = (unsigned)strlen(fallback_road); }
    ev = legacy_event(type); side = legacy_side(type);
    if (max < road_n + 24) return 0;
    out[o++] = 0x0a; o += put_varint(out + o, road_n); if (road_n) memcpy(out + o, road, road_n); o += road_n;   /* road (required) */
    out[o++] = 0x10; o += put_varint(out + o, (uint64_t)side);
    out[o++] = 0x18; o += put_varint(out + o, (uint64_t)ev);
    if (ev == 13 || ev == 11 || ev == 12) {
        if (have_exit) { out[o++] = 0x28; o += put_varint(out + o, exitn); }
        if (have_angle) { out[o++] = 0x30; o += put_varint(out + o, angle); }
    }
    return (int)o;
}

/* 0x8006 steps[0].lanes -> "shape,hl[,shape,hl...]|..." (NavigationLane.LaneDirection: shape 0..9, other
 * values -> 0 UNKNOWN; hl = is_highlighted 0/1), lanes in the order sent, at most NAV_LANE_DIRS_MAX
 * directions per lane. Returns the lane count; 0 (out = "") for no step, no lanes, more than
 * NAV_LANES_MAX lanes or anything malformed: no lanes is always safe, wrong lanes are not. */
#define NAV_LANES_MAX 8
#define NAV_LANE_DIRS_MAX 4
#define NAV_LANES_BODY 160            /* >= 8 lanes x (4 x "s,h" + 3 ",") + 7 "|" + NUL */
#define NAV_LANES_REC 256
#define NAV_LANES_PATH "/tmp/sq5_aa_lanes"
static int nav_state_lanes(const unsigned char *in, unsigned n, char *out)
{
    pb b = { in, in + n }, s, x, y, z; unsigned f, w, o = 0; uint64_t v; int lanes = 0;
    out[0] = 0;
    while (pb_next(&b, &f, &w, &v, &s)) {
        if (f != 1 || w != 2) continue;                       /* steps: first one only */
        while (pb_next(&s, &f, &w, &v, &x)) {
            unsigned dirs = 0;
            if (f != 3 || w != 2) continue;                   /* lanes */
            if (lanes == NAV_LANES_MAX) { out[0] = 0; return 0; }
            if (lanes++) out[o++] = '|';
            while (pb_next(&x, &f, &w, &v, &y)) {
                unsigned shape = 0, hl = 0;
                if (f != 1 || w != 2) continue;               /* lane_directions */
                while (pb_next(&y, &f, &w, &v, &z)) {
                    if (w != 0) continue;
                    if (f == 1) shape = v <= 9 ? (unsigned)v : 0;
                    else if (f == 2) hl = v != 0;
                }
                if (y.p != y.e) { out[0] = 0; return 0; }
                if (dirs == NAV_LANE_DIRS_MAX) continue;
                if (dirs++) out[o++] = ',';
                out[o++] = (char)('0' + shape); out[o++] = ','; out[o++] = (char)('0' + hl);
            }
            if (x.p != x.e) { out[0] = 0; return 0; }
        }
        if (s.p != s.e) { out[0] = 0; return 0; }
        break;
    }
    out[o] = 0;
    return lanes;
}

/* The /tmp/sq5_aa_lanes record: "SQ5L1 <seq>\nlanes=<n>\n<nav_state_lanes text>\nend=<seq>\n", space-padded
 * to NAV_LANES_REC bytes, last byte '\n'. Fixed length, so an in-place rewrite never leaves a short or
 * empty file (/tmp = /dev/shmem: no rename); the reader drops a record whose two seq values differ. */
static unsigned nav_lanes_record(unsigned seq, int lanes, const char *text, char *rec)
{
    int k = snprintf(rec, NAV_LANES_REC, "SQ5L1 %u\nlanes=%d\n%s\nend=%u\n", seq, lanes, text, seq);
    if (k < 0 || k >= NAV_LANES_REC) return 0;
    memset(rec + k, ' ', (size_t)(NAV_LANES_REC - k));
    rec[NAV_LANES_REC - 1] = '\n';
    return NAV_LANES_REC;
}

/* "0.3" -> 300 (x1000, rounded); returns -1 if not a plain decimal */
static long e3(const unsigned char *s, unsigned n)
{
    long whole = 0, frac = 0, scale = 1000; unsigned i = 0; int dot = 0, digits = 0;
    for (; i < n; i++) {
        if (s[i] >= '0' && s[i] <= '9') {
            digits = 1;
            if (!dot) whole = whole * 10 + (s[i] - '0');
            else if (scale > 1) { scale /= 10; frac += (s[i] - '0') * scale; }
        } else if ((s[i] == '.' || s[i] == ',') && !dot) dot = 1;
        else return -1;
    }
    return digits ? whole * 1000 + frac : -1;
}

/* 0x8007 NavigationCurrentPosition body -> 0x8005 NavigationNextTurnDistanceEvent body. Also returns
 * the current road name (for 0x8006 without a step road). */
int nav_position_to_legacy(const unsigned char *in, unsigned n, unsigned char *out, unsigned max,
                           char *road_out, unsigned road_max)
{
    pb b = { in, in + n }, s, x, y; unsigned f, w, o = 0; uint64_t v;
    int have = 0; long meters = 0, secs = 0, disp = -1; unsigned units = 0;
    if (road_out && road_max) road_out[0] = 0;
    while (pb_next(&b, &f, &w, &v, &s)) {
        if (f == 1 && w == 2) {                               /* step_distance */
            while (pb_next(&s, &f, &w, &v, &x)) {
                if (f == 2 && w == 0) secs = (long)(int64_t)v;
                else if (f == 1 && w == 2) {                  /* NavigationDistance */
                    have = 1;
                    while (pb_next(&x, &f, &w, &v, &y)) {
                        if (f == 1 && w == 0) meters = (long)(int32_t)v;
                        else if (f == 2 && w == 2) disp = e3(y.p, (unsigned)(y.e - y.p));
                        else if (f == 3 && w == 0) units = (unsigned)v;
                    }
                }
            }
        } else if (f == 3 && w == 2 && road_out && road_max) { /* current_road { name = 1 } */
            while (pb_next(&s, &f, &w, &v, &x)) if (f == 1 && w == 2) {
                unsigned k = (unsigned)(x.e - x.p); if (k >= road_max) k = road_max - 1;
                memcpy(road_out, x.p, k); road_out[k] = 0;
            }
        }
    }
    if (!have || max < 24) return 0;
    if (meters < 0) meters = 0;
    if (secs < 0) secs = 0;
    out[o++] = 0x08; o += put_varint(out + o, (uint64_t)meters);           /* distance_meters (required) */
    out[o++] = 0x10; o += put_varint(out + o, (uint64_t)secs);             /* time_to_turn_seconds (required) */
    if (disp >= 0) { out[o++] = 0x18; o += put_varint(out + o, (uint64_t)disp); }
    if (units) { out[o++] = 0x20; o += put_varint(out + o, units); }
    return (int)o;
}

#ifndef NAVX_HOST_TEST
static uint32_t rd(const void *p, unsigned off) { uint32_t v; memcpy(&v, (const char *)p + off, 4); return v; }
static void wr(void *p, unsigned off, uint32_t v) { memcpy((char *)p + off, &v, 4); }
static char current_road[96];
static unsigned logged;
static char lanes_last[NAV_LANES_BODY];
static int lanes_last_n = -1;
static unsigned lanes_seq, lanes_logged;

/* steps[0].lanes of a 0x8006 body -> /tmp/sq5_aa_lanes, only when they changed (lanes=0 when none).
 * open(O_CREAT) + one pwrite at offset 0, never truncated; a failed write is retried on the next 0x8006. */
static void lanes_publish(const unsigned char *body, unsigned n)
{
    char text[NAV_LANES_BODY], rec[NAV_LANES_REC]; int k = nav_state_lanes(body, n, text), fd;
    if (k == lanes_last_n && !strcmp(text, lanes_last)) return;
    if (!nav_lanes_record(lanes_seq + 1, k, text, rec)) return;
    fd = open(NAV_LANES_PATH, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) return;
    if (pwrite(fd, rec, NAV_LANES_REC, 0) == NAV_LANES_REC) {
        lanes_seq++; lanes_last_n = k; strcpy(lanes_last, text);
        if (lanes_logged++ < 64) probe_log("navxlate.lanes seq=%u lanes=%d %s", lanes_seq, k, text);
    }
    close(fd);
}

/* Called for every routed message before gal sees it. shared = shared_ptr<IoBuffer>: +4 -> IoBuffer
 * {base +0, off +8, end +12}; the payload starts with the 2-byte message id. */
void live_nav_translate(unsigned char ch, const void *shared)
{
    void *io; uint32_t base, off, end; unsigned char *p, tmp[512]; unsigned n, id; int m;
    if (ch != NAV_CHANNEL || !shared) return;
    io = (void *)(uintptr_t)rd(shared, 4);
    if (!io) return;
    base = rd(io, 0); off = rd(io, 8); end = rd(io, 12);
    if (!base || end < off + 2 || end - off > 64u * 1024u) return;
    p = (unsigned char *)(uintptr_t)(base + off); n = end - off;
    id = ((unsigned)p[0] << 8) | p[1];
    if (id != 0x8006 && id != 0x8007) return;
    if (access("/fs/sda0/sq5_cluster_navxlate_off", F_OK) == 0) return;
    if (logged < 6) {                                 /* raw sample to check the parser against real data */
        char hex[3 * 96 + 1]; unsigned i, k = n < 96 ? n : 96;
        for (i = 0; i < k; i++) { static const char d[] = "0123456789abcdef"; hex[3*i] = d[p[i] >> 4]; hex[3*i+1] = d[p[i] & 15]; hex[3*i+2] = ' '; }
        hex[3 * k] = 0;
        probe_log("navxlate.raw id=0x%04x bytes=%u %s", id, n, hex);
    }
    if (id == 0x8006) lanes_publish(p + 2, n - 2);    /* reads the body before the rewrite below */
    if (id == 0x8007) m = nav_position_to_legacy(p + 2, n - 2, tmp, sizeof(tmp), current_road, sizeof(current_road));
    else m = nav_state_to_legacy(p + 2, n - 2, tmp, sizeof(tmp), current_road[0] ? current_road : NULL);
    if (m <= 0 || (unsigned)m + 2 > n) {
        if (logged++ < 32) probe_log("navxlate.skip id=0x%04x bytes=%u result=%d", id, n, m);
        return;
    }
    p[0] = 0x80; p[1] = id == 0x8006 ? 0x04 : 0x05;
    memcpy(p + 2, tmp, (size_t)m);
    wr(io, 12, off + 2 + (uint32_t)m);
    if (logged++ < 32) probe_log("navxlate id=0x%04x->0x%04x bytes=%u->%u", id, id == 0x8006 ? 0x8004 : 0x8005, n, (unsigned)m + 2);
}
#endif
