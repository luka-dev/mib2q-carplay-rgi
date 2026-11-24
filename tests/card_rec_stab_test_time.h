/*
 * card_rec_stab_test: scenarios of the time-driven feed - the observed iOS
 * burst-then-silence pattern, a stall mid-animation, REJECT bursts, changes
 * inside one tick (60 fps flip-flops), the arithmetic coast over long gaps and
 * the clock edge cases (late stamps, going back, resets).  Included by
 * card_rec_stab_test.c after mk(), tk(), same_core().
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define L_COAST ((uint32_t)(CARD_REC_STAB_UNIT_K * N5 + 1))   /* pushes a gap may cost */

typedef struct {
    card_rec_stab_clock_t c;    /* the timed feed */
    card_rec_stab_t f;          /* the frame rule fed the same real frames */
    int pub, fpub;
    card_rec_shown_t sh, fsh;
} sc_t;

static sc_t g_s;

static void sc_init(sc_t *s) {
    card_rec_stab_clock_init(&s->c, N5);
    card_rec_stab_init(&s->f, N5);
    s->pub = s->fpub = 0;
}

static int sc_frame(sc_t *s, const char *tok, int64_t t) {
    card_rec_result_t r;
    mk(tok, &r);
    s->fpub = card_rec_stab_push(&s->f, &r, &s->fsh);
    return s->pub = card_rec_stab_push_at(&s->c, &r, t, &s->sh);
}

static int sc_adv(sc_t *s, int64_t t) { return s->pub = card_rec_stab_advance(&s->c, t, &s->sh); }

static int is_shown(int pub, const card_rec_shown_t *sh, card_rec_class_t cls, int m) {
    return pub && sh->cls == cls && sh->distance_m == m;
}

/* grid from 0: N, then LEFT 40 m at 30 fps (published at the 10th), then
 * advance() for 1 s: it stays shown (confirmed replays); returns the time */
static int64_t publish_l40(sc_t *s) {
    int64_t t;
    int j;
    sc_frame(s, "N", 0);
    for (j = 1; j <= 12; ++j)
        sc_frame(s, "L40", T30(j));
    CHECK(is_shown(s->pub, &s->sh, CARD_REC_CLASS_LEFT_TURN, 40), "steady L40 must publish");
    for (t = T30(12) + 50; t < T30(12) + 1050; t += 50)
        CHECK(is_shown(sc_adv(s, t), &s->sh, CARD_REC_CLASS_LEFT_TURN, 40), "L40 must stay in silence");
    return t;
}

/* `cnt` frames of tok at 60 fps from t, then advance() every 50 ms to t_end.
 * The timed feed must show (cls, m) exactly from the first call that
 * completes tick k1 + need - 1 (k1: the first tick tok counts in) and never
 * before.  Returns the first time shown (-1: never); *fr: the 60 fps frame at
 * which the frame rule fed the same frames first showed it (-1: never). */
static int64_t burst(sc_t *s, const char *tok, int cnt, int64_t t, int64_t t_end, int need, card_rec_class_t cls,
                     int m, int *fr, const char *what) {
    int64_t k1 = 0, first = -1, tt = t;
    int j, bad = 0, sh = 0;
    *fr = -1;
    for (j = 0; !bad; ++j) {
        tt = j < cnt ? t + T60(j) : t + T60(cnt - 1) + 50 * (j - cnt + 1);
        if (tt > t_end)
            break;
        if (j < cnt)
            sc_frame(s, tok, tt);
        else
            sc_adv(s, tt);
        if (j == 0)                         /* on top of an earlier frame of that tick, too */
            k1 = s->c.tick;
        if (j < cnt && *fr < 0 && is_shown(s->fpub, &s->fsh, cls, m))
            *fr = j;
        sh = is_shown(s->pub, &s->sh, cls, m);
        bad = sh != (s->c.tick >= k1 + need - 1);
        if (sh && first < 0)
            first = tt;
    }
    CHECK(!bad, "%s: at %lld ms shown %d, tick %lld, needed tick %lld", what, (long long)tt, sh,
          (long long)s->c.tick, (long long)(k1 + need - 1));
    return first;
}

