/* Card display scenarios (c) two 111 streams, for tests/altscreen_card_display_test.c
 * (included by altscreen_card_display_cases.h; (d) is in ..._rollback.h). */
#ifndef ALTSCREEN_CARD_DISPLAY_STREAMS_H
#define ALTSCREEN_CARD_DISPLAY_STREAMS_H

/* Annex-B of t_avcc, then t_idr and t_pau with start codes */
static const uint8_t k_cap_cfg[] = {0, 0, 0, 1, 0x67, 0x64, 0, 0x1f, 0, 0, 0, 1, 0x68, 0xce};
static const uint8_t k_cap_idr[] = {0, 0, 0, 1, 0x65, 0x88, 0x84, 0, 0x10};
static const uint8_t k_cap_pau[] = {0, 0, 0, 1, 0x41, 0x9a, 0x02};

static void t_send_cfg_idr(int fd)
{
    t_phone_send(fd, 1, t_avcc, sizeof(t_avcc));
    t_phone_send(fd, 0, t_idr, sizeof(t_idr));
}
static const char *t_last_url(const char *type, const char *uuid)
{
    const char *url = NULL;
    unsigned i;
    pthread_mutex_lock(&t_ev_lock);
    for (i = 0; i < t_cmd_count; ++i)
        if (!strcmp(t_cmds[i].type, type) && !strcmp(t_cmds[i].uuid, uuid)) url = t_cmds[i].url;
    pthread_mutex_unlock(&t_ev_lock);
    return url;
}

/* descriptor dump: every stream logged, secrets and CFData bytes never */
static void t_descriptor_dump(void *sess)
{
    tcf_t *req = (tcf_t *)t_dictm(NULL, 0, NULL, NULL), *arr = (tcf_t *)t_arrm(NULL, 0, NULL);
    tcf_t *e = (tcf_t *)t_dictm(NULL, 0, NULL, NULL), *shk, *blob;
    void *probe_kv = (void *)card_cf_dict_kv;
    shk = tcf_num(TCF_DATA, 0);
    shk->dlen = 32;
    blob = tcf_num(TCF_DATA, 0);
    blob->dlen = 12;
    tcf_set_i64(e, "type", 96);
    tcf_set_obj(e, "shk", shk);
    tcf_set_s(e, "streamKeyHint", "s3cr3t-value");
    tcf_set_s(e, "audioType", "default");
    tcf_set_i64(e, "ct", 2);
    tcf_set_obj(e, "payload", blob);
    tcf_app_raw(arr, e);
    t_rel(e);
    tcf_set_obj(req, "streams", arr);
    assert(t_setup(sess, req, NULL, ++t_seq) == 0);          /* no 111: nothing set up */
    assert(t_log_has("stream[0/1] type=96 keys=walk count=6 type=96 shk=<redacted> "
                     "streamKeyHint=<redacted> audioType=\"default\" ct=2 payload=<data 12 bytes>"));
    card_cf_dict_kv = NULL;                                  /* no CFDictionaryGetKeysAndValues */
    assert(t_setup(sess, req, NULL, ++t_seq) == 0);
    card_cf_dict_kv = (fn_cfdict_kv_t)probe_kv;
    assert(t_log_has("stream[0/1] type=96 keys=probe count=6 type=96 audioType=\"default\" ct=2"));
    assert(!t_log_has("s3cr3t"));
    t_rel(req);
}

