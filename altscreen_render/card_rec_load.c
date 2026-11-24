/*
 * CarPlay instruction-card recogniser: template loading (see card_rec.h).
 *
 * Runs once at start-up, so it may allocate; nothing here is on the frame
 * path.  Every malformed input (bad manifest row, wrong PGM size, unknown
 * output group, file name escaping the template root) and every incomplete
 * root (an output group without a template, a distance digit or unit the
 * glyphs cannot spell, ring templates without ring digits 0-9) fails the
 * whole load: a missing template can only turn its card into a confident
 * wrong match of a neighbour, never into a REJECT.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define _POSIX_C_SOURCE 200809L
#define CARD_REC_INTERNAL
#include "card_rec.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PATH_MAX_ 1024
#define LINE_MAX_ 512
#define PGM_MAX_SIDE 128
#define MAX_TEMPLATES 4096

typedef struct {
    char label[CARD_REC_LABEL_MAX];
    int dir, w, h, top, bottom;
    uint8_t *px;
} raw_glyph_t;

typedef struct {
    raw_glyph_t *v;
    int n, cap, dir;
} raw_list_t;

static void set_err(char *err, size_t len, const char *fmt, ...) {
    va_list ap;
    if (!err || !len)
        return;
    va_start(ap, fmt);
    vsnprintf(err, len, fmt, ap);
    va_end(ap);
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int join(char *out, const char *a, const char *b, const char *c) {
    int n = snprintf(out, PATH_MAX_, "%s/%s%s%s", a, b, c ? "/" : "", c ? c : "");
    return n > 0 && n < PATH_MAX_;
}

/* manifest file names stay inside their directory */
static int safe_name(const char *s) {
    return *s && !strchr(s, '/') && !strstr(s, "..");
}

static int parse_int(const char *s, int lo, int hi, int *out) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end || v < lo || v > hi)
        return 0;
    *out = (int)v;
    return 1;
}

/* Binary P5, maxval 255: "P5" WS width WS height WS 255 <one WS> pixels. */
static uint8_t *read_pgm(const char *path, int *w, int *h) {
    char tok[4][16];
    int i, c, len;
    uint8_t *px;
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    for (i = 0; i < 4; ++i) {
        while ((c = fgetc(f)) != EOF && isspace(c))
            ;
        for (len = 0; c != EOF && !isspace(c) && len < 15; c = fgetc(f))
            tok[i][len++] = (char)c;
        tok[i][len] = 0;
        if (c == EOF || !isspace(c))    /* the 4th token is followed by one WS */
            break;
    }
    if (i < 4 || strcmp(tok[0], "P5") || strcmp(tok[3], "255") || !parse_int(tok[1], 1, PGM_MAX_SIDE, w) ||
        !parse_int(tok[2], 1, PGM_MAX_SIDE, h)) {
        fclose(f);
        return 0;
    }
    px = (uint8_t *)malloc((size_t)(*w * *h));
    if (px && fread(px, 1, (size_t)(*w * *h), f) != (size_t)(*w * *h)) {
        free(px);
        px = 0;
    }
    fclose(f);
    return px;
}