static void burst_tests(void) {
    int64_t t, first;
    int j, fr;
    /* fresh: no-card burst, then the card burst covers 8 ticks; COLD_K * n = 10 */
    sc_init(&g_s);
    for (j = 0; j < 16; ++j)
        sc_frame(&g_s, "N", 1000 + T60(j));
    t = 1000 + T60(16);
    first = burst(&g_s, "L40", 16, t, t + 3000, C_TICKS, CARD_REC_CLASS_LEFT_TURN, 40, &fr, "fresh burst");
    CHECK(first > 0 && fr == C_TICKS - 1, "fresh burst: timed %lld, frame rule frame %d", (long long)first, fr);
    snprintf(g_summary, sizeof(g_summary), "16-frame 60 fps burst: fresh card %lld ms (frame rule %lld ms)",
             (long long)(first - t), (long long)T60(fr));
    /* half-changed: LEFT 40 m shown, the new card keeps the distance: SPLIT_K * n = 40 ticks */
    sc_init(&g_s);
    t = publish_l40(&g_s);
    first = burst(&g_s, "R40", 16, t, t + 3000, SP_TICKS, CARD_REC_CLASS_RIGHT_TURN, 40, &fr, "split burst");
    CHECK(first > 0 && fr < 0 && !g_s.fpub, "split burst: timed %lld, frame rule frame %d", (long long)first, fr);
    snprintf(g_summary + strlen(g_summary), sizeof(g_summary) - strlen(g_summary),
             ", half-changed %lld ms (frame rule %s)", (long long)(first - t), fr < 0 ? "never" : "shown");
}

/* LEFT 40 m shown; npre frames of `pre` (60 fps), then `last` frames of
 * RIGHT 40 m 34 ms apart, then 10 s of silence.  Only LEFT 40 m (before the
 * RIGHT frames) or RIGHT 40 m may ever show; want: RIGHT 40 m shows */
static void stall_case(const char *pre, int npre, int last, int want, const char *what) {
    int64_t t, tt, end;
    int j, shown = 0, wrong = 0;
    sc_init(&g_s);
    t = publish_l40(&g_s);
    for (j = 0; j < npre; ++j)
        wrong |= sc_frame(&g_s, pre, t + T60(j)) && !is_shown(1, &g_s.sh, CARD_REC_CLASS_LEFT_TURN, 40);
    for (j = 0, tt = t + T60(npre); j < last; ++j, tt += 34)
        wrong |= sc_frame(&g_s, "R40", tt) && !is_shown(1, &g_s.sh, CARD_REC_CLASS_RIGHT_TURN, 40);
    for (end = tt + 10000; tt < end; tt += 50) {
        shown |= is_shown(sc_adv(&g_s, tt), &g_s.sh, CARD_REC_CLASS_RIGHT_TURN, 40);
        wrong |= g_s.pub && !is_shown(1, &g_s.sh, CARD_REC_CLASS_RIGHT_TURN, 40);
    }
    CHECK(!wrong && shown == want, "stall %s: shown %d (want %d), other output %d", what, shown, want, wrong);
}

static void stall_tests(void) {
    stall_case("X", 4, 1, 0, "4 transition REJECTs then one RIGHT 40 m frame");
    stall_case("X", 4, 2, 0, "... then RIGHT 40 m in 2 ticks");
    stall_case("L40", 15, 1, 0, "LEFT 40 m burst ending on one RIGHT 40 m frame");
    stall_case("X", 4, 3, 1, "... then RIGHT 40 m in 3 ticks (confirmed: replayed)");
}

