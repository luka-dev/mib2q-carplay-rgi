/*
 * card_rec_test: synthetic, licence-safe template roots and frames.
 *
 * Every template here is generated from plain shapes: a 'turn' arrow (a Gamma
 * of two bars), a square ring with a corner notch and a hole for the exit
 * number, 8 x 8 blocks for the other output groups, bars and blocks for the
 * distance glyphs and seven-segment ring digits.  No third-party art, font or
 * capture is involved.  Included by card_rec_test.c (after its includes).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define FW 1440
#define FH 542
#define N32 CARD_REC_NORM

enum {  /* make_root() variations */
    R_NO_GLYPHS = 1 << 0, R_NO_DIGIT7 = 1 << 1, R_NO_K = 1 << 2, R_NO_LEFT = 1 << 3, R_NO_RING_DIGITS = 1 << 4,
    R_RING_NO5 = 1 << 5, R_ICON_SIZE = 1 << 6, R_CARE_SIZE = 1 << 7, R_BAD_HOLE = 1 << 8, R_RING_TWIN = 1 << 9,
    R_STRAIGHT = 1 << 10, R_BAD_ROW = 1 << 11, R_ESCAPE = 1 << 12, R_BAD_GROUP = 1 << 13, R_RING_NO_HOLE = 1 << 14,
    R_KEEP = 1 << 15
};

static void put_pgm(const char *path, int w, int h, const uint8_t *px) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(px, 1, (size_t)(w * h), f);
    fclose(f);
}

static void put_text(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    fputs(text, f);
    fclose(f);
}

static void remove_tree(const char *path) {
    struct stat st;
    DIR *d;
    struct dirent *e;
    char p[1024];
    if (lstat(path, &st) != 0)
        return;
    if (S_ISDIR(st.st_mode) && (d = opendir(path))) {
        while ((e = readdir(d)))
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
                snprintf(p, sizeof(p), "%s/%s", path, e->d_name);
                remove_tree(p);
            }
        closedir(d);
        rmdir(path);
    } else {
        remove(path);
    }
}

/* Gamma: vertical bar cols 6..13, top bar rows 0..7 cols 6..25 (the 40 x 64
 * card icon padded to 64 x 64 and halved) */
static int gamma_cell(int x, int y) { return (x >= 6 && x <= 13) || (y <= 7 && x >= 6 && x <= 25); }
/* ring: 4-cell frame + 6 x 6 corner notch (top-left; rot180 = bottom-right) */
static int ring_cell(int x, int y, int rot) {
    int nx = rot ? 31 - x : x, ny = rot ? 31 - y : y;
    return x < 4 || x > 27 || y < 4 || y > 27 || (nx >= 4 && nx <= 9 && ny >= 4 && ny <= 9);
}
static int hole_cell(int x, int y) { return (2 * x - 31) * (2 * x - 31) + (2 * y - 31) * (2 * y - 31) <= 324; }

/* seven segments a..g on a 7 x 12 digit, 2 px strokes */
static int seg_px(const char *segs, int x, int y) {
    int on = 0;
    for (; *segs; ++segs)
        switch (*segs) {
        case 'a': on |= y <= 1; break;
        case 'b': on |= y <= 6 && x >= 5; break;
        case 'c': on |= y >= 5 && x >= 5; break;
        case 'd': on |= y >= 10; break;
        case 'e': on |= y >= 5 && x <= 1; break;
        case 'f': on |= y <= 6 && x <= 1; break;
        case 'g': on |= y == 5 || y == 6; break;
        }
    return on;
}
static const char *const k_segs[10] = {"abcdef", "", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg",
                                       "abcdfg"};

/* 'Z': a '7' with a 2 x 2 nub in the middle (a confusable reject letter:
 * close to '7' only, which is far from every other digit) */
static int z_px(int x, int y) { return seg_px(k_segs[7], x, y) || (x >= 3 && x <= 4 && y >= 5 && y <= 6); }