/* Calls row() for every non-comment manifest row (whitespace-split tokens). */
static int each_row(const char *dir, int (*row)(void *, const char *, char **, int, char *, size_t), void *ctx,
                    char *err, size_t err_len) {
    char path[PATH_MAX_], line[LINE_MAX_], *tok[12], *save;
    int n, lineno = 0;
    FILE *f;
    if (!join(path, dir, "manifest.txt", 0) || !(f = fopen(path, "r"))) {
        set_err(err, err_len, "cannot read %s/manifest.txt", dir);
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        ++lineno;
        if (!strchr(line, '\n') && !feof(f)) {
            set_err(err, err_len, "%s/manifest.txt:%d: line too long", dir, lineno);
            fclose(f);
            return 0;
        }
        for (n = 0, tok[0] = strtok_r(line, " \t\r\n\v\f", &save); tok[n] && n < 11;)
            tok[++n] = strtok_r(0, " \t\r\n\v\f", &save);
        if (!n || tok[0][0] == '#')
            continue;
        if (!row(ctx, dir, tok, n, err, err_len)) {
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    return 1;
}

static int icon_row(void *ctx, const char *dir, char **t, int n, char *err, size_t err_len) {
    card_rec_t *c = (card_rec_t *)ctx;
    card_rec_icon_t *ic;
    char path[PATH_MAX_];
    uint8_t *px = 0;
    int w, h, i, cp, end = 0;
    if (n != 9 || c->nicons >= CARD_REC_MAX_ICONS || !safe_name(t[0]) || strlen(t[2]) >= sizeof(ic->name)) {
        set_err(err, err_len, "%s: bad icon row '%s'", dir, n ? t[0] : "");
        return 0;
    }
    ic = &c->icons[c->nicons];
    memset(ic, 0, sizeof(*ic));
    strcpy(ic->name, t[2]);
    ic->group = card_rec_group_index(t[3]);
    ic->side = t[5][0];
    if (ic->group < 0 || t[5][1] || (ic->side != 'R' && ic->side != 'L' && ic->side != '-') ||
        !parse_int(t[4], 0, 99, &cp) || !parse_int(t[6], 1, CARD_REC_MAX_ICON, &ic->exp_w) ||
        !parse_int(t[7], 1, CARD_REC_MAX_ICON, &ic->exp_h)) {
        set_err(err, err_len, "%s: bad icon fields for %s", dir, t[2]);
        return 0;
    }
    if (strcmp(t[8], "-")) {
        ic->has_hole = sscanf(t[8], "%d,%d,%d%n", &ic->hole_cx_pm, &ic->hole_cy_pm, &ic->hole_r_pm, &end) == 3 &&
                       !t[8][end];
        if (!ic->has_hole || ic->hole_cx_pm < 0 || ic->hole_cx_pm > 2000 || ic->hole_cy_pm < 0 ||
            ic->hole_cy_pm > 2000 || ic->hole_r_pm < 1 || ic->hole_r_pm > 2000) {
            set_err(err, err_len, "%s: bad hole for %s", dir, t[2]);
            return 0;
        }
    }
    if (ic->has_hole != card_rec_group_is_ring(ic->group)) {   /* exit number <=> ring */
        set_err(err, err_len, "%s: %s: hole and group %s disagree", dir, t[2], t[3]);
        return 0;
    }
    if (!join(path, dir, t[0], 0) || !(px = read_pgm(path, &w, &h)) || w != CARD_REC_NORM || h != CARD_REC_NORM) {
        free(px);                       /* NULL, or a PGM of the wrong size */
        set_err(err, err_len, "%s: icon %s is not a %dx%d P5 PGM", dir, t[0], CARD_REC_NORM, CARD_REC_NORM);
        return 0;
    }
    memcpy(ic->img, px, sizeof(ic->img));
    free(px);
    px = 0;
    memset(ic->care, 2, sizeof(ic->care));
    ic->has_care = strcmp(t[1], "-") != 0;
    if (ic->has_care) {
        if (!safe_name(t[1]) || !join(path, dir, t[1], 0) || !(px = read_pgm(path, &w, &h)) ||
            w != CARD_REC_NORM || h != CARD_REC_NORM) {
            free(px);
            set_err(err, err_len, "%s: care mask %s is not a %dx%d P5 PGM", dir, t[1], CARD_REC_NORM, CARD_REC_NORM);
            return 0;
        }
        for (i = 0; i < CARD_REC_NORM * CARD_REC_NORM; ++i)
            ic->care[i] = (uint8_t)(px[i] >= 192 ? 2 : px[i] >= 64 ? 1 : 0);
        free(px);
    }
    ++c->nicons;
    return 1;
}

static int glyph_row(void *ctx, const char *dir, char **t, int n, char *err, size_t err_len) {
    raw_list_t *l = (raw_list_t *)ctx;
    raw_glyph_t *g;
    char path[PATH_MAX_];
    if (n != 4 || !safe_name(t[0]) || strlen(t[1]) >= CARD_REC_LABEL_MAX || l->n >= MAX_TEMPLATES) {
        set_err(err, err_len, "%s: bad glyph row '%s'", dir, n ? t[0] : "");
        return 0;
    }
    if (l->n == l->cap) {
        int cap = l->cap ? 2 * l->cap : 64;
        raw_glyph_t *v = (raw_glyph_t *)realloc(l->v, (size_t)cap * sizeof(*v));
        if (!v) {
            set_err(err, err_len, "out of memory");
            return 0;
        }
        l->v = v;
        l->cap = cap;
    }
    g = &l->v[l->n];
    memset(g, 0, sizeof(*g));
    strcpy(g->label, t[1]);
    g->dir = l->dir;
    if (!parse_int(t[2], -PGM_MAX_SIDE, PGM_MAX_SIDE, &g->top) ||
        !parse_int(t[3], -PGM_MAX_SIDE, PGM_MAX_SIDE, &g->bottom) || !join(path, dir, t[0], 0) ||
        !(g->px = read_pgm(path, &g->w, &g->h))) {
        set_err(err, err_len, "%s: bad glyph %s", dir, t[0]);
        return 0;
    }
    ++l->n;
    return 1;
}

static int all_digits(const char *s) {
    if (!*s)
        return 0;
    for (; *s; ++s)
        if (*s < '0' || *s > '9')
            return 0;
    return 1;
}

static void free_set(card_rec_glyph_set_t *s) {
    int i;
    for (i = 0; i < s->count; ++i)
        free(s->t[i].px);
    free(s->t);
    memset(s, 0, sizeof(*s));
}

/* Stack glyph directories: labels keep their first-seen order; a label takes
 * the templates of the LAST directory that lists it (car-harvested glyphs
 * override the font bootstrap label by label). */
static int load_glyphs(card_rec_glyph_set_t *s, const char *root, const char *const *subs, int nsub,
                       char *err, size_t err_len) {
    raw_list_t l;
    char dir[PATH_MAX_];
    int i, j, k, ok = 1;
    memset(&l, 0, sizeof(l));
    memset(s, 0, sizeof(*s));
    s->dot = s->comma = -1;
    for (i = 0; i < nsub && ok; ++i) {
        l.dir = i;
        ok = join(dir, root, subs[i], 0) && each_row(dir, glyph_row, &l, err, err_len);
    }
    if (ok && l.n && !(s->t = (card_rec_glyph_t *)calloc((size_t)l.n, sizeof(*s->t)))) {
        set_err(err, err_len, "out of memory");
        ok = 0;
    }
    for (i = 0; i < l.n && ok; ++i) {
        int last = l.v[i].dir, seen = 0;
        for (j = 0; j < i && !seen; ++j)
            seen = !strcmp(l.v[j].label, l.v[i].label);
        if (seen)
            continue;
        if (s->nlabels == CARD_REC_MAX_LABELS) {
            set_err(err, err_len, "more than %d glyph labels", CARD_REC_MAX_LABELS);
            ok = 0;
            break;
        }
        for (j = i; j < l.n; ++j)
            if (!strcmp(l.v[j].label, l.v[i].label) && l.v[j].dir > last)
                last = l.v[j].dir;
        strcpy(s->labels[s->nlabels], l.v[i].label);
        s->is_digit[s->nlabels] = (uint8_t)all_digits(l.v[i].label);
        s->nletters += !s->is_digit[s->nlabels];
        if (!strcmp(l.v[i].label, "."))
            s->dot = s->nlabels;
        if (!strcmp(l.v[i].label, ","))
            s->comma = s->nlabels;
        for (j = i; j < l.n; ++j) {
            card_rec_glyph_t *g;
            if (strcmp(l.v[j].label, l.v[i].label) || l.v[j].dir != last)
                continue;
            g = &s->t[s->count++];
            g->label = s->nlabels;
            g->w = l.v[j].w;
            g->h = l.v[j].h;
            g->top = l.v[j].top;
            g->bottom = l.v[j].bottom;
            g->px = l.v[j].px;
            l.v[j].px = 0;
            for (k = 0; k < g->w * g->h; ++k)
                g->sum += g->px[k];
        }
        ++s->nlabels;
    }
    for (i = 0; i < l.n; ++i)
        free(l.v[i].px);
    free(l.v);
    if (!ok)
        free_set(s);
    return ok;
}

static int has_label(const card_rec_glyph_set_t *s, const char *label) {
    int i;
    for (i = 0; i < s->nlabels; ++i)
        if (!strcmp(s->labels[i], label))
            return 1;
    return 0;
}

/* `word` is a sequence of the set's labels ('ft' may be one ligature label) */
static int spellable(const card_rec_glyph_set_t *s, const char *word) {
    unsigned char reach[CARD_REC_LABEL_MAX * 2] = {1};
    size_t n = strlen(word), i, len;
    int j;
    if (n >= sizeof(reach))
        return 0;
    for (i = 0; i < n; ++i)
        for (j = 0; reach[i] && j < s->nlabels; ++j)
            if ((len = strlen(s->labels[j])) && i + len <= n && !strncmp(word + i, s->labels[j], len))
                reach[i + len] = 1;
    return reach[n];
}

static int digits_missing(const card_rec_glyph_set_t *s, char *missing) {
    for (missing[0] = '0', missing[1] = 0; missing[0] <= '9'; ++missing[0])
        if (!has_label(s, missing))
            return 1;
    return 0;
}

/* every output group, distance digit and unit; ring digits when a ring exists */
static int complete(card_rec_t *c, char *err, size_t err_len) {
    char d[2];
    const char *u;
    int g, i, ring = 0, found;
    for (g = 0; g < card_rec_group_count(); ++g) {
        for (i = 0, found = 0; i < c->nicons; ++i)
            found |= c->icons[i].group == g;
        if (!found) {
            set_err(err, err_len, "no icon template for output group %s", card_rec_group_name(g));
            return 0;
        }
    }
    if (digits_missing(&c->glyphs, d)) {
        set_err(err, err_len, "distance glyphs lack the digit %s", d);
        return 0;
    }
    for (i = 0; (u = card_rec_unit_label(i)) != 0; ++i)
        if (!spellable(&c->glyphs, u)) {
            set_err(err, err_len, "distance glyphs cannot spell the unit '%s'", u);
            return 0;
        }
    for (i = 0; i < c->nicons; ++i)
        ring |= c->icons[i].has_hole;
    if (ring && digits_missing(&c->ring, d)) {
        set_err(err, err_len, "ring templates need ring_digits/ with 0-9 (missing %s)", d);
        return 0;
    }
    for (i = 0, c->ring_cap = 0; i < c->ring.count; ++i)
        if (c->ring.is_digit[c->ring.t[i].label])
            c->ring_cap = cr_max(c->ring_cap, c->ring.t[i].bottom - c->ring.t[i].top + 1);
    return 1;
}

card_rec_status_t card_rec_open(card_rec_t **out, const char *dir, char *err, size_t err_len) {
    static const char *const glyph_dirs[] = {"glyphs_sf", "glyphs_car"};
    const char *subs[2];
    char path[PATH_MAX_];
    card_rec_t *c;
    int i, n = 0;
    set_err(err, err_len, "ok");
    if (!out)
        return CARD_REC_DISABLED;
    *out = 0;
    if (!dir || !*dir)
        dir = getenv(CARD_REC_TEMPLATES_ENV);
    if (!dir || !*dir) {
        set_err(err, err_len, "%s not set", CARD_REC_TEMPLATES_ENV);
        return CARD_REC_DISABLED;
    }
    if (strlen(dir) > PATH_MAX_ - 64 || !is_dir(dir)) {
        set_err(err, err_len, "no template root at %s", dir);
        return CARD_REC_DISABLED;
    }
    c = (card_rec_t *)calloc(1, sizeof(*c));
    if (!c) {
        set_err(err, err_len, "out of memory");
        return CARD_REC_ERR_NOMEM;
    }
    card_rec_default_params(&c->p);
    c->path = CARD_REC_PATH_GPU;
    for (i = 0; i < 2; ++i)
        if (join(path, dir, glyph_dirs[i], 0) && is_dir(path))
            subs[n++] = glyph_dirs[i];
    if (!n) {
        set_err(err, err_len, "no glyphs_sf/ or glyphs_car/ in %s", dir);
        card_rec_close(c);
        return CARD_REC_ERR_TEMPLATES;
    }
    if (!join(path, dir, "icons", 0) || !each_row(path, icon_row, c, err, err_len) ||
        !load_glyphs(&c->glyphs, dir, subs, n, err, err_len)) {
        card_rec_close(c);
        return CARD_REC_ERR_TEMPLATES;
    }
    subs[0] = "ring_digits";
    if ((join(path, dir, subs[0], 0) && is_dir(path) && !load_glyphs(&c->ring, dir, subs, 1, err, err_len)) ||
        !complete(c, err, err_len)) {
        card_rec_close(c);
        return CARD_REC_ERR_TEMPLATES;
    }
    card_rec_build_decoys(c);
    *out = c;
    return CARD_REC_OK;
}

void card_rec_close(card_rec_t *c) {
    if (!c)
        return;
    free_set(&c->glyphs);
    free_set(&c->ring);
    free(c);
}

card_rec_params_t *card_rec_params(card_rec_t *c) {
    return c ? &c->p : 0;
}

void card_rec_set_input_path(card_rec_t *c, card_rec_path_t path) {
    if (c && path >= CARD_REC_PATH_Y && path < CARD_REC_PATH_COUNT)
        c->path = path;
}

void card_rec_template_counts(const card_rec_t *c, int *icons, int *glyphs, int *ring_digits) {
    if (icons)
        *icons = c ? c->nicons : 0;
    if (glyphs)
        *glyphs = c ? c->glyphs.count : 0;
    if (ring_digits)
        *ring_digits = c ? c->ring.count : 0;
}

const char *card_rec_icon_name(const card_rec_t *c, int index) {
    return c && index >= 0 && index < c->nicons ? c->icons[index].name : "-";
}

const char *card_rec_icon_group(const card_rec_t *c, int index) {
    return c && index >= 0 && index < c->nicons ? card_rec_group_name(c->icons[index].group) : "-";
}

#define CARD_REC_NAME_REASON_(n, s) s,
const char *card_rec_reason_name(card_rec_reason_t r) {
    static const char *const names[] = {CARD_REC_REASONS(CARD_REC_NAME_REASON_)};
    return (int)r >= 0 && r < CARD_REC_R_COUNT ? names[r] : "?";
}

const char *card_rec_class_name(card_rec_class_t c) {
    static const char *const names[] = {
        "-", "LEFT_TURN", "RIGHT_TURN", "STRAIGHT", "KEEP_LEFT", "KEEP_RIGHT", "ARRIVE", "UTURN_LEFT",
        "UTURN_RIGHT", "ROUNDABOUT_NUMBERED", "ROUNDABOUT_LEFT", "ROUNDABOUT_STRAIGHT", "ROUNDABOUT_RIGHT",
        "ROUNDABOUT_UTURN"};
    return (int)c >= 0 && c < CARD_REC_CLASS_COUNT ? names[c] : "?";
}

const char *card_rec_unit_name(card_rec_unit_t u) {
    static const char *const names[] = {"-", "m", "km", "ft", "yd", "mi"};
    return (int)u >= 0 && u < CARD_REC_UNIT_COUNT ? names[u] : "?";
}

const char *card_rec_zone_name(card_rec_zone_kind_t z) {
    static const char *const names[] = {"-", "fullscreen", "sportscreen", "classicscreen", "custom", "detect"};
    return (int)z >= 0 && z <= CARD_REC_ZONE_DETECT ? names[z] : "?";
}