static void reject_tests(void) {
    static const char *const pre[] = {"X", "R40", "L400"};
    card_rec_stab_clock_t keep;
    card_rec_stab_t ref;
    card_rec_result_t a, b;
    int64_t t, tt, k;
    int i, j, fr, any = 0;
    /* a REJECT burst withdraws at once; silence keeps it withdrawn */
    sc_init(&g_s);
    t = publish_l40(&g_s);
    for (j = 0; j < 16; ++j)
        any |= sc_frame(&g_s, "X", t + T60(j)) | g_s.fpub;
    for (tt = t + T60(15) + 50; tt < t + 5000; tt += 50)
        any |= sc_adv(&g_s, tt);
    CHECK(!any, "REJECT burst: something shown");
    /* the published card again (memory): n ticks, as the frame rule says */
    burst(&g_s, "L40", 16, tt, tt + 1000, N5, CARD_REC_CLASS_LEFT_TURN, 40, &fr, "memory after REJECTs");
    CHECK(fr == N5 - 1, "memory: frame rule at frame %d", fr);
    /* a change inside one tick is pushed on top, never hidden: a REJECT, another
     * key or an upward jump, then LEFT 40 m 5 ms later, withdraws and stays so;
     * the memory republishes n ticks from the LEFT 40 m one (as the frame rule) */
    for (i = 0; i < 3; ++i) {
        sc_init(&g_s);
        k = tk(0, publish_l40(&g_s)) + 2;
        keep = g_s.c;
        mk(pre[i], &a);
        mk("L40", &b);
        card_rec_stab_push_at(&keep, &a, T30(k), 0);
        ref = keep.core;
        card_rec_stab_push(&ref, &b, 0);
        CHECK(!sc_frame(&g_s, pre[i], T30(k)) && !sc_frame(&g_s, "L40", T30(k) + 5) && g_s.c.tick == k &&
              g_s.c.conf == 1 && same_core(&ref, &g_s.c.core), "%s + LEFT 40 m in one tick", pre[i]);
        for (j = 1; j < N5; ++j)
            CHECK(sc_frame(&g_s, "L40", T30(k + j)) == (j == N5 - 1), "%s: memory at tick +%d: %d", pre[i], j,
                  g_s.pub);
    }
    CHECK(sc_frame(&g_s, "L40", T30(k + 40)), "L40 after replayed silence");
    CHECK(!sc_frame(&g_s, "X", T30(k + 40) + 5), "ACCEPT + REJECT in one tick must withdraw");
    /* a repeat inside one tick counts once (no 60 fps double count); a change is pushed on top */
    sc_init(&g_s);
    k = tk(0, publish_l40(&g_s)) + 2;
    sc_frame(&g_s, "L40", T30(k));
    keep = g_s.c;
    sc_frame(&g_s, "L40", T30(k) + 10);
    CHECK(same_core(&keep.core, &g_s.c.core) && g_s.c.steps == keep.steps && g_s.c.conf == keep.conf,
          "a repeat in one tick must count once");
    mk("L30", &b);
    ref = keep.core;
    card_rec_stab_push(&ref, &b, 0);
    sc_frame(&g_s, "L30", T30(k) + 12);
    CHECK(same_core(&ref, &g_s.c.core) && g_s.c.tick == k, "a change in one tick must be pushed on top");
}

/* 16 no-card frames at 60 fps from 1000 ms, then 34 frames flipping between
 * LEFT 40 m and RIGHT 40 m every `per` frames at 60 fps from `start`, then 3 s
 * of advance() every 50 ms: never shown, at any sub-tick phase (the frame
 * rule fed the same frames never shows it either) */
static void flip_tests(void) {
    static const int64_t starts[] = {1266, 1270, 1275, 1280, 1285, 1290, 1295};
    int64_t t, end;
    size_t i;
    int j, per, any;
    for (per = 1; per <= 3; ++per)
        for (i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
            sc_init(&g_s);
            for (j = 0; j < 16; ++j)
                sc_frame(&g_s, "N", 1000 + T60(j));
            for (j = 0, any = 0; j < 34; ++j)
                any |= sc_frame(&g_s, j / per % 2 ? "R40" : "L40", starts[i] + T60(j)) | g_s.fpub;
            for (t = starts[i] + T60(33) + 50, end = t + 3000; t < end; t += 50)
                any |= sc_adv(&g_s, t);
            CHECK(!any, "LEFT / RIGHT 40 m flip every %d frames at 60 fps from %lld ms: shown", per,
                  (long long)starts[i]);
        }
}

