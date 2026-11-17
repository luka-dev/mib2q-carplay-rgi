/*
 * CarPlay instruction-card recogniser (navigation maneuver card on the cluster stream).
 *
 * iOS draws the CPManeuver of the active navigation app into the cluster zone
 * as a card: a white maneuver icon, the distance ("10 m") right of it and grey
 * street text below, on black, filling the zone's safe box.  This module reads
 * that card back from a decoded RGBA picture so the head unit can drive the
 * native HUD arrow.  It is a C port of a hardened Python reference recogniser
 * (not part of this repository); tests/card_rec_test.c holds it to
 * exact decision parity with that reference.
 *
 * Safety: a wrong arrow on the HUD is worse than none.  Every stage fails
 * closed - no card box, a fading, blurred or ghosted (transition) card, an
 * unknown, ambiguous, rotated or mirrored icon, an unreadable distance or exit
 * number all REJECT, and STRAIGHT / ARRIVE are read but never published.  A
 * single ACCEPT is still one frame: the temporal rule (card_rec_stab_push_at /
 * _advance, card_rec_stab.h) must be applied before anything reaches the HUD.
 *
 * Templates: never committed.  card_rec_open() loads them at run time from a
 * template root (explicit path, else $CARD_REC_TEMPLATES) built outside the
 * repository by scripts/gen_card_templates.py.  Without one the recogniser is
 * DISABLED and every call rejects.  Layout of a template root:
 *   icons/manifest.txt   <file.pgm> <care.pgm|-> <name> <group> <cp> <side>
 *                        <exp_w> <exp_h> <hole: -|cx_pm,cy_pm,r_pm>
 *   glyphs_sf/, glyphs_car/  (at least one; a later directory replaces a
 *                        label of an earlier one)  manifest.txt rows:
 *                        <file.pgm> <label> <top_off> <bottom_off>
 *                        a label starting with '~' is a lookalike reject
 *                        label ('~B' reads as 8): never read, it REJECTs a
 *                        glyph it comes within glyph_look_margin of
 *   ring_digits/         same format, exit number in the ring; non-digit
 *                        labels (confusable letters) are reject labels
 * PGM = binary P5, 8 bit.  '#' lines are comments.  An incomplete root (an
 * output group without a template, a distance digit or unit that cannot be
 * spelled, a ring template without ring digits 0-9) fails to load.
 *
 * Arithmetic: integer only (per-mille scores, exact rounded divisions), no
 * floating point.  Memory: card_rec_open() allocates everything; the per-frame
 * path (card_rec_recognise) never allocates and touches only the context's
 * scratch, so one context serves one thread at a time.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_CARD_REC_H
#define ALTSCREEN_CARD_REC_H

#include <stddef.h>
#include <stdint.h>

#define CARD_REC_TEMPLATES_ENV  "CARD_REC_TEMPLATES"

/* Static limits.  A box bigger than MAX_BOX is not a card (rejected).  The
 * zones are at most 780 x 294, but DETECT may return any black box inside the
 * 1440 x 542 cluster stream, so the box scratch covers the whole stream. */
