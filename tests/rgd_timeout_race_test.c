/* Real RGI handler/timer with an active iAP2 update racing the timer's bus
 * publication. Only framework/socket leaves are replaced. */
#include "routeguidance/rgd_hook.c"
#include <assert.h>
#include <errno.h>

static hook_context_t ctx;
hook_context_t* hook_framework_get_context(void) { return &ctx; }
bool hook_is_ready(void) { return false; }
hook_result_t hook_inject_message(uint16_t m, const uint8_t* p, size_t n) {
    (void)m; (void)p; (void)n; return HOOK_OK;
}
uint8_t* hook_identify_grow(hook_context_t* c, size_t l, size_t n, size_t* cap) {
    (void)c; (void)l; (void)n; (void)cap; return NULL;
}

static pthread_mutex_t capture_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t capture_cond = PTHREAD_COND_INITIALIZER;
static pthread_t active_thread;
static bool race_next_zero, active_started, active_done;
static int frames, last_state;
static char last_frame[8192];

static void update_byte(uint16_t key, uint8_t value) {
    uint8_t raw[11] = {0x40, 0x40, 0, 11, 0x52, 0x01, 0, 5, 0, 1, 0};
    write_be16(raw + 8, key);
    raw[10] = value;
    iap2_frame_t frame;
    memset(&frame, 0, sizeof(frame)); frame.msgid = 0x5201; frame.frame_len = sizeof(raw);
    ctx.direction = MSG_DIR_INCOMING; ctx.raw_buf = raw; ctx.raw_len = sizeof(raw);
    rgd_message_handler(&ctx, &frame);
}

static void route_state(uint8_t state) { update_byte(RGD_TLV_ROUTE_GUIDANCE_STATE, state); }

static void* activate(void* unused) {
    (void)unused;
    pthread_mutex_lock(&capture_lock);
    active_started = true; pthread_cond_broadcast(&capture_cond);
    pthread_mutex_unlock(&capture_lock);
    route_state(1);
    pthread_mutex_lock(&capture_lock);
    active_done = true; pthread_cond_broadcast(&capture_cond);
    pthread_mutex_unlock(&capture_lock);
    return NULL;
}

hook_result_t bus_send_text(uint16_t type, uint8_t flags, bus_text_builder_t* b) {
    char text[8192];
    (void)type; (void)flags;
    assert(!b->overflow && b->len < sizeof(text));
    memcpy(text, b->buf, b->len); text[b->len] = '\0';
    bool zero = strstr(text, "route_state:n:0\n") != NULL;
    if (zero && race_next_zero) {
        race_next_zero = false;
        assert(pthread_create(&active_thread, NULL, activate, NULL) == 0);
        pthread_mutex_lock(&capture_lock);
        while (!active_started) pthread_cond_wait(&capture_cond, &capture_lock);
        /* On the old implementation activation overtakes this paused stop.
         * A correctly serialized timer makes activation wait until the stop
         * is published. Bound the wait so that both orders can finish. */
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec++;
        while (!active_done) {
            int rc = pthread_cond_timedwait(&capture_cond, &capture_lock, &deadline);
            if (rc == ETIMEDOUT) break;
            assert(rc == 0);
        }
        pthread_mutex_unlock(&capture_lock);
    }
    pthread_mutex_lock(&capture_lock);
    memcpy(last_frame, text, b->len + 1);
    if (zero) last_state = 0;
    else if (strstr(text, "route_state:n:1\n")) last_state = 1;
    frames++;
    pthread_mutex_unlock(&capture_lock);
    bus_text_free(b);
    return HOOK_OK;
}

static void expire_pending(void) {
    pthread_mutex_lock(&g_rgd_debounce_lock);
    assert(g_rgd.state_zero_started_ms != 0);
    g_rgd.state_zero_deadline_ms = 1; // deterministic clock advance, no grace-period sleep
    pthread_mutex_unlock(&g_rgd_debounce_lock);
}

static void source_name(const char* source) {
    uint8_t raw[256] = {0x40, 0x40, 0, 0, 0x52, 0x01};
    size_t len = strlen(source);
    assert(len < sizeof(raw) - 10);
    write_be16(raw + 2, 10 + len);
    write_be16(raw + 6, 4 + len);
    write_be16(raw + 8, RGD_TLV_SOURCE_NAME);
    memcpy(raw + 10, source, len);
    iap2_frame_t frame;
    memset(&frame, 0, sizeof(frame)); frame.msgid = 0x5201; frame.frame_len = 10 + len;
    ctx.direction = MSG_DIR_INCOMING; ctx.raw_buf = raw; ctx.raw_len = frame.frame_len;
    rgd_message_handler(&ctx, &frame);
}

