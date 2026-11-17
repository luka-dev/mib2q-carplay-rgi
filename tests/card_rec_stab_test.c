/*
 * card_rec stabiliser: the time-driven feed (card_rec_stab_push_at /
 * card_rec_stab_advance, card_rec_stab.h) against the frame rule
 * (card_rec_stab_push).  Always runs on synthetic results; with
 * CARD_REC_FIXTURES the reference decisions of goldens.txt are replayed as
 * sequences, too (the parity of those decisions is card_rec_test's job).
 *
 *   parity     frames exactly 33.333 ms apart (floor / round ms, with and
 *              without advance() calls between them): every output and the
 *              final state equal the frame rule's, for the pure stabiliser cases,
 *              random sequences (n 5, 1, 3) and the fixture sets
 *   iOS burst  16 frames at 60 fps then silence with advance() every 50 ms:
 *              a fresh card shows after COLD_K * n ticks, a half-changed one
 *              after SPLIT_K * n ticks of wall time (the frame rule: never)
 *   stall      a burst whose last frame differs from those before it is never
 *              replayed into a publish; 3 alike ticks are
 *   reject     a REJECT withdraws at once; silence keeps it withdrawn; the
 *              memory republishes.  Inside one tick a change (REJECT, other
 *              key, upward jump) is pushed on top, a repeat counts once
 *   flip       LEFT / RIGHT flip-flops at 60 fps never show, at any phase
 *   confirm    CONF counts distinct ticks of one input (text included); a
 *              frame late after advance() counts in the next tick
 *   coast      a gap of d ticks equals d pushes, field by field, for every
 *              kind of result and across AGE_MAX; any gap costs <= UNIT_K n + 1
 *   clock      time going back > 1 s (reset), stale stamps <= 1 s (count now),
 *              advance() back (nothing), int64 extremes, NULL (reset, wall time)
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define _POSIX_C_SOURCE 200809L
#include "../altscreen_render/card_rec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static char g_summary[512];                     /* the one OK line */
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

#define N5 CARD_REC_STAB_N
#define C_TICKS (CARD_REC_STAB_COLD_K * N5)
#define SP_TICKS (CARD_REC_STAB_SPLIT_K * N5)
#define AGE_MAX CARD_REC_STAB_AGE_MAX
#define T60(j) ((int64_t)(j) * 50 / 3)          /* 60 fps frame j, ms */
#define T30(j) ((int64_t)(j) * 100 / 3)         /* 30 fps frame j, ms */

/* case tokens: N no card; X reject; S<m> STRAIGHT "<m> m" (internal);
 * R<m> / L<m> right / left "<m> m"; I<m> right "<m / 1609> mi"; E<e> ring
 * exit e at 300 m (as card_rec_test_api.h stab_frame) */
static void mk(const char *t, card_rec_result_t *r) {
    int v = atoi(t + 1);
    memset(r, 0, sizeof(*r));
    r->accept = !strchr("NXS", t[0]);
    r->reason = t[0] == 'N' ? CARD_REC_R_NO_CARD_BOX : t[0] == 'X' ? CARD_REC_R_SOFT_EDGES
              : t[0] == 'S' ? CARD_REC_R_CLASS_NOT_PUBLISHED : CARD_REC_R_OK;
    r->internal.distance_m = t[0] == 'S' ? v : -1;
    r->internal.cls = CARD_REC_CLASS_STRAIGHT;
    r->internal.cp_maneuver_type = 3;
    r->internal.distance_unit = CARD_REC_UNIT_M;
    r->distance_m = r->accept ? (t[0] == 'E' ? 300 : v) : -1;
    r->distance_unit = t[0] == 'I' ? CARD_REC_UNIT_MI : CARD_REC_UNIT_M;
    r->cls = t[0] == 'L' ? CARD_REC_CLASS_LEFT_TURN : t[0] == 'E' ? CARD_REC_CLASS_ROUNDABOUT_NUMBERED
                                                                  : CARD_REC_CLASS_RIGHT_TURN;
    r->cp_maneuver_type = t[0] == 'L' ? 1 : t[0] == 'E' ? 27 + v : 2;
    r->exit_number = t[0] == 'E' ? v : 0;
    r->traffic_side = t[0] == 'E' ? 'R' : 0;
    if (t[0] == 'I')
        snprintf(r->distance_text, sizeof(r->distance_text), "%d mi", v / 1609);
    else if (r->accept)
        snprintf(r->distance_text, sizeof(r->distance_text), "%d m", r->distance_m);
}

