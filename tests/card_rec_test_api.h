/*
 * card_rec_test: API tests on the synthetic root (always run, no fixtures):
 * open / disable, every loader error path, the pipeline and its gates on
 * synthetic cards (card_rec_test_gates.h), the ring exit number (white box),
 * DETECT, argument validation and the stabiliser (its pure cases).  Included by
 * card_rec_test.c after card_rec_test_synth.h.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
static uint8_t *g_rgba, *g_flip;
static card_rec_result_t g_r;

static int rec(card_rec_t *c, const card_rec_zone_t *z) {
    return card_rec_recognise(c, g_rgba, FW * 4, FW, FH, z, &g_r);
}

#define EXPECT_REJECT(c, z, why, what) CHECK(!rec(c, z) && g_r.reason == (why) && g_r.distance_m == -1 && \
    !g_r.distance_text[0], "%s: want %s, got %s", what, card_rec_reason_name(why), card_rec_reason_name(g_r.reason))

#include "card_rec_test_gates.h"

static void loader_tests(const char *root) {
    static const struct { int fl; const char *hint; } bad[] = {
        {R_NO_GLYPHS, "glyphs_sf"}, {R_NO_DIGIT7, "digit 7"}, {R_NO_K, "unit 'km'"}, {R_NO_LEFT, "group LEFT"},
        {R_NO_RING_DIGITS, "ring_digits"}, {R_RING_NO5, "missing 5"}, {R_ICON_SIZE, "icon blk03.pgm"},
        {R_CARE_SIZE, "care mask"}, {R_BAD_HOLE, "bad hole"}, {R_BAD_ROW, "bad icon row"}, {R_ESCAPE, "bad icon row"},
        {R_BAD_GROUP, "bad icon fields"}, {R_RING_NO_HOLE, "disagree"}};
    char err[256], p[1024];
    card_rec_t *c;
    card_rec_status_t st;
    size_t i;
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        make_root(root, bad[i].fl);
        err[0] = 0;
        c = (card_rec_t *)(void *)err;      /* a failed open must reset it to NULL */
        st = card_rec_open(&c, root, err, sizeof(err));
        CHECK(st == CARD_REC_ERR_TEMPLATES && !c && strstr(err, bad[i].hint),
              "root variant %#x must fail with '%s': %s", bad[i].fl, bad[i].hint, err);
        if (st == CARD_REC_OK)
            card_rec_close(c);
    }
    c = 0;
    make_root(root, 0);
    snprintf(p, sizeof(p), "%s/icons/turn.pgm", root);
    remove(p);
    CHECK(card_rec_open(&c, root, err, sizeof(err)) == CARD_REC_ERR_TEMPLATES && !c && strstr(err, "turn.pgm"),
          "missing icon file: %s", err);
}

/* ring exit number on a 64 x 64 crop against the ring template (white box) */
static card_rec_reason_t ring_read(card_rec_t *c, const uint8_t *crop, int *ex) {
    int i;
    *ex = 0;
    for (i = 0; i < c->nicons && !c->icons[i].has_hole; ++i)
        ;
    return i < c->nicons ? card_rec_read_ring(c, crop, 64, 64, 64, &c->icons[i], ex) : CARD_REC_R_BAD_ARGS;
}