#define CARD_REC_NORM           32      /* normalised icon grid */
#define CARD_REC_MAX_BOX_W      1440
#define CARD_REC_MAX_BOX_H      542
#define CARD_REC_MAX_ICON       96      /* padded icon square (layout caps it at 66) */
#define CARD_REC_MAX_GLYPHS     12      /* glyphs in the distance string */
#define CARD_REC_MAX_GLYPH_W    64
#define CARD_REC_MAX_GLYPH_H    48
#define CARD_REC_MAX_FRAME      4096    /* frame width/height limit (detect counters) */
#define CARD_REC_MAX_STRIDE     (8 * CARD_REC_MAX_FRAME)   /* |stride| limit, bytes */
#define CARD_REC_MAX_ICONS      32      /* icon templates */
#define CARD_REC_MAX_LABELS     64      /* distinct glyph labels per set */
#define CARD_REC_LABEL_MAX      8       /* label bytes incl. NUL (UTF-8, 'ft' ligature) */
#define CARD_REC_TEXT_MAX       112     /* 12 labels + separators */
#define CARD_REC_D4             7       /* non-identity rotations / flips of the grid */
#define CARD_REC_STAB_N         5       /* identical reads before the HUD shows one */
#define CARD_REC_STAB_COLD_K    2       /* x n: first publish with no memory */
#define CARD_REC_STAB_SPLIT_K   8       /* x n: half-changed vs the memory */
#define CARD_REC_STAB_UNIT_K    18      /* x n: other unit system than the memory */
#define CARD_REC_STAB_DROP_ABS_M 50     /* same-key drops beyond max(50 m, 25 %) */
#define CARD_REC_STAB_DROP_PM   250     /*   are half-changed ... */
#define CARD_REC_STAB_V_MAX_MM  2334    /*   ... + 70 m/s (mm per 30 fps frame) x frames unseen */
#define CARD_REC_STAB_HIST_N    4       /* distinct recent reads kept (screen history) */
#define CARD_REC_STAB_HIST_AGE  900     /*   and forgotten after 30 s */
#define CARD_REC_STAB_FRESH_MAX 8       /* a run <= 8 frames after a no-card frame is fresh */
#define CARD_REC_STAB_AGE_MAX   (1 << 20)   /* ages / counters saturate here */
#define CARD_REC_STAB_LADDER_M  10      /* metric 'm' cards count down in 10 m steps */

typedef enum {
    CARD_REC_OK = 0,
    CARD_REC_DISABLED,          /* no template root configured / present */
    CARD_REC_ERR_TEMPLATES,     /* template root present but malformed or incomplete */
    CARD_REC_ERR_NOMEM
} card_rec_status_t;

/* Input path the picture came from.  The white-level and sharpness thresholds
 * differ: a GPU readback is expanded limited->full range and clipped, which
 * steepens every edge.  GPU is the default (it is also the strictest). */
typedef enum {
    CARD_REC_PATH_Y = 0,        /* full-range decoder Y plane */
    CARD_REC_PATH_LIMITED,      /* limited-range Y plane */
    CARD_REC_PATH_GPU,          /* RGBA readback of the presented picture */
    CARD_REC_PATH_COUNT
} card_rec_path_t;

typedef enum {
    CARD_REC_ZONE_AUTO = 0,     /* try every known zone; exactly one must be a card */
    CARD_REC_ZONE_FULLSCREEN,   /* 330, 88, 780 x 294 */
    CARD_REC_ZONE_SPORTSCREEN,  /* 108, 88, 348 x 294 */
    CARD_REC_ZONE_CLASSICSCREEN,/* 540, 88, 360 x 294 */
    CARD_REC_ZONE_BOX,          /* explicit box (x, y, w, h); result zone "custom";
                                   x, y >= 0, 0 < w, h <= MAX_BOX, else BAD_ARGS */
    CARD_REC_ZONE_DETECT        /* black-box auto-detect; the box must snap to a
                                   known zone (params.detect_any: any box, diagnostic) */
} card_rec_zone_kind_t;

typedef struct {
    card_rec_zone_kind_t kind;
    int x, y, w, h;             /* CARD_REC_ZONE_BOX only */
} card_rec_zone_t;