/* nearest tick of t on a grid anchored at t0 (t >= t0) */
static int64_t tk(int64_t t0, int64_t t) { return (3 * (t - t0) + 50) / 100; }
/* earliest time whose tick is K (K >= 1) */
static int64_t t_of(int64_t t0, int64_t k) { return t0 + (100 * k - 48) / 3; }

static int same_rd(const card_rec_stab_read_t *a, const card_rec_stab_read_t *b) {
    return a->cls == b->cls && a->cp_maneuver_type == b->cp_maneuver_type && a->exit_number == b->exit_number &&
           a->traffic_side == b->traffic_side && a->m == b->m && a->imp == b->imp && a->age == b->age &&
           a->cnt == b->cnt;
}

static int same_shown(const card_rec_shown_t *a, const card_rec_shown_t *b) {
    return a->cls == b->cls && a->cp_maneuver_type == b->cp_maneuver_type && a->exit_number == b->exit_number &&
           a->traffic_side == b->traffic_side && a->distance_m == b->distance_m &&
           !strcmp(a->distance_text, b->distance_text);
}

/* every field of the frame rule's state */
static int same_core(const card_rec_stab_t *a, const card_rec_stab_t *b) {
    int i, ok = a->n == b->n && a->pub == b->pub && a->pub_imp == b->pub_imp && a->run_n == b->run_n &&
                a->last_m == b->last_m && a->run_need == b->run_need && same_rd(&a->run, &b->run) &&
                a->cand_m == b->cand_m && a->cand_n == b->cand_n && a->cand_need == b->cand_need &&
                a->cand_imp == b->cand_imp && !strcmp(a->cand_text, b->cand_text) &&
                same_shown(&a->shown, &b->shown) && a->mem_valid == b->mem_valid && same_rd(&a->mem, &b->mem) &&
                a->hist_n == b->hist_n && a->empty_age == b->empty_age && a->ep_ok == b->ep_ok &&
                a->ep_valid == b->ep_valid && same_rd(&a->ep, &b->ep);
    for (i = 0; ok && i < a->hist_n; ++i)
        ok = same_rd(&a->hist[i], &b->hist[i]);
    return ok;
}

/* ------------------------------------------------------------------ parity */
/* the frame rule vs the timed feed on frames exactly 33.333 ms apart from t0;
 * mode 0 floor ms, 1 round ms, 2 floor ms + advance() 16 and 32 ms after each */
static void parity_seq(const card_rec_result_t *seq, int len, int n, int mode, int64_t t0, const char *what) {
    static card_rec_stab_clock_t c;
    card_rec_stab_t f;
    card_rec_shown_t sa, sb;
    int i, a, b, bad = 0;
    card_rec_stab_init(&f, n);
    card_rec_stab_clock_init(&c, n);
    for (i = 0; i < len && !bad; ++i) {
        int64_t t = t0 + (mode == 1 ? ((int64_t)i * 100 + 1) / 3 : T30(i));
        a = card_rec_stab_push(&f, &seq[i], &sa);
        b = card_rec_stab_push_at(&c, &seq[i], t, &sb);
        bad = a != b || (a && !same_shown(&sa, &sb));
        if (mode == 2 && !bad) {
            b = card_rec_stab_advance(&c, t + 16, &sb) == a;
            b &= card_rec_stab_advance(&c, t + 32, &sb) == a;
            bad = !b || (a && !same_shown(&sa, &sb));
        }
    }
    CHECK(!bad && same_core(&f, &c.core), "parity %s n %d mode %d: frame %d differs", what, n, mode, i - 1);
}

static void parity_all(const card_rec_result_t *seq, int len, int n, const char *what) {
    static const int64_t t0s[] = {0, 1000003, -7777, INT64_MAX / 2, INT64_MIN + 1000};
    int m;
    size_t k;
    for (k = 0; k < sizeof(t0s) / sizeof(t0s[0]); ++k)
        for (m = 0; m < 3; ++m)
            parity_seq(seq, len, n, m, t0s[k], what);
}

#define SEQ_MAX 8192
static card_rec_result_t g_seq[SEQ_MAX];

