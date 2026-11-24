/*
 * CarPlay instruction-card recogniser: the temporal publish rule (see
 * card_rec.h, card_rec_stab.h).
 *
 *   frame rule   card_rec_stab_push: one result per 30 fps frame
 *   timed feed   card_rec_stab_push_at / card_rec_stab_advance: the frame rule
 *                pushed once per 30 fps tick of wall time (and per change
 *                inside one), silence replayed
 *
 * Integer only, no allocation.  Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define CARD_REC_INTERNAL
#include "card_rec.h"

#include <string.h>

/* -------------------------------------------------------------- stabiliser */
#define AGE_MAX CARD_REC_STAB_AGE_MAX

void card_rec_stab_init(card_rec_stab_t *s, int n) {
    if (!s)
        return;
    memset(s, 0, sizeof(*s));       /* a zeroed state never publishes early: not fresh */
    s->n = n < 1 || n > 1000 ? CARD_REC_STAB_N : n;
    s->mem.age = s->empty_age = AGE_MAX;
}

static int sat(int v) { return v < AGE_MAX ? v + 1 : AGE_MAX; }

static int same_key(const card_rec_stab_read_t *a, const card_rec_stab_read_t *b) {
    return a->cls == b->cls && a->cp_maneuver_type == b->cp_maneuver_type && a->exit_number == b->exit_number &&
           a->traffic_side == b->traffic_side;
}

static int same_read(const card_rec_stab_read_t *a, const card_rec_stab_read_t *b) {
    return same_key(a, b) && a->m == b->m && a->imp == b->imp;
}

static int mk_read(card_rec_stab_read_t *rd, card_rec_class_t cls, int cp, int ex, char side, int m,
                   card_rec_unit_t u) {
    memset(rd, 0, sizeof(*rd));
    rd->cls = cls;
    rd->cp_maneuver_type = cp;
    rd->exit_number = ex;
    rd->traffic_side = side;
    rd->m = m;
    rd->imp = u == CARD_REC_UNIT_FT || u == CARD_REC_UNIT_YD || u == CARD_REC_UNIT_MI;
    return 1;
}

/* frames needed for candidate c against one reference read */
static int rel_need(const card_rec_stab_read_t *ref, const card_rec_stab_read_t *c, int n) {
    int kc = !same_key(c, ref), dc = c->m != ref->m, tol;
    if (c->imp != ref->imp)
        return CARD_REC_STAB_UNIT_K * n;
    if (kc == dc)
        return n;
    if (kc || c->m > ref->m)
        return CARD_REC_STAB_SPLIT_K * n;
    tol = cr_max(CARD_REC_STAB_DROP_ABS_M, (int)((long)ref->m * CARD_REC_STAB_DROP_PM / 1000)) +
          CARD_REC_STAB_V_MAX_MM * cr_min(ref->age, CARD_REC_STAB_HIST_AGE) / 1000;
    return ref->m - c->m > tol ? CARD_REC_STAB_SPLIT_K * n : n;
}

/* identical reads needed to publish candidate c; ladder: metric 'm' */
static int need_n(const card_rec_stab_t *s, const card_rec_stab_read_t *c, int ladder) {
    int i, need = s->n;
    if (s->mem_valid && same_read(&s->mem, c))
        return s->n;                /* the published read again (after a reject / misread) */
    if (s->mem_valid)
        need = cr_max(need, rel_need(&s->mem, c, s->n));
    for (i = 0; i < s->hist_n; ++i)
        if (s->hist[i].age <= CARD_REC_STAB_HIST_AGE && !same_read(&s->hist[i], c))
            need = cr_max(need, rel_need(&s->hist[i], c, s->n));
    if (ladder && c->m % CARD_REC_STAB_LADDER_M)
        need = cr_max(need, CARD_REC_STAB_SPLIT_K * s->n);
    return need;
}