/* CONF counts distinct ticks of one input; another input (only its text
 * differs) restarts it.  A frame up to one tick late after advance() filled
 * its tick counts in the next tick, on top of the filled one. */
static void confirm_tests(void) {
    card_rec_result_t a;
    card_rec_stab_t ref;
    int64_t k;
    sc_init(&g_s);
    sc_frame(&g_s, "N", 0);
    sc_frame(&g_s, "L40", T30(1));
    sc_frame(&g_s, "L40", T30(1) + 10);
    CHECK(g_s.c.conf == 1, "two frames in one tick: conf %d", g_s.c.conf);
    sc_frame(&g_s, "L40", T30(2));
    CHECK(g_s.c.conf == 2, "second tick: conf %d", g_s.c.conf);
    mk("L40", &a);
    snprintf(a.distance_text, sizeof(a.distance_text), "40m");
    card_rec_stab_push_at(&g_s.c, &a, T30(3), 0);
    CHECK(g_s.c.conf == 1, "other text: conf %d", g_s.c.conf);
    sc_frame(&g_s, "S800", T30(4));
    sc_frame(&g_s, "S800", T30(5));
    CHECK(g_s.c.conf == 2, "STRAIGHT 800 m twice: conf %d", g_s.c.conf);
    sc_frame(&g_s, "S790", T30(6));
    CHECK(g_s.c.conf == 1, "STRAIGHT 790 m: conf %d", g_s.c.conf);
    sc_frame(&g_s, "N", T30(7));
    sc_frame(&g_s, "N", T30(8));
    sc_frame(&g_s, "X", T30(9));
    CHECK(g_s.c.conf == 1, "no card, then a REJECT: conf %d", g_s.c.conf);
    sc_init(&g_s);
    publish_l40(&g_s);
    k = g_s.c.tick;
    sc_adv(&g_s, t_of(0, k + 2));
    ref = g_s.c.core;
    mk("X", &a);
    card_rec_stab_push(&ref, &a, 0);
    sc_frame(&g_s, "X", t_of(0, k + 1) + 5);
    CHECK(g_s.c.tick == k + 2 && g_s.c.open && same_core(&ref, &g_s.c.core), "late frame: tick %lld of %lld",
          (long long)g_s.c.tick, (long long)(k + 2));
}

/* ------------------------------------------------------------------ coast */
static int add_ref(int v, int64_t d) { return v + d > AGE_MAX ? AGE_MAX : (int)(v + d); }

static void age_only_ref(card_rec_stab_t *s, int64_t d) {
    int i;
    for (i = 0; i < s->hist_n; ++i)
        s->hist[i].age = add_ref(s->hist[i].age, d);
    s->mem.age = add_ref(s->mem.age, d);
    s->empty_age = add_ref(s->empty_age, d);
}

/* a gap of d ticks after state c0 (grid t0) must equal, field by field, d
 * pushes of the newest result when it is confirmed (CONF alike ticks), else d
 * ticks of ages only, and cost <= L_COAST pushes.  preset: counters just
 * below AGE_MAX first.  d past AGE_MAX + 100 (every counter saturated)
 * compares with the gap of AGE_MAX + 100 instead of pushing it. */
static void coast_one(const card_rec_stab_clock_t *c0, int64_t t0, int64_t d, int preset, const char *what) {
    static card_rec_stab_clock_t c, fix;
    card_rec_stab_t ref;
    int64_t i;
    int confirmed = c0->have_last && c0->conf >= CARD_REC_STAB_CONF;
    c = *c0;
    if (preset) {
        for (i = 0; i < c.core.hist_n; ++i)
            c.core.hist[i].age = c.core.hist[i].cnt = AGE_MAX - 7;
        c.core.mem.age = c.core.empty_age = AGE_MAX - 7;
        if (c.core.run_n)
            c.core.run_n = c.core.cand_n = AGE_MAX - 7;
    }
    ref = c.core;
    if (!confirmed) {
        age_only_ref(&ref, d);
    } else if (d <= AGE_MAX + 100) {
        for (i = 0; i < d; ++i)
            card_rec_stab_push(&ref, &c.last, 0);
    } else {
        fix = c;
        card_rec_stab_advance(&fix, t_of(t0, c0->tick + AGE_MAX + 101), 0);
        ref = fix.core;
    }
    card_rec_stab_advance(&c, t_of(t0, c0->tick + d + 1), 0);
    CHECK(c.tick == c0->tick + d && c.steps - c0->steps <= (confirmed ? L_COAST : 0u),
          "coast %s d %lld: tick %lld, %u pushes", what, (long long)d, (long long)c.tick, c.steps - c0->steps);
    CHECK(same_core(&ref, &c.core), "coast %s d %lld%s: state differs", what, (long long)d, preset ? " preset" : "");
}