#define CARD_REC_REASONS(X) \
    X(OK, "ok") \
    X(DISABLED, "disabled") \
    X(BAD_ARGS, "bad_args") \
    X(NO_BLACK_BOX, "no_black_box") \
    X(BLACK_BOX_TOO_SMALL, "black_box_too_small") \
    X(DETECT_NOT_ZONE, "detect_not_zone") \
    X(NO_CARD_BOX, "no_card_box") \
    X(CARD_BOX_MULTI, "card_box_multi") \
    X(DIM_CONTENT, "dim_content") \
    X(MOSTLY_NON_BLACK, "mostly_non_black") \
    X(TOO_LITTLE_CONTENT, "too_little_content") \
    X(NO_CONTENT, "no_content") \
    X(FIRST_LINE_HEIGHT, "first_line_height") \
    X(NO_DISTANCE_TEXT, "no_distance_text") \
    X(ICON_NOT_AT_LEFT, "icon_not_at_left") \
    X(ICON_TEXT_GAP, "icon_text_gap") \
    X(LINE1_EXTRA_CONTENT, "line1_extra_content") \
    X(ICON_SIZE, "icon_size") \
    X(DISTANCE_TEXT_HEIGHT, "distance_text_height") \
    X(DISTANCE_NOT_BESIDE_ICON, "distance_not_beside_icon") \
    X(DISTANCE_NOT_AT_ANCHOR, "distance_not_at_anchor") \
    X(SOFT_EDGES, "soft_edges") \
    X(TEXT_GHOST, "text_ghost") \
    X(DISTANCE_SIDE_LIGHT, "distance_side_light") \
    X(ICON_UNKNOWN, "icon_unknown") \
    X(ICON_AMBIGUOUS, "icon_ambiguous") \
    X(ICON_KEEP_UNSURE, "icon_keep_unsure") \
    X(ICON_DECOY, "icon_decoy") \
    X(RING_SIDE_AMBIGUOUS, "ring_side_ambiguous") \
    X(RING_ROTATED, "ring_rotated") \
    X(ICON_SOFT_EDGES, "icon_soft_edges") \
    X(ICON_GHOST, "icon_ghost") \
    X(ICON_GHOST_HALO, "icon_ghost_halo") \
    X(ICON_MID_TONES, "icon_mid_tones") \
    X(RING_NO_DIGIT_TEMPLATES, "ring_no_digit_templates") \
    X(RING_DIGIT_TOUCHES_RING, "ring_digit_touches_ring") \
    X(RING_NO_EXIT_NUMBER, "ring_no_exit_number") \
    X(RING_DIGIT_OFF_CENTRE, "ring_digit_off_centre") \
    X(RING_DIGIT_HEIGHT, "ring_digit_height") \
    X(RING_DIGIT_COUNT, "ring_digit_count") \
    X(RING_DIGIT_SIZE, "ring_digit_size") \
    X(RING_DIGIT_NO_TEMPLATE_FITS, "ring_digit_no_template_fits") \
    X(RING_DIGIT_GLYPH_UNKNOWN, "ring_digit_glyph_unknown") \
    X(RING_DIGIT_GLYPH_AMBIGUOUS, "ring_digit_glyph_ambiguous") \
    X(RING_DIGIT_LETTER, "ring_digit_letter") \
    X(RING_DIGIT_RESIDUAL, "ring_digit_residual") \
    X(RING_EXIT_OUT_OF_RANGE, "ring_exit_out_of_range") \
    X(DISTANCE_GLYPH_COUNT, "distance_glyph_count") \
    X(DISTANCE_GLYPH_TOO_BIG, "distance_glyph_too_big") \
    X(DISTANCE_NO_TEMPLATE_FITS, "distance_no_template_fits") \
    X(DISTANCE_GLYPH_UNKNOWN, "distance_glyph_unknown") \
    X(DISTANCE_GLYPH_AMBIGUOUS, "distance_glyph_ambiguous") \
    X(DISTANCE_GLYPH_LOOKALIKE, "distance_glyph_lookalike") \
    X(DISTANCE_SEPARATOR_SHAPE, "distance_separator_shape") \
    X(DISTANCE_GLYPH_SPACING, "distance_glyph_spacing") \
    X(DISTANCE_RESIDUAL, "distance_residual") \
    X(DISTANCE_GRAMMAR, "distance_grammar") \
    X(DISTANCE_UNIT_UNKNOWN, "distance_unit_unknown") \
    X(DISTANCE_NUMBER_GRAMMAR, "distance_number_grammar") \
    X(DISTANCE_IMPLAUSIBLE, "distance_implausible") \
    X(CLASS_NOT_PUBLISHED, "class_not_published")