static void ring_tests(card_rec_t *c) {
    static const struct { const char *digits; int dx; card_rec_reason_t want; int exit_n; } k[] = {
        {"5", -1, CARD_REC_R_OK, 5}, {"12", -1, CARD_REC_R_OK, 12}, {"19", -1, CARD_REC_R_OK, 19},
        {"7", -1, CARD_REC_R_OK, 7}, {"Z", -1, CARD_REC_R_RING_DIGIT_LETTER, 0},
        {"5", 33, CARD_REC_R_RING_DIGIT_OFF_CENTRE, 0}, {"", -1, CARD_REC_R_RING_NO_EXIT_NUMBER, 0},
        {"111", -1, CARD_REC_R_RING_DIGIT_COUNT, 0}, {"0", -1, CARD_REC_R_RING_EXIT_OUT_OF_RANGE, 0},
        {"25", -1, CARD_REC_R_RING_EXIT_OUT_OF_RANGE, 0}, {"20", -1, CARD_REC_R_RING_EXIT_OUT_OF_RANGE, 0},
        {"05", -1, CARD_REC_R_RING_EXIT_OUT_OF_RANGE, 0}};
    uint8_t crop[64 * 64];
    card_rec_reason_t why;
    size_t i;
    int ex, x, y, keep;
    for (i = 0; i < sizeof(k) / sizeof(k[0]); ++i) {
        ring_crop(crop, 0, k[i].digits, k[i].dx, 26);
        why = ring_read(c, crop, &ex);
        CHECK(why == k[i].want && (why != CARD_REC_R_OK || ex == k[i].exit_n), "ring '%s' dx %d: want %s %d, got %s %d",
              k[i].digits, k[i].dx, card_rec_reason_name(k[i].want), k[i].exit_n, card_rec_reason_name(why), ex);
    }
    ring_crop(crop, 0, "5", -1, 26);            /* faint mark the digit does not explain */
    for (y = 40; y < 42; ++y)
        for (x = 40; x < 42; ++x)
            crop[y * 64 + x] = 100;
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_DIGIT_RESIDUAL, "ring residual: %s",
          card_rec_reason_name(why));
    ring_crop(crop, 0, "5", -1, 26);
    crop[17 * 64 + 32] = 255;                   /* ink on the hole rim */
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_DIGIT_TOUCHES_RING, "ring touch: %s",
          card_rec_reason_name(why));
    ring_crop(crop, 0, "", -1, 26);             /* a 2 x 16 bar: taller than the cap height */
    for (y = 24; y < 40; ++y)
        crop[y * 64 + 31] = crop[y * 64 + 32] = 255;
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_DIGIT_HEIGHT, "ring height: %s",
          card_rec_reason_name(why));
    ring_crop(crop, 0, "", -1, 26);             /* a 12 x 12 block: no digit is that wide */
    for (y = 26; y < 38; ++y)
        for (x = 26; x < 38; ++x)
            crop[y * 64 + x] = 255;
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_DIGIT_NO_TEMPLATE_FITS, "ring block: %s",
          card_rec_reason_name(why));
    ring_crop(crop, 0, "8", -1, 26);            /* '8' with a half-lit middle bar: '8' or '0' */
    for (y = 31; y < 33; ++y)
        for (x = 31; x < 34; ++x)
            crop[y * 64 + x] = 128;
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_DIGIT_GLYPH_AMBIGUOUS, "ring 8/0: %s",
          card_rec_reason_name(why));
    ring_crop(crop, 0, "5", -1, 26);
    keep = c->p.ring_digit_h_min;
    c->p.ring_digit_h_min = 13;
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_DIGIT_SIZE, "ring size: %s", card_rec_reason_name(why));
    c->p.ring_digit_h_min = keep;
    keep = c->ring.count;
    c->ring.count = 0;
    CHECK((why = ring_read(c, crop, &ex)) == CARD_REC_R_RING_NO_DIGIT_TEMPLATES, "ring set: %s",
          card_rec_reason_name(why));
    c->ring.count = keep;
}

