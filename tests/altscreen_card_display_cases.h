/* Card display scenarios (b)-(d) for tests/altscreen_card_display_test.c.
 * Included after the hook, the fixture and scenario (a); full build only. */
#ifndef ALTSCREEN_CARD_DISPLAY_CASES_H
#define ALTSCREEN_CARD_DISPLAY_CASES_H
#include <ctype.h>

static uint8_t t_sess_b[1024] __attribute__((aligned(16)));
static uint32_t t_seq = 10;     /* control/media SETUP sequence; (a) used 1..3 */

/* the card logger's optional CF symbols, over the fake CF */
static long t_dict_count(const void *d)
{
    tcf_t *t = tcf_of(d);
    return (t && t->kind == TCF_DICT) ? t->n : -1;
}
static void t_dict_kv(const void *d, const void **k, const void **v)
{
    tcf_t *t = tcf_of(d);
    int i;
    pthread_mutex_lock(&t_cf_lock);
    for (i = 0; t && i < t->n; ++i) {
        k[i] = t->keys[i];
        v[i] = t->vals[i];
    }
    pthread_mutex_unlock(&t_cf_lock);
}
static unsigned long t_type_num(void)  { return TCF_NUM; }
static unsigned long t_type_bool(void) { return TCF_BOOL; }
static unsigned long t_type_data(void) { return TCF_DATA; }
static long t_data_len(const void *v)
{
    tcf_t *t = tcf_of(v);
    return (t && t->kind == TCF_DATA) ? (long)t->dlen : -1;
}

/* every card_log line, for assertions on what the car log will show */
static char t_log[262144];
static size_t t_log_len;
static void t_tap(const char *line)
{
    size_t n = strlen(line);
    pthread_mutex_lock(&t_ev_lock);
    if (t_log_len + n + 2 < sizeof(t_log)) {
        memcpy(t_log + t_log_len, line, n);
        t_log_len += n;
        t_log[t_log_len++] = '\n';
        t_log[t_log_len] = 0;
    }
    pthread_cond_broadcast(&t_ev_cond);
    pthread_mutex_unlock(&t_ev_lock);
}
static int t_log_has(const char *needle)
{
    int has;
    pthread_mutex_lock(&t_ev_lock);
    has = strstr(t_log, needle) != NULL;
    pthread_mutex_unlock(&t_ev_lock);
    return has;
}
static void t_wait_log(const char *needle)
{
    struct timespec until;
    alt_deadline_after_ms(&until, 3000);
    pthread_mutex_lock(&t_ev_lock);
    while (!strstr(t_log, needle))
        assert(pthread_cond_timedwait(&t_ev_cond, &t_ev_lock, &until) != ETIMEDOUT);
    pthread_mutex_unlock(&t_ev_lock);
}
static void t_log_reset(void)
{
    pthread_mutex_lock(&t_ev_lock);
    t_log_len = 0;
    t_log[0] = 0;
    pthread_mutex_unlock(&t_ev_lock);
}
static void t_wire_card(void)
{
    card_cf_dict_count = t_dict_count;
    card_cf_dict_kv = t_dict_kv;
    card_cf_numtype = t_type_num;
    card_cf_booltype = t_type_bool;
    card_cf_datatype = t_type_data;
    card_cf_datalen = t_data_len;
    card_cf_resolved = 1;            /* never dlsym a host CoreFoundation */
    g_card_log_tap = t_tap;
    t_session_init(t_sess_b);
}