#define CARD_REC_ENUM_REASON_(n, s) CARD_REC_R_##n,
typedef enum { CARD_REC_REASONS(CARD_REC_ENUM_REASON_) CARD_REC_R_COUNT } card_rec_reason_t;

typedef enum {
    CARD_REC_CLASS_NONE = 0,
    CARD_REC_CLASS_LEFT_TURN, CARD_REC_CLASS_RIGHT_TURN, CARD_REC_CLASS_STRAIGHT,
    CARD_REC_CLASS_KEEP_LEFT, CARD_REC_CLASS_KEEP_RIGHT, CARD_REC_CLASS_ARRIVE,
    CARD_REC_CLASS_UTURN_LEFT, CARD_REC_CLASS_UTURN_RIGHT,
    CARD_REC_CLASS_ROUNDABOUT_NUMBERED, CARD_REC_CLASS_ROUNDABOUT_LEFT,
    CARD_REC_CLASS_ROUNDABOUT_STRAIGHT, CARD_REC_CLASS_ROUNDABOUT_RIGHT,
    CARD_REC_CLASS_ROUNDABOUT_UTURN,
    CARD_REC_CLASS_COUNT
} card_rec_class_t;

typedef enum {
    CARD_REC_UNIT_NONE = 0, CARD_REC_UNIT_M, CARD_REC_UNIT_KM, CARD_REC_UNIT_FT,
    CARD_REC_UNIT_YD, CARD_REC_UNIT_MI, CARD_REC_UNIT_COUNT
} card_rec_unit_t;

typedef enum {
    CARD_REC_RB_NONE = 0, CARD_REC_RB_LEFT, CARD_REC_RB_STRAIGHT, CARD_REC_RB_RIGHT,
    CARD_REC_RB_UTURN
} card_rec_rb_dir_t;

/* Thresholds (as tuned on the Python reference).  Levels are on the black..white
 * stretched 0..255 scale, scores in per-mille, sizes in px at card scale.
 * card_rec_default_params() fills the tuned set; card_rec_recognise() rejects
 * with BAD_ARGS when a value is outside its documented range. */
typedef struct {
    int band, band_tol, band_min_pm, black_max, ring, ring_min_pm;
    int white_rank, contrast_min, white_min[CARD_REC_PATH_COUNT], cover_max_pm, ink_min;
    int sharp_min_pm[CARD_REC_PATH_COUNT], mid_max_pm[CARD_REC_PATH_COUNT];
    int icon_sharp_min_pm[CARD_REC_PATH_COUNT];
    int line_gap, icon_h_min, icon_h_max, icon_w_min, icon_w_max;
    int icon_x_min, icon_x_max, icon_gap, icon_gap_max;
    int dist_gap_max, dist_h_min, dist_h_max, dist_dy_max;
    int icon_d_max, icon_margin, side_margin, keep_d_max, decoy_eq_d, orient_margin;
    int ghost_t, ghost_tz, ghost_max_pm, halo_k, halo_gap, halo_max_pm;
    int faint_t, faint_max, text_ghost_max, text_ghost_t, text_res_max, icon_mid_tol_pm;
    int opt_tol, opt_white, opt_white_max_pm;
    int size_tol_min, size_tol_pm;
    int glyph_d_max, glyph_margin, glyph_size_tol, glyph_off_tol, space_min, space_pm;
    int glyph_look_margin;                          /* '~' lookalike labels */
    int gap_min, gap_max, gap_one_min, gap_one_max; /* number glyph gaps (-1 by . ,) */
    int sep_d_max, sep_peak_min, sep_flat_max_pm;   /* decimal separator shape */
    int dist_x0, dist_x0_tol;                       /* distance ink start in the box */
    int side_light_gap, side_light_t;               /* light right of the distance */
    int ring_digit_d_max, ring_digit_margin, ring_digit_h_min, ring_digit_h_max;
    int ring_letter_margin, ring_digit_ctr_tol, ring_digit_cap_tol, ring_res_max;
    int detect_any;             /* 1 = diagnostic: DETECT accepts a box that is no zone */
    unsigned unpublished;       /* classes read but never published: bit (1u << class) */
} card_rec_params_t;