/* glyph bitmap of ring character ch ('0'..'9', 'Z'); returns the height */
static int ring_glyph(char ch, uint8_t *px, int *w) {
    int x, y;
    *w = ch == '1' ? 2 : 7;
    for (y = 0; y < 12; ++y)
        for (x = 0; x < *w; ++x)
            px[y * *w + x] = (uint8_t)(ch == '1' || (ch == 'Z' ? z_px(x, y) : seg_px(k_segs[ch - '0'], x, y)) ? 255 : 0);
    return 12;
}

static const char *const k_other_groups[] = {"LEFT", "STRAIGHT", "KEEP_LEFT", "KEEP_RIGHT", "ARRIVE", "UTURN_L",
    "UTURN_R", "RB_LEFT_R", "RB_LEFT_L", "RB_STRAIGHT_R", "RB_STRAIGHT_L", "RB_RIGHT_R", "RB_RIGHT_L", "RB_UTURN_R",
    "RB_UTURN_L"};
static const char *const k_fill_labels[] = {"0", "2", "3", "4", "5", "6", "7", "8", "9", ",", "k", "i", "y", "d",
                                            "f", "t", "\xd0\xbc", "\xd0\xba"};

/* '.': a 6 x 6 disc (radius 3, 4 x 4 supersampled coverage) */
static uint8_t disc_px(int x, int y) {
    int i, j, n = 0;
    for (j = 0; j < 4; ++j)
        for (i = 0; i < 4; ++i) {
            int u = 8 * x + 2 * i + 1 - 24, v = 8 * y + 2 * j + 1 - 24;
            n += u * u + v * v <= 576;
        }
    return (uint8_t)(255 * n / 16);
}