static void pipeline_tests(card_rec_t *c) {
    card_rec_zone_t box = {CARD_REC_ZONE_BOX, 330, 88, 780, 294};
    int i;
    synth_card(g_rgba, 330, 255);
    for (i = 0; i < FH; ++i)            /* bottom-up copy for the negative stride */
        memcpy(g_flip + (size_t)(FH - 1 - i) * FW * 4, g_rgba + (size_t)i * FW * 4, (size_t)FW * 4);
    CHECK(rec(c, 0), "synthetic card: %s", card_rec_reason_name(g_r.reason));
    CHECK(g_r.cls == CARD_REC_CLASS_RIGHT_TURN && g_r.cp_maneuver_type == 2 && g_r.distance_value_x100 == 1100 &&
          g_r.distance_value_x10 == 110 && g_r.distance_unit == CARD_REC_UNIT_M && g_r.distance_m == 11 &&
          !strcmp(g_r.distance_text, "11 m") && g_r.zone == CARD_REC_ZONE_FULLSCREEN && g_r.score == 0 &&
          g_r.margin > 100 && !strcmp(card_rec_icon_group(c, g_r.icon_template), "RIGHT"),
          "synthetic fields %s %d %d '%s' %d", card_rec_class_name(g_r.cls), g_r.cp_maneuver_type,
          g_r.distance_value_x100, g_r.distance_text, g_r.margin);
    CHECK(card_rec_recognise(c, g_flip + (size_t)(FH - 1) * FW * 4, -FW * 4, FW, FH, &box, &g_r) &&
          g_r.zone == CARD_REC_ZONE_BOX && g_r.distance_value_x100 == 1100, "bottom-up custom box");
    box.x = 331;
    EXPECT_REJECT(c, &box, CARD_REC_R_NO_CARD_BOX, "shifted box");
    for (i = 0; i < 25; ++i)            /* isolated faint ink beside the distance */
        fill_rect(g_rgba, 330 + 160 + 6 * i, 88 + 130, 1, 1, 40);
    EXPECT_REJECT(c, 0, CARD_REC_R_TEXT_GHOST, "ghost text");
    synth_card(g_rgba, 330, 255);
    fill_rect(g_rgba, 330 + 36 + 24, 88 + 101 + 30, 12, 21, 40);   /* faint art where the Gamma is empty */
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_GHOST, "ghost icon");
    synth_card(g_rgba, 330, 255);
    fill_rect(g_rgba, 330 + 36 - 5, 88 + 101, 1, 64, 40);           /* a faint halo column */
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_GHOST_HALO, "icon halo");
    synth_card(g_rgba, 330, 255);       /* Gamma with 2 px ramps: blurred art */
    gamma_at(g_rgba, 330 + 36, 88 + 101, 2, 85);
    gamma_at(g_rgba, 330 + 36, 88 + 101, 1, 170);
    gamma_at(g_rgba, 330 + 36, 88 + 101, 0, 255);
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_SOFT_EDGES, "soft icon");
    synth_card(g_rgba, 330, 255);
    fill_rect(g_rgba, 330 + 36, 88 + 101, 40, 64, 255);             /* no template looks like a block */
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_UNKNOWN, "block icon");
    synth_card(g_rgba, 330, 178);       /* 70 % fade: a transition frame */
    EXPECT_REJECT(c, 0, CARD_REC_R_DIM_CONTENT, "fade");
    synth_card(g_rgba, 330, 240);       /* below WHITE_MIN of the GPU path, fine on the limited one */
    EXPECT_REJECT(c, 0, CARD_REC_R_DIM_CONTENT, "white 240 gpu");
    card_rec_set_input_path(c, CARD_REC_PATH_LIMITED);
    CHECK(rec(c, 0) && g_r.distance_m == 11, "white 240 limited: %s", card_rec_reason_name(g_r.reason));
    card_rec_set_input_path(c, (card_rec_path_t)7);                 /* ignored */
    CHECK(rec(c, 0), "invalid path must be ignored: %s", card_rec_reason_name(g_r.reason));
    card_rec_set_input_path(c, CARD_REC_PATH_GPU);
    fill_rect(g_rgba, 0, 0, FW, FH, 0);
    EXPECT_REJECT(c, 0, CARD_REC_R_NO_CARD_BOX, "black frame");
    synth_ring_card(g_rgba, 0, "12");
    CHECK(rec(c, 0) && g_r.cls == CARD_REC_CLASS_ROUNDABOUT_NUMBERED && g_r.cp_maneuver_type == 39 &&
          g_r.exit_number == 12 && g_r.traffic_side == 'R' && g_r.distance_m == 11, "ring card: %s %s %d",
          card_rec_reason_name(g_r.reason), card_rec_class_name(g_r.cls), g_r.exit_number);
    synth_ring_card(g_rgba, 1, "12");
    EXPECT_REJECT(c, 0, CARD_REC_R_RING_ROTATED, "ring turned by 180 deg");
}