static void coast_tests(void) {
    static const char *const seqs[] = {
        "N L40*12",                 /* ACCEPT, published */
        "N L40*12 R40*3",           /* ACCEPT, half-changed: needs SPLIT_K * n */
        "N L40*12 I19312*3",        /* ACCEPT, other unit system: needs UNIT_K * n = L - 1 */
        "N L13*3",                  /* ACCEPT off the 10 m ladder, cold */
        "N S800*3",                 /* CLASS_NOT_PUBLISHED reading */
        "N*3",                      /* no card */
        "N L40*12 X*3",             /* REJECT */
        "N L40*12 R40",             /* unconfirmed: ages only */
        "N L40*12 X R40 R40"};      /* unconfirmed (2 ticks) */
    static const int64_t ds[] = {1, 2, 3, 39, 40, 41, 89, 90, 91, 92, 93, 150, 1000, 4000, 1000000000LL};
    static card_rec_stab_clock_t c;
    const int64_t t0 = 5000;
    size_t i, k;
    int len, j;
    for (i = 0; i <= sizeof(seqs) / sizeof(seqs[0]); ++i) {
        len = tokens(i < sizeof(seqs) / sizeof(seqs[0]) ? seqs[i] : seqs[0], g_seq, SEQ_MAX);
        card_rec_stab_clock_init(&c, N5);
        for (j = 0; j < len; ++j)
            card_rec_stab_push_at(&c, &g_seq[j], t0 + T30(j), 0);
        if (i == sizeof(seqs) / sizeof(seqs[0])) {      /* one gap pushed in full across AGE_MAX */
            coast_one(&c, t0, AGE_MAX + 100, 0, "N L40*12 (full)");
            break;
        }
        for (k = 0; k < sizeof(ds) / sizeof(ds[0]); ++k)
            coast_one(&c, t0, ds[k], 0, seqs[i]);
        coast_one(&c, t0, 150, 1, seqs[i]);
    }
}