/* frames of one key a run needs */
static int run_need(const card_rec_stab_t *s, int fresh) {
    int i;
    for (i = 0; i < s->hist_n && !s->mem_valid; ++i)
        if (s->hist[i].age <= CARD_REC_STAB_HIST_AGE && s->hist[i].cnt >= CARD_REC_STAB_SPLIT_K * s->n)
            return s->n;
    return s->mem_valid ? s->n : (fresh ? CARD_REC_STAB_COLD_K : CARD_REC_STAB_SPLIT_K) * s->n;
}

/* move-to-front insert of read rd (age 0) into the history */
static void hist_push(card_rec_stab_t *s, const card_rec_stab_read_t *rd) {
    card_rec_stab_read_t e = *rd;
    int i, j = s->hist_n;
    e.age = 0;
    e.cnt = 1;
    for (i = 0; i < s->hist_n; ++i)
        if (same_read(&s->hist[i], rd)) {
            e.cnt = sat(s->hist[i].cnt);
            j = i;
            break;
        }
    if (j == CARD_REC_STAB_HIST_N)
        j = CARD_REC_STAB_HIST_N - 1;   /* full: the oldest read drops out */
    else if (j == s->hist_n)
        s->hist_n++;
    memmove(&s->hist[1], &s->hist[0], (size_t)j * sizeof(s->hist[0]));
    s->hist[0] = e;
}

int card_rec_stab_push(card_rec_stab_t *s, const card_rec_result_t *r, card_rec_shown_t *shown) {
    card_rec_stab_read_t rd;
    int i, have = 0, ladder = 0, newc = 1, need, n;
    if (shown)
        memset(shown, 0, sizeof(*shown));
    if (!s)
        return 0;
    if (s->n < 1 || s->n > 1000)
        s->n = CARD_REC_STAB_N;
    n = s->n;
    if (r && r->accept && r->distance_m >= 0) {
        have = mk_read(&rd, r->cls, r->cp_maneuver_type, r->exit_number, r->traffic_side, r->distance_m,
                       r->distance_unit);
        ladder = r->distance_unit == CARD_REC_UNIT_M;
    } else if (r && r->reason == CARD_REC_R_CLASS_NOT_PUBLISHED && r->internal.distance_m >= 0) {
        have = mk_read(&rd, r->internal.cls, r->internal.cp_maneuver_type, r->internal.exit_number,
                       r->internal.traffic_side, r->internal.distance_m, r->internal.distance_unit);
    }
    for (i = 0; i < s->hist_n; ++i)
        s->hist[i].age = sat(s->hist[i].age);
    s->mem.age = sat(s->mem.age);
    s->empty_age = sat(s->empty_age);
    if (r && (r->reason == CARD_REC_R_NO_CARD_BOX || r->reason == CARD_REC_R_NO_BLACK_BOX ||
              r->reason == CARD_REC_R_BLACK_BOX_TOO_SMALL)) {
        s->empty_age = 0;           /* nothing card-like on screen: the next card is fresh */
        s->ep_ok = 1;
        s->ep_valid = 0;
    }
    if (have && s->ep_ok) {
        if (!s->ep_valid && s->empty_age <= CARD_REC_STAB_FRESH_MAX) {
            s->ep = rd;
            s->ep_valid = 1;
        } else if (!s->ep_valid || !same_key(&s->ep, &rd)) {
            s->ep_ok = 0;
        }
    }
    if (!r || !r->accept || r->distance_m < 0) {    /* any REJECT resets and withdraws */
        if (have)
            hist_push(s, &rd);
        s->pub = s->run_n = s->last_m = s->run_need = 0;
        s->cand_m = s->cand_n = s->cand_need = s->cand_imp = 0;
        s->cand_text[0] = 0;
        return 0;
    }
    if (!s->run_n || !same_key(&rd, &s->run) || rd.m > s->last_m) {
        int newkey = !s->run_n || !same_key(&rd, &s->run);  /* else an upward jump */
        card_rec_stab_read_t pk;
        mk_read(&pk, s->shown.cls, s->shown.cp_maneuver_type, s->shown.exit_number, s->shown.traffic_side, 0,
                CARD_REC_UNIT_NONE);
        if (s->pub && (!same_key(&pk, &rd) || !newkey))
            s->pub = 0;             /* no longer on screen / an upward jump: withdraw */
        s->run_need = run_need(s, newkey && s->ep_ok && s->ep_valid && same_key(&s->ep, &rd));
        s->run = rd;
        s->run_n = 1;
        s->last_m = rd.m;
    } else {
        s->run_n = sat(s->run_n);
        s->last_m = rd.m;
        newc = !(rd.m == s->cand_m && !strcmp(r->distance_text, s->cand_text));
    }
    if (newc) {
        s->cand_need = need_n(s, &rd, ladder);
        s->cand_m = rd.m;
        s->cand_n = 1;
        s->cand_imp = rd.imp;
        memcpy(s->cand_text, r->distance_text, CARD_REC_TEXT_MAX);
        s->cand_text[CARD_REC_TEXT_MAX - 1] = 0;
    } else {
        s->cand_n = sat(s->cand_n);
    }
    hist_push(s, &rd);
    need = s->cand_need;
    if (s->pub && need > n)                         /* contradicts the shown card */
        s->pub = 0;
    if (s->cand_n >= need && (s->pub ? s->cand_m <= s->shown.distance_m : s->run_n >= cr_max(s->run_need, need))) {
        s->shown.cls = rd.cls;
        s->shown.cp_maneuver_type = rd.cp_maneuver_type;
        s->shown.exit_number = rd.exit_number;
        s->shown.traffic_side = rd.traffic_side;
        s->shown.distance_m = s->cand_m;
        memcpy(s->shown.distance_text, s->cand_text, sizeof(s->cand_text));
        s->pub = 1;
        s->pub_imp = s->cand_imp;
    }
    if (s->pub) {                                   /* memory = what is published */
        mk_read(&s->mem, s->shown.cls, s->shown.cp_maneuver_type, s->shown.exit_number, s->shown.traffic_side,
                s->shown.distance_m, CARD_REC_UNIT_NONE);
        s->mem.imp = s->pub_imp;
        s->mem_valid = 1;
        if (shown)
            *shown = s->shown;
    }
    return s->pub;
}