typedef struct {                        /* a complete reading withheld from the output */
    card_rec_class_t cls;
    int cp_maneuver_type, exit_number, distance_value_x100, distance_m;   /* -1 / 0 / -1 / -1 */
    card_rec_unit_t distance_unit;
    char traffic_side;
    char distance_text[CARD_REC_TEXT_MAX];
} card_rec_reading_t;

typedef struct {
    int accept;                         /* 1 ACCEPT, 0 REJECT */
    card_rec_reason_t reason;
    card_rec_class_t cls;
    /* Waze: map cls via WazeCardManeuver (Java), NEVER cp via ManeuverMapper.map() (cp 6 = RB L/S/R -> right turn) */
    int cp_maneuver_type;               /* CPManeuverType; 28 + exit - 1 for the
                                           numbered ring; -1 when rejected */
    int exit_number;                    /* 1..19 for the numbered ring, else 0 */
    int distance_value_x10;             /* "1.2 km" -> 12; -1 when absent or when
                                           the value has hundredths (use x100) */
    int distance_value_x100;            /* exact value, -1 when absent */
    card_rec_unit_t distance_unit;
    int distance_m;                     /* rounded metres, -1 when absent */
    char traffic_side;                  /* 'R' / 'L' (art drawn for that side) / 0 */
    card_rec_rb_dir_t roundabout_direction;
    int score;                          /* best icon distance (per-mille), -1 */
    int margin;                         /* next other-group template - best, -1 */
    int icon_template;                  /* index of the best icon template, -1 */
    card_rec_zone_kind_t zone;          /* zone the card box was found in; BOX =
                                           custom; AUTO = none */
    int box_x, box_y, box_w, box_h;
    int black, white, cover_pm, ink, sharp_pm, mid_pm;   /* -1 = not reached */
    char distance_text[CARD_REC_TEXT_MAX];               /* UTF-8, "" if none */
    card_rec_reading_t internal;        /* CLASS_NOT_PUBLISHED (STRAIGHT / ARRIVE): the
                                           reading, for the stabiliser's screen history */
} card_rec_result_t;

typedef struct card_rec card_rec_t;

void card_rec_default_params(card_rec_params_t *p);
int card_rec_params_valid(const card_rec_params_t *p);   /* 1 when every value is in range */

/* Load the template root `dir` (NULL or "" = $CARD_REC_TEMPLATES).  Returns
 * CARD_REC_OK with *out set, or DISABLED (nothing configured / the root is
 * absent) / ERR_* with *out = NULL; err (optional) gets a one-line reason. */
card_rec_status_t card_rec_open(card_rec_t **out, const char *dir, char *err, size_t err_len);
void card_rec_close(card_rec_t *ctx);

/* Mutable thresholds of a context (tests, field tuning).  Never NULL for an
 * open context; out-of-range values make every recognise call BAD_ARGS. */
card_rec_params_t *card_rec_params(card_rec_t *ctx);
void card_rec_set_input_path(card_rec_t *ctx, card_rec_path_t path);
void card_rec_template_counts(const card_rec_t *ctx, int *icons, int *glyphs, int *ring_digits);
const char *card_rec_icon_name(const card_rec_t *ctx, int index);
const char *card_rec_icon_group(const card_rec_t *ctx, int index);   /* output group, "-" */

