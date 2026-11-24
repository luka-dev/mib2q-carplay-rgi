/* Card display scenario (d): self-disarm and the two-phase marker, for
 * tests/altscreen_card_display_test.c (included by altscreen_card_display_streams.h). */
#ifndef ALTSCREEN_CARD_DISPLAY_ROLLBACK_H
#define ALTSCREEN_CARD_DISPLAY_ROLLBACK_H

/* what a dio_manager restart leaves: no record, no claim on <marker>.active */
static void t_forget_record(void)
{
    pthread_mutex_lock(&g_card_lock);
    memset(&g_card, 0, sizeof(g_card));
    g_card_active_mine = 0;
    __atomic_store_n(&g_card_live_seq, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_alt_card_guard, 0, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&g_card_lock);
}
/* the next connect builds exactly today's 2-display tree */
static void t_assert_next_is_today(void *sess)
{
    trace_reset(1);
    trace_add("-- control SETUP");
    t_control(sess);
    trace_add("-- /info");
    assert(!g_advert_card && !alt_card_seq_for_session(sess));
    t_assert_golden_tree();
    trace_reset(0);
}

static void scenario_self_disarm(void)
{
    void *sess = t_sess_b;
    int ports[2];

    /* advertised session shorter than 20 s: the reason is left for the logging card */
    t_open(sess, 0);
    t_end(sess);
    assert(!t_armed() && strstr(g_card_last_disarm, "(<20 s, end=teardown)"));
    assert(t_file_has(t_mfile(".disarmed"), "(<20 s, end=teardown)") && !t_exists(t_mfile(".active")));
    t_assert_next_is_today(sess);
    t_end(sess);

    /* 20 s or longer and the map decoded a keyframe: the marker comes back */
    t_open(sess, 0);
    t_prove(sess);
    t_end(sess);
    assert(t_armed() && !strcmp(g_card_last_end, "teardown") && !strcmp(g_card_last_marker, "restored"));

    /* ... unless the logging card revoked the loan meanwhile (trigger removed) */
    t_open(sess, 0);
    t_prove(sess);
    unlink(t_mfile(".active"));
    t_end(sess);
    assert(!t_armed() && !strcmp(g_card_last_marker, "revoked") && t_log_has("marker=revoked"));
    t_touch(t_card_marker, 1);

    /* 20 s or longer but the map never decoded a keyframe (stuck connect, black VC) */
    t_open(sess, 0);
    t_age_record();
    t_end(sess);
    assert(!t_armed() && strstr(g_card_last_disarm, "without a map keyframe (end=teardown)"));

    /* superseded within 20 s: closed before the new connect reads the marker */
    t_open(sess, 0);
    t_control(sess);
    assert(!strcmp(g_card_last_end, "superseded") && !t_armed());
    assert(!g_advert_card && !alt_card_seq_for_session(sess));
    t_end(sess);

    /* superseded: the session lasted until its last frame, not until the next connect */
    t_open(sess, 0);
    t_prove(sess);                                  /* one keyframe at the start, 25 s ago */
    t_control(sess);
    assert(!strcmp(g_card_last_end, "superseded") && !t_armed() && g_card_last_dur_us < 1000000u);
    t_end(sess);
    t_open(sess, 0);
    t_map_keyframe(sess);
    __atomic_store_n(&g_card_rx_ms, 25001u, __ATOMIC_RELAXED);   /* frames until 25 s */
    t_control(sess);                                /* restored, so this connect arms again */
    assert(!strcmp(g_card_last_end, "superseded") && !strcmp(g_card_last_marker, "restored"));
    assert(g_advert_card && alt_card_seq_for_session(sess) != 0);
    t_end(sess);                                    /* never advertised: marker back */
    assert(t_armed());

    /* never advertised: no disarm */
    t_control(sess);
    assert(alt_card_seq_for_session(sess) != 0 && !t_armed());
    t_end(sess);
    assert(t_armed() && !g_card_last.advertised);

    /* a dio_manager restart mid-session: the held marker is gone, the next connect is today's */
    t_open(sess, 0);
    t_forget_record();
    t_assert_next_is_today(sess);
    t_end(sess);
    /* ... and when the logging card re-armed it meanwhile, the stale .active disarms */
    t_touch(t_card_marker, 1);
    t_control(sess);
    assert(!g_advert_card && !t_armed() && !t_exists(t_mfile(".active")));
    assert(strstr(g_card_last_disarm, "never closed") && t_file_has(t_mfile(".disarmed"), "never closed"));
    t_end(sess);

    /* an older control SETUP never replaces a newer record */
    t_open(sess, 0);
    {
        uint32_t seq = alt_card_seq_for_session(sess), adv;
        pthread_mutex_lock(&g_card_lock);
        adv = g_card.advert_seq;
        pthread_mutex_unlock(&g_card_lock);
        assert(alt_card_open(t_sess_a, adv - 1) == 0 && t_log_has("stale open ignored"));
        assert(alt_card_seq_for_session(sess) == seq && !alt_card_seq_for_session(t_sess_a));
    }
    t_end(sess);

    /* malformed card SETUPs disarm; the map slot from the earlier SETUP lives on */
    t_open(sess, 0);
    {
        t_stream_t a = {111, 71, NULL}, b = {111, 0, NULL};
        assert(t_media(sess, 1, &a, ports) == 0);
        assert(t_media(sess, 1, &b, ports) == kVersionErr);
    }
    assert(!t_armed() && strstr(g_card_last_disarm, "card SETUP failed"));
    assert(t_slot_live(sess, 71) && alt_generation_for_session(sess) != 0);
    t_end(sess);

    t_open(sess, 0);
    {
        t_stream_t a = {111, 72, NULL};
        t_stream_t b[] = {{111, 73, NULL}, {111, 74, NULL}, {111, 75, NULL}};
        assert(t_media(sess, 1, &a, ports) == 0);
        assert(t_media(sess, 3, b, ports) == kMalformedErr);
    }
    assert(!t_armed() && strstr(g_card_last_disarm, "offered 3 type-111 streams"));
    assert(t_slot_live(sess, 72) && !t_slot(sess, 73).found);
    t_end(sess);

    t_open(sess, 0);
    {
        t_stream_t a = {111, 76, NULL}, b = {111, 77, NULL};
        assert(t_media(sess, 1, &a, ports) == 0);
        t_fail_sockopen = 1;
        assert(t_media(sess, 1, &b, ports) != 0);
        assert(t_fail_sockopen == 0);
    }
    assert(!t_armed() && strstr(g_card_last_disarm, "card SETUP failed"));
    assert(t_slot_live(sess, 76) && alt_generation_for_session(sess) != 0);
    t_end(sess);
    assert(!t_armed() && !strcmp(g_card_last_marker, "removed"));
}
#endif