/* -------------------------------------------------------------- timed feed */
/* What the frame rule reads from a result: an ACCEPT with a distance (a read,
 * run and candidate), a CLASS_NOT_PUBLISHED reading (a read, history only), a
 * no-card frame (freshness) or any other REJECT.  The four partition every
 * result exactly as card_rec_stab_push branches on it. */
enum { IN_REJECT = 0, IN_ACCEPT, IN_READ, IN_NOCARD };

static int in_kind(const card_rec_result_t *r) {
    if (r->accept && r->distance_m >= 0)
        return IN_ACCEPT;
    if (r->reason == CARD_REC_R_NO_CARD_BOX || r->reason == CARD_REC_R_NO_BLACK_BOX ||
        r->reason == CARD_REC_R_BLACK_BOX_TOO_SMALL)
        return IN_NOCARD;
    return r->reason == CARD_REC_R_CLASS_NOT_PUBLISHED && r->internal.distance_m >= 0 ? IN_READ : IN_REJECT;
}

static int imp_of(card_rec_unit_t u) {
    return u == CARD_REC_UNIT_FT || u == CARD_REC_UNIT_YD || u == CARD_REC_UNIT_MI;
}

/* 1 when the frame rule cannot tell a from b */
static int same_input(const card_rec_result_t *a, const card_rec_result_t *b) {
    const card_rec_reading_t *x = &a->internal, *y = &b->internal;
    int k = in_kind(a);
    if (k != in_kind(b))
        return 0;
    if (k == IN_ACCEPT)
        return a->cls == b->cls && a->cp_maneuver_type == b->cp_maneuver_type && a->exit_number == b->exit_number &&
               a->traffic_side == b->traffic_side && a->distance_m == b->distance_m &&
               imp_of(a->distance_unit) == imp_of(b->distance_unit) &&
               (a->distance_unit == CARD_REC_UNIT_M) == (b->distance_unit == CARD_REC_UNIT_M) &&
               !strncmp(a->distance_text, b->distance_text, CARD_REC_TEXT_MAX);
    if (k == IN_READ)
        return x->cls == y->cls && x->cp_maneuver_type == y->cp_maneuver_type && x->exit_number == y->exit_number &&
               x->traffic_side == y->traffic_side && x->distance_m == y->distance_m &&
               imp_of(x->distance_unit) == imp_of(y->distance_unit);
    return 1;
}