/* Recognise the card in one picture.  rgba: 8-bit R,G,B,A rows, `stride`
 * bytes apart (4 * width <= |stride| <= MAX_STRIDE; negative for a bottom-up
 * GL readback, pass the address of the top row).  zone NULL = AUTO.  ctx NULL
 * (disabled) rejects with CARD_REC_R_DISABLED.  Returns out->accept. */
int card_rec_recognise(card_rec_t *ctx, const uint8_t *rgba, int stride, int width, int height,
                       const card_rec_zone_t *zone, card_rec_result_t *out);

/* Temporal rule per 30 fps frame (card_rec_stab.h
 * feeds it by time).  A maneuver is shown only
 * after max(run_need, need) consecutive ACCEPTs of (class, cp, exit, side)
 * AND `need` identical distance reads (the candidate).  READS: every ACCEPT
 * plus the internal reading of a CLASS_NOT_PUBLISHED card.  References: the
 * memory (last published read, survives REJECTs) and a history of the last
 * HIST_N distinct reads (published or not, forgotten after HIST_AGE frames).
 * need (fixed when the candidate appears) = n for the published read again;
 * else the largest, over the references, of: other unit system (m km vs ft
 * yd mi) UNIT_K * n; half-changed (new key with the old distance, or the old
 * key with a distance that went up or dropped by more than max(DROP_ABS_M,
 * DROP_PM per-mille) + V_MAX_MM per frame the reference was unseen) SPLIT_K
 * * n; else n; and SPLIT_K * n for a metric 'm' value off the 10 m ladder.
 * run_need = n with a memory or a history read seen for SPLIT_K * n frames;
 * else COLD_K * n when the card freshly appeared (first read <= FRESH_MAX
 * frames after a no-card frame, no other key since), else SPLIT_K * n.  Any
 * REJECT resets the run and withdraws what is shown; an upward distance jump
 * inside one maneuver, or a candidate that needs more than n, does the same;
 * a smaller distance replaces the shown one after `need` identical reads. */
typedef struct {
    card_rec_class_t cls;
    int cp_maneuver_type, exit_number;
    char traffic_side;
    int distance_m;
    char distance_text[CARD_REC_TEXT_MAX];
} card_rec_shown_t;

typedef struct {                        /* one read: maneuver key, metres, unit system */
    card_rec_class_t cls;
    int cp_maneuver_type, exit_number;
    char traffic_side;
    int m, imp;                         /* metres; imp 1 = ft / yd / mi */
    int age, cnt;                       /* frames since last read; frames read (history) */
} card_rec_stab_read_t;

typedef struct {
    int n, pub, pub_imp;                /* pub: shown is valid */
    int run_n, last_m, run_need;        /* the current key run (run_n 0: none) */
    card_rec_stab_read_t run;
    int cand_m, cand_n, cand_need, cand_imp;
    char cand_text[CARD_REC_TEXT_MAX];
    card_rec_shown_t shown;
    int mem_valid;                      /* memory: the last published read, mem.age */
    card_rec_stab_read_t mem;
    int hist_n;                         /* screen history, newest first */
    card_rec_stab_read_t hist[CARD_REC_STAB_HIST_N];
    int empty_age, ep_ok, ep_valid;     /* frames since a no-card frame; that appearance */
    card_rec_stab_read_t ep;            /*   is still fresh; the key of its first read */
} card_rec_stab_t;

void card_rec_stab_init(card_rec_stab_t *s, int n);      /* n outside 1..1000: CARD_REC_STAB_N */
/* Feed one result (NULL = no frame: reset).  Returns 1 and fills *shown (may
 * be NULL) when the HUD may show a maneuver after this frame, else 0. */
int card_rec_stab_push(card_rec_stab_t *s, const card_rec_result_t *r, card_rec_shown_t *shown);

