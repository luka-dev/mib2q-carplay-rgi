/* Lane guidance end to end, native half: the real rgd_hook.c message handler
 * fed iAP2 0x5201/0x5202/0x5204 frames the way iOS CarKitNavigation sends them
 * (lane events in a capacity-6 window from currentLaneGuidanceIndex, numbered
 * on their own and lagging the maneuver indices).  Every bus text frame the
 * hook publishes goes to <dir>/NNN.txt; <dir>/steps lists "F NNN" (feed that
 * frame) and "E <event> <seed>" (expected visible lanes: event index and the
 * content seed, -1 = no lanes), and "D <meters> <visible>" (maneuver distance
 * and progress). LaneGuidanceE2ETest.java replays these through
 * RouteGuidance.parse and BAPBridge.update.  Built by scripts/test_lane_guidance_e2e.sh. */
#include "routeguidance/rgd_hook.c"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ---- framework seams the handler reaches; none is on the path under test ---- */
static hook_context_t g_ctx;
hook_context_t* hook_framework_get_context(void) { return &g_ctx; }
bool hook_is_ready(void) { return false; }
hook_result_t hook_inject_message(uint16_t m, const uint8_t* p, size_t n) { (void)m; (void)p; (void)n; return HOOK_OK; }
uint8_t* hook_identify_grow(hook_context_t* c, size_t l, size_t n, size_t* cap) {
    (void)c; (void)l; (void)n; (void)cap; return NULL;
}

/* ---- bus output: one file per published frame ---- */
static const char* g_dir;
static FILE* g_steps;
static int g_frames;
static char g_last_frame[8192];
hook_result_t bus_send_text(uint16_t type, uint8_t flags, bus_text_builder_t* b) {
    char path[512];
    FILE* f;
    (void)type; (void)flags;
    if (!b || !b->buf || b->overflow) { fprintf(stderr, "bus frame overflow\n"); exit(1); }
    assert(b->len < sizeof(g_last_frame));
    memcpy(g_last_frame, b->buf, b->len); g_last_frame[b->len] = '\0';
    snprintf(path, sizeof(path), "%s/%03d.txt", g_dir, g_frames);
    f = fopen(path, "wb");
    if (!f || fwrite(b->buf, 1, b->len, f) != b->len) { perror(path); exit(1); }
    fclose(f);
    fprintf(g_steps, "F %03d\n", g_frames++);
    bus_text_free(b);
    return HOOK_OK;
}

/* ---- iAP2 frame builder ---- */
static uint8_t g_msg[2048];
static size_t g_len;
static void begin(uint16_t msgid) { g_msg[0] = g_msg[1] = 0x40; write_be16(g_msg + 4, msgid); g_len = 6; }
static uint8_t* tlv(uint8_t* at, size_t* len, uint16_t id, const uint8_t* v, size_t n) {
    write_be16(at + *len, (uint16_t)(n + 4)); write_be16(at + *len + 2, id);
    memcpy(at + *len + 4, v, n); *len += n + 4; return at;
}
static void u8(uint16_t id, uint8_t v) { tlv(g_msg, &g_len, id, &v, 1); }
static void u16(uint16_t id, uint16_t v) { uint8_t p[2]; write_be16(p, v); tlv(g_msg, &g_len, id, p, 2); }
static void u32(uint16_t id, uint32_t v) { uint8_t p[4]; write_be32(p, v); tlv(g_msg, &g_len, id, p, 4); }
static void send(void) {
    iap2_frame_t frame;
    write_be16(g_msg + 2, (uint16_t)g_len);
    memset(&frame, 0, sizeof(frame));
    frame.msgid = read_be16(g_msg + 4);
    frame.frame_len = (uint16_t)g_len;
    g_ctx.raw_buf = g_msg; g_ctx.raw_len = g_len;
    rgd_message_handler(&g_ctx, &frame);
}