/* ── small helpers ────────────────────────────────────────────────────────── */
static void t_touch(const char *path, int on)
{
    if (on) {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        assert(fd >= 0);
        close(fd);
    } else {
        unlink(path);
    }
}
static int t_armed(void) { return access(t_card_marker, F_OK) == 0; }
/* <marker>.active / <marker>.disarmed */
static const char *t_mfile(const char *suffix)
{
    static char p[2][600];
    char *b = p[suffix[1] == 'a'];
    snprintf(b, sizeof(p[0]), "%s%s", t_card_marker, suffix);
    return b;
}
static int t_exists(const char *path) { return access(path, F_OK) == 0; }
static int t_file_has(const char *path, const char *needle)
{
    char buf[256];
    size_t n;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = 0;
    return strstr(buf, needle) != NULL;
}
static int t_cmds_locked(const char *type, const char *uuid)
{
    int n;
    pthread_mutex_lock(&t_ev_lock);
    n = t_count_cmds(type, uuid);
    pthread_mutex_unlock(&t_ev_lock);
    return n;
}
static tcf_t *t_disp(int i) { return (tcf_t *)t_arridx(g_advert_displays, i); }
static int64_t t_i(tcf_t *d, const char *k)
{
    tcf_t *v = tcf_get(d, k);
    assert(v && v->kind == TCF_NUM);
    return v->num;
}
static const char *t_s(tcf_t *d, const char *k)
{
    tcf_t *v = tcf_get(d, k);
    assert(v && v->kind == TCF_STR);
    return v->text;
}
static int t_n(tcf_t *d, const char *k)
{
    tcf_t *v = tcf_get(d, k);
    assert(v && v->kind == TCF_ARR);
    return v->n;
}
static void t_rect(tcf_t *r, int x, int y, int w, int h)
{
    assert(r && t_i(r, "originXPixels") == x && t_i(r, "originYPixels") == y &&
           t_i(r, "widthPixels") == w && t_i(r, "heightPixels") == h);
}
static long t_fsize(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 ? (long)st.st_size : -1;
}
static void t_wait_fsize(const char *p, long n)
{
    int spins;
    for (spins = 0; spins < 3000 && t_fsize(p) < n; ++spins) usleep(1000);
    assert(t_fsize(p) == n);
}
static void t_age_record(void)       /* the open record started 25 s ago */
{
    pthread_mutex_lock(&g_card_lock);
    assert(g_card.seq && g_card.start_us > UINT64_C(25000000));
    g_card.start_us -= UINT64_C(25000000);
    pthread_mutex_unlock(&g_card_lock);
}
/* the map stream of the open record decoded a keyframe (what a live map receiver notes) */
static void t_map_keyframe(void *sess)
{
    uint32_t seq = alt_card_seq_for_session(sess);
    assert(seq);
    alt_card_note_map_frame(seq, 1, alt_timing_now_us());
}
/* proven good: 25 s old and the map decoded a keyframe */
static void t_prove(void *sess)
{
    t_map_keyframe(sess);
    t_age_record();
}

typedef struct { int found, role, inUse, closing, exited; uint32_t gen; } t_slot_t;
static t_slot_t t_slot(void *sess, int64_t conn)
{
    t_slot_t r;
    int i;
    memset(&r, 0, sizeof(r));
    pthread_mutex_lock(&alt_lock);
    for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
        altslot_t *s = &alt_slots[i];
        if (!s->inUse || s->session != sess || s->connID != conn) continue;
        r.found = 1;
        r.role = s->role;
        r.inUse = s->inUse;
        r.closing = s->closing || s->stop;
        r.exited = s->threadExited;
        r.gen = s->avGeneration;
    }
    pthread_mutex_unlock(&alt_lock);
    return r;
}
static int t_slot_live(void *sess, int64_t conn)
{
    t_slot_t s = t_slot(sess, conn);
    return s.found && !s.closing && !s.exited;
}

static void t_control(void *sess)
{
    tcf_t *req = t_req_control(), *resp;
    assert(t_setup(sess, req, &resp, ++t_seq) == 0);
    t_rel(req);
    t_rel(resp);
}
/* marker armed, control SETUP opens a record, /info advertises 3 displays */
static void t_open(void *sess, int night)
{
    t_touch(t_card_marker, 1);
    t_control(sess);
    assert(alt_card_seq_for_session(sess) != 0 && g_advert_card == 1);
    /* two-phase marker: the open record holds it */
    assert(!t_armed() && t_exists(t_mfile(".active")));
    t_info(sess, night);
    assert(g_card.advertised == 1);
}
static int t_media(void *sess, int n, const t_stream_t *s, int *ports)
{
    tcf_t *req = t_req_streams(n, s), *resp;
    int rc = t_setup(sess, req, &resp, ++t_seq), i;
    if (rc == 0 && ports)
        for (i = 0; i < t_resp_count(resp); ++i) ports[i] = t_resp_port(resp, i);
    t_rel(req);
    t_rel(resp);
    return rc;
}
/* per-stream TEARDOWN with one type-111 entry (conn 0 / uuid NULL: key absent) */
static void t_teardown_one(void *sess, int64_t conn, const char *uuid)
{
    t_stream_t s = {111, conn, uuid};
    tcf_t *req = t_req_streams(1, &s);
    alt_on_teardown(sess, req, 0);
    t_rel(req);
}
static void t_end(void *sess)
{
    alt_on_teardown(sess, NULL, 0);
    t_drain_all();
}

/* D<id>/A<id> renumbered by first appearance: compares tree shapes whose object
 * ids are offset by unrelated allocations. */