#include "card_rec_stab.h"              /* the time-driven feed of this rule */

const char *card_rec_reason_name(card_rec_reason_t r);
const char *card_rec_class_name(card_rec_class_t c);
const char *card_rec_unit_name(card_rec_unit_t u);
const char *card_rec_zone_name(card_rec_zone_kind_t z);

#ifdef CARD_REC_INTERNAL
/* Context layout, shared by card_rec.c (pipeline), card_rec_match.c (icon
 * classification, glyph matching), card_rec_text.c (distance),
 * card_rec_gates.c (icon gates, ring exit number), card_rec_stab.c
 * (stabiliser) and card_rec_load.c (templates).  Callers use the API above. */
typedef struct {
    char name[24];
    int group;                  /* index into the output-group table */
    char side;                  /* 'R' / 'L' traffic side of the art, '-' */
    int exp_w, exp_h;           /* expected stroke bbox at card scale */
    int has_hole, hole_cx_pm, hole_cy_pm, hole_r_pm;  /* ring digit hole */
    int has_care;               /* a care mask was given (else all scored) */
    uint8_t img[CARD_REC_NORM * CARD_REC_NORM];
    uint8_t care[CARD_REC_NORM * CARD_REC_NORM];       /* 2 scored, 1 grey, 0 off */
} card_rec_icon_t;

typedef struct {
    int label;                  /* label id: templates are grouped by label */
    int w, h, top, bottom;      /* top/bottom row minus the baseline row */
    uint32_t sum;
    uint8_t *px;
} card_rec_glyph_t;

typedef struct {
    int count, nlabels, dot, comma;   /* dot/comma: label ids of '.' / ',' or -1 */
    int nletters;                     /* labels that are not all digits */
    card_rec_glyph_t *t;
    char labels[CARD_REC_MAX_LABELS][CARD_REC_LABEL_MAX];
    uint8_t is_digit[CARD_REC_MAX_LABELS];
} card_rec_glyph_set_t;

struct card_rec {
    card_rec_params_t p;
    card_rec_path_t path;
    int nicons, ring_cap;       /* ring_cap: tallest ring digit (manifest rows) */
    card_rec_icon_t icons[CARD_REC_MAX_ICONS];
    card_rec_glyph_set_t glyphs, ring;
    /* D4 decoys: output groups (bit mask) each transform of each icon template
     * is equivalent to; built for (decoy_tol, decoy_eq) */
    uint32_t decoy_groups[CARD_REC_MAX_ICONS][CARD_REC_D4];
    int decoy_tol, decoy_eq;
    /* per-frame scratch (never reallocated) */
    uint8_t box[CARD_REC_MAX_BOX_W * CARD_REC_MAX_BOX_H];
    uint8_t expl[CARD_REC_MAX_ICON * CARD_REC_MAX_BOX_W];   /* pasted templates */
    uint8_t sq[CARD_REC_MAX_ICON * CARD_REC_MAX_ICON];
    uint8_t sub[CARD_REC_MAX_ICON * CARD_REC_MAX_ICON];
    uint8_t norm[CARD_REC_NORM * CARD_REC_NORM];
    uint8_t dn[CARD_REC_NORM * CARD_REC_NORM];               /* transformed norm */
    uint8_t flag[CARD_REC_MAX_FRAME];
    uint32_t cnt[CARD_REC_MAX_FRAME];
};

int card_rec_group_index(const char *name);   /* -1 when unknown */
int card_rec_group_count(void);
const char *card_rec_group_name(int group);
int card_rec_group_is_ring(int group);
int card_rec_group_is_keep(int group);
const char *card_rec_unit_label(int i);       /* grammar unit labels, NULL past the end */

/* card_rec_match.c: icon classification */
card_rec_reason_t card_rec_match_icon(card_rec_t *c, const uint8_t *s, int stride, int iw, int ih, char *side,
                                      card_rec_result_t *out);
