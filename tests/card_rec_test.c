/*
 * card_rec (CarPlay instruction-card recogniser) tests.
 *
 * Part 1 always runs (card_rec_test_api.h): open / disable, every loader
 * error path, the pipeline and its fail-closed gates on synthetic cards, the
 * ring exit number, DETECT, argument validation and the stabiliser.  Its
 * templates and frames are plain shapes generated here - no third-party art.
 *
 * Part 2 needs fixtures that can never be committed (car captures, the
 * navigation app's art, web screenshots, simulator recordings):
 * CARD_REC_FIXTURES points at a directory with templates/, frames/ (gzip
 * PGM/PPM), goldens.txt and meta.json from the Python reference recogniser
 * (not part of this repository).  For every golden decision the C result must match the
 * reference exactly (decision, reason, zone, class, CPManeuverType, exit,
 * distance, unit, side, text, icon score / margin and the gate metrics).
 * Every expectation token is judged: a WRONG accept (label contradicted,
 * must-reject frame accepted, attack frame read as a near or no truth) and a
 * GATE_LEAK (a must-reject attack frame read exactly) are counted separately;
 * one that is not on the reviewed known-attack list fails the run, as does a
 * listed one that no longer happens (card_rec_test_ratchet.h), an unknown
 * token, a reference
 * threshold that differs from card_rec_default_params() or a decision count
 * that differs from meta.json.  Without fixtures the part SKIPS.  Frames
 * alternate between packed, padded and bottom-up (negative stride) RGBA.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define _POSIX_C_SOURCE 200809L
#define CARD_REC_INTERNAL                   /* white-box ring tests */
#include "../altscreen_render/card_rec.h"

#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

#include "card_rec_test_synth.h"
#include "card_rec_test_api.h"

/* ------------------------------------------------------- reference params */
#define PARAMS_1(X) X(BAND, band) X(BAND_TOL, band_tol) X(BAND_MIN_PM, band_min_pm) X(BLACK_MAX, black_max) \
    X(RING, ring) X(RING_MIN_PM, ring_min_pm) X(WHITE_RANK, white_rank) X(CONTRAST_MIN, contrast_min) \
    X(COVER_MAX_PM, cover_max_pm) X(INK_MIN, ink_min) X(LINE_GAP, line_gap) X(ICON_H_MIN, icon_h_min) \
    X(ICON_H_MAX, icon_h_max) X(ICON_W_MIN, icon_w_min) X(ICON_W_MAX, icon_w_max) X(ICON_X_MIN, icon_x_min) \
    X(ICON_X_MAX, icon_x_max) X(ICON_GAP, icon_gap) X(ICON_GAP_MAX, icon_gap_max) X(DIST_GAP_MAX, dist_gap_max) \
    X(DIST_H_MIN, dist_h_min) X(DIST_H_MAX, dist_h_max) X(DIST_DY_MAX, dist_dy_max) X(ICON_D_MAX, icon_d_max) \
    X(ICON_MARGIN, icon_margin) X(SIDE_MARGIN, side_margin) X(KEEP_D_MAX, keep_d_max) X(DECOY_EQ_D, decoy_eq_d) \
    X(ORIENT_MARGIN, orient_margin) X(GHOST_T, ghost_t) X(GHOST_TZ, ghost_tz) X(GHOST_MAX_PM, ghost_max_pm) \
    X(HALO_K, halo_k) X(HALO_GAP, halo_gap) X(HALO_MAX_PM, halo_max_pm) X(FAINT_T, faint_t) \
    X(FAINT_MAX, faint_max) X(TEXT_GHOST_MAX, text_ghost_max) X(TEXT_GHOST_T, text_ghost_t) \
    X(TEXT_RES_MAX, text_res_max) X(ICON_MID_TOL_PM, icon_mid_tol_pm) X(OPT_TOL, opt_tol) X(OPT_WHITE, opt_white) \
    X(OPT_WHITE_MAX_PM, opt_white_max_pm) X(SIZE_TOL_MIN, size_tol_min) X(SIZE_TOL_PM, size_tol_pm) \
    X(GLYPH_D_MAX, glyph_d_max) X(GLYPH_MARGIN, glyph_margin) X(GLYPH_SIZE_TOL, glyph_size_tol) \
    X(GLYPH_OFF_TOL, glyph_off_tol) X(SPACE_MIN, space_min) X(SPACE_PM, space_pm) \
    X(GLYPH_LOOK_MARGIN, glyph_look_margin) X(GAP_MIN, gap_min) X(GAP_MAX, gap_max) X(GAP_ONE_MIN, gap_one_min) \
    X(GAP_ONE_MAX, gap_one_max) X(SEP_D_MAX, sep_d_max) X(SEP_PEAK_MIN, sep_peak_min) \
    X(SEP_FLAT_MAX_PM, sep_flat_max_pm) X(DIST_X0, dist_x0) X(DIST_X0_TOL, dist_x0_tol) \
    X(SIDE_LIGHT_GAP, side_light_gap) X(SIDE_LIGHT_T, side_light_t) \
    X(RING_DIGIT_D_MAX, ring_digit_d_max) X(RING_DIGIT_MARGIN, ring_digit_margin) \
    X(RING_DIGIT_H_MIN, ring_digit_h_min) X(RING_DIGIT_H_MAX, ring_digit_h_max) \
    X(RING_LETTER_MARGIN, ring_letter_margin) X(RING_DIGIT_CTR_TOL, ring_digit_ctr_tol) \
    X(RING_DIGIT_CAP_TOL, ring_digit_cap_tol) X(RING_RES_MAX, ring_res_max) X(DETECT_ANY, detect_any)