static void maneuver_type(uint16_t idx, uint8_t type, const char* road) {
    uint8_t raw[256] = {0x40, 0x40, 0, 0, 0x52, 0x02};
    size_t off = 6;
    write_be16(raw + off, 6); write_be16(raw + off + 2, MAN_TLV_INDEX); write_be16(raw + off + 4, idx); off += 6;
    write_be16(raw + off, 5); write_be16(raw + off + 2, MAN_TLV_TYPE); raw[off + 4] = type; off += 5;
    if (road) {
        size_t rlen = strlen(road);
        write_be16(raw + off, (uint16_t)(4 + rlen));
        write_be16(raw + off + 2, MAN_TLV_AFTER_ROAD_NAME);
        memcpy(raw + off + 4, road, rlen);
        off += 4 + rlen;
    }
    write_be16(raw + 2, (uint16_t)off);
    iap2_frame_t frame;
    memset(&frame, 0, sizeof(frame)); frame.msgid = 0x5202; frame.frame_len = (uint16_t)off;
    ctx.direction = MSG_DIR_INCOMING; ctx.raw_buf = raw; ctx.raw_len = off;
    rgd_message_handler(&ctx, &frame);
}

static void source_with_route(const char* source, uint8_t state, uint16_t first_idx) {
    uint8_t raw[256] = {0x40, 0x40, 0, 0, 0x52, 0x01};
    size_t off = 6, len = strlen(source);
    write_be16(raw + off, 5); write_be16(raw + off + 2, RGD_TLV_ROUTE_GUIDANCE_STATE); raw[off + 4] = state; off += 5;
    write_be16(raw + off, (uint16_t)(4 + len)); write_be16(raw + off + 2, RGD_TLV_SOURCE_NAME);
    memcpy(raw + off + 4, source, len); off += 4 + len;
    if (state > 0) {
        write_be16(raw + off, 6); write_be16(raw + off + 2, RGD_TLV_MANEUVER_COUNT); write_be16(raw + off + 4, 1); off += 6;
        write_be16(raw + off, 6); write_be16(raw + off + 2, RGD_TLV_MANEUVER_LIST); write_be16(raw + off + 4, first_idx); off += 6;
    }
    write_be16(raw + 2, (uint16_t)off);
    iap2_frame_t frame;
    memset(&frame, 0, sizeof(frame)); frame.msgid = 0x5201; frame.frame_len = (uint16_t)off;
    ctx.direction = MSG_DIR_INCOMING; ctx.raw_buf = raw; ctx.raw_len = off;
    rgd_message_handler(&ctx, &frame);
}

