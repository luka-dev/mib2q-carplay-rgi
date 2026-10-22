/*
 * Cluster layout options: the hook's half of /mnt/app/carplay_layout.cfg.
 *
 * One small text file, shared with the Java side (com.luka.carplay.cluster.ClusterLayoutConfig):
 *
 *     # comment
 *     full    270 90 900 290      safe area of view area 0 (fullscreen)      x y w h
 *     sport    90 90 360 300      safe area of view area 1 (compact, Sport)  x y w h
 *     classic 540 90 360 300      safe area of view area 2 (compact, Classic) x y w h
 *     cards   right               phone turn card + ETA column: right | left | center
 *     eta     yes                 phone ETA in that column: yes | no
 *     speed_limit no              phone speed-limit sign on the cluster: yes | no
 *     car_profile auto            auto = use the per-car safe areas below | off = built-in only
 *     view_source auto            (Java) auto | choice | vc
 *     compact auto                (Java) auto | sport | classic
 *
 * Per-car profile: the HMI (com.luka.carplay.cluster.VehicleLayoutProfile) writes
 * /tmp/carplay_layout.vehicle with the safe areas for the car's stock Layout class (Q7 today;
 * nothing for the A5-measured B9).  Order: built-in < car profile < user file, and the user
 * file's "car_profile off" skips the profile (cl_layout_build).
 *
 * The hook reads the files when it builds the cluster display for the phone (every /info),
 * so a change applies on the next phone connection.  CarPlay has no live safe-area command:
 * updateViewArea only selects one of the areas declared at negotiation.
 *
 * Every line is checked on its own.  A bad line is ignored and logged, and that setting keeps
 * its built-in value; nothing in the file can widen the visible frame (always the full panel,
 * CarKit rejects the stream otherwise).  With no file the result is byte-identical to the
 * compiled defaults, so an unconfigured car behaves exactly as before.
 *
 * Pure parsing and formatting, header-only so the host tests use the exact code.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ALTSCREEN_CLUSTER_LAYOUT_H
#define ALTSCREEN_CLUSTER_LAYOUT_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CLUSTER_LAYOUT_CFG      "/mnt/app/carplay_layout.cfg"
#define CLUSTER_LAYOUT_VEHICLE  "/tmp/carplay_layout.vehicle"
#define CLUSTER_LAYOUT_MAX_FILE 4096

#define CL_CANVAS_W 1440
#define CL_CANVAS_H 542

enum { CL_AREA_FULL = 0, CL_AREA_SPORT = 1, CL_AREA_CLASSIC = 2, CL_AREA_COUNT = 3 };
enum { CL_CARDS_RIGHT = 0, CL_CARDS_LEFT = 1, CL_CARDS_CENTER = 2 };

typedef struct { int x, y, w, h; } cl_rect_t;

typedef struct {
    cl_rect_t safe[CL_AREA_COUNT];
    unsigned  safe_from_file;    /* bit per area: value came from the user file */
    unsigned  safe_from_vehicle; /* bit per area: value came from the car profile */
    int       car_profile;       /* 1 = auto (default), 0 = off */
    int       cards;             /* CL_CARDS_* */
    int       eta;               /* 1 = yes */
    int       speed_limit;       /* 1 = yes */
    int       options_from_file; /* any of cards/eta/speed_limit came from the file */
    int       rejected;          /* lines ignored as invalid or unknown */
    int       first_bad_line;    /* 1-based, 0 = none */
} cl_layout_t;

/* CarPlaySupport (CPSInstrumentClusterCardLayout, docs/cluster-and-rgi/THIRD_PARTY_NAV_APPS.md):
 * the card platter is clamp(safeW, 165, 300) wide, ETA needs safe height > 202, and a safe area
 * narrower than ~498 px needs 0.3 * height > ~87 px or the app shows no cluster content at all.
 * Keep every configurable area inside those limits. */
#define CL_MIN_W           165
#define CL_MIN_H           203
#define CL_NARROW_W        498
#define CL_NARROW_MIN_H    290