#define PARAMS_3(X) X(WHITE_MIN, white_min) X(SHARP_MIN_PM, sharp_min_pm) X(MID_MAX_PM, mid_max_pm) \
    X(ICON_SHARP_MIN_PM, icon_sharp_min_pm)
#define PCOUNT_(N, f) +1
enum { N_REF_PARAMS = 0 PARAMS_1(PCOUNT_) + 3 * (0 PARAMS_3(PCOUNT_)) + 1 };   /* + UNPUBLISHED */

static int *param_ref(card_rec_params_t *q, const char *k) {
#define PREF1_(N, f) if (!strcmp(k, #N)) return &q->f;
#define PREF3_(N, f) if (!strcmp(k, #N ".y")) return &q->f[0]; if (!strcmp(k, #N ".limited")) return &q->f[1]; \
    if (!strcmp(k, #N ".gpu")) return &q->f[2];
    PARAMS_1(PREF1_) PARAMS_3(PREF3_)
    return 0;
}

/* '# params K=V ...' must name every threshold once, at the C default */
static int check_params(char *line, const card_rec_params_t *d) {
    card_rec_params_t q = *d;
    char *tok, *save, *eq;
    int seen = 0, bad = 0, c;
    for (tok = strtok_r(line + 9, " \n", &save); tok; tok = strtok_r(0, " \n", &save), ++seen) {
        int *v;
        if (!(eq = strchr(tok, '='))) {
            ++bad;
            continue;
        }
        *eq++ = 0;
        if (!strcmp(tok, "UNPUBLISHED")) {
            unsigned m = 0;
            char *cl, *s2;
            for (cl = strtok_r(eq, ",", &s2); cl; cl = strtok_r(0, ",", &s2)) {
                for (c = 0; c < CARD_REC_CLASS_COUNT && strcmp(cl, card_rec_class_name((card_rec_class_t)c)); ++c)
                    ;
                m |= c < CARD_REC_CLASS_COUNT ? 1u << c : 1u << 31;
            }
            if (m != d->unpublished && ++bad)
                printf("FAIL reference UNPUBLISHED differs from the C default\n");
        } else if (!(v = param_ref(&q, tok)) || *v != atoi(eq)) {
            printf("FAIL reference param %s=%s: C %s\n", tok, eq, v ? "default differs" : "has no such param");
            ++bad;
        }
    }
    if (seen != N_REF_PARAMS) {
        printf("FAIL reference lists %d params, C has %d\n", seen, N_REF_PARAMS);
        ++bad;
    }
    return bad;
}

/* ------------------------------------------------------------ fixture I/O */
typedef struct {
    char name[96];
    int w, h, rgb;
    uint8_t *px;
} img_t;

static int load_frame(const char *dir, const char *name, img_t *im) {
    char path[1024], hdr[32];
    int i, len, n, maxv;
    gzFile g;
    if (!strcmp(im->name, name))
        return 1;
    im->name[0] = 0;
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!(g = gzopen(path, "rb")))
        return 0;
    for (i = 0, len = 0; i < 4 && len < (int)sizeof(hdr) - 1; ++len) {   /* "P5\nW H\n255\n" */
        if (gzread(g, hdr + len, 1) != 1)
            break;
        i += hdr[len] == '\n' || hdr[len] == ' ';
    }
    hdr[len] = 0;
    if (sscanf(hdr, "P%d %d %d %d", &n, &im->w, &im->h, &maxv) != 4 || (n != 5 && n != 6) || maxv != 255 ||
        im->w < 1 || im->w > FW || im->h < 1 || im->h > FH) {
        gzclose(g);
        return 0;
    }
    im->rgb = n == 6;
    n = im->w * im->h * (im->rgb ? 3 : 1);
    if (gzread(g, im->px, (unsigned)n) != n) {
        gzclose(g);
        return 0;
    }
    gzclose(g);
    snprintf(im->name, sizeof(im->name), "%s", name);
    return 1;
}

/* a fixture luma value as the given input path would deliver it */
static int to_path(int y, card_rec_path_t p) {
    int v;
    if (p == CARD_REC_PATH_GPU) {
        v = (y - 16) * 255 + 109;
        return v < 0 ? 0 : (v / 219 > 255 ? 255 : v / 219);
    }
    return p == CARD_REC_PATH_LIMITED ? 16 + (y * 219 + 127) / 255 : y;
}

static void build_rgba(const img_t *im, card_rec_path_t path, uint8_t *dst, long stride) {
    int x, y;
    for (y = 0; y < im->h; ++y)
        for (x = 0; x < im->w; ++x) {
            uint8_t *q = dst + y * stride + 4L * x;
            const uint8_t *s = im->px + ((size_t)y * im->w + x) * (im->rgb ? 3 : 1);
            if (im->rgb && path == CARD_REC_PATH_Y) {
                q[0] = s[0]; q[1] = s[1]; q[2] = s[2];
            } else {
                int l = im->rgb ? (77 * s[0] + 150 * s[1] + 29 * s[2]) >> 8 : s[0];
                q[0] = q[1] = q[2] = (uint8_t)to_path(l, path);
            }
            q[3] = 255;
        }
}

/* ------------------------------------------------------------ expectations */
enum { V_OK = 0, V_WRONG, V_LEAK, V_BAD_TOKEN };

static int tok_int(const char *s) { return strcmp(s, "-") ? atoi(s) : -1; }

static int num_tok(const char *s, int *v) {
    char *end;
    long n = strtol(s, &end, 10);
    if (!*s || *end || n < INT_MIN || n > INT_MAX)
        return 0;
    *v = (int)n;
    return 1;
}

static int split(char *s, int sep, char **f, int max) {
    int n = 1;
    f[0] = s;
    for (; *s; ++s)
        if (*s == sep) {
            if (n == max)
                return max + 1;
            *s = 0;
            f[n++] = s + 1;
        }
    return n;
}

/* atk:M:TRUTHS (the reference attack judge): truths kind/groups/m/exit/side */
static int judge_attack(char *e, const card_rec_result_t *r, card_rec_t *c) {
    char *tr[16], *f[5], *g[8];
    int nt, i, j, m = 0, ex = 0, verdict = -1, hit;
    if ((e[0] != 'R' && e[0] != 'A') || e[1] != ':' || !e[2])
        return V_BAD_TOKEN;
    nt = strcmp(e + 2, "~") ? split(e + 2, ';', tr, 16) : 0;
    if (nt > 16)
        return V_BAD_TOKEN;
    for (i = 0; i < nt; ++i) {
        int ng;
        if (split(tr[i], '/', f, 5) != 5 || (strcmp(f[0], "e") && strcmp(f[0], "n")) || !num_tok(f[2], &m) ||
            (strcmp(f[3], "-") && !num_tok(f[3], &ex)) || (strcmp(f[4], "-") && strcmp(f[4], "R") && strcmp(f[4], "L")))
            return V_BAD_TOKEN;
        if (!strcmp(f[3], "-"))
            ex = 0;
        if ((ng = split(f[1], '+', g, 8)) > 8)
            return V_BAD_TOKEN;
        for (j = 0, hit = 0; j < ng; ++j) {
            if (card_rec_group_index(g[j]) < 0)
                return V_BAD_TOKEN;
            hit |= r->accept && !strcmp(g[j], card_rec_icon_group(c, r->icon_template));
        }
        if (!hit || r->distance_m != m || r->exit_number != ex)
            continue;
        if (f[4][0] != '-' && (r->traffic_side == 'R' || r->traffic_side == 'L') && r->traffic_side != f[4][0])
            continue;
        if (f[0][0] == 'e' && verdict < 0)     /* the first exact truth decides */
            verdict = e[0] == 'R' ? V_LEAK : V_OK;
    }
    /* no exact truth: NEAR_WRONG or WRONG, a wrong arrow either way */
    return !r->accept ? V_OK : verdict < 0 ? V_WRONG : verdict;
}

/* none | free | atk:... | CLS:cp:exit:v100:unit:side:text (exactly 7 fields;
 * '-' / '*' = unchecked, v100 'X' = no correct distance exists) */
static int judge(const char *exp, const card_rec_result_t *r, card_rec_t *c) {
    char e[512], *f[8], *u;
    int cls, cp, v, n;
    if (!strcmp(exp, "none") || !strcmp(exp, "free"))
        return r->accept && exp[0] == 'n' ? V_WRONG : V_OK;
    if (strlen(exp) >= sizeof(e))
        return V_BAD_TOKEN;
    strcpy(e, exp);
    if (!strncmp(e, "atk:", 4))
        return judge_attack(e + 4, r, c);
    if (split(e, ':', f, 7) != 7 || !num_tok(f[1], &cp) || (strcmp(f[2], "-") && !num_tok(f[2], &v)) ||
        (strcmp(f[3], "X") && strcmp(f[3], "*") && !num_tok(f[3], &v)) ||
        (strcmp(f[5], "-") && strcmp(f[5], "R") && strcmp(f[5], "L")))
        return V_BAD_TOKEN;
    for (cls = 0; cls < CARD_REC_CLASS_COUNT && strcmp(f[0], card_rec_class_name((card_rec_class_t)cls)); ++cls)
        ;
    for (n = 0; n < CARD_REC_UNIT_COUNT && strcmp(f[4], card_rec_unit_name((card_rec_unit_t)n)); ++n)
        ;
    if (cls == CARD_REC_CLASS_COUNT || (n == CARD_REC_UNIT_COUNT && strcmp(f[4], "*") && strcmp(f[4], "X")))
        return V_BAD_TOKEN;
    if (!r->accept)
        return V_OK;
    if ((int)r->cls != cls || r->cp_maneuver_type != cp || (strcmp(f[2], "-") && atoi(f[2]) != r->exit_number))
        return V_WRONG;
    if (!strcmp(f[3], "X") || (strcmp(f[3], "*") && (atoi(f[3]) != r->distance_value_x100 ||
                                                    strcmp(f[4], card_rec_unit_name(r->distance_unit)))))
        return V_WRONG;
    if (f[5][0] != '-' && (r->traffic_side == 'R' || r->traffic_side == 'L') && r->traffic_side != f[5][0])
        return V_WRONG;
    for (u = f[6]; *u; ++u)
        if (*u == '_')
            *u = ' ';
    return strcmp(f[6], "-") && strcmp(f[6], r->distance_text) ? V_WRONG : V_OK;
}

/* "decisions": {"set": n, ...} and "decisions_total": n of meta.json */
static int meta_count(const char *meta, const char *set) {
    char key[64];
    const char *p = strstr(meta, "\"decisions\""), *end = p ? strchr(p, '}') : 0, *q;
    int n;
    if (!set)
        return (q = strstr(meta, "\"decisions_total\":")) && sscanf(q + 18, "%d", &n) == 1 ? n : -1;
    snprintf(key, sizeof(key), "\"%s\":", set);
    return p && end && (q = strstr(p, key)) && q < end && sscanf(q + strlen(key), "%d", &n) == 1 ? n : -1;
}

#define NSETS 16
typedef struct {
    char name[16];
    int n, acc, mismatch, wrong, leak, fresh;   /* fresh: wrong / leak not on the known list */
} set_t;

#include "card_rec_test_ratchet.h"

static int parse_zone(const char *s, card_rec_zone_t *z) {
    static const char *const names[] = {"auto", "fullscreen", "sportscreen", "classicscreen", "-", "detect"};
    int i, used = 0;
    memset(z, 0, sizeof(*z));
    if (!strncmp(s, "box:", 4)) {
        z->kind = CARD_REC_ZONE_BOX;
        return sscanf(s + 4, "%d,%d,%d,%d%n", &z->x, &z->y, &z->w, &z->h, &used) == 4 && !s[4 + used];
    }
    for (i = 0; i < 6 && strcmp(names[i], s); ++i)
        ;
    z->kind = (card_rec_zone_kind_t)i;
    return i < 6 && i != CARD_REC_ZONE_BOX;
}

static int parity_tests(const char *fx) {
    char path[1024], line[8192], err[256], *t[27], *save, *meta = 0;
    set_t sets[NSETS];
    int nsets = 0, n, i, s, lines = 0, bad = 0, total = 0, mm_t = 0, wrong_t = 0, leak_t = 0, mode, sw;
    long stride, msz = 0;
    card_rec_t *c = 0;
    card_rec_params_t defaults;
    img_t im;
    uint8_t *buf, *base;
    clock_t spent = 0;
    FILE *f;
    snprintf(path, sizeof(path), "%s/templates", fx);
    if (card_rec_open(&c, path, err, sizeof(err)) != CARD_REC_OK) {
        printf("FAIL fixture templates: %s\n", err);
        return 1;
    }
    defaults = *card_rec_params(c);
    snprintf(path, sizeof(path), "%s/meta.json", fx);
    if ((f = fopen(path, "rb")) && fseek(f, 0, SEEK_END) == 0 && (msz = ftell(f)) > 0 && msz < (1L << 20) &&
        (meta = (char *)calloc(1, (size_t)msz + 1)) && fseek(f, 0, SEEK_SET) == 0)
        meta[fread(meta, 1, (size_t)msz, f)] = 0;
    if (f)
        fclose(f);
    snprintf(path, sizeof(path), "%s/goldens.txt", fx);
    f = fopen(path, "r");
    memset(&im, 0, sizeof(im));
    im.px = (uint8_t *)malloc((size_t)FW * FH * 3);
    buf = (uint8_t *)malloc((size_t)(FW * 4 + 64) * FH);
    if (!f || !im.px || !buf || !meta) {
        printf("FAIL cannot read %s or meta.json\n", path);
        return 1;
    }
    memset(sets, 0, sizeof(sets));
    known_load();
    while (fgets(line, sizeof(line), f)) {
        card_rec_result_t r;
        card_rec_zone_t zone;
        card_rec_path_t p;
        char text[CARD_REC_TEXT_MAX], side[2] = {0, 0};
        int mm = 0, v;
        clock_t t0;
        if (!strchr(line, '\n') && !feof(f)) {
            printf("FAIL golden line %d longer than %d bytes\n", lines + 1, (int)sizeof(line) - 2);
            ++bad;
            break;
        }
        if (!strncmp(line, "# params ", 9)) {
            bad += check_params(line, &defaults);
            continue;
        }
        if (line[0] == '#')
            continue;
        for (n = 0, t[0] = strtok_r(line, " \n", &save); t[n] && n < 26;)
            t[++n] = strtok_r(0, " \n", &save);
        p = !strcmp(t[3], "gpu") ? CARD_REC_PATH_GPU : !strcmp(t[3], "limited") ? CARD_REC_PATH_LIMITED
                                                                                : CARD_REC_PATH_Y;
        if (n != 25 || !load_frame(fx, t[2], &im) || !parse_zone(t[4], &zone) ||
            (strcmp(t[3], "y") && p == CARD_REC_PATH_Y) || (strcmp(t[5], "default") && strcmp(t[5], "nogate"))) {
            printf("FAIL bad golden line %d (%d fields) or frame %s\n", lines + 1, n, n > 2 ? t[2] : "?");
            ++bad;
            break;
        }
        for (s = 0; s < nsets && strcmp(sets[s].name, t[0]); ++s)
            ;
        if (s == nsets) {
            if (nsets == NSETS) {
                printf("FAIL more than %d sets\n", NSETS);
                ++bad;
                break;
            }
            snprintf(sets[nsets++].name, sizeof(sets[0].name), "%s", t[0]);
        }
        *card_rec_params(c) = defaults;
        if (!strcmp(t[5], "nogate"))            /* eval.NOGATE */
            for (i = 0; i < CARD_REC_PATH_COUNT; ++i) {
                card_rec_params(c)->sharp_min_pm[i] = 0;
                card_rec_params(c)->mid_max_pm[i] = 1000;
                card_rec_params(c)->icon_sharp_min_pm[i] = 0;
            }
        card_rec_set_input_path(c, p);
        mode = lines % 3;                       /* packed / padded / bottom-up */
        sw = im.w * 4;
        stride = mode == 1 ? sw + 64 : sw;
        base = mode == 2 ? buf + (im.h - 1) * stride : buf;
        build_rgba(&im, p, base, mode == 2 ? -stride : stride);
        t0 = clock();
        card_rec_recognise(c, base, (int)(mode == 2 ? -stride : stride), im.w, im.h,
                           zone.kind == CARD_REC_ZONE_AUTO ? 0 : &zone, &r);
        spent += clock() - t0;
        view_internal(&r);                      /* CLASS_NOT_PUBLISHED: the withheld reading */
        snprintf(text, sizeof(text), "%s", t[16]);
        for (i = 0; text[i]; ++i)
            if (text[i] == '_')
                text[i] = ' ';
        side[0] = r.traffic_side ? r.traffic_side : '-';
        mm |= strcmp(t[7], r.accept ? "ACCEPT" : "REJECT") || strcmp(t[8], card_rec_reason_name(r.reason));
        mm |= strcmp(t[9], card_rec_zone_name(r.zone)) || strcmp(t[10], card_rec_class_name(r.cls));
        mm |= tok_int(t[11]) != r.cp_maneuver_type || atoi(t[12]) != r.exit_number;
        mm |= tok_int(t[13]) != r.distance_value_x100 || strcmp(t[14], card_rec_unit_name(r.distance_unit));
        mm |= strcmp(t[15], side) || strcmp(strcmp(text, "-") ? text : "", r.distance_text);
        mm |= tok_int(t[17]) != r.score || tok_int(t[18]) != r.margin;
        mm |= tok_int(t[19]) != r.black || tok_int(t[20]) != r.white || tok_int(t[21]) != r.cover_pm;
        mm |= tok_int(t[22]) != r.ink || tok_int(t[23]) != r.sharp_pm || tok_int(t[24]) != r.mid_pm;
        if (mm && sets[s].mismatch < 5)
            printf("MISMATCH %s %s: ref %s %s %s %s cp=%s exit=%s v100=%s %s side=%s '%s' d=%s m=%s "
                   "[%s %s %s %s %s %s] | C %s %s %s %s cp=%d exit=%d v100=%d %s side=%s '%s' d=%d m=%d "
                   "[%d %d %d %d %d %d]\n", t[0], t[1], t[7], t[8], t[9], t[10], t[11], t[12], t[13], t[14], t[15],
                   t[16], t[17], t[18], t[19], t[20], t[21], t[22], t[23], t[24], r.accept ? "ACCEPT" : "REJECT",
                   card_rec_reason_name(r.reason), card_rec_zone_name(r.zone), card_rec_class_name(r.cls),
                   r.cp_maneuver_type, r.exit_number, r.distance_value_x100, card_rec_unit_name(r.distance_unit),
                   side, r.distance_text, r.score, r.margin, r.black, r.white, r.cover_pm, r.ink, r.sharp_pm, r.mid_pm);
        sets[s].n++;
        sets[s].acc += r.accept;
        sets[s].mismatch += mm;
        v = judge(t[6], &r, c);
        if (v == V_BAD_TOKEN) {
            printf("FAIL unknown expectation token '%s' (%s %s)\n", t[6], t[0], t[1]);
            ++bad;
        } else if (v != V_OK) {
            int known = g_known && known_hit(v == V_WRONG, t[0], t[1]);
            sets[s].wrong += v == V_WRONG;
            sets[s].leak += v == V_LEAK;
            sets[s].fresh += !known;
            if (!known && sets[s].fresh <= 20)
                printf("NEW %s %s %s: expect %s got %s %s '%s'\n", v == V_WRONG ? "WRONG ACCEPT" : "GATE LEAK", t[0],
                       t[1], t[6], card_rec_icon_group(c, r.icon_template), card_rec_class_name(r.cls),
                       r.distance_text);
        }
        ++lines;
    }
    fclose(f);
    printf("\n  %-11s %9s %7s %7s %9s %6s %6s\n", "set", "decisions", "accept", "reject", "mismatch", "wrong", "leak");
    for (s = 0; s < nsets; ++s) {
        int want = meta_count(meta, sets[s].name);
        printf("  %-11s %9d %7d %7d %9d %6d %6d%s\n", sets[s].name, sets[s].n, sets[s].acc, sets[s].n - sets[s].acc,
               sets[s].mismatch, sets[s].wrong, sets[s].leak, want == sets[s].n ? "" : "  <- meta.json differs");
        bad += want != sets[s].n;
        total += sets[s].n;
        mm_t += sets[s].mismatch;
        wrong_t += sets[s].wrong;
        leak_t += sets[s].leak;
    }
    printf("  %-11s %9d %24s %9d %6d %6d   (%.2f ms/decision)\n", "total", total, "", mm_t, wrong_t, leak_t,
           total ? 1000.0 * (double)spent / CLOCKS_PER_SEC / total : 0.0);
    if (meta_count(meta, 0) != total) {
        printf("FAIL %d decisions, meta.json decisions_total %d\n", total, meta_count(meta, 0));
        ++bad;
    }
    for (s = 0, i = 0; s < nsets; ++s)
        i += sets[s].fresh;
    n = known_stale(sets, nsets);
    printf("  wrong %d + leak %d: %d on the known list (%s), %d new, %d stale list entries\n", wrong_t, leak_t,
           wrong_t + leak_t - i, g_known ? g_known_path : "none", i, n);
    if (i || n)
        printf("FAIL %d new wrong accept(s) / gate leak(s), %d stale known entries: a wrong arrow is worse "
               "than none\n", i, n);
    bad += i + n;
    free(meta);
    free(im.px);
    free(buf);
    card_rec_close(c);
    return bad || mm_t || !total;
}

int main(void) {
    const char *fx = getenv("CARD_REC_FIXTURES");
    char probe[1024];
    struct stat st;
    api_tests();
    if (failures) {
        printf("%d api check(s) failed\n", failures);
        return 1;
    }
    snprintf(probe, sizeof(probe), "%s/goldens.txt", fx ? fx : "");
    if (!fx || !*fx || stat(probe, &st) != 0) {
        printf("OK (api); SKIP parity: %s\n", fx && *fx ? "no goldens.txt under CARD_REC_FIXTURES"
                                                         : "CARD_REC_FIXTURES not set");
        return 0;
    }
    if (parity_tests(fx))
        return 1;
    printf("OK\n");
    return 0;
}