static int tokens(const char *s, card_rec_result_t *out, int max) {
    char tok[16];
    int n = 0, k, rep, used;
    while (sscanf(s, " %15s%n", tok, &used) == 1) {
        char *star = strchr(tok, '*');
        s += used;
        rep = star ? atoi(star + 1) : 1;
        if (star)
            *star = 0;
        for (k = 0; k < rep && n < max; ++k)
            mk(tok, &out[n++]);
    }
    return n;
}

static uint32_t g_rnd = 20261117u;
static uint32_t rnd(uint32_t m) { g_rnd = g_rnd * 1103515245u + 12345u; return (g_rnd >> 8) % m; }

static void parity_tests(void) {
    static const char *const cases[] = {   /* the pure stabiliser case inputs (card_rec_test_api.h) */
        "N R300*12", "R300*41", "N R300*9 X R300*10", "N R300*10 X R300", "N R300*10 X R300*5",
        "N R10*10 L10*39 L800*40", "N R10*10 R800*39 L800*40", "N R10*10 L10*40", "N R10*10 L800*5",
        "N R300*10 R290*5", "N R300*10 R350*40", "N R300*10 R800 R300*5", "N R300*10 R30 R300*6",
        "N R300*9 L800 R300*9", "N R300 R290 R300 R290 R300 R290 R300 R290 R300 R290", "N I19312*10 R12*90",
        "N E8*10 E3*39", "N R10*10 N S20*3 L20*40", "N L30*10 L20*3 R20*39", "N R300*10 R200*40",
        "N R300*10 X*40 R200*5", "N R13*40", "N R300*9 L800*40", "N R300*4 R290*6", "N S800*40 R300*5"};
    static const char *const kinds = "NXSRLRLRLIEE";
    static const int ms[] = {10, 20, 30, 40, 200, 290, 300, 350, 800, 13, 1609, 19312};
    static const int ns[] = {N5, 1, 3};
    char tok[16], name[32];
    size_t i;
    int s, len, k, j, rep;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        len = tokens(cases[i], g_seq, SEQ_MAX);
        parity_all(g_seq, len, N5, cases[i]);
    }
    for (s = 0; s < 240; ++s) {            /* random runs of random reads */
        for (len = 0; len < 400;) {
            k = kinds[rnd(12)];
            if (k == 'N' || k == 'X')
                snprintf(tok, sizeof(tok), "%c", k);
            else
                snprintf(tok, sizeof(tok), "%c%d", k, k == 'E' ? 1 + (int)rnd(12) : ms[rnd(12)]);
            rep = rnd(5) ? 1 + (int)rnd(12) : 1 + (int)rnd(100);
            for (j = 0; j < rep && len < 400; ++j)
                mk(tok, &g_seq[len++]);
        }
        snprintf(name, sizeof(name), "random %d", s);
        parity_all(g_seq, len, ns[s % 3], name);
    }
}

/* ------------------------------------------------------- fixture sequences */
static int by_name(const char *s, const char *(*fn)(int), int count) {
    int i;
    for (i = 0; i < count && strcmp(s, fn(i)); ++i)
        ;
    return i < count ? i : -1;
}
static const char *reason_s(int i) { return card_rec_reason_name((card_rec_reason_t)i); }
static const char *class_s(int i) { return card_rec_class_name((card_rec_class_t)i); }
static const char *unit_s(int i) { return card_rec_unit_name((card_rec_unit_t)i); }

/* one goldens.txt row -> the result the reference decided (card_rec_test
 * holds the C recogniser to it); returns 0 for a comment / bad row */