/* 0x5201: state, maneuver list [first, first+1], lane event (-1 = absent) and showing. */
static void update(int state, int first, int lane, int showing) {
    uint8_t list[4];
    begin(0x5201);
    u8(RGD_TLV_ROUTE_GUIDANCE_STATE, (uint8_t)state);
    if (state != 0) {
        u16(RGD_TLV_MANEUVER_COUNT, 2);
        write_be16(list, (uint16_t)first); write_be16(list + 2, (uint16_t)(first + 1));
        tlv(g_msg, &g_len, RGD_TLV_MANEUVER_LIST, list, 4);
        if (lane >= 0) u16(RGD_TLV_LANE_GUIDANCE_INDEX, (uint16_t)lane);
        u8(RGD_TLV_LANE_GUIDANCE_SHOWING, (uint8_t)showing);
    }
    send();
}
static void maneuver(int idx) {
    begin(0x5202);
    u16(MAN_TLV_INDEX, (uint16_t)idx);
    u8(MAN_TLV_TYPE, MAN_TYPE_LEFT_TURN);
    send();
}
/* Lane content is derived from a seed, so the Java side can tell events apart. */
static const int16_t kAngles[5] = {-90, -45, 0, 45, 90};
static int lanes_for(int seed) { return 2 + seed % 5; }
static void lane_event(int idx, int seed) {
    begin(0x5204);
    u16(LANE_MSG_TLV_LANE_GUIDANCE_INDEX, (uint16_t)idx);
    for (int i = 0; i < lanes_for(seed); i++) {
        uint8_t v[32]; size_t n = 0; uint8_t p[2];
        write_be16(p, (uint16_t)i); tlv(v, &n, LANE_INFO_TLV_INDEX, p, 2);
        p[0] = (uint8_t)((seed + i) % 3); tlv(v, &n, LANE_INFO_TLV_STATUS, p, 1);
        write_be16(p, (uint16_t)kAngles[(seed + i) % 5]); tlv(v, &n, LANE_INFO_TLV_ANGLES, p, 2);
        tlv(g_msg, &g_len, LANE_MSG_TLV_LANE_INFORMATIONS, v, n);
    }
    send();
}
/* 0x5204 with lane data but no lane index (iOS always sends one). */
static void lane_event_without_index(int seed) {
    uint8_t v[32]; size_t n = 0; uint8_t p[2];
    begin(0x5204);
    write_be16(p, 0); tlv(v, &n, LANE_INFO_TLV_INDEX, p, 2);
    p[0] = (uint8_t)(seed % 3); tlv(v, &n, LANE_INFO_TLV_STATUS, p, 1);
    write_be16(p, (uint16_t)kAngles[seed % 5]); tlv(v, &n, LANE_INFO_TLV_ANGLES, p, 2);
    tlv(g_msg, &g_len, LANE_MSG_TLV_LANE_INFORMATIONS, v, n);
    send();
}
static void expect(int event, int seed) { fprintf(g_steps, "E %d %d\n", event, seed); }
static void expect_distance(int meters, int visible) { fprintf(g_steps, "D %d %d\n", meters, visible); }
static void expect_maneuver(int icon, const char* road) { fprintf(g_steps, "M %d %s\n", icon, road); }

static void maneuver_named(int idx, uint8_t type, int16_t angle, const char* road) {
    begin(0x5202);
    u16(MAN_TLV_INDEX, (uint16_t)idx);
    u8(MAN_TLV_TYPE, type);
    u8(MAN_TLV_JUNCTION_TYPE, 0);
    u16(MAN_TLV_EXIT_ANGLE, (uint16_t)angle);
    if (road) tlv(g_msg, &g_len, MAN_TLV_AFTER_ROAD_NAME, (const uint8_t*)road, strlen(road));
    send();
}

static void update_sourced(const char* source, int state, int first, int count, int dist, int lane, int showing) {
    begin(0x5201);
    u8(RGD_TLV_ROUTE_GUIDANCE_STATE, (uint8_t)state);
    if (source) tlv(g_msg, &g_len, RGD_TLV_SOURCE_NAME, (const uint8_t*)source, strlen(source));
    if (count >= 0) u16(RGD_TLV_MANEUVER_COUNT, (uint16_t)count);
    if (first >= 0) {
        uint8_t list[4];
        write_be16(list, (uint16_t)first); write_be16(list + 2, (uint16_t)(first + 1));
        tlv(g_msg, &g_len, RGD_TLV_MANEUVER_LIST, list, 4);
    } else if (count == 0) {
        tlv(g_msg, &g_len, RGD_TLV_MANEUVER_LIST, (const uint8_t*)"", 0);
    }
    if (dist >= 0) u32(RGD_TLV_DIST_TO_MANEUVER, (uint32_t)dist);
    if (lane >= 0) u16(RGD_TLV_LANE_GUIDANCE_INDEX, (uint16_t)lane);
    if (showing >= 0) u8(RGD_TLV_LANE_GUIDANCE_SHOWING, (uint8_t)showing);
    send();
}

