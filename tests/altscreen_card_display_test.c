/* Card display experiment (marker-gated third display, capture-only second 111 stream).
 * (a) no marker: /info, SETUP, commands and teardown are the recorded golden trace
 *     of the hook before this feature;
 * (b) marker: 3 displays, every card key, map URL guard, fixed card night, precedence,
 *     card-dict failure falls back to the golden 2-display tree;
 * (c) two 111 streams: the map keeps the transport, the card captures and gets one
 *     showUI, teardown is per stream (both ways), uuid/order binding, deferred kick,
 *     a guessed TEARDOWN stops only the card, an exited receiver frees its role,
 *     one capture writer (no symlink), a card receiver error disarms;
 * (d) self-disarm and the two-phase marker: short, unproven (no map keyframe),
 *     superseded (by its last frame), dio restart, stale open, malformed or failed
 *     card SETUP.
 * -DCARD_TEST_GOLDEN_ONLY builds (a) alone against the pre-feature hook.
 */
#define ENABLE_LOGGING 0
#ifndef CARD_TEST_GOLDEN_ONLY
static char t_card_marker[512];
#define ALT_CARD_DISPLAY_MARKER ((const char *)t_card_marker)
static char t_card_capture[512];
#define ALT_CARD_CAPTURE_PATH t_card_capture
#endif
static char t_legacy[512];
#define ALT_ADVERT_LEGACY_MARKER t_legacy
static char t_url_diag[512];
#define ALT_URL_DIAG_MARKER t_url_diag
#include "../hook/altscreen/altscreen_hook.c"
#include "altscreen_card_display_fixture.h"

static uint8_t t_sess_a[1024] __attribute__((aligned(16)));

static void t_session_init(uint8_t *s)
{
    memset(s, 0, 1024);
    s[OFF_FAMILY] = AF_INET;
}
static void t_trace_cmds_from(unsigned from)
{
    unsigned i;
    for (i = from; i < t_cmd_count; ++i)
        trace_add("cmd %s uuid=%s url=%s %s", t_cmds[i].type, t_cmds[i].uuid, t_cmds[i].url,
                  t_cmds[i].extra);
}
static void t_tmp_path(char *buf, size_t n, const char *name)
{
    const char *dir = getenv("TMPDIR");
    snprintf(buf, n, "%s/%s.%ld", dir && dir[0] ? dir : "/tmp", name, (long)getpid());
    unlink(buf);
}

/* ── (a) no marker: today's /info, SETUP, commands and teardown ──────────── */
/* Recorded from the hook before the card display existed (CARD_TEST_DUMP_GOLDEN=1,
 * -DCARD_TEST_GOLDEN_ONLY).  Object ids count every CF allocation, so equality also
 * proves the no-marker path allocates exactly as before. */