static void detect_tests(card_rec_t *c) {
    card_rec_zone_t det = {CARD_REC_ZONE_DETECT, 0, 0, 0, 0};
    synth_card(g_rgba, 330, 255);
    CHECK(rec(c, &det) && g_r.zone == CARD_REC_ZONE_FULLSCREEN && g_r.box_x == 330, "detect fullscreen: %s",
          card_rec_reason_name(g_r.reason));
    synth_card(g_rgba, 300, 255);
    EXPECT_REJECT(c, &det, CARD_REC_R_DETECT_NOT_ZONE, "detect off-zone box");
    card_rec_params(c)->detect_any = 1;
    CHECK(rec(c, &det) && g_r.zone == CARD_REC_ZONE_BOX && g_r.box_x == 300 && g_r.box_w == 780,
          "detect_any box: %s", card_rec_reason_name(g_r.reason));
    card_rec_params(c)->detect_any = 0;
    fill_rect(g_rgba, 0, 0, FW, FH, 200);
    EXPECT_REJECT(c, &det, CARD_REC_R_NO_BLACK_BOX, "bright frame");
    fill_rect(g_rgba, 0, 0, FW, FH, 26);
    fill_rect(g_rgba, 500, 200, 300, 100, 0);
    EXPECT_REJECT(c, &det, CARD_REC_R_BLACK_BOX_TOO_SMALL, "small black box");
}

static void bad_args_tests(card_rec_t *c) {
    static const card_rec_zone_t zones[] = {
        {CARD_REC_ZONE_BOX, 0, 0, INT_MAX, INT_MAX}, {CARD_REC_ZONE_BOX, INT_MAX, INT_MAX, 780, 294},
        {CARD_REC_ZONE_BOX, INT_MIN, 88, 780, 294}, {CARD_REC_ZONE_BOX, 330, -1, 780, 294},
        {CARD_REC_ZONE_BOX, 330, 88, 0, 294}, {CARD_REC_ZONE_BOX, 330, 88, 780, -294},
        {CARD_REC_ZONE_BOX, 330, 88, CARD_REC_MAX_BOX_W + 1, 294}, {(card_rec_zone_kind_t)99, 0, 0, 0, 0},
        {(card_rec_zone_kind_t)-1, 0, 0, 0, 0}};
    static const int strides[] = {INT_MIN, INT_MIN + 1, INT_MAX, -INT_MAX, FW * 4 - 4, -(FW * 4 - 4), 0,
                                   CARD_REC_MAX_STRIDE + 4};
    card_rec_zone_t edge = {CARD_REC_ZONE_BOX, 1000, 300, 780, 294};
    card_rec_params_t keep = *card_rec_params(c);
    size_t i;
    synth_card(g_rgba, 330, 255);
    for (i = 0; i < sizeof(zones) / sizeof(zones[0]); ++i)
        EXPECT_REJECT(c, &zones[i], CARD_REC_R_BAD_ARGS, "bad zone");
    for (i = 0; i < sizeof(strides) / sizeof(strides[0]); ++i)
        CHECK(!card_rec_recognise(c, g_rgba, strides[i], FW, FH, 0, &g_r) && g_r.reason == CARD_REC_R_BAD_ARGS,
              "stride %d must be BAD_ARGS", strides[i]);
    CHECK(!card_rec_recognise(c, 0, FW * 4, FW, FH, 0, &g_r) && g_r.reason == CARD_REC_R_BAD_ARGS, "null pixels");
    CHECK(!card_rec_recognise(c, g_rgba, FW * 4, 0, FH, 0, &g_r) && g_r.reason == CARD_REC_R_BAD_ARGS, "width 0");
    CHECK(!card_rec_recognise(c, g_rgba, FW * 4, FW, CARD_REC_MAX_FRAME + 1, 0, &g_r) &&
          g_r.reason == CARD_REC_R_BAD_ARGS, "height");
    CHECK(!card_rec_recognise(c, g_rgba, FW * 4, FW, FH, 0, 0), "null result");
    EXPECT_REJECT(c, &edge, CARD_REC_R_NO_CARD_BOX, "box past the frame edge");
    card_rec_params(c)->band = -1;
    EXPECT_REJECT(c, 0, CARD_REC_R_BAD_ARGS, "band -1");
    *card_rec_params(c) = keep;
    card_rec_params(c)->white_min[CARD_REC_PATH_GPU] = 256;
    EXPECT_REJECT(c, 0, CARD_REC_R_BAD_ARGS, "white_min 256");
    *card_rec_params(c) = keep;
    card_rec_params(c)->ring_digit_h_min = keep.ring_digit_h_max + 1;
    EXPECT_REJECT(c, 0, CARD_REC_R_BAD_ARGS, "ring digit h_min > h_max");
    *card_rec_params(c) = keep;
    CHECK(card_rec_params_valid(&keep) && !card_rec_params_valid(0), "params_valid");
    CHECK(rec(c, 0), "restored params: %s", card_rec_reason_name(g_r.reason));
}