/* ── (c) two 111 descriptors in one SETUP, bound by order ─────────────────── */
static void scenario_two_streams(void)
{
    void *sess = t_sess_b;
    t_stream_t s[] = {{111, 21, NULL}, {111, 22, NULL}};
    int ports[2], fm, fc, mshow0, cshow0, ccard0, night0;
    unsigned au0;
    long want = (long)(sizeof(k_cap_cfg) + sizeof(k_cap_idr) + sizeof(k_cap_pau));
    uint8_t buf[64];
    t_slot_t m, c;
    FILE *f;

    t_log_reset();
    unlink(t_card_capture);
    __atomic_store_n(&g_alt_nav_card, 1, __ATOMIC_RELEASE);  /* Waze guiding */
    t_open(sess, 0);
    t_descriptor_dump(sess);
    mshow0 = t_cmds_locked("showUI", CLUSTER_DISPLAY_UUID);
    cshow0 = t_cmds_locked("showUI", CARD_DISPLAY_UUID);
    ccard0 = t_cmds_locked(NULL, CARD_DISPLAY_UUID);
    au0 = t_au_count;
    trace_reset(1);
    assert(t_media(sess, 2, s, ports) == 0 && ports[0] != ports[1]);
    assert(strstr(t_trace, "derive(21)") && strstr(t_trace, "derive(22)"));
    trace_reset(0);
    m = t_slot(sess, 21);
    c = t_slot(sess, 22);
    assert(m.found && m.role == ALT_ROLE_MAP && m.gen == g_alt_session_gen && g_alt_session == sess);
    assert(t_owner == m.gen);
    assert(c.found && c.role == ALT_ROLE_CARD && c.gen && c.gen != m.gen);
    assert(t_log_has("desc[0] connID=21 -> role=map by=order") &&
           t_log_has("desc[1] connID=22 -> role=card by=order"));
    assert(t_log_has("SETUP seq=") && t_log_has("stream[1/2] type=111 keys=walk"));

    fm = t_phone_connect(ports[0]);
    fc = t_phone_connect(ports[1]);
    t_send_cfg_idr(fm);
    t_phone_send(fm, 0, t_pau, sizeof(t_pau));
    t_wait_aus(au0 + 2);
    t_wait_cmds("showUI", CLUSTER_DISPLAY_UUID, mshow0 + 1);
    t_phone_send(fc, 1, t_avcc, sizeof(t_avcc));
    t_phone_send(fc, 0, t_pau, sizeof(t_pau));               /* pre-IDR: not captured */
    t_phone_send(fc, 0, t_idr, sizeof(t_idr));
    t_phone_send(fc, 0, t_pau, sizeof(t_pau));
    t_wait_cmds("showUI", CARD_DISPLAY_UUID, cshow0 + 1);
    t_wait_log("showUI ack status=0");                       /* a WARN line in the car log */
    t_wait_fsize(t_card_capture, want);
    f = fopen(t_card_capture, "rb");
    assert(f && fread(buf, 1, sizeof(buf), f) == (size_t)want);
    fclose(f);
    assert(!memcmp(buf, k_cap_cfg, sizeof(k_cap_cfg)) &&
           !memcmp(buf + sizeof(k_cap_cfg), k_cap_idr, sizeof(k_cap_idr)) &&
           !memcmp(buf + sizeof(k_cap_cfg) + sizeof(k_cap_idr), k_cap_pau, sizeof(k_cap_pau)));
    t_phone_send(fc, 0, t_idr, sizeof(t_idr));               /* second IDR: no second kick */
    want += (long)sizeof(k_cap_idr);
    t_wait_fsize(t_card_capture, want);
    usleep(50000);
    assert(t_cmds_locked("showUI", CARD_DISPLAY_UUID) == cshow0 + 1);
    assert(!strcmp(t_last_url("showUI", CARD_DISPLAY_UUID), ALT_CARD_DISPLAY_URL));
    assert(strcmp(t_last_url("showUI", CLUSTER_DISPLAY_UUID), ALT_CLUSTER_URL_CARD_ONLY) != 0);
    assert(!t_av_gen_seen(c.gen) && t_au_count == au0 + 2);
    /* runtime night stays cluster-only */
    night0 = t_cmds_locked("setNightMode", CLUSTER_DISPLAY_UUID);
    assert(alt_send_cluster_night_mode(alt_generation_for_session(sess), 1, "test") == 0);
    t_wait_cmds("setNightMode", CLUSTER_DISPLAY_UUID, night0 + 1);
    g_alt_night_mode = 0;
    assert(t_cmds_locked(NULL, CARD_DISPLAY_UUID) == ccard0 + 1);

    /* per-stream TEARDOWN of the card: the map, its commands and the advert stay */
    t_teardown_one(sess, 22, NULL);
    t_wait_gen_exited(c.gen);
    t_wait_log("cap111b wrote 39 bytes frames=3");
    assert(t_log_has("TEARDOWN 111 desc[0] connID=22 -> role=card by=connID"));
    assert(t_log_has("TEARDOWN stopped role(s) card only"));
    assert(alt_generation_for_session(sess) == m.gen && g_advert_ready && g_advert_session == sess);
    assert(t_slot_live(sess, 21));
    t_phone_send(fm, 0, t_pau, sizeof(t_pau));
    t_wait_aus(au0 + 3);
    t_end(sess);
    close(fm);
    close(fc);
    assert(g_card_last.advertised && g_card_last.streams111 == 2 && g_card_last.card_streams == 1);
    assert((long)g_card_last_bytes == t_fsize(t_card_capture) && g_card_last_frames == 3);
    assert(g_card_last_map_idr && g_card_last_map_frames == 3 && !g_card_last_map_healthy);
    assert(!strcmp(g_card_last_end, "teardown") && !t_armed());   /* < 20 s: self-disarm */
    assert(t_log_has("session summary") && t_log_has("map_healthy=no"));
    __atomic_store_n(&g_alt_nav_card, 0, __ATOMIC_RELEASE);
}