/* card_rec_text.c: the distance on the stretched box (glyphs, spacing,
 * separator shape, residual, grammar) */
card_rec_reason_t card_rec_read_distance(card_rec_t *c, const uint8_t *b, int stride, int bw, int bh,
                                         card_rec_result_t *out);
int card_rec_icon_distance(const uint8_t *o, const uint8_t *img, const uint8_t *care, int opt_tol);
void card_rec_d4(const uint8_t *src, uint8_t *dst, int k, int scatter);
void card_rec_build_decoys(card_rec_t *c);

typedef struct {
    int x0, x1, y0, y1;
} card_rec_gbox_t;

enum { CARD_REC_MATCH_OK = 0, CARD_REC_MATCH_NO_FIT, CARD_REC_MATCH_UNKNOWN, CARD_REC_MATCH_AMBIGUOUS };
/* ALL: every readable label ('~' lookalike labels excluded); REJECT: only
 * the '~' labels; DIGITS / LETTERS: digit / non-digit labels */
enum { CARD_REC_LABELS_ALL = 0, CARD_REC_LABELS_DIGITS, CARD_REC_LABELS_LETTERS, CARD_REC_LABELS_REJECT };

typedef struct {
    int label, d;               /* best label id and its distance (1000: none fits) */
    const card_rec_glyph_t *t;  /* first template of that label at d */
    int dx, dy;                 /* its best +-1 px placement */
} card_rec_gmatch_t;

int card_rec_segment(const uint8_t *b, int stride, int bw, int bh, card_rec_gbox_t *g, int max);
int card_rec_baseline(const card_rec_gbox_t *g, int n);
int card_rec_match_glyph(const card_rec_glyph_set_t *set, const card_rec_params_t *p, const uint8_t *img,
                         int stride, const card_rec_gbox_t *g, int base, int filter, int d_max, int margin_min,
                         card_rec_gmatch_t *m);
void card_rec_paste(uint8_t *ex, int ew, int eh, int x0, int y1, const card_rec_gmatch_t *m);

/* card_rec_gates.c */
int card_rec_faint_count(const uint8_t *s, int stride, int w, int h, int ft, int fmax);
card_rec_reason_t card_rec_icon_gates(card_rec_t *c, const uint8_t *s, int bw, int bh, const int *icon_bb,
                                      int d0, const card_rec_icon_t *t);
card_rec_reason_t card_rec_read_ring(card_rec_t *c, const uint8_t *crop, int cs, int w, int h,
                                     const card_rec_icon_t *t, int *exit_n);

#define T_INK 128   /* levels on the black..white stretched scale */
#define T_MID 32
#define T_ANY 16

static inline int cr_abs(int v) { return v < 0 ? -v : v; }
static inline int cr_min(int a, int b) { return a < b ? a : b; }
static inline int cr_max(int a, int b) { return a > b ? a : b; }

/* 3 x 3 max of a (w x h, row stride as) at (x, y), window clipped to the grid */
static inline int cr_dil3(const uint8_t *a, int as, int w, int h, int x, int y) {
    int i, j, m = 0;
    for (j = cr_max(0, y - 1); j <= cr_min(h - 1, y + 1); ++j)
        for (i = cr_max(0, x - 1); i <= cr_min(w - 1, x + 1); ++i)
            m = cr_max(m, a[j * as + i]);
    return m;
}

/* rows y0..y1 (inclusive) of columns x0..x1 that hold ink: first / last */
static inline int cr_ink_rows(const uint8_t *s, int stride, int x0, int x1, int y0, int y1, int *first,
                              int *last) {
    int x, y, f = -1, l = -1;
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x)
            if (s[y * stride + x] >= T_INK) {
                if (f < 0)
                    f = y;
                l = y;
                break;
            }
    *first = f;
    *last = l;
    return f >= 0;
}
#endif

#endif /* ALTSCREEN_CARD_REC_H */