/* nearest 30 fps tick of t on the grid anchored at t0: round(3 (t - t0) / 100)
 * half up (floor((3 (t - t0) + 50) / 100)), exact for every int64 pair -
 * 100 ms = 3 ticks is split off first, so nothing overflows */
static int64_t tick_of(int64_t t0, int64_t t) {
    uint64_t a = t >= t0 ? (uint64_t)t - (uint64_t)t0 : (uint64_t)t0 - (uint64_t)t;
    int64_t q3 = (int64_t)(a / 100u) * 3;
    unsigned r3 = (unsigned)(a % 100u) * 3u;
    return t >= t0 ? q3 + (int64_t)((r3 + 50u) / 100u) : -(q3 + (int64_t)((r3 + 49u) / 100u));
}

/* d (> 0) saturating increments of a counter at once */
static int add_sat(int v, int64_t d) { return d >= (int64_t)AGE_MAX - v ? AGE_MAX : v + (int)d; }

/* d ticks of time without evidence: every age advances, nothing else moves */
static void age_only(card_rec_stab_t *s, int64_t d) {
    int i;
    for (i = 0; i < s->hist_n; ++i)
        s->hist[i].age = add_sat(s->hist[i].age, d);
    s->mem.age = add_sat(s->mem.age, d);
    s->empty_age = add_sat(s->empty_age, d);
}

/* d more pushes of a result of kind k, valid after L = UNIT_K * n + 1 pushes of
 * it in a row: then the run is one key with an unchanged candidate, published
 * (ACCEPT: cand_n, run_n >= L > every need), the read heads the history and
 * the freshness bookkeeping is settled, so a push only moves counters
 * (tests: equal to d single pushes, field by field) */
static void coast(card_rec_stab_t *s, int64_t d, int k) {
    int i, read = k == IN_ACCEPT || k == IN_READ;
    for (i = read; i < s->hist_n; ++i)          /* a read is pushed again: age 0, cnt + 1 */
        s->hist[i].age = add_sat(s->hist[i].age, d);
    if (read && s->hist_n)
        s->hist[0].cnt = add_sat(s->hist[0].cnt, d);
    if (!s->pub)                                /* published: memory rebuilt at age 0 */
        s->mem.age = add_sat(s->mem.age, d);
    if (k != IN_NOCARD)                         /* no card: empty_age 0 every push */
        s->empty_age = add_sat(s->empty_age, d);
    if (s->run_n) {                             /* REJECT kinds: no run */
        s->run_n = add_sat(s->run_n, d);
        s->cand_n = add_sat(s->cand_n, d);
    }
}

static void core_push(card_rec_stab_clock_t *c, const card_rec_result_t *r) {
    card_rec_stab_push(&c->core, r, 0);
    c->steps++;
}

/* stream reset: today's card_rec_stab_push(NULL); the replay source and the
 * grid are dropped (the next frame anchors one in phase with its stream).  The
 * old grid stays in t0 / tick (prev), so that frame still ages the state by
 * the wall time since; the caller clears prev when the clock went back. */