static inline void cl_layout_defaults(cl_layout_t *l, const cl_rect_t def[CL_AREA_COUNT])
{
    int i;
    memset(l, 0, sizeof(*l));
    for (i = 0; i < CL_AREA_COUNT; i++) l->safe[i] = def[i];
    l->cards = CL_CARDS_RIGHT;
    l->car_profile = 1;
    l->eta = 1;
    l->speed_limit = 0;
}

static inline int cl_rect_valid(const cl_rect_t *r)
{
    if (r->x < 0 || r->y < 0 || r->w < CL_MIN_W || r->h < CL_MIN_H) return 0;
    if (r->x > CL_CANVAS_W - r->w || r->y > CL_CANVAS_H - r->h) return 0;
    if (r->w < CL_NARROW_W && r->h < CL_NARROW_MIN_H) return 0;
    return 1;
}

static inline int cl_lower(int c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

/* Case-insensitive compare of token [s, s+n) with a lowercase literal. */
static inline int cl_tok_is(const char *s, size_t n, const char *lit)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (!lit[i] || cl_lower((unsigned char)s[i]) != lit[i]) return 0;
    }
    return lit[n] == 0;
}

/* Splits one line into at most 6 tokens; returns the count, 7 if there are more. */
static inline int cl_tokens(const char *s, const char *end, const char **tok, size_t *len)
{
    int n = 0;
    while (s < end) {
        while (s < end && (*s == ' ' || *s == '\t' || *s == '\r')) s++;
        if (s >= end || *s == '#') break;
        if (n == 6) return 7;
        tok[n] = s;
        while (s < end && *s != ' ' && *s != '\t' && *s != '\r' && *s != '#') s++;
        len[n] = (size_t)(s - tok[n]);
        n++;
    }
    return n;
}