/* ── (c) reverse teardown, uuid binding + healthy map, separate SETUPs,
 *       card-first deferred kick, ambiguous teardown ───────────────────────── */
static void scenario_reverse_and_binding(void)
{
    void *sess = t_sess_b;
    int ports[2], fm, fc, i, cshow0;
    unsigned au0;
    long size;
    t_slot_t m, c;

    /* reverse: tearing down the map leaves the card capturing */
    t_log_reset();
    unlink(t_card_capture);
    t_open(sess, 0);
    {
        t_stream_t s[] = {{111, 23, NULL}, {111, 24, NULL}};
        assert(t_media(sess, 2, s, ports) == 0);
    }
    m = t_slot(sess, 23);
    c = t_slot(sess, 24);
    assert(m.role == ALT_ROLE_MAP && c.role == ALT_ROLE_CARD);
    cshow0 = t_cmds_locked("showUI", CARD_DISPLAY_UUID);
    au0 = t_au_count;
    fm = t_phone_connect(ports[0]);
    fc = t_phone_connect(ports[1]);
    t_send_cfg_idr(fm);
    t_send_cfg_idr(fc);
    t_wait_aus(au0 + 1);
    t_wait_cmds("showUI", CARD_DISPLAY_UUID, cshow0 + 1);
    t_wait_fsize(t_card_capture, (long)(sizeof(k_cap_cfg) + sizeof(k_cap_idr)));
    size = t_fsize(t_card_capture);
    t_teardown_one(sess, 23, NULL);
    t_wait_gen_exited(m.gen);
    assert(g_alt_session == NULL && alt_generation_for_session(sess) == 0);
    assert(t_log_has("TEARDOWN stopped role(s) map only"));
    t_phone_send(fc, 0, t_pau, sizeof(t_pau));
    t_wait_fsize(t_card_capture, size + (long)sizeof(k_cap_pau));
    assert(t_slot_live(sess, 24) && !t_av_gen_seen(c.gen));
    t_end(sess);
    close(fm);
    close(fc);

    /* uuid binding wins over order; 30 map AUs make the map healthy */
    t_log_reset();
    t_open(sess, 0);
    {
        t_stream_t s[] = {{111, 41, CARD_DISPLAY_UUID}, {111, 42, CLUSTER_DISPLAY_UUID}};
        assert(t_media(sess, 2, s, ports) == 0);
    }
    assert(t_slot(sess, 41).role == ALT_ROLE_CARD && t_slot(sess, 42).role == ALT_ROLE_MAP);
    assert(t_log_has("connID=41 -> role=card by=uuid:uuid") &&
           t_log_has("connID=42 -> role=map by=uuid:uuid"));
    au0 = t_au_count;
    fm = t_phone_connect(ports[1]);                          /* response order = request order */
    t_send_cfg_idr(fm);
    for (i = 0; i < 29; ++i) t_phone_send(fm, 0, t_pau, sizeof(t_pau));
    t_wait_aus(au0 + 30);
    t_end(sess);
    close(fm);
    assert(g_card_last_map_frames == 30 && g_card_last_map_healthy && t_log_has("map_healthy=yes"));
    assert(g_card_last.bind_uuid == 2 && g_card_last.bind_order == 0 && t_log_has("bind_uuid=2 bind_order=0"));

    /* separate SETUPs: first 111 is the map, the next one the card */
    t_open(sess, 0);
    {
        t_stream_t a = {111, 51, NULL}, b = {111, 52, NULL};
        assert(t_media(sess, 1, &a, ports) == 0 && t_media(sess, 1, &b, ports + 1) == 0);
    }
    assert(t_slot(sess, 51).role == ALT_ROLE_MAP && t_slot(sess, 52).role == ALT_ROLE_CARD);
    assert(t_slot_live(sess, 51) && t_slot_live(sess, 52));
    t_end(sess);

    /* card SETUP first: its kick waits for the map's committed generation, once */
    t_log_reset();
    t_open(sess, 0);
    {
        t_stream_t a = {111, 61, CARD_DISPLAY_UUID};
        assert(t_media(sess, 1, &a, ports) == 0);
    }
    assert(t_slot(sess, 61).role == ALT_ROLE_CARD && alt_generation_for_session(sess) == 0);
    cshow0 = t_cmds_locked("showUI", CARD_DISPLAY_UUID);
    fc = t_phone_connect(ports[0]);
    t_send_cfg_idr(fc);
    t_wait_log("showUI deferred (no committed map generation)");
    {
        t_stream_t b = {111, 62, NULL};
        assert(t_media(sess, 1, &b, ports + 1) == 0);
    }
    assert(t_slot(sess, 62).role == ALT_ROLE_MAP && alt_generation_for_session(sess) != 0);
    assert(t_slot_live(sess, 61));
    usleep(1100000);                                         /* retry is at most once per second */
    t_phone_send(fc, 0, t_pau, sizeof(t_pau));
    t_wait_cmds("showUI", CARD_DISPLAY_UUID, cshow0 + 1);
    usleep(1100000);
    t_phone_send(fc, 0, t_pau, sizeof(t_pau));
    t_phone_send(fc, 0, t_idr, sizeof(t_idr));
    usleep(50000);
    assert(t_cmds_locked("showUI", CARD_DISPLAY_UUID) == cshow0 + 1);

    /* a 111 entry naming no stream stops only the capture-only card (a guess, so it
     * disarms); the map keeps running.  The next one leaves no choice: the map. */
    {
        uint32_t g61 = t_slot(sess, 61).gen;
        t_teardown_one(sess, 0, NULL);
        t_wait_gen_exited(g61);
    }
    assert(t_log_has("names no stream; stopping the card only"));
    assert(!t_slot_live(sess, 61) && t_slot_live(sess, 62) && alt_generation_for_session(sess) != 0);
    assert(!t_armed() && strstr(g_card_last_disarm, "named no stream"));
    t_teardown_one(sess, 0, NULL);
    t_wait_gen_exited(0);
    assert(t_log_has("stopping the remaining role(s) map"));
    assert(!t_slot_live(sess, 62) && alt_generation_for_session(sess) == 0);
    t_end(sess);
    assert(g_card_last.teardown_guess == 1 && t_log_has("teardown_guess=1"));
    close(fc);
}