static void make_root(const char *root, int fl) {
    char p[1024], man[4096], line[256];
    uint8_t img[N32 * N32], care[N32 * N32], gl[128 * 32];
    int i, y, w;
    size_t k;
    remove_tree(root);
    mkdir(root, 0700);
    snprintf(p, sizeof(p), "%s/icons", root); mkdir(p, 0700);
    for (i = 0; i < N32 * N32; ++i)
        img[i] = (uint8_t)(gamma_cell(i % N32, i / N32) ? 255 : 0);
    snprintf(p, sizeof(p), "%s/icons/turn.pgm", root); put_pgm(p, N32, N32, img);
    for (i = 0; i < N32 * N32; ++i) {
        img[i] = (uint8_t)(ring_cell(i % N32, i / N32, 0) ? 255 : 0);
        care[i] = (uint8_t)(hole_cell(i % N32, i / N32) ? 0 : 255);
    }
    snprintf(p, sizeof(p), "%s/icons/ring.pgm", root); put_pgm(p, N32, N32, img);
    snprintf(p, sizeof(p), "%s/icons/ring_care.pgm", root); put_pgm(p, fl & R_CARE_SIZE ? 16 : N32, N32, care);
    snprintf(man, sizeof(man), "# synthetic\n%s - turn %s 2 - 40 64 -\nring.pgm ring_care.pgm ring %s 0 R 64 64 %s\n",
             fl & R_ESCAPE ? "../turn.pgm" : "turn.pgm", fl & R_BAD_GROUP ? "NOT_A_GROUP" : fl & R_STRAIGHT ? "STRAIGHT" :
             fl & R_KEEP ? "KEEP_RIGHT" : "RIGHT",
             fl & R_RING_NO_HOLE ? "RIGHT" : "RB_RING", fl & R_BAD_HOLE ? "500,500,250x" : "500,500,250");
    if (fl & R_RING_TWIN)
        strcat(man, "ring.pgm ring_care.pgm ring_l RB_RING 0 L 64 64 500,500,250\n");
    for (k = 0; k < sizeof(k_other_groups) / sizeof(k_other_groups[0]); ++k) {
        const char *g = (fl & R_STRAIGHT && !strcmp(k_other_groups[k], "STRAIGHT")) ||
                        (fl & R_KEEP && !strcmp(k_other_groups[k], "KEEP_RIGHT")) ? "RIGHT" : k_other_groups[k];
        if (fl & R_NO_LEFT && !strcmp(g, "LEFT"))
            continue;
        for (i = 0; i < N32 * N32; ++i)
            img[i] = (uint8_t)(i % N32 / 8 == (int)k % 4 && i / N32 / 8 == (int)k / 4 ? 255 : 0);
        snprintf(p, sizeof(p), "%s/icons/blk%02d.pgm", root, (int)k);
        put_pgm(p, fl & R_ICON_SIZE && k == 3 ? 16 : N32, N32, img);
        snprintf(line, sizeof(line), "blk%02d.pgm - blk%02d %s 0 %c 32 32 -\n", (int)k, (int)k, g, g[strlen(g) - 1] == 'L' &&
                 g[0] == 'R' ? 'L' : g[0] == 'R' && g[1] == 'B' ? 'R' : '-');
        strcat(man, line);
    }
    if (fl & R_BAD_ROW)
        strcat(man, "blk00.pgm - extra RIGHT 2 -\n");
    snprintf(p, sizeof(p), "%s/icons/manifest.txt", root); put_text(p, man);
    if (!(fl & R_NO_GLYPHS)) {          /* '1' 4 x 25 bar, 'm' 20 x 19 block, '.' disc, fillers 30+ x 8 */
        snprintf(p, sizeof(p), "%s/glyphs_sf", root); mkdir(p, 0700);
        memset(gl, 255, sizeof(gl));
        snprintf(p, sizeof(p), "%s/glyphs_sf/one.pgm", root); put_pgm(p, 4, 25, gl);
        snprintf(p, sizeof(p), "%s/glyphs_sf/em.pgm", root); put_pgm(p, 20, 19, gl);
        snprintf(p, sizeof(p), "%s/glyphs_sf/look.pgm", root); put_pgm(p, 5, 25, gl);   /* '~I': a wider '1' */
        for (i = 0; i < 36; ++i)
            gl[i] = disc_px(i % 6, i / 6);
        snprintf(p, sizeof(p), "%s/glyphs_sf/dot.pgm", root); put_pgm(p, 6, 6, gl);
        memset(gl, 255, sizeof(gl));
        strcpy(man, "# file label top bottom\none.pgm 1 -24 0\nem.pgm m -18 0\nlook.pgm ~I -24 0\ndot.pgm . -5 0\n");
        for (k = 0; k < sizeof(k_fill_labels) / sizeof(k_fill_labels[0]); ++k) {
            if ((fl & R_NO_DIGIT7 && !strcmp(k_fill_labels[k], "7")) || (fl & R_NO_K && !strcmp(k_fill_labels[k], "k")))
                continue;
            snprintf(p, sizeof(p), "%s/glyphs_sf/f%02d.pgm", root, (int)k);
            put_pgm(p, 30 + (int)k, 8, gl);
            snprintf(line, sizeof(line), "f%02d.pgm %s -7 0\n", (int)k, k_fill_labels[k]);
            strcat(man, line);
        }
        snprintf(p, sizeof(p), "%s/glyphs_sf/manifest.txt", root); put_text(p, man);
    }
    if (!(fl & R_NO_RING_DIGITS)) {
        snprintf(p, sizeof(p), "%s/ring_digits", root); mkdir(p, 0700);
        man[0] = 0;
        for (i = 0; i < 11; ++i) {
            char ch = i < 10 ? (char)('0' + i) : 'Z';
            if (fl & R_RING_NO5 && ch == '5')
                continue;
            y = ring_glyph(ch, gl, &w);
            snprintf(p, sizeof(p), "%s/ring_digits/r%c.pgm", root, ch); put_pgm(p, w, y, gl);
            snprintf(line, sizeof(line), "r%c.pgm %c -11 0\n", ch, ch);
            strcat(man, line);
        }
        snprintf(p, sizeof(p), "%s/ring_digits/manifest.txt", root); put_text(p, man);
    }
}