/* Non-negative decimal integer below 10000; -1 otherwise. */
static inline int cl_uint(const char *s, size_t n)
{
    int v = 0;
    size_t i;
    if (n == 0 || n > 4) return -1;
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static inline int cl_yes_no(const char *s, size_t n)
{
    if (cl_tok_is(s, n, "yes") || cl_tok_is(s, n, "on") || cl_tok_is(s, n, "1")) return 1;
    if (cl_tok_is(s, n, "no") || cl_tok_is(s, n, "off") || cl_tok_is(s, n, "0")) return 0;
    return -1;
}

static inline void cl_reject(cl_layout_t *l, int line)
{
    l->rejected++;
    if (!l->first_bad_line) l->first_bad_line = line;
}

/* Applies the file text on top of the defaults already in *l.  Returns the number of lines
 * that changed something. */
static inline int cl_layout_parse(cl_layout_t *l, const char *text, size_t size)
{
    const char *p = text, *end = text + size;
    int line = 0, applied = 0;
    while (p < end) {
        const char *eol = p;
        const char *tok[6];
        size_t len[6];
        int n;
        while (eol < end && *eol != '\n') eol++;
        line++;
        n = cl_tokens(p, eol, tok, len);
        p = eol < end ? eol + 1 : end;
        if (n == 0) continue;
        if (cl_tok_is(tok[0], len[0], "full") || cl_tok_is(tok[0], len[0], "sport") ||
            cl_tok_is(tok[0], len[0], "classic")) {
            int area = cl_tok_is(tok[0], len[0], "full") ? CL_AREA_FULL
                : (cl_tok_is(tok[0], len[0], "sport") ? CL_AREA_SPORT : CL_AREA_CLASSIC);
            cl_rect_t r;
            if (n != 5) { cl_reject(l, line); continue; }
            r.x = cl_uint(tok[1], len[1]); r.y = cl_uint(tok[2], len[2]);
            r.w = cl_uint(tok[3], len[3]); r.h = cl_uint(tok[4], len[4]);
            if (!cl_rect_valid(&r)) { cl_reject(l, line); continue; }
            l->safe[area] = r;
            l->safe_from_file |= 1u << area;
            applied++;
        } else if (cl_tok_is(tok[0], len[0], "cards")) {
            int v = -1;
            if (n == 2) {
                if (cl_tok_is(tok[1], len[1], "right")) v = CL_CARDS_RIGHT;
                else if (cl_tok_is(tok[1], len[1], "left")) v = CL_CARDS_LEFT;
                else if (cl_tok_is(tok[1], len[1], "center") || cl_tok_is(tok[1], len[1], "centre") ||
                         cl_tok_is(tok[1], len[1], "top")) v = CL_CARDS_CENTER;
            }
            if (v < 0) { cl_reject(l, line); continue; }
            l->cards = v;
            l->options_from_file = 1;
            applied++;
        } else if (cl_tok_is(tok[0], len[0], "eta") || cl_tok_is(tok[0], len[0], "speed_limit")) {
            int v = n == 2 ? cl_yes_no(tok[1], len[1]) : -1;
            if (v < 0) { cl_reject(l, line); continue; }
            if (cl_tok_is(tok[0], len[0], "eta")) l->eta = v; else l->speed_limit = v;
            l->options_from_file = 1;
            applied++;
        } else if (cl_tok_is(tok[0], len[0], "car_profile")) {
            int v = -1;
            if (n == 2) {
                if (cl_tok_is(tok[1], len[1], "auto") || cl_tok_is(tok[1], len[1], "on")) v = 1;
                else if (cl_tok_is(tok[1], len[1], "off")) v = 0;
            }
            if (v < 0) { cl_reject(l, line); continue; }
            l->car_profile = v;
            applied++;
        } else if (cl_tok_is(tok[0], len[0], "view_source") || cl_tok_is(tok[0], len[0], "compact")) {
            /* Java's keys (ClusterLayoutConfig); not this side's to judge. */
        } else {
            cl_reject(l, line);
        }
    }
    return applied;
}

/* Built-in < car profile (vehicle text, safe-area lines only) < user file.  The user file's
 * car_profile decides whether the profile is used, so it is read first; rejected lines and the
 * first bad line are reported for the user file only (the profile is generated). */
static inline void cl_layout_build(cl_layout_t *l, const cl_rect_t def[CL_AREA_COUNT],
                                   const char *vehicle, size_t vlen, const char *user, size_t ulen)
{
    cl_layout_t probe;
    int i;
    cl_layout_defaults(&probe, def);
    if (user) cl_layout_parse(&probe, user, ulen);
    cl_layout_defaults(l, def);
    if (vehicle && probe.car_profile) {
        cl_layout_t veh;
        cl_layout_defaults(&veh, def);
        cl_layout_parse(&veh, vehicle, vlen);
        for (i = 0; i < CL_AREA_COUNT; i++) {
            if (veh.safe_from_file & (1u << i)) {
                l->safe[i] = veh.safe[i];
                l->safe_from_vehicle |= 1u << i;
            }
        }
    }
    if (user) {
        cl_layout_parse(l, user, ulen);
        l->safe_from_vehicle &= ~l->safe_from_file;
    }
}

/* Maps cluster scene URLs (keys and values: docs/cluster-and-rgi/THIRD_PARTY_NAV_APPS.md).
 * With default options both equal luka's fixed URLs byte for byte. */
static inline int cl_url_rgi_off(const cl_layout_t *l, char *buf, size_t n)
{
    const char *layout = l->cards == CL_CARDS_LEFT ? "leftAligned"
        : (l->cards == CL_CARDS_CENTER ? "topAligned" : "rightAligned");
    int r = snprintf(buf, n, "maps:/car/instrumentcluster?showETA=%s&maneuverLayout=%s&showSpeedLimit=%s",
                     l->eta ? "yes" : "no", layout, l->speed_limit ? "yes" : "no");
    return (r > 0 && (size_t)r < n) ? 0 : -1;
}

static inline int cl_url_rgi_on(const cl_layout_t *l, char *buf, size_t n)
{
    int r = snprintf(buf, n, "maps:/car/instrumentcluster/map?showETA=no&maneuverLayout=no&showSpeedLimit=%s",
                     l->speed_limit ? "yes" : "no");
    return (r > 0 && (size_t)r < n) ? 0 : -1;
}

#endif /* ALTSCREEN_CLUSTER_LAYOUT_H */