static void t_norm(const char *in, size_t len, char *out, size_t cap)
{
    static unsigned from[4096];
    unsigned nmap = 0, i;
    size_t w = 0, r = 0;
    while (r < len && w + 16 < cap) {
        const char *p = in + r;
        if ((*p == 'D' || *p == 'A') && (r == 0 || p[-1] == '\n' || p[-1] == '=') &&
            isdigit((unsigned char)p[1])) {
            char *end;
            unsigned long id = strtoul(p + 1, &end, 10);
            for (i = 0; i < nmap && from[i] != id; ++i) {}
            if (i == nmap) {
                assert(nmap < 4096);
                from[nmap++] = (unsigned)id;
            }
            w += (size_t)snprintf(out + w, cap - w, "%c%u", *p, i + 1);
            r = (size_t)(end - in);
            continue;
        }
        out[w++] = in[r++];
    }
    assert(r == len);
    out[w] = 0;
}
/* the control-SETUP part of t_trace equals the golden's, ids renumbered */
static void t_assert_golden_tree(void)
{
    static char a[65536], b[65536];
    const char *gend = strstr(k_golden_no_marker, "-- /info\n");
    const char *tend = strstr(t_trace, "-- /info\n");
    assert(gend && tend);
    t_norm(k_golden_no_marker, (size_t)(gend - k_golden_no_marker), a, sizeof(a));
    t_norm(t_trace, (size_t)(tend - t_trace), b, sizeof(b));
    if (strcmp(a, b) != 0) {
        fprintf(stderr, "control SETUP tree differs from the golden:\n%s", t_trace);
        assert(!"2-display tree changed");
    }
}