static const char k_golden_no_marker[] =
    "-- control SETUP\n"
    "D7.uid=\"MAIN-UUID\"\n"
    "D7.primaryInputDevice=0\n"
    "D7.cornerMasks=0\n"
    "D7.nightMode=0\n"
    "D7.limitedUI=0\n"
    "D7.limitedUIElements=A33\n"
    "D36.originXPixels=0\n"
    "D36.originYPixels=0\n"
    "D36.uuid=\"MAIN-UUID\"\n"
    "D36.zIndex=0\n"
    "A35+=D36\n"
    "D7.initialVideoStreams=A35\n"
    "D44.name=\"Instrument Cluster\"\n"
    "D44.type=111\n"
    "D44.maxFPS=30\n"
    "D44.widthPixels=1440\n"
    "D44.heightPixels=542\n"
    "D44.widthPhysical=292\n"
    "D44.heightPhysical=110\n"
    "D44.uid=\"C1057E12-A0D1-4E20-B0C0-1057E12A0D10\"\n"
    "D44.uuid=\"C1057E12-A0D1-4E20-B0C0-1057E12A0D10\"\n"
    "A64+=\"Instrument Cluster\"\n"
    "A64+=\"showsInstruments\"\n"
    "D44.properties=A64\n"
    "D69.originXPixels=0\n"
    "D69.originYPixels=0\n"
    "D69.widthPixels=1440\n"
    "D69.heightPixels=542\n"
    "D78.originXPixels=330\n"
    "D78.originYPixels=88\n"
    "D78.widthPixels=780\n"
    "D78.heightPixels=294\n"
    "D69.safeArea=D78\n"
    "D88.originXPixels=0\n"
    "D88.originYPixels=0\n"
    "D88.widthPixels=1440\n"
    "D88.heightPixels=542\n"
    "D97.originXPixels=108\n"
    "D97.originYPixels=88\n"
    "D97.widthPixels=348\n"
    "D97.heightPixels=294\n"
    "D88.safeArea=D97\n"
    "D107.originXPixels=0\n"
    "D107.originYPixels=0\n"
    "D107.widthPixels=1440\n"
    "D107.heightPixels=542\n"
    "D116.originXPixels=540\n"
    "D116.originYPixels=88\n"
    "D116.widthPixels=360\n"
    "D116.heightPixels=294\n"
    "D107.safeArea=D116\n"
    "A68+=D69\n"
    "A68+=D88\n"
    "A68+=D107\n"
    "D44.viewAreas=A68\n"
    "D44.initialViewArea=0\n"
    "D44.adjacentViewAreas=A129\n"
    "D44.initialURL=\"maps:/car/instrumentcluster?showETA=yes&maneuverLayout=rightAligned&showSpeedLimit=no\"\n"
    "D44.cornerMasks=0\n"
    "D44.nightMode=0\n"
    "D44.limitedUI=0\n"
    "D44.limitedUIElements=A139\n"
    "D142.originXPixels=0\n"
    "D142.originYPixels=0\n"
    "D142.uuid=\"C1057E12-A0D1-4E20-B0C0-1057E12A0D10\"\n"
    "D142.zIndex=0\n"
    "A141+=D142\n"
    "D44.initialVideoStreams=A141\n"
    "A151+=D7\n"
    "A151+=D44\n"
    "A152+=\"altScreen\"\n"
    "A152+=\"viewAreas\"\n"
    "A155+=\"maps:/car/instrumentcluster\"\n"
    "A155+=\"maps:/car/instrumentcluster/map\"\n"
    "A155+=\"maps:/car/instrumentcluster/instructioncard\"\n"
    "A155+=\"maps:/\"\n"
    "A161+=\"altScreen\"\n"
    "A161+=\"viewAreas\"\n"
    "D5.enabledFeatures=A161\n"
    "-- /info\n"
    "D7.nightMode=0\n"
    "D44.nightMode=0\n"
    "D166.extendedFeatures=A152\n"
    "D166.displays=A151\n"
    "D166.displayPanels=A151\n"
    "D166.altScreenSuggestUIURLs=A155\n"
    "-- media SETUP [111/11]\n"
    "derive(11)\n"
    "D191.type=111\n"
    "D191.dataPort=<port>\n"
    "A197+=D191\n"
    "D186.streams=A197\n"
    "claim(1)=1\n"
    "cfg(1,14)\n"
    "D198.uuid=\"C1057E12-A0D1-4E20-B0C0-1057E12A0D10\"\n"
    "D198.url=\"maps:/car/instrumentcluster?showETA=yes&maneuverLayout=rightAligned&showSpeedLimit=no\"\n"
    "D203.type=\"showUI\"\n"
    "D203.params=D198\n"
    "au(1,9)\n"
    "-- media SETUP [111/31, 111/32]\n"
    "derive(31)\n"
    "D225.type=111\n"
    "D225.dataPort=<port>\n"
    "A231+=D225\n"
    "D220.streams=A231\n"
    "claim(2)=1\n"
    "shut(1)\n"
    "-- full TEARDOWN\n"
    "shut(2)\n"
    "-- commands\n"
    "cmd showUI uuid=C1057E12-A0D1-4E20-B0C0-1057E12A0D10 url=maps:/car/instrumentcluster?showETA=yes&maneuverLayout=rightAligned&showSpeedLimit=no \n"
    "-- live objects +0\n";