/* Pure stabiliser cases (the reference's names and order).  Frames: N no-card frame;
 * X reject; R<m> / L<m> right / left turn "<m> m"; I<m> right turn "<m / 1609>
 * mi"; E<e> ring exit e at 300 m; S<m> STRAIGHT "<m> m" (class_not_published,
 * internal reading).  Shown: - or R<m> / L<m> / O<m> (roundabout); "*k"
 * repeats.  cold C = COLD_K * N = 10, split SP = 40, unit U = 90 (N 5). */
#define STAB_TOK 16
#define STAB_MAX 128
static int stab_tokens(const char *s, char out[][STAB_TOK], int max) {
    char tok[16];
    int n = 0, k, rep, used;
    while (sscanf(s, " %15s%n", tok, &used) == 1) {
        char *star = strchr(tok, '*');
        s += used;
        rep = star ? atoi(star + 1) : 1;
        if (star)
            *star = 0;
        for (k = 0; k < rep && n < max; ++k)
            snprintf(out[n++], STAB_TOK, "%s", tok);
    }
    return n;
}

static void stab_frame(const char *t, card_rec_result_t *r) {
    int v = atoi(t + 1);
    memset(r, 0, sizeof(*r));
    r->accept = !strchr("NXS", t[0]);
    r->reason = t[0] == 'N' ? CARD_REC_R_NO_CARD_BOX : t[0] == 'X' ? CARD_REC_R_SOFT_EDGES
              : t[0] == 'S' ? CARD_REC_R_CLASS_NOT_PUBLISHED : CARD_REC_R_OK;
    r->internal.distance_m = t[0] == 'S' ? v : -1;
    r->internal.cls = CARD_REC_CLASS_STRAIGHT;
    r->internal.cp_maneuver_type = 3;
    r->internal.distance_unit = CARD_REC_UNIT_M;
    snprintf(r->internal.distance_text, sizeof(r->internal.distance_text), "%d m", v);
    r->distance_m = r->accept ? (t[0] == 'E' ? 300 : v) : -1;
    r->distance_unit = t[0] == 'I' ? CARD_REC_UNIT_MI : CARD_REC_UNIT_M;
    r->cls = t[0] == 'L' ? CARD_REC_CLASS_LEFT_TURN : t[0] == 'E' ? CARD_REC_CLASS_ROUNDABOUT_NUMBERED
                                                                  : CARD_REC_CLASS_RIGHT_TURN;
    r->cp_maneuver_type = t[0] == 'L' ? 1 : t[0] == 'E' ? 27 + v : 2;
    r->exit_number = t[0] == 'E' ? v : 0;
    r->traffic_side = t[0] == 'E' ? 'R' : 0;
    if (t[0] == 'I')
        snprintf(r->distance_text, sizeof(r->distance_text), "%d mi", v / 1609);
    else
        snprintf(r->distance_text, sizeof(r->distance_text), "%d m", r->distance_m);
}