/* ── (c) a receiver that left on its own (the phone closed its connection, no
 *       TEARDOWN) frees its role: the re-SETUP binds to it, the map claims again ─ */
static void t_wait_slot_exited(void *sess, int64_t conn)
{
    int spins;
    for (spins = 0; spins < 3000 && !t_slot(sess, conn).exited; ++spins) usleep(1000);
    assert(t_slot(sess, conn).exited);
}
static int t_record_disarmed(void)
{
    int d;
    pthread_mutex_lock(&g_card_lock);
    d = g_card.seq && g_card.disarmed;
    pthread_mutex_unlock(&g_card_lock);
    return d;
}
static void scenario_exited_resetup(void)
{
    void *sess = t_sess_b;
    int ports[2], port, fm, fc;
    unsigned au0;
    t_slot_t m, n;
    t_stream_t s[] = {{111, 81, NULL}, {111, 82, NULL}}, rc = {111, 83, NULL}, rm = {111, 84, NULL};

    t_log_reset();
    t_open(sess, 0);
    assert(t_media(sess, 2, s, ports) == 0);
    m = t_slot(sess, 81);
    fm = t_phone_connect(ports[0]);
    fc = t_phone_connect(ports[1]);
    au0 = t_au_count;
    t_send_cfg_idr(fm);
    t_send_cfg_idr(fc);
    t_wait_aus(au0 + 1);
    close(fc);                                  /* the card connection ends */
    t_wait_slot_exited(sess, 82);
    assert(t_media(sess, 1, &rc, &port) == 0);
    n = t_slot(sess, 83);
    assert(n.found && n.role == ALT_ROLE_CARD && alt_generation_for_session(sess) == m.gen);
    assert(t_log_has("connID=83 -> role=card by=order") && !t_record_disarmed());
    fc = t_phone_connect(port);
    close(fm);                                  /* the map connection ends */
    t_wait_slot_exited(sess, 81);
    assert(t_media(sess, 1, &rm, &port) == 0);
    n = t_slot(sess, 84);
    assert(n.found && n.role == ALT_ROLE_MAP && n.gen == g_alt_session_gen && t_owner == n.gen);
    assert(t_log_has("connID=84 -> role=map by=order") && !t_record_disarmed());
    assert(t_slot_live(sess, 83));
    fm = t_phone_connect(port);
    au0 = t_au_count;
    t_send_cfg_idr(fm);
    t_wait_aus(au0 + 1);
    assert(t_av_gen_seen(n.gen) && !t_av_gen_seen(t_slot(sess, 83).gen));
    t_end(sess);
    close(fm);
    close(fc);
}