/* ── (b) marker: 3 displays, every card key, guard, night, precedence ───── */
static void scenario_card_dict(void)
{
    void *sess = t_sess_b;
    tcf_t *card, *cl, *va, *vs, *props, *info;
    unsigned live0;

    t_log_reset();
    t_touch(t_card_marker, 1);
    t_control(sess);
    assert(g_advert_card == 1 && g_advert_card_dict && t_arrcount(g_advert_displays) == 3);
    assert(alt_card_seq_for_session(sess) != 0 && alt_card_guard_get() == 1);
    assert(!strcmp(t_s(t_disp(0), "uuid"), "MAIN-UUID"));
    cl = t_disp(1);
    card = t_disp(2);
    assert(card == (tcf_t *)g_advert_card_dict);
    assert(!strcmp(t_s(cl, "uuid"), CLUSTER_DISPLAY_UUID) && t_n(cl, "viewAreas") == 3 &&
           t_n(cl, "properties") == 2);
    assert(!strcmp(t_s(cl, "initialURL"), ALT_CLUSTER_URL_RGI_OFF_RIGHT));
    /* the card: every key, nothing else */
    assert(card->n == 19);
    assert(!strcmp(t_s(card, "name"), "Instruction Card"));
    assert(t_i(card, "type") == 111 && t_i(card, "maxFPS") == 30);
    assert(t_i(card, "widthPixels") == 1440 && t_i(card, "heightPixels") == 542);
    assert(t_i(card, "widthPhysical") == 292 && t_i(card, "heightPhysical") == 110);
    assert(!strcmp(t_s(card, "uid"), CARD_DISPLAY_UUID) && !strcmp(t_s(card, "uuid"), CARD_DISPLAY_UUID));
    props = tcf_get(card, "properties");
    assert(props && props->n == 1 && !strcmp(props->vals[0]->text, "showsInstruments"));
    va = tcf_get(card, "viewAreas");
    assert(va && va->n == 1 && va->vals[0]->n == 5);
    t_rect(va->vals[0], 0, 0, 1440, 542);
    t_rect(tcf_get(va->vals[0], "safeArea"), 330, 88, 780, 294);
    assert(t_i(card, "initialViewArea") == 0 && t_n(card, "adjacentViewAreas") == 0);
    assert(!strcmp(t_s(card, "initialURL"), "maps:/car/instrumentcluster/instructioncard"));
    assert(t_i(card, "cornerMasks") == 0 && t_i(card, "nightMode") == 0 && t_i(card, "limitedUI") == 0);
    assert(t_n(card, "limitedUIElements") == 0);
    vs = tcf_get(card, "initialVideoStreams");
    assert(vs && vs->n == 1 && vs->vals[0]->n == 4);
    assert(t_i(vs->vals[0], "originXPixels") == 0 && t_i(vs->vals[0], "originYPixels") == 0 &&
           !strcmp(t_s(vs->vals[0], "uuid"), CARD_DISPLAY_UUID) && t_i(vs->vals[0], "zIndex") == 0);
    assert(!tcf_get(card, "primaryInputDevice") && !tcf_get(card, "features"));
    assert(t_log_has("armed=1 (3 displays") && t_log_has("declared uuid=" CARD_DISPLAY_UUID));

    /* /info: one array under both keys; stock night 1 reaches main + map, never the card */
    info = (tcf_t *)t_dictm(NULL, 0, NULL, NULL);
    tcf_set_i64(info, "nightMode", 1);
    alt_on_server_info(sess, info);
    assert(tcf_get(info, "displays") == (tcf_t *)g_advert_displays &&
           tcf_get(info, "displayPanels") == (tcf_t *)g_advert_displays);
    assert(t_n(info, "altScreenSuggestUIURLs") == 4 && tcf_get(info, "extendedFeatures"));
    assert(t_i(t_disp(0), "nightMode") == 1 && t_i(cl, "nightMode") == 1 && t_i(card, "nightMode") == 0);
    assert(g_card.advertised == 1 && t_log_has("injected 3-display contract"));
    t_rel(info);
    g_alt_night_mode = 0;
    assert(!t_armed() && t_exists(t_mfile(".active")));        /* held while open */
    t_prove(sess);
    t_end(sess);                                   /* proven good: summary, marker back */
    assert(t_armed() && g_card_last.advertised && !strcmp(g_card_last_end, "teardown"));
    assert(!strcmp(g_card_last_marker, "restored") && !t_exists(t_mfile(".active")));
    assert(alt_card_guard_get() == 0 && t_log_has("session summary") && t_log_has("marker=restored"));

    /* map URL guard: Waze guiding never puts /instructioncard on the map display */
    __atomic_store_n(&g_alt_nav_card, 1, __ATOMIC_RELEASE);
    assert(!strcmp(alt_cluster_url(), ALT_CLUSTER_URL_CARD_ONLY));   /* no record: today */
    t_control(sess);
    assert(strcmp(t_s(t_disp(1), "initialURL"), ALT_CLUSTER_URL_CARD_ONLY) != 0);
    assert(strcmp(alt_cluster_url(), ALT_CLUSTER_URL_CARD_ONLY) != 0);
    alt_card_close(sess, "test");                  /* not advertised: no disarm, marker back */
    assert(t_armed() && !strcmp(g_card_last_marker, "restored"));
    assert(!strcmp(alt_cluster_url(), ALT_CLUSTER_URL_CARD_ONLY));
    __atomic_store_n(&g_alt_nav_card, 0, __ATOMIC_RELEASE);
    /* ... and the url-diag Center mapping falls back to the plain map URL */
    t_touch(t_url_diag, 1);
    alt_layout_set(ALT_LAYOUT_CENTER);
    t_control(sess);
    assert(!strcmp(t_s(t_disp(1), "initialURL"), ALT_CLUSTER_URL_PLAIN));
    assert(!strcmp(alt_cluster_url(), ALT_CLUSTER_URL_PLAIN));
    alt_card_close(sess, "test");
    assert(!strcmp(alt_cluster_url(), ALT_CLUSTER_URL_CARD_ONLY));
    t_touch(t_url_diag, 0);
    alt_layout_set(ALT_LAYOUT_RIGHT);
    alt_url_diag_refresh();

    /* precedence: legacy wins; a wireless owner never arms */
    t_touch(t_legacy, 1);
    t_control(sess);
    assert(!g_advert_card && g_advert_legacy && t_arrcount(g_advert_displays) == 2);
    assert(!alt_card_seq_for_session(sess) && t_armed() && t_log_has("ignored (legacy advert armed)"));
    t_touch(t_legacy, 0);
    setenv("MHI2Q_DIO_OWNER_MODE", "wireless", 1);
    assert(alt_card_armed_for_advert(0) == 0 && t_log_has("ignored (wireless owner)"));
    unsetenv("MHI2Q_DIO_OWNER_MODE");
    assert(alt_card_armed_for_advert(0) == 1);
    t_end(sess);

    /* card dict build failure: today's 2-display tree, marker removed, no leak */
    live0 = t_live_count();
    t_fail_str = "Instruction Card";
    trace_reset(1);
    trace_add("-- control SETUP");
    t_control(sess);
    trace_add("-- /info");
    t_fail_str = NULL;
    assert(!g_advert_card && !g_advert_card_dict && t_arrcount(g_advert_displays) == 2);
    assert(!alt_card_seq_for_session(sess) && !t_armed());
    assert(strstr(g_card_last_disarm, "card display dict build failed"));
    assert(t_file_has(t_mfile(".disarmed"), "card display dict build failed"));
    t_assert_golden_tree();
    trace_reset(0);
    t_end(sess);
    assert(t_live_count() == live0);
}

#include "altscreen_card_display_streams.h"
#endif