static void stab_tests(void) {
    static const char *const cases[][3] = {
        {"cold_publish_after_2N", "N R300*12", "-*10 R300*3"},
        {"cold_no_appearance_SPLIT_N", "R300*41", "-*39 R300*2"},
        {"reject_resets_run", "N R300*9 X R300*10", "-*20 R300"},
        {"reject_withdraws", "N R300*10 X R300", "-*10 R300 - -"},
        {"memory_republish_after_N", "N R300*10 X R300*5", "-*10 R300 -*5 R300"},
        {"split_iconB_old_distance", "N R10*10 L10*39 L800*40", "-*10 R10 -*78 L800"},
        {"split_iconA_new_distance", "N R10*10 R800*39 L800*40", "-*10 R10 -*78 L800"},
        {"split_dwell_SPLIT_N_publishes", "N R10*10 L10*40", "-*10 R10 -*39 L10"},
        {"new_card_both_changed_after_N", "N R10*10 L800*5", "-*10 R10 -*4 L800"},
        {"countdown_holds_old_until_N", "N R300*10 R290*5", "-*10 R300*5 R290"},
        {"upward_jump_needs_SPLIT_N", "N R300*10 R350*40", "-*10 R300 -*39 R350"},
        {"misread_up_once", "N R300*10 R800 R300*5", "-*10 R300 -*5 R300"},
        {"misread_down_once", "N R300*10 R30 R300*6", "-*10 R300 -*5 R300*2"},
        {"class_flicker", "N R300*9 L800 R300*9", "-*20"},
        {"distance_flicker_never", "N R300 R290 R300 R290 R300 R290 R300 R290 R300 R290 R300 R290 R300 R290 "
                                   "R300 R290 R300 R290 R300 R290", "-*21"},
        {"unit_switch_needs_UNIT_N", "N I19312*10 R12*90", "-*10 R19312 -*89 R12"},
        {"ring_exit_change_same_distance", "N E8*10 E3*39", "-*10 O300 -*39"},
        {"stale_memory_split", "N R10*10 N S20*3 L20*40", "-*10 R10 -*43 L20"},
        {"lagging_countdown_split", "N L30*10 L20*3 R20*39", "-*10 L30*4 -*39"},
        {"observed_big_drop_SPLIT_N", "N R300*10 R200*40", "-*10 R300 -*39 R200"},
        {"burst_gap_allows_drop_N", "N R300*10 X*40 R200*5", "-*10 R300 -*44 R200"},
        {"off_ladder_SPLIT_N", "N R13*40", "-*40 R13"},
        {"cold_card_change_SPLIT_N", "N R300*9 L800*40", "-*49 L800"},
        {"cold_countdown_run_publishes", "N R300*4 R290*6", "-*10 R290"},
        {"confirmed_unpublished_ref_N", "N S800*40 R300*5", "-*45 R300"}};
    static char seq[STAB_MAX][STAB_TOK], exp[STAB_MAX][STAB_TOK];
    char got[24];
    card_rec_stab_t st;
    card_rec_shown_t sh;
    card_rec_result_t r;
    size_t i;
    int n, m, k, pub = 0;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        n = stab_tokens(cases[i][1], seq, STAB_MAX);
        m = stab_tokens(cases[i][2], exp, STAB_MAX);
        CHECK(n == m && n < STAB_MAX, "stab %s: %d frames, %d expectations", cases[i][0], n, m);
        card_rec_stab_init(&st, CARD_REC_STAB_N);
        for (k = 0; k < n && k < m; ++k) {
            stab_frame(seq[k], &r);
            pub = card_rec_stab_push(&st, &r, &sh);
            if (pub)
                snprintf(got, sizeof(got), "%c%d", sh.cls == CARD_REC_CLASS_RIGHT_TURN ? 'R' :
                         sh.cls == CARD_REC_CLASS_LEFT_TURN ? 'L' : 'O', sh.distance_m);
            else
                strcpy(got, "-");
            CHECK(!strcmp(got, exp[k]), "stab %s frame %d: want %s got %s", cases[i][0], k, exp[k], got);
        }
    }
    card_rec_stab_init(&st, 0);
    CHECK(st.n == CARD_REC_STAB_N, "stab n < 1 -> default");
    card_rec_stab_init(&st, 1001);
    CHECK(st.n == CARD_REC_STAB_N, "stab n > 1000 -> default");
    stab_frame("N", &r);
    card_rec_stab_push(&st, &r, 0);
    stab_frame("R10", &r);
    for (k = 0; k < CARD_REC_STAB_COLD_K * CARD_REC_STAB_N; ++k)
        pub = card_rec_stab_push(&st, &r, 0);
    CHECK(pub && !strcmp(st.shown.distance_text, "10 m"), "cold publish");
    CHECK(!card_rec_stab_push(&st, 0, &sh) && sh.distance_m == 0 && !st.pub && st.mem_valid, "no frame resets");
    for (k = 0; k < CARD_REC_STAB_N; ++k)
        pub = card_rec_stab_push(&st, &r, 0);
    CHECK(pub, "memory republishes after N");
    st.n = 0;                           /* a corrupted n falls back to the default */
    CHECK(card_rec_stab_push(&st, &r, &sh) && st.n == CARD_REC_STAB_N && sh.distance_m == 10, "corrupt n");
    memset(&st, 0, sizeof(st));         /* a zeroed state is not fresh: SPLIT_K * n */
    for (k = 0, pub = 0; k < CARD_REC_STAB_SPLIT_K * CARD_REC_STAB_N - 1; ++k)
        pub |= card_rec_stab_push(&st, &r, 0);
    CHECK(!pub && card_rec_stab_push(&st, &r, 0), "zeroed state publishes after SPLIT_K * n only");
}