/* ── (c) one capture writer: a re-SETUP's receiver takes the file over; never a symlink ─ */
static void scenario_capture_owner(void)
{
    static const uint8_t a[] = {1, 2, 3}, b[] = {4, 5};
    char other[600];
    const char *p1, *p2, *p3, *p4;
    alt_capture_t c;
    t_log_reset();
    unlink(t_card_capture);
    assert(card_cap_take(901) == 1 && card_cap_append(901, 0, a, sizeof(a), 1, 0) == 0);
    assert(card_cap_take(902) == 1);
    assert(card_cap_append(901, 0, a, sizeof(a), 1, 0) == -1);  /* the retired one stops writing */
    assert(card_cap_append(902, 0, b, sizeof(b), 1, 0) == 0);
    card_cap_release(901);                                      /* not the writer: no-op */
    assert(g_card_cap_gen == 902);
    card_cap_release(902);
    assert(t_fsize(t_card_capture) == 5 && g_card_cap_gen == 0);
    pthread_mutex_lock(&t_ev_lock);                             /* custom.sh cap111b_closed order */
    p1 = strstr(t_log, "cap111b armed gen=901 existing=0");
    p2 = strstr(t_log, "cap111b wrote 3 bytes frames=1 skipped=0 gen=901 (taken over by gen=902)");
    p3 = strstr(t_log, "cap111b armed gen=902 existing=3");
    p4 = strstr(t_log, "cap111b wrote 2 bytes frames=1 skipped=0 gen=902\n");
    pthread_mutex_unlock(&t_ev_lock);
    assert(p1 && p2 && p3 && p4 && p1 < p2 && p2 < p3 && p3 < p4);
    /* a symlink at the capture path is refused, whatever it points to */
    snprintf(other, sizeof(other), "%s.target", t_card_capture);
    rename(t_card_capture, other);
    assert(symlink(other, t_card_capture) == 0);
    assert(alt_capture_open(&c, t_card_capture, 1024) == -2 && c.fd < 0);
    assert(card_cap_take(903) == 0 && t_log_has("cap111b target is not a regular file"));
    assert(card_cap_append(903, 0, a, sizeof(a), 1, 0) == -1 && t_fsize(other) == 5);
    unlink(t_card_capture);
    unlink(other);
}

/* ── (c) a card receiver error ends only the card and disarms ─────────────── */
static void scenario_card_receiver_error(void)
{
    static const uint8_t bad_avcc[] = {1, 0x64, 0, 0x1f, 0xff, 0xe1, 0, 60, 0x67};
    void *sess = t_sess_b;
    int ports[2], fc;
    t_stream_t s[] = {{111, 93, NULL}, {111, 94, NULL}};
    t_log_reset();
    t_open(sess, 0);
    assert(t_media(sess, 2, s, ports) == 0);
    fc = t_phone_connect(ports[1]);
    t_phone_send(fc, 1, bad_avcc, sizeof(bad_avcc));
    t_wait_log("self-disarm (card receiver error: invalid avcC config)");
    assert(t_log_has("error=invalid avcC config") && t_record_disarmed() && !t_armed());
    assert(t_slot_live(sess, 93) && alt_generation_for_session(sess) == t_slot(sess, 93).gen);
    t_prove(sess);                       /* even a proven session keeps it removed */
    t_end(sess);
    assert(!t_armed() && !strcmp(g_card_last_marker, "removed"));
    close(fc);
}

#include "altscreen_card_display_rollback.h"
#endif
