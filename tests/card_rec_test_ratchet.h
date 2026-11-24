/*
 * card_rec_test: known-attack ratchet for the fixture parity run.
 *
 * Some attack fixtures are accepted although the attack expectation says
 * REJECT: frames that are pixel-equivalent to a real card (a split card that
 * pairs one card's icon with the other's text, a 'mi' whose 'i' is fully
 * erased) or gates that read an Apple-style / mid-transition card exactly
 * (GATE_LEAK).  Those single-frame cases are reviewed and listed; the
 * stabiliser is what keeps them off the HUD.  The parity run then passes when
 * every WRONG accept / GATE_LEAK is listed, and FAILS on
 *   - any wrong accept or leak that is not listed (a regression), and
 *   - any listed entry that matched nothing (fixed: delete it - the list may
 *     only shrink).
 * File: $CARD_REC_KNOWN, else tests/card_rec_known_attacks.txt relative to
 * the working directory (scripts/run_tests.sh runs from the repo root).  Rows
 *   wrong <set> <id>      leak <set> <id>      ('#' comments)
 * where <id> may end in '*' to match every id with that prefix.  The list
 * holds fixture ids only - no frame data.  Without the file every wrong /
 * leak fails, as before.
 *
 * Included by card_rec_test.c after its set_t.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#define KNOWN_MAX 4096

typedef struct {
    int wrong, prefix, hits;
    char set[16], id[168];
} known_t;

static known_t *g_known;
static int g_nknown;
static char g_known_path[1024];

/* load the list once; returns 1 when a list is in force */
static int known_load(void) {
    const char *env = getenv("CARD_REC_KNOWN");
    char line[512], kind[16], set[16], id[168];
    FILE *f;
    if (g_known)
        return 1;
    snprintf(g_known_path, sizeof(g_known_path), "%s",
             env && *env ? env : "tests/card_rec_known_attacks.txt");
    if (!(f = fopen(g_known_path, "r")))
        return 0;
    g_known = (known_t *)calloc(KNOWN_MAX, sizeof(*g_known));
    while (g_known && fgets(line, sizeof(line), f)) {
        size_t n;
        known_t *k;
        if (line[0] == '#' || sscanf(line, "%15s %15s %167s", kind, set, id) != 3)
            continue;
        if (g_nknown == KNOWN_MAX || (strcmp(kind, "wrong") && strcmp(kind, "leak"))) {
            printf("FAIL known list %s: bad or too many rows at '%s'\n", g_known_path, kind);
            ++failures;
            break;
        }
        k = &g_known[g_nknown++];
        k->wrong = !strcmp(kind, "wrong");
        snprintf(k->set, sizeof(k->set), "%s", set);
        n = strlen(id);
        k->prefix = n && id[n - 1] == '*';
        if (k->prefix)
            id[n - 1] = 0;
        snprintf(k->id, sizeof(k->id), "%s", id);
    }
    fclose(f);
    return g_known != 0;
}

/* 1 when (wrong / leak, set, id) is listed; counts the hit */
static int known_hit(int wrong, const char *set, const char *id) {
    int i;
    for (i = 0; i < g_nknown; ++i) {
        known_t *k = &g_known[i];
        if (k->wrong == wrong && !strcmp(k->set, set) &&
            (k->prefix ? !strncmp(id, k->id, strlen(k->id)) : !strcmp(id, k->id))) {
            ++k->hits;
            return 1;
        }
    }
    return 0;
}

/* entries of the parity sets that matched nothing: stale, the list must shrink */
static int known_stale(const set_t *sets, int nsets) {
    int i, s, stale = 0;
    for (i = 0; i < g_nknown; ++i) {
        for (s = 0; s < nsets && strcmp(sets[s].name, g_known[i].set); ++s)
            ;
        if (s < nsets && !g_known[i].hits) {
            if (++stale <= 20)
                printf("STALE known %s %s %s%s: no longer happens - remove it from %s\n",
                       g_known[i].wrong ? "wrong" : "leak", g_known[i].set, g_known[i].id,
                       g_known[i].prefix ? "*" : "", g_known_path);
        }
    }
    return stale;
}

/* A CLASS_NOT_PUBLISHED golden row carries the withheld reading (fixture export):
 * compare r->internal there (the stabiliser keeps it in its screen history). */
static void view_internal(card_rec_result_t *r) {
    const card_rec_reading_t *w = &r->internal;
    if (r->reason != CARD_REC_R_CLASS_NOT_PUBLISHED)
        return;
    r->cls = w->cls;
    r->cp_maneuver_type = w->cp_maneuver_type;
    r->exit_number = w->exit_number;
    r->distance_value_x100 = w->distance_value_x100;
    r->distance_unit = w->distance_unit;
    r->traffic_side = w->traffic_side;
    memcpy(r->distance_text, w->distance_text, sizeof(r->distance_text));
}