static void clock_reset(card_rec_stab_clock_t *c) {
    core_push(c, 0);
    c->prev |= c->started;
    c->started = c->open = c->have_last = c->conf = 0;
}

/* ticks c->tick + 1 .. k had no real frame */
static void fill(card_rec_stab_clock_t *c, int64_t k) {
    int64_t d = k - c->tick, i, l;
    if (d <= 0)
        return;
    c->tick = k;
    c->open = 0;
    if (!c->have_last || c->conf < CARD_REC_STAB_CONF) {
        age_only(&c->core, d);                  /* unconfirmed: time, no evidence */
        return;
    }
    if (c->core.n < 1 || c->core.n > 1000)
        c->core.n = CARD_REC_STAB_N;
    l = (int64_t)CARD_REC_STAB_UNIT_K * c->core.n + 1;
    for (i = 0; i < d && i < l; ++i)            /* the screen still shows `last` */
        core_push(c, &c->last);
    if (d > l)
        coast(&c->core, d - l, in_kind(&c->last));
}

static int clock_out(const card_rec_stab_clock_t *c, card_rec_shown_t *shown) {
    if (shown) {
        memset(shown, 0, sizeof(*shown));
        if (c && c->core.pub)
            *shown = c->core.shown;
    }
    return c && c->core.pub;
}

void card_rec_stab_clock_init(card_rec_stab_clock_t *c, int n) {
    if (!c)
        return;
    memset(c, 0, sizeof(*c));
    card_rec_stab_init(&c->core, n);
}

int card_rec_stab_push_at(card_rec_stab_clock_t *c, const card_rec_result_t *r, int64_t t_ms,
                          card_rec_shown_t *shown) {
    int64_t k = 0;
    if (!c)
        return clock_out(0, shown);
    if (!r) {
        clock_reset(c);
        return clock_out(c, shown);
    }
    if (c->started && (k = tick_of(c->t0, t_ms)) < c->tick - CARD_REC_STAB_LATE) {
        clock_reset(c);                         /* the clock went back: a new stream, */
        c->prev = 0;                            /* the gap unknown (not aged) */
    }
    if (!c->started) {                          /* anchor a grid: this frame is tick 0 */
        if (c->prev && (k = tick_of(c->t0, t_ms) - c->tick - 1) > 0)
            age_only(&c->core, k);              /* the ticks since the old grid's newest */
        c->started = 1;
        c->prev = 0;
        c->t0 = t_ms;
        c->tick = -1;
        k = 0;
    } else if (k <= c->tick) {                  /* late (a stale stamp): it counts now */
        k = c->open ? c->tick : c->tick + 1;
    }
    if (k != c->tick) {                         /* a new tick */
        fill(c, k - 1);
        core_push(c, r);
        c->tick = k;
        c->open = 1;
    } else if (!same_input(r, &c->last)) {      /* the open tick changed: never hidden */
        core_push(c, r);
    }                                           /* else a repeat (60 fps): counts once */
    if (!c->have_last || !same_input(r, &c->last))
        c->conf = 1;
    else if (k != c->last_tick && c->conf < CARD_REC_STAB_CONF)
        c->conf++;
    c->last = *r;
    c->last.distance_text[CARD_REC_TEXT_MAX - 1] = c->last.internal.distance_text[CARD_REC_TEXT_MAX - 1] = 0;
    c->have_last = 1;
    c->last_tick = k;
    return clock_out(c, shown);
}

int card_rec_stab_advance(card_rec_stab_clock_t *c, int64_t t_ms, card_rec_shown_t *shown) {
    if (c && c->started)                        /* complete ticks only: a frame may still come; */
        fill(c, tick_of(c->t0, t_ms) - 1);      /* at or before the newest tick: nothing to do */
    return clock_out(c, shown);
}
