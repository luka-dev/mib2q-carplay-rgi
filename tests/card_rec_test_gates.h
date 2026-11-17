/*
 * card_rec_test: the distance and icon gates on synthetic cards (always run,
 * licence-safe shapes only).  Every gate is shown to fire with its own reason
 * on a card that differs from an accepted one in that one property, and the
 * new distance gates also on the accepted side of their threshold:
 *   anchor        the distance must start at x 106 +- 4 (a lost leading digit)
 *   side light    no light >= 40 right of the distance (a dimmed 'i' of 'mi')
 *   spacing       '1' then '1' only 5..6 px apart (a closed-up lost separator;
 *                 '11' is no kerned pair)
 *   glyph count   2 glyphs only for a zero ('0 m'): '1 m' is '1 mi' minus its 'i'
 *   separator     '.' bright, well matched, no flat side (a square speck)
 *   lookalike     a glyph as close to a '~' reject label ('~I') as to its reading
 *   residual      light inside the distance box no template explains
 *   icon          decoy (as close to a flipped other-class arrow), keep unsure,
 *                 mid tones
 * Included by card_rec_test_api.h after its rec() and EXPECT_REJECT.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

/* 6 x 6 '.' with its top-left at (x, y): the template disc scaled to white,
 * flat 1: its top row lit fully (a flat edge), flat 2: a full square speck */
static void put_dot(uint8_t *rgba, int x, int y, int flat, uint8_t white) {
    int i, j;
    for (j = 0; j < 6; ++j)
        for (i = 0; i < 6; ++i)
            fill_rect(rgba, x + i, y + j, 1, 1, (uint8_t)((flat > 1 || (flat && !j) ? 255 : disc_px(i, j)) * white / 255));
}

/* the synthetic card with "1.1 m" (bar, 5 px, dot, 3 px, bar, 17 px, 'm') */
static void sep_card(int flat, uint8_t dot_white) {
    const int bx = 330, by = 88;
    synth_card(g_rgba, bx, 255);
    fill_rect(g_rgba, bx + 100, by + 110, 200, 40, 0);
    fill_rect(g_rgba, bx + 106, by + 120, 4, 25, 255);
    put_dot(g_rgba, bx + 115, by + 139, flat, dot_white);
    fill_rect(g_rgba, bx + 124, by + 120, 4, 25, 255);
    fill_rect(g_rgba, bx + 145, by + 126, 20, 19, 255);
}

/* the synthetic card with "11 m": first bar w1 wide, then gap px */
static void dist_card(int x0, int w1, int gap) {
    synth_card(g_rgba, 330, 255);
    fill_rect(g_rgba, 330 + 100, 88 + 110, 200, 40, 0);
    put_dist(g_rgba, 330, 88, x0, w1, gap, 255);
}

#define EXPECT_ACCEPT(c, what) CHECK(rec(c, 0) && g_r.distance_m == 11 && !strcmp(g_r.distance_text, "11 m"), \
    "%s: want 11 m, got %s '%s'", what, card_rec_reason_name(g_r.reason), g_r.distance_text)

static void distance_gate_tests(card_rec_t *c) {
    dist_card(96, 4, 5);                /* a leading glyph lost: the text starts early / late */
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_NOT_AT_ANCHOR, "distance at x 96");
    dist_card(111, 4, 5);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_NOT_AT_ANCHOR, "distance at x 111");
    dist_card(110, 4, 5);
    EXPECT_ACCEPT(c, "distance at x 110 (tolerance 4)");
    synth_card(g_rgba, 330, 255);       /* a dim mark right of the text, too faint for ink */
    fill_rect(g_rgba, 330 + 170, 88 + 130, 3, 3, 100);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_SIDE_LIGHT, "side light 100");
    synth_card(g_rgba, 330, 255);
    fill_rect(g_rgba, 330 + 170, 88 + 130, 3, 3, 30);
    EXPECT_ACCEPT(c, "side light 30");
    dist_card(106, 4, 4);               /* '1' '1' closer / wider than the font sets them */
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_GLYPH_SPACING, "1|1 gap 4");
    dist_card(106, 4, 7);               /* (4 and 7 are kerned gaps of '10' '14' '17' / '15' only) */
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_GLYPH_SPACING, "1|1 gap 7");
    dist_card(106, 4, 8);               /* a word space: '1 1 m' */
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_GRAMMAR, "1|1 gap 8");
    dist_card(106, 4, 6);
    EXPECT_ACCEPT(c, "1|1 gap 6");
    sep_card(0, 255);                   /* a real '.': shape ok, then 'm' takes no decimals */
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_NUMBER_GRAMMAR, "1.1 m with a disc");
    sep_card(1, 255);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_SEPARATOR_SHAPE, "'.' with a flat side");
    sep_card(2, 255);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_SEPARATOR_SHAPE, "square speck");
    sep_card(0, 224);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_SEPARATOR_SHAPE, "dim '.'");
    dist_card(106, 5, 5);               /* 5 px bar: '1' at 200, the '~I' lookalike at 0 */
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_GLYPH_LOOKALIKE, "lookalike '~I'");
    synth_card(g_rgba, 330, 255);       /* "1 m": 2 glyphs pass only for a zero ('3 mi' minus its 'i') */
    fill_rect(g_rgba, 330 + 100, 88 + 110, 200, 40, 0);
    fill_rect(g_rgba, 330 + 106, 88 + 120, 4, 25, 255);
    fill_rect(g_rgba, 330 + 127, 88 + 126, 20, 19, 255);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_GLYPH_COUNT, "1 m: one non-zero digit");
    synth_card(g_rgba, 330, 255);       /* a speck between '1' and 'm' no template explains */
    fill_rect(g_rgba, 330 + 123, 88 + 125, 3, 3, 100);
    EXPECT_REJECT(c, 0, CARD_REC_R_DISTANCE_RESIDUAL, "distance residual");
}

/* the Gamma with a bar over its bottom quarter: a 'C', the Gamma and its
 * vertical mirror at the same distance (about 214) */
static void c_card(void) {
    synth_card(g_rgba, 330, 255);
    fill_rect(g_rgba, 330 + 36, 88 + 101 + 48, 40, 16, 255);
}

static void icon_gate_tests(card_rec_t *c) {
    c_card();
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_DECOY, "Gamma + bottom bar: as close to the mirrored Gamma");
    synth_card(g_rgba, 330, 255);       /* a mid-grey stripe down the stem: blended art */
    fill_rect(g_rgba, 330 + 36 + 6, 88 + 101, 4, 64, 110);
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_MID_TONES, "mid-tone stem");
}

/* KEEP classes need a closer match (keep_d_max) */
static void keep_tests(const char *root) {
    card_rec_t *c = 0;
    char err[256];
    make_root(root, R_KEEP);
    if (card_rec_open(&c, root, err, sizeof(err)) != CARD_REC_OK) {
        CHECK(0, "keep root: %s", err);
        return;
    }
    synth_card(g_rgba, 330, 255);
    CHECK(rec(c, 0) && g_r.cls == CARD_REC_CLASS_KEEP_RIGHT && g_r.cp_maneuver_type == 14, "keep right: %s",
          card_rec_reason_name(g_r.reason));
    c_card();
    EXPECT_REJECT(c, 0, CARD_REC_R_ICON_KEEP_UNSURE, "keep icon at 214");
    card_rec_close(c);
}