int main(int argc, char** argv) {
    char path[512];
    int e;
    if (argc != 2) { fprintf(stderr, "usage: %s <out dir>\n", argv[0]); return 2; }
    g_dir = argv[1];
    snprintf(path, sizeof(path), "%s/steps", g_dir);
    g_steps = fopen(path, "w");
    if (!g_steps) { perror(path); return 1; }

    /* iOS order (CRAccNavController delegateUpdatedRouteGuidance): maneuvers,
     * then the lane window, then the 0x5201 that makes one of them current. */

    /* Route A.  iOS has no current lane event yet, so it sends the first window
     * (events 0..5) while the maneuvers are already well ahead (4, 5, ...). */
    /* Distance may precede the first authoritative list. */
    begin(0x5201); u8(RGD_TLV_ROUTE_GUIDANCE_STATE, 1);
    u32(RGD_TLV_DIST_TO_MANEUVER, 100); send();
    update(1, 4, -1, 0); expect(-1, 0);
    /* A prefetched secondary payload must not compact the current list.
     * Its empty primary slot carries a version so Java clears reused data. */
    maneuver(5);
    {
        int first = rgd_find_slot_for_iap_index_no_touch(4);
        int next = rgd_find_slot_for_iap_index_no_touch(5);
        char expected[64];
        assert(first >= 0 && next >= 0);
        assert(!(g_rgd.slot_cache[first].present & RGD_MAN_TYPE));
        snprintf(expected, sizeof(expected), "maneuver_list:s:%d,%d\n", first, next);
        assert(strstr(g_last_frame, expected));
        snprintf(expected, sizeof(expected), "m%d_ver:n:", first);
        assert(strstr(g_last_frame, expected));
    }
    /* Replacing the SAME identity must clear omitted fields just as a fresh
     * Java consumer would see them in the sticky snapshot. */
    begin(0x5202);
    u16(MAN_TLV_INDEX, 4); u8(MAN_TLV_TYPE, MAN_TYPE_RIGHT_TURN);
    u8(MAN_TLV_JUNCTION_TYPE, 0); u8(MAN_TLV_DRIVING_SIDE, 1);
    u16(MAN_TLV_EXIT_ANGLE, 90); u16(MAN_TLV_JUNCTION_ANGLES, 90);
    tlv(g_msg, &g_len, MAN_TLV_DESCRIPTION, (const uint8_t*)"Old turn", 8);
    tlv(g_msg, &g_len, MAN_TLV_AFTER_ROAD_NAME, (const uint8_t*)"Old road", 8);
    tlv(g_msg, &g_len, MAN_TLV_EXIT_INFO, (const uint8_t*)"Exit 7", 6);
    { uint8_t distance[4]; write_be32(distance, 6000);
      tlv(g_msg, &g_len, MAN_TLV_DISTANCE_BETWEEN, distance, sizeof(distance)); }
    send();
    expect_distance(100, 1);
    begin(0x5202); u16(MAN_TLV_INDEX, 5); u8(MAN_TLV_TYPE, MAN_TYPE_LEFT_TURN);
    u8(MAN_TLV_JUNCTION_TYPE, 0); send();
    /* New primary without distance must not inherit the old primary's 100 m. */
    update(1, 5, -1, 0); expect_distance(-1, 0);
    begin(0x5201); u32(RGD_TLV_DIST_TO_MANEUVER, 150); send(); expect_distance(150, 1);
    /* A tail-only list change still belongs to the same primary. */
    { uint8_t list[4]; write_be16(list, 5); write_be16(list + 2, 7);
      begin(0x5201); tlv(g_msg, &g_len, RGD_TLV_MANEUVER_LIST, list, sizeof(list)); send(); }
    expect_distance(150, 1);
    /* Empty list invalidates the previous owner, but a later independent
     * distance can precede the next nonempty list. */
    begin(0x5201); tlv(g_msg, &g_len, RGD_TLV_MANEUVER_LIST, (const uint8_t*)"", 0); send();
    expect_distance(-1, 0);
    begin(0x5201); u32(RGD_TLV_DIST_TO_MANEUVER, 120); send();
    update(1, 5, -1, 0); expect_distance(120, 1);
    /* Fresh distance in the head-changing packet wins over invalidation. */
    { uint8_t list[4]; write_be16(list, 4); write_be16(list + 2, 5);
      begin(0x5201); tlv(g_msg, &g_len, RGD_TLV_MANEUVER_LIST, list, sizeof(list));
      u32(RGD_TLV_DIST_TO_MANEUVER, 100); send(); }
    expect_distance(100, 1);
    begin(0x5202); u16(MAN_TLV_INDEX, 4); u8(MAN_TLV_TYPE, MAN_TYPE_RIGHT_TURN);
    tlv(g_msg, &g_len, MAN_TLV_JUNCTION_ANGLES, (const uint8_t*)"", 0);
    tlv(g_msg, &g_len, MAN_TLV_DESCRIPTION, (const uint8_t*)"", 0);
    send();
    assert(strstr(g_last_frame, "m0_junction_angles:s:\n"));
    maneuver(4); // type remains, geometry/text/step length disappear
    begin(0x5202); u16(MAN_TLV_INDEX, 4); send(); // type disappears too
    for (int m = 4; m < 10; m++) maneuver(m);
    for (e = 0; e < 6; e++) lane_event(e, e);
    expect(-1, 0);
    /* Junction after junction: lanes show, hide between junctions, and only
     * the event the window just reached is new.  Maneuvers run 2 ahead per
     * junction, so lane events drop further and further below them. */
    for (e = 0; e < 45; e++) {
        int first = 4 + 2 * e;
        maneuver(first + 6); maneuver(first + 7);
        if (e > 0) lane_event(e + 5, e + 5);        /* the window's new tail */
        update(1, first, e, 1); expect(e, e);
        update(1, first + 1, e, 0); expect(-1, 0);
    }

    /* Route C: a reroute (no reset, so iOS keeps its sent-cache) whose new lane
     * source is numbered from 0 again.  Its window arrives while the old current
     * event (44) is still active and must survive until its own 0x5201; later
     * the new events 44..49 replace route A's events with the same numbers. */
    update(5, 200, 44, 0); expect(-1, 0);
    for (e = 0; e < 6; e++) lane_event(e, 300 + e);
    for (e = 0; e < 45; e++) {
        int first = 200 + 2 * e;
        maneuver(first + 6); maneuver(first + 7);
        if (e > 0) lane_event(e + 5, 300 + e + 5);
        update(1, first, e, 1); expect(e, 300 + e);
        update(1, first + 1, e, 0); expect(-1, 0);
    }

    /* Route B: a new route (reset frame) numbers its lane events from 0 again.
     * Until its own event 0 arrives nothing may show, least of all an old one. */
    update(0, 0, -1, 0);
    update(1, 0, 0, 1); expect(-1, 0);
    lane_event(0, 100); expect(0, 100);
    lane_event(1, 101);
    /* lane data without a lane index must not land on any event, in particular
     * not on the event numbered like the first maneuver (1) */
    update(1, 1, 0, 1); lane_event_without_index(77); expect(0, 100);
    update(1, 1, 1, 1); expect(1, 101);

    /* Route D (Apple Maps cold start: 0x5204 -> 0x5202 -> 0x5201 with !have_update).
     * CarClusterNavigationController sends lanes and up to 16 maneuvers before
     * the very first 0x5201; none may be dropped or emit headless frames. */
    rgd_clear_state("apple_cold_start");
    {
        int before = g_frames;
        for (e = 0; e < 3; e++) lane_event(e, 400 + e);
        maneuver_named(0, MAN_TYPE_LEFT_TURN, -90, "AppleColdRd");
        for (int m = 1; m < 16; m++) maneuver_named(m, MAN_TYPE_RIGHT_TURN, 90, "AppleNextRd");
        assert(g_frames == before); /* staged silently until 0x5201 arrives */
        update_sourced("Maps", 1, 0, 16, 400, 0, 1);
        expect(0, 400);
        expect_maneuver(1, "AppleColdRd");
    }

    /* Route E (Google Maps -> resetActiveComponents state=0 without source_name ->
     * Apple Maps 0x5202 + 0x5204 -> 0x5201 LOADING -> 0x5201 ROUTE_SET). */
    update_sourced("Google Maps", 1, 0, 2, 300, -1, 0);
    maneuver_named(0, MAN_TYPE_RIGHT_TURN, 90, "GoogleRd");
    maneuver_named(1, MAN_TYPE_LEFT_TURN, -90, "GoogleRd2");
    expect_maneuver(1, "GoogleRd");
    update(0, 0, -1, 0); /* CRAccNavProvider.resetActiveComponents omits source_name */
    for (int m = 0; m < 16; m++)
        maneuver_named(m, m == 0 ? MAN_TYPE_LEFT_TURN : MAN_TYPE_RIGHT_TURN, m == 0 ? -90 : 90,
                       m == 0 ? "AppleSwitchRd" : "AppleLaterRd");
    for (e = 0; e < 3; e++) lane_event(e, 500 + e);
    update_sourced("Maps", 3, -1, 0, -1, -1, 0); /* LOADING with source_changed */
    update_sourced("Maps", 1, 0, 16, 250, 0, 1); /* ROUTE_SET activates pre-sent batch */
    expect(0, 500);
    expect_maneuver(1, "AppleSwitchRd");

    /* Route F (Hot provider switch without state=0: Maps active at [10,11] ->
     * Yandex sends 0x5202(0..3) + 0x5204(0..1) -> 0x5201(source="Yandex Maps")). */
    maneuver_named(10, MAN_TYPE_RIGHT_TURN, 90, "MapsOld10");
    maneuver_named(11, MAN_TYPE_RIGHT_TURN, 90, "MapsOld11");
    update_sourced("Maps", 1, 10, 16, 200, 2, 1);
    expect(2, 502);
    for (int m = 0; m < 4; m++)
        maneuver_named(m, MAN_TYPE_LEFT_TURN, -90, m == 0 ? "YandexHotRd" : "YandexNextRd");
    lane_event(0, 600); lane_event(1, 601);
    update_sourced("Yandex Maps", 1, 0, 4, 350, 0, 1);
    assert(rgd_find_slot_for_iap_index_no_touch(10) < 0); /* stale provider wiped */
    expect(0, 600);
    expect_maneuver(1, "YandexHotRd");

    /* Route G (Same-app route change renumbering to 0 without state=0 or REROUTING:
     * active list is [2,3] at lane=1, new route's 0x5202(0,1) + 0x5204(0) arrive
     * before 0x5201([0,1], lane=0)). */
    update_sourced("Yandex Maps", 1, 2, 4, 180, 1, 1);
    expect(1, 601);
    maneuver_named(0, MAN_TYPE_RIGHT_TURN, 90, "RenumberedRd");
    maneuver_named(1, MAN_TYPE_LEFT_TURN, -90, "RenumberedRd2");
    lane_event(0, 700);
    update_sourced("Yandex Maps", 1, 0, 2, 500, 0, 1);
    expect(0, 700);
    expect_maneuver(1, "RenumberedRd");

    /* Route H (Google Maps order: 0x5201 -> 0x5202 -> 0x5204) and
     * Route I (Interleaved order: 0x5202 -> 0x5201 -> 0x5204). */
    rgd_clear_state("google_order");
    update_sourced("Google Maps", 3, -1, 0, -1, -1, 0);
    update_sourced("Google Maps", 1, 0, 2, 320, 0, 1);
    expect(-1, 0);
    maneuver_named(0, MAN_TYPE_LEFT_TURN, -90, "GoogleLateManRd");
    maneuver_named(1, MAN_TYPE_RIGHT_TURN, 90, "GoogleLateManRd2");
    expect_maneuver(1, "GoogleLateManRd");
    lane_event(0, 800);
    expect(0, 800);

    rgd_clear_state("interleaved_order");
    maneuver_named(0, MAN_TYPE_RIGHT_TURN, 90, "InterleavedRd");
    update_sourced("2GIS", 1, 0, 1, 210, 0, 1);
    expect(-1, 0);
    expect_maneuver(1, "InterleavedRd");
    lane_event(0, 900);
    expect(0, 900);

    fclose(g_steps);
    printf("native: %d bus frames\n", g_frames);
    return 0;
}