/* ---------------------------------------------------------------- frames */
static void fill_rect(uint8_t *rgba, int x, int y, int w, int h, uint8_t v) {
    int i, j;
    for (j = y; j < y + h; ++j)
        for (i = x; i < x + w; ++i) {
            uint8_t *q = rgba + ((size_t)j * FW + (size_t)i) * 4;
            q[0] = q[1] = q[2] = v;
            q[3] = 255;
        }
}

/* "11 m" at box x x0 (first bar w1 wide, gap px to the second), right of an
 * icon whose top-left is (bx + 36, by + 101); baseline row by + 144 */
static void put_dist(uint8_t *rgba, int bx, int by, int x0, int w1, int gap, uint8_t white) {
    fill_rect(rgba, bx + x0, by + 120, w1, 25, white);
    fill_rect(rgba, bx + x0 + w1 + gap, by + 120, 4, 25, white);
    fill_rect(rgba, bx + x0 + w1 + gap + 17, by + 126, 20, 19, white);
}

static void put_distance(uint8_t *rgba, int bx, int by, int x0, uint8_t white) {
    put_dist(rgba, bx, by, x0, 4, 5, white);
}

/* the Gamma (40 x 64) with its top-left at (x, y), grown by g px */
static void gamma_at(uint8_t *rgba, int x, int y, int g, uint8_t v) {
    fill_rect(rgba, x - g, y - g, 16 + 2 * g, 64 + 2 * g, v);
    fill_rect(rgba, x - g, y - g, 40 + 2 * g, 16 + 2 * g, v);
}

/* grey cluster, black card at (bx, 88): the Gamma turn icon, "11 m" */
static void synth_card(uint8_t *rgba, int bx, uint8_t white) {
    const int by = 88;
    fill_rect(rgba, 0, 0, FW, FH, 26);
    fill_rect(rgba, bx, by, 780, 294, 0);
    gamma_at(rgba, bx + 36, by + 101, 0, white);
    put_distance(rgba, bx, by, 106, white);
}

static int ring_text_w(const char *digits) {
    int i, w = 0;
    for (i = 0; digits[i]; ++i)
        w += (digits[i] == '1' ? 2 : 7) + (i ? 2 : 0);
    return w;
}

/* 64 x 64 ring crop: frame, notch, digits from column dx (-1: centred), row dy */
static void ring_crop(uint8_t *crop, int rot, const char *digits, int dx, int dy) {
    uint8_t gl[16 * 12];
    int x, y, w, i;
    for (y = 0; y < 64; ++y)
        for (x = 0; x < 64; ++x)
            crop[y * 64 + x] = (uint8_t)(ring_cell(x / 2, y / 2, rot) ? 255 : 0);
    if (dx < 0)
        dx = (65 - ring_text_w(digits)) / 2;
    for (i = 0; digits[i]; ++i) {
        ring_glyph(digits[i], gl, &w);
        for (y = 0; y < 12; ++y)
            for (x = 0; x < w; ++x)
                crop[(dy + y) * 64 + dx + x] = gl[y * w + x];
        dx += w + 2;
    }
}

static void synth_ring_card(uint8_t *rgba, int rot, const char *digits) {
    uint8_t crop[64 * 64];
    int x, y;
    fill_rect(rgba, 0, 0, FW, FH, 26);
    fill_rect(rgba, 330, 88, 780, 294, 0);
    ring_crop(crop, rot, digits, -1, 26);
    for (y = 0; y < 64; ++y)
        for (x = 0; x < 64; ++x)
            fill_rect(rgba, 330 + 20 + x, 88 + 101 + y, 1, 1, crop[y * 64 + x]);
    put_distance(rgba, 330, 88, 106, 255);
}