static void scenario_no_marker(void)
{
    void *sess = t_sess_a;
    tcf_t *req, *resp;
    int port, fd;
    unsigned cmd0 = t_cmd_count, au0 = t_au_count, live0 = t_live_count();
    t_session_init(t_sess_a);
    trace_reset(1);
    trace_add("-- control SETUP");
    req = t_req_control();
    assert(t_setup(sess, req, &resp, 1) == 0);
    t_rel(req);
    t_rel(resp);
    trace_add("-- /info");
    t_info(sess, 0);
    trace_add("-- media SETUP [111/11]");
    {
        t_stream_t s[] = {{111, 11, NULL}};
        req = t_req_streams(1, s);
    }
    assert(t_setup(sess, req, &resp, 2) == 0);
    assert(t_resp_count(resp) == 1);
    port = t_resp_port(resp, 0);
    t_rel(req);
    t_rel(resp);
    fd = t_phone_connect(port);
    t_phone_send(fd, 1, t_avcc, sizeof(t_avcc));
    t_phone_send(fd, 0, t_idr, sizeof(t_idr));
    t_wait_aus(au0 + 1);
    t_wait_cmds("showUI", NULL, (int)cmd0 + 1);
    trace_add("-- media SETUP [111/31, 111/32]");
    {
        t_stream_t s[] = {{111, 31, NULL}, {111, 32, NULL}};
        req = t_req_streams(2, s);
    }
    assert(t_setup(sess, req, &resp, 3) == 0);
    assert(t_resp_count(resp) == 1);
    t_rel(req);
    t_rel(resp);
    t_wait_gen_exited(1);
    trace_add("-- full TEARDOWN");
    alt_on_teardown(sess, NULL, 0);
    t_drain_all();
    close(fd);
    trace_add("-- commands");
    t_trace_cmds_from(cmd0);
    trace_add("-- live objects %+d", (int)t_live_count() - (int)live0);
    if (getenv("CARD_TEST_DUMP_GOLDEN")) {
        fputs(t_trace, stdout);
        exit(0);
    }
    if (strcmp(t_trace, k_golden_no_marker) != 0) {
        fprintf(stderr, "no-marker trace differs from the golden:\n%s", t_trace);
        assert(!"no-marker trace changed");
    }
    trace_reset(0);
}

#ifndef CARD_TEST_GOLDEN_ONLY
#include "altscreen_card_display_cases.h"
#endif

int main(void)
{
    alarm(20);
    t_tmp_path(t_legacy, sizeof(t_legacy), "card_test_legacy");
    t_tmp_path(t_url_diag, sizeof(t_url_diag), "card_test_url_diag");
#ifndef CARD_TEST_GOLDEN_ONLY
    t_tmp_path(t_card_marker, sizeof(t_card_marker), "card_test_marker");
    t_tmp_path(t_card_capture, sizeof(t_card_capture), "card_test_cap111b");
#endif
    t_wire_hook();
    scenario_no_marker();                  /* first: the hook's once-only work runs here */
#ifdef CARD_TEST_GOLDEN_ONLY
    puts("altscreen_card_display_test: (a) no-marker golden PASS");
#else
    t_wire_card();
    scenario_card_dict();
    scenario_two_streams();
    scenario_reverse_and_binding();
    scenario_exited_resetup();
    scenario_capture_owner();
    scenario_card_receiver_error();
    scenario_self_disarm();
    altscreen_shutdown();
    unlink(t_card_marker);
    unlink(t_mfile(".active"));
    unlink(t_mfile(".disarmed"));
    unlink(t_card_capture);
    puts("altscreen_card_display_test: (a) golden (b) card dict (c) two 111 streams (d) self-disarm PASS");
#endif
    return 0;
}