static int golden_result(char *line, char *set, card_rec_result_t *r) {
    static const long mm_per[CARD_REC_UNIT_COUNT] = {0, 1000L, 1000000L, 305L, 914L, 1609344L};
    char *t[26], *save, *p;
    int n, unit, v100, cls, why;
    if (line[0] == '#')
        return 0;
    for (n = 0, t[0] = strtok_r(line, " \n", &save); t[n] && n < 25;)
        t[++n] = strtok_r(0, " \n", &save);
    if (n != 25 || (why = by_name(t[8], reason_s, CARD_REC_R_COUNT)) < 0)
        return 0;
    memset(r, 0, sizeof(*r));
    snprintf(set, 16, "%s", t[0]);
    unit = by_name(t[14], unit_s, CARD_REC_UNIT_COUNT);
    v100 = strcmp(t[13], "-") ? atoi(t[13]) : -1;
    cls = by_name(t[10], class_s, CARD_REC_CLASS_COUNT);
    r->accept = !strcmp(t[7], "ACCEPT");
    r->reason = (card_rec_reason_t)why;
    for (p = t[16]; *p; ++p)
        if (*p == '_')
            *p = ' ';
    if (r->reason == CARD_REC_R_CLASS_NOT_PUBLISHED) {   /* the row carries the withheld reading */
        r->internal.cls = (card_rec_class_t)(cls < 0 ? 0 : cls);
        r->internal.cp_maneuver_type = atoi(t[11]);
        r->internal.exit_number = atoi(t[12]);
        r->internal.distance_unit = (card_rec_unit_t)(unit < 0 ? 0 : unit);
        r->internal.distance_m = v100 >= 0 && unit > 0 ? (int)((v100 * mm_per[unit] + 50000) / 100000) : -1;
        r->internal.traffic_side = t[15][0] == '-' ? 0 : t[15][0];
        r->distance_m = -1;
        return 1;
    }
    r->cls = (card_rec_class_t)(cls < 0 ? 0 : cls);
    r->cp_maneuver_type = strcmp(t[11], "-") ? atoi(t[11]) : -1;
    r->exit_number = atoi(t[12]);
    r->distance_unit = (card_rec_unit_t)(unit < 0 ? 0 : unit);
    r->distance_m = v100 >= 0 && unit > 0 ? (int)((v100 * mm_per[unit] + 50000) / 100000) : -1;
    r->traffic_side = t[15][0] == '-' ? 0 : t[15][0];
    snprintf(r->distance_text, sizeof(r->distance_text), "%s", strcmp(t[16], "-") ? t[16] : "");
    return 1;
}

static void fixture_tests(const char *fx) {
    static char sets[16][16];
    static int set_of[SEQ_MAX];
    static card_rec_result_t all[SEQ_MAX];
    char path[1024], line[8192], set[16];
    int n = 0, ns = 0, s, i, len, pub = 0;
    FILE *f;
    snprintf(path, sizeof(path), "%s/goldens.txt", fx);
    if (!(f = fopen(path, "r"))) {
        CHECK(0, "cannot read %s", path);
        return;
    }
    while (n < SEQ_MAX && fgets(line, sizeof(line), f))
        if (golden_result(line, set, &all[n])) {
            for (s = 0; s < ns && strcmp(sets[s], set); ++s)
                ;
            if (s == ns && ns < 16)
                snprintf(sets[ns++], 16, "%s", set);
            set_of[n++] = s;
        }
    fclose(f);
    for (s = 0; s < ns; ++s) {
        for (i = 0, len = 0; i < n; ++i)
            if (set_of[i] == s)
                g_seq[len++] = all[i];
        parity_all(g_seq, len, N5, sets[s]);
    }
    parity_all(all, n, N5, "all fixture rows");
    {   /* sanity: the sequences exercise publishing */
        card_rec_stab_t st;
        card_rec_stab_init(&st, N5);
        for (i = 0; i < n; ++i)
            pub += card_rec_stab_push(&st, &all[i], 0);
    }
    CHECK(n > 1000 && pub > 0, "fixture rows %d, published %d", n, pub);
    snprintf(g_summary + strlen(g_summary), sizeof(g_summary) - strlen(g_summary),
             "; fixture sequences: %d decisions in %d sets, parity", n, ns);
}

#include "card_rec_stab_test_time.h"

int main(void) {
    const char *fx = getenv("CARD_REC_FIXTURES");
    char probe[1024];
    FILE *f;
    parity_tests();
    burst_tests();
    stall_tests();
    reject_tests();
    flip_tests();
    confirm_tests();
    coast_tests();
    clock_tests();
    snprintf(probe, sizeof(probe), "%s/goldens.txt", fx ? fx : "");
    if (fx && *fx && (f = fopen(probe, "r"))) {
        fclose(f);
        fixture_tests(fx);
    } else {
        snprintf(g_summary + strlen(g_summary), sizeof(g_summary) - strlen(g_summary),
                 "; SKIP fixture sequences: %s", fx && *fx ? "no goldens.txt" : "CARD_REC_FIXTURES not set");
    }
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("OK (%s)\n", g_summary);
    return 0;
}