static void api_tests(void) {
    char err[256], root[512];          /* every path buffer is 1024 */
    const char *tmp = getenv("TMPDIR");
    card_rec_t *c = 0;
    int icons = 0, glyphs = 0, ring = 0;
    g_rgba = (uint8_t *)malloc((size_t)FW * FH * 4);
    g_flip = (uint8_t *)malloc((size_t)FW * FH * 4);
    if (!g_rgba || !g_flip) {
        printf("FAIL alloc\n");
        exit(1);
    }
    snprintf(root, sizeof(root), "%s/card_rec_test.%ld", tmp && *tmp && strlen(tmp) < 400 ? tmp : "/tmp",
             (long)getpid());
    unsetenv(CARD_REC_TEMPLATES_ENV);
    CHECK(card_rec_open(&c, 0, err, sizeof(err)) == CARD_REC_DISABLED && !c, "unset env must disable");
    CHECK(card_rec_open(&c, "/nonexistent/card_rec", err, sizeof(err)) == CARD_REC_DISABLED && !c,
          "absent root must disable");
    CHECK(!card_rec_recognise(0, g_rgba, FW * 4, FW, FH, 0, &g_r) && g_r.reason == CARD_REC_R_DISABLED,
          "disabled context must reject");
    loader_tests(root);
    make_root(root, 0);
    setenv(CARD_REC_TEMPLATES_ENV, root, 1);
    CHECK(card_rec_open(&c, 0, err, sizeof(err)) == CARD_REC_OK && c, "env root must load: %s", err);
    unsetenv(CARD_REC_TEMPLATES_ENV);
    if (c) {
        card_rec_template_counts(c, &icons, &glyphs, &ring);
        CHECK(icons == 17 && glyphs == 22 && ring == 11 && !strcmp(card_rec_icon_group(c, -1), "-"),
              "template counts %d %d %d", icons, glyphs, ring);
        pipeline_tests(c);
        distance_gate_tests(c);
        icon_gate_tests(c);
        ring_tests(c);
        detect_tests(c);
        bad_args_tests(c);
        card_rec_close(c);
    }
    make_root(root, R_RING_TWIN);
    if (card_rec_open(&c, root, err, sizeof(err)) == CARD_REC_OK) {
        synth_ring_card(g_rgba, 0, "5");
        EXPECT_REJECT(c, 0, CARD_REC_R_RING_SIDE_AMBIGUOUS, "ring of both traffic sides");
        card_rec_close(c);
    } else {
        CHECK(0, "twin ring root: %s", err);
    }
    make_root(root, R_STRAIGHT);
    if (card_rec_open(&c, root, err, sizeof(err)) == CARD_REC_OK) {
        synth_card(g_rgba, 330, 255);
        EXPECT_REJECT(c, 0, CARD_REC_R_CLASS_NOT_PUBLISHED, "straight is never published");
        card_rec_params(c)->unpublished = 0;
        CHECK(rec(c, 0) && g_r.cls == CARD_REC_CLASS_STRAIGHT && g_r.cp_maneuver_type == 3, "straight allowed: %s",
              card_rec_reason_name(g_r.reason));
        card_rec_close(c);
    } else {
        CHECK(0, "straight root: %s", err);
    }
    keep_tests(root);
    stab_tests();
    remove_tree(root);
    free(g_rgba);
    free(g_flip);
}