/* ------------------------------------------------------------------ clock */
static void clock_tests(void) {
    card_rec_result_t r;
    card_rec_shown_t sh;
    uint32_t steps;
    int64_t t, tt, t0, k;
    int j, z, pub = 0, off = 0;
    /* the clock goes back 2 s (> LATE ticks): reset (withdraw), the frame anchors a new grid */
    sc_init(&g_s);
    t = publish_l40(&g_s);
    CHECK(sc_frame(&g_s, "L40", t), "L40 at %lld", (long long)t);
    CHECK(!sc_frame(&g_s, "L40", t - 2000) && g_s.c.t0 == t - 2000 && g_s.c.tick == 0, "back 2 s: reset");
    for (j = 1; j < N5; ++j)
        CHECK(sc_frame(&g_s, "L40", t - 2000 + T30(j)) == (j == N5 - 1), "memory after reset, tick %d", j);
    /* up to LATE ticks back is a stale stamp: it counts now, on the same grid;
     * advance() back never resets */
    t = t - 2000 + T30(N5 + 1);
    t0 = g_s.c.t0;
    CHECK(sc_frame(&g_s, "L40", t) && sc_frame(&g_s, "L40", t - 20) && sc_frame(&g_s, "L40", t - 1000) &&
          g_s.c.open && g_s.c.t0 == t0 && g_s.c.tick == tk(t0, t), "frames 20 ms / 1 s back");
    CHECK(sc_adv(&g_s, t - 30) && sc_adv(&g_s, t - 80) && sc_adv(&g_s, t - 5000) && g_s.c.t0 == t0 &&
          g_s.c.tick == tk(t0, t), "advance back: no reset");
    /* frames stamped 100 ms before the 50 ms timer that already completed
     * their ticks, a stale advance() between: the card stays shown, one grid */
    sc_init(&g_s);
    t = publish_l40(&g_s);
    t0 = g_s.c.t0;
    for (tt = t; tt < t + 2000; tt += 50) {
        off |= !sc_adv(&g_s, tt);
        off |= !sc_frame(&g_s, "L40", tt - 100);
        off |= !sc_adv(&g_s, tt - 70);
    }
    CHECK(!off && g_s.c.t0 == t0, "late stamps: withdrawn or reset");
    CHECK(!sc_frame(&g_s, "X", tt - 900) && g_s.c.t0 == t0, "a REJECT 900 ms late must withdraw, no reset");
    /* int64 extremes: no overflow, bounded work */
    sc_init(&g_s);
    for (j = 0; j < 3; ++j)
        sc_frame(&g_s, "L40", INT64_MIN + T30(j));
    steps = g_s.c.steps;
    pub = sc_adv(&g_s, INT64_MAX);
    CHECK(pub && g_s.c.steps - steps <= L_COAST && g_s.c.core.run_n == AGE_MAX, "INT64_MIN -> INT64_MAX: %u pushes",
          g_s.c.steps - steps);
    CHECK(!sc_frame(&g_s, "L40", INT64_MIN) && g_s.c.t0 == INT64_MIN, "INT64_MAX -> INT64_MIN: reset");
    CHECK(!sc_frame(&g_s, "L40", INT64_MAX) && g_s.c.tick > 0 && g_s.c.conf == 2, "one frame at INT64_MAX");
    k = g_s.c.tick;
    CHECK(!sc_adv(&g_s, INT64_MIN) && g_s.c.started && g_s.c.tick == k, "advance to INT64_MIN: nothing");
    /* NULL: no clock, and a NULL result = stream reset (memory kept, the gap aged) */
    mk("L40", &r);
    memset(&sh, 0x55, sizeof(sh));
    CHECK(!card_rec_stab_push_at(0, &r, 0, &sh) && !sh.distance_m && !card_rec_stab_advance(0, 0, 0), "NULL clock");
    card_rec_stab_clock_init(0, N5);
    sc_init(&g_s);
    steps = g_s.c.steps;
    CHECK(!sc_adv(&g_s, 12345) && g_s.c.steps == steps, "advance before any frame");
    t = publish_l40(&g_s);
    CHECK(!card_rec_stab_push_at(&g_s.c, 0, t, &sh) && !g_s.c.started && g_s.c.prev && !g_s.c.have_last &&
          g_s.c.core.mem_valid && !sh.distance_m, "NULL result: reset");
    CHECK(!sc_adv(&g_s, t + 10000), "silence after a reset never replays");
    for (j = 0; j < N5; ++j)
        pub = sc_frame(&g_s, "L40", t + 20000 + T30(j));
    CHECK(pub, "memory republish after a NULL reset");
    /* a reset keeps wall time: no card, (NULL,) 10 min of silence, then LEFT
     * 40 m at 30 fps is not fresh - SPLIT_K * n frames, NULL or not */
    for (z = 0; z < 2; ++z) {
        sc_init(&g_s);
        sc_frame(&g_s, "N", 0);
        if (z)
            card_rec_stab_push_at(&g_s.c, 0, 10, 0);
        for (t = 50; t < 600000; t += 50)
            sc_adv(&g_s, t);
        for (j = 0; j < 100 && !sc_frame(&g_s, "L40", 600000 + T30(j)); ++j)
            ;
        CHECK(j == SP_TICKS - 1, "%s, 10 min, LEFT 40 m: shown at frame %d", z ? "N NULL" : "N", j);
    }
}