static void source_lifecycle(void) {
    rgd_clear_state("source test");
    route_state(1);
    assert(strstr(last_frame, "source_name:s:\n"));
    g_rgd.slot_cache[0].present = RGD_MAN_TYPE;
    g_rgd.slot_cache[0].maneuver_type = 31;
    uint64_t generation = g_rgd.route_generation;
    source_name("百度地图");
    assert(g_rgd.route_generation == generation && g_rgd.slot_cache[0].present == RGD_MAN_TYPE);
    assert(strstr(last_frame, "source_name:s:百度地图\n"));
    source_name("百度地图");
    assert(g_rgd.route_generation == generation && g_rgd.slot_cache[0].present == RGD_MAN_TYPE);
    route_state(1); // Source omitted from this wire packet, retained in snapshot.
    assert(strstr(last_frame, "source_name:s:百度地图\n"));
    g_rgd.lane_cache[0].present = RGD_LANE_GUIDANCE_INDEX;
    source_name("Yandex Navi");
    assert(g_rgd.route_generation > generation);
    assert(g_rgd.slot_cache[0].present == 0 && g_rgd.lane_cache[0].present == 0);
    assert(strstr(last_frame, "source_name:s:Yandex Navi\n"));
    route_state(0); // Soft reset: source is session metadata, not route geometry.
    assert(g_rgd.update_cache.present & RGD_UPD_SOURCE_NAME);
    assert(strcmp(g_rgd.update_cache.source_name, "Yandex Navi") == 0);
    route_state(1);
    assert(strstr(last_frame, "source_name:s:Yandex Navi\n"));
    generation = g_rgd.route_generation;
    source_name("");
    assert(g_rgd.route_generation > generation && strstr(last_frame, "source_name:s:\n"));
    source_name("百度地图");
    route_state(0); // A source change must cancel the old provider's pending timeout.
    source_name("Yandex Navi");
    assert(g_rgd.state_zero_started_ms == 0);
    assert(strstr(last_frame, "source_name:s:Yandex Navi\n"));
    rgd_clear_state("source disconnect"); route_state(1);
    assert(strstr(last_frame, "source_name:s:\n"));
    route_state(0); source_name("百度地图"); route_state(1);
    assert(strstr(last_frame, "source_name:s:百度地图\n"));
    g_rgd.slot_cache[0].present = RGD_MAN_TYPE;
    g_rgd.lane_cache[0].present = RGD_LANE_GUIDANCE_INDEX;
    generation = g_rgd.route_generation;
    update_byte(RGD_TLV_SOURCE_SUPPORTS_RG, 0); // No route_state in the phone packet.
    assert(strstr(last_frame, "source_name:s:\n"));
    assert(!(g_rgd.update_cache.present & RGD_UPD_SOURCE_NAME));
    assert(g_rgd.route_generation > generation && last_state == 0);
    assert(g_rgd.slot_cache[0].present == 0 && g_rgd.lane_cache[0].present == 0);

    // iOS CRAccNavProvider resetActiveComponents (state=0 without source_name)
    // followed by Apple Maps 0x5202 burst BEFORE 0x5201(source="Maps") must keep
    // the new provider's maneuvers while bumping route_generation.
    rgd_clear_state("google_to_apple");
    maneuver_type(0, MAN_TYPE_RIGHT_TURN, "Google St");
    source_with_route("Google Maps", 1, 0);
    assert(strstr(last_frame, "source_name:s:Google Maps\n"));
    assert(strstr(last_frame, "m0_after_road:s:Google St\n"));
    route_state(0); // CRAccNavProvider.resetActiveComponents omits source_name
    maneuver_type(0, MAN_TYPE_LEFT_TURN, "Apple St");
    generation = g_rgd.route_generation;
    source_with_route("Maps", 3, 0); // LOADING first, then ROUTE_SET
    assert(g_rgd.route_generation > generation);
    source_with_route("Maps", 1, 0);
    assert(strstr(last_frame, "source_name:s:Maps\n"));
    assert(strstr(last_frame, "m0_type:n:1\n"));
    assert(strstr(last_frame, "m0_after_road:s:Apple St\n"));

    // Orphaned pre-activation 0x5202 followed by explicit state=0 must be wiped.
    rgd_clear_state("orphan_pre_5201");
    int before_frames = frames;
    maneuver_type(0, MAN_TYPE_LEFT_TURN, "Orphan Rd");
    assert(frames == before_frames); // No headless snapshot before 0x5201
    route_state(0);
    assert(rgd_find_slot_for_iap_index_no_touch(0) < 0);

    puts("RGI sources: real TLV -> cached snapshot, late source, provider switch, pre-5201 Apple Maps handoff, soft reset, explicit empty and disconnect PASS");
}

int main(void) {
    route_state(1); route_state(0); expire_pending();
    race_next_zero = true;
    rgd_periodic_tick();
    pthread_join(active_thread, NULL);
    assert(g_rgd.emitted_route_state == 1 && last_state == 1);

    // Activation winning before the timeout cancels it without publishing zero.
    route_state(0); expire_pending(); route_state(1);
    int before = frames; rgd_periodic_tick(); assert(frames == before && last_state == 1);

    // Genuine silence ends the route exactly once.
    route_state(0); expire_pending(); rgd_periodic_tick(); assert(last_state == 0);
    before = frames; rgd_periodic_tick(); assert(frames == before);

    // A reset while rerouting retains its no-timeout policy.
    route_state(5); route_state(0); expire_pending();
    before = frames; rgd_periodic_tick(); assert(frames == before);
    puts("RGI timeout: concurrent activation wins final publication, cancellation, genuine end and reroute hold PASS");
    source_lifecycle();
    return 0;
}
