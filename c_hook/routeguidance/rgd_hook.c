/*
 * Route Guidance Hook Module Implementation
 */

#include "rgd_hook.h"
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

DEFINE_LOG_MODULE(RGD);

/* ================================================================
 * Cluster renderer process management
 * ================================================================ */

#define CR_BINARY_PATH  "/mnt/app/root/hooks/maneuver_render"

static pid_t g_renderer_pid = -1;

static void renderer_kill_previous(void) {
    /* Kill any orphaned maneuver_render from a previous hook load.
     * slay -f = force (SIGKILL), -Q = quiet (no error if not found). */
    if (g_renderer_pid > 0) {
        kill(g_renderer_pid, SIGKILL);
        waitpid(g_renderer_pid, NULL, 0);
        g_renderer_pid = -1;
        LOG_INFO(LOG_MODULE, "renderer: killed tracked pid");
    }
    /* Also slay by name in case PID was lost (hook reload, crash, etc.) */
    system("slay -f -Q maneuver_render 2>/dev/null");
}

static void renderer_start(void) {
    renderer_kill_previous();

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR(LOG_MODULE, "renderer: fork failed: %s", strerror(errno));
        return;
    }

    if (pid == 0) {
        /* Child process */
        execl(CR_BINARY_PATH, "maneuver_render", (char *)NULL);
        /* If exec fails, try relative path (same folder as hook .so) */
        execl("./maneuver_render", "maneuver_render", (char *)NULL);
        _exit(127);
    }

    g_renderer_pid = pid;
    LOG_INFO(LOG_MODULE, "renderer: started pid=%d", (int)pid);

    /* Brief wait to detect immediate exec failure (child exits with 127). */
    usleep(50000); /* 50ms */
    int wst = 0;
    pid_t ret = waitpid(pid, &wst, WNOHANG);
    if (ret == pid) {
        g_renderer_pid = -1;
        LOG_ERROR(LOG_MODULE, "renderer: exec failed (exit %d), continuing without render",
                  WIFEXITED(wst) ? WEXITSTATUS(wst) : -1);
    }
}

static void renderer_stop(void) {
    if (g_renderer_pid <= 0) return;

    /* Send TCP CMD_SHUTDOWN (48-byte packet, protocol.h) for graceful exit.
     * Java's RendererClient handles primary shutdown; this is the C-side backstop. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(19800);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            uint8_t pkt[48];
            memset(pkt, 0, sizeof(pkt));
            pkt[0] = 0x03; /* CMD_SHUTDOWN */
            send(sock, pkt, sizeof(pkt), 0);
        }
        close(sock);
    }

    /* Give it a moment to exit gracefully */
    usleep(200000); /* 200ms */

    /* Check if still running, force kill if needed */
    int status;
    pid_t ret = waitpid(g_renderer_pid, &status, WNOHANG);
    if (ret == 0) {
        /* Still running, send SIGTERM */
        kill(g_renderer_pid, SIGTERM);
        usleep(100000);
        ret = waitpid(g_renderer_pid, &status, WNOHANG);
        if (ret == 0) {
            kill(g_renderer_pid, SIGKILL);
            waitpid(g_renderer_pid, &status, 0);
        }
    }

    LOG_INFO(LOG_MODULE, "renderer: stopped pid=%d", (int)g_renderer_pid);
    g_renderer_pid = -1;
}

/* Module state */
static struct {
    pps_handle_t* pps;
    bool active;
    bool sent_5200;
    bool sent_5203;
    bool got_520x;
    uint16_t component_id;
    bool component_valid;

    /* MHI3-like maneuver storage mapping:
     * iOS uses monotonically increasing maneuver indexes (can exceed small caches).
     * The MHI3 stack stores a bounded vector and drops oldest when capacity is exceeded.
     *
     * We can't expose arbitrary-key maps through PPS easily, so we remap iOS maneuver indexes
     * to fixed slots [0..MANEUVER_CACHE_SIZE-1] and publish maneuver_list as slot order.
     */
	    bool have_update;
	    uint8_t last_route_state;
	    uint16_t current_list[MAX_MANEUVER_LIST];
	    bool current_list_present; /* true if 0x5201 included ManeuverList TLV (can be empty) */
	    uint16_t current_list_count;
	    uint16_t slot_to_iap_idx[MANEUVER_CACHE_SIZE]; /* 0xFFFF = empty */
	    uint32_t slot_seq[MANEUVER_CACHE_SIZE];        /* allocation order for eviction */
	    uint32_t seq_counter;
	    /*
	     * Per-slot maneuver data cache.
	     * QNX PPS delta delivery can drop intermediate writes during rapid
	     * 0x5202 bursts, causing Java to miss per-slot data.  Cache the
	     * last 0x5202 data per slot and re-publish it inside the 0x5201
	     * PPS write when maneuver_list is present.  This ensures Java
	     * gets slot data atomically with the maneuver_list.
	     */
	    rgd_maneuver_t slot_cache[MANEUVER_CACHE_SIZE];
	    /* Last merged 0x5201 snapshot (used to make PPS writes full-state). */
	    rgd_update_t update_cache;
		} g_rgd = {
    .pps = NULL,
    .active = false,
    .sent_5200 = false,
    .sent_5203 = false,
    .got_520x = false,
    .component_id = 0x0010,
    .component_valid = false,
	    .have_update = false,
	    .last_route_state = 0,
	    .current_list_present = false,
	    .current_list_count = 0,
	    .seq_counter = 0
	};

/* Forward declarations */
static bool rgd_message_handler(hook_context_t* ctx, const iap2_frame_t* frame);
static size_t rgd_identify_patcher(hook_context_t* ctx, uint8_t* buf, size_t len, size_t max_len);
static void rgd_state_handler(hook_context_t* ctx, int event, void* event_data);
static void rgd_transport_handler(hook_context_t* ctx, uint16_t msgid);
static void write_pps_update_partial(const rgd_update_t* upd);
static void write_pps_maneuver_partial(const rgd_maneuver_t* man);
static void write_pps_lane_guidance_partial(const rgd_lane_guidance_t* lane);
static void write_pps_snapshot_from_cache(int extra_slot, const rgd_maneuver_t* extra_man,
                                          const rgd_update_t* current_upd);
static void write_slot_data_keys(unsigned slot, const rgd_maneuver_t* man);
static void rgd_lazy_init(void);

static void rgd_update_cache_reset(void) {
    memset(&g_rgd.update_cache, 0, sizeof(g_rgd.update_cache));
}

static void rgd_update_cache_merge(const rgd_update_t* upd) {
    rgd_update_t* c = &g_rgd.update_cache;
    if (!upd) return;

    if (upd->present & RGD_UPD_COMPONENT_IDS) {
        c->component_count = upd->component_count;
        if (c->component_count > MAX_COMPONENT_LIST) c->component_count = MAX_COMPONENT_LIST;
        memcpy(c->component_ids, upd->component_ids, sizeof(c->component_ids));
    }
    if (upd->present & RGD_UPD_ROUTE_STATE) c->route_state = upd->route_state;
    if (upd->present & RGD_UPD_MANEUVER_STATE) c->maneuver_state = upd->maneuver_state;
    if (upd->present & RGD_UPD_CURRENT_ROAD) memcpy(c->current_road, upd->current_road, sizeof(c->current_road));
    if (upd->present & RGD_UPD_DESTINATION) memcpy(c->destination, upd->destination, sizeof(c->destination));
    if (upd->present & RGD_UPD_ETA) c->eta = upd->eta;
    if (upd->present & RGD_UPD_TIME_REMAINING) c->time_remaining = upd->time_remaining;
    if (upd->present & RGD_UPD_DISTANCE_REMAINING) c->distance_remaining = upd->distance_remaining;
    if (upd->present & RGD_UPD_DISTANCE_STRING) memcpy(c->distance_string, upd->distance_string, sizeof(c->distance_string));
    if (upd->present & RGD_UPD_DISTANCE_UNITS) c->distance_units = upd->distance_units;
    if (upd->present & RGD_UPD_DIST_TO_MANEUVER) c->dist_to_maneuver = upd->dist_to_maneuver;
    if (upd->present & RGD_UPD_DIST_TO_MANEUVER_STR) memcpy(c->dist_to_maneuver_string, upd->dist_to_maneuver_string, sizeof(c->dist_to_maneuver_string));
    if (upd->present & RGD_UPD_DIST_TO_MANEUVER_UNI) c->dist_to_maneuver_units = upd->dist_to_maneuver_units;
    if (upd->present & RGD_UPD_MANEUVER_COUNT) c->maneuver_count = upd->maneuver_count;
    if (upd->present & RGD_UPD_MANEUVER_LIST) {
        c->maneuver_list_count = upd->maneuver_list_count;
        if (c->maneuver_list_count > MAX_MANEUVER_LIST) c->maneuver_list_count = MAX_MANEUVER_LIST;
        memcpy(c->maneuver_list, upd->maneuver_list, sizeof(c->maneuver_list));
    }
    if (upd->present & RGD_UPD_VISIBLE_IN_APP) c->visible_in_app = upd->visible_in_app;
    if (upd->present & RGD_UPD_LANE_INDEX) c->lane_guidance_index = upd->lane_guidance_index;
    if (upd->present & RGD_UPD_LANE_TOTAL) c->lane_guidance_total = upd->lane_guidance_total;
    if (upd->present & RGD_UPD_LANE_SHOWING) c->lane_guidance_showing = upd->lane_guidance_showing;
    if (upd->present & RGD_UPD_SOURCE_NAME) memcpy(c->source_name, upd->source_name, sizeof(c->source_name));
    if (upd->present & RGD_UPD_SOURCE_SUPPORTS_RG) c->source_supports_route_guidance = upd->source_supports_route_guidance;

    c->present |= upd->present;
}

		static void rgd_maneuver_map_reset(void) {
		    g_rgd.current_list_present = false;
		    g_rgd.current_list_count = 0;
		    g_rgd.seq_counter = 0;
	    for (int i = 0; i < MANEUVER_CACHE_SIZE; i++) {
	        g_rgd.slot_to_iap_idx[i] = 0xFFFF;
	        g_rgd.slot_seq[i] = 0;
	        g_rgd.slot_cache[i].present = 0;
		    }
		}

static const uint64_t RGD_UPD_WRITE_MASK = RGD_UPD_ROUTE_STATE |
                                            RGD_UPD_MANEUVER_STATE |
                                            RGD_UPD_DISTANCE_REMAINING |
                                            RGD_UPD_DIST_TO_MANEUVER |
                                            RGD_UPD_ETA |
                                            RGD_UPD_TIME_REMAINING |
                                            RGD_UPD_CURRENT_ROAD |
                                            RGD_UPD_DESTINATION |
                                            RGD_UPD_DISTANCE_UNITS |
                                            RGD_UPD_DIST_TO_MANEUVER_UNI |
                                            RGD_UPD_DISTANCE_STRING |
                                            RGD_UPD_DIST_TO_MANEUVER_STR |
                                            RGD_UPD_LANE_INDEX |
                                            RGD_UPD_LANE_TOTAL |
                                            RGD_UPD_LANE_SHOWING |
                                            RGD_UPD_SOURCE_NAME |
                                            RGD_UPD_SOURCE_SUPPORTS_RG |
                                            RGD_UPD_VISIBLE_IN_APP |
                                            RGD_UPD_COMPONENT_IDS |
                                            RGD_UPD_MANEUVER_COUNT |
                                            RGD_UPD_MANEUVER_LIST;

static int rgd_min_current_index(void) {
    if (g_rgd.current_list_count == 0) return -1;
    uint16_t min = g_rgd.current_list[0];
    for (uint16_t i = 1; i < g_rgd.current_list_count; i++) {
        if (g_rgd.current_list[i] < min) min = g_rgd.current_list[i];
    }
    return (int)min;
}

	static bool rgd_can_process_maneuver_index(uint16_t idx) {
	    /* Mirror MHI3 isRouteGuidanceManeuverIndexGreater semantics (simplified):
	     * - if no route update exists => reject
	     * - if current list is null (ManeuverList TLV not present) => accept
	     * - if current list is empty => accept (was: reject - but transient
	     *   state=0 clears the list before 0x5202 data arrives for the new
	     *   route; rejecting here permanently loses that data since iOS never
	     *   resends 0x5202 for already-sent indices)
	     * - if route_state == REROUTING => accept all
	     * - else accept if idx >= min(current_list)
	     */
	    if (!g_rgd.have_update) return false;
	    if (!g_rgd.current_list_present) return true;
	    if (g_rgd.current_list_count == 0) return true;
	    if (g_rgd.last_route_state == RGD_STATE_REROUTING) return true;
	    int min = rgd_min_current_index();
	    if (min < 0) return false;
	    return (int)idx >= min;
	}

/*
 * Helper: check if a lane guidance index has a corresponding cached maneuver.
 * iOS uses the same index space for maneuvers and lane guidance -- lane idx N
 * corresponds to maneuver idx N. Accept 0x5204 updates when the maneuver
 * exists in our slot cache, even if the index is below the current
 * ManeuverList minimum (which may be stale from a previous route).
 */
static bool rgd_has_cached_maneuver(uint16_t idx) {
    for (int s = 0; s < MANEUVER_CACHE_SIZE; s++) {
        if (g_rgd.slot_to_iap_idx[s] == idx) return true;
    }
    return false;
}

/*
 * Helper: check if an iAP2 index is in the active ManeuverList.
 * Slots for active maneuvers must never be evicted.
 */
static bool rgd_is_active_index(uint16_t iap_idx) {
    if (!g_rgd.current_list_present) return false;
    for (uint16_t i = 0; i < g_rgd.current_list_count; i++) {
        if (g_rgd.current_list[i] == iap_idx) return true;
    }
    return false;
}

static int rgd_slot_for_iap_index(uint16_t idx, bool create) {
    /* Find existing mapping - touch seq (LRU refresh) so actively
     * used slots don't become eviction victims. */
    for (int s = 0; s < MANEUVER_CACHE_SIZE; s++) {
        if (g_rgd.slot_to_iap_idx[s] == idx) {
            g_rgd.slot_seq[s] = ++g_rgd.seq_counter;
            return s;
        }
    }
    if (!create) return -1;

    /* Find free slot */
    for (int s = 0; s < MANEUVER_CACHE_SIZE; s++) {
        if (g_rgd.slot_to_iap_idx[s] == 0xFFFF) {
            g_rgd.slot_to_iap_idx[s] = idx;
            g_rgd.slot_seq[s] = ++g_rgd.seq_counter;
            return s;
        }
    }

    /* Evict oldest slot, but NEVER evict slots in the active ManeuverList.
     * Without this protection, incoming 0x5202 data for future maneuvers
     * can evict slots still being displayed on the cluster, causing the
     * icon to get stuck on a stale maneuver direction. */
    int victim = -1;
    uint32_t best = UINT32_MAX;
    for (int s = 0; s < MANEUVER_CACHE_SIZE; s++) {
        if (rgd_is_active_index(g_rgd.slot_to_iap_idx[s])) continue;
        if (g_rgd.slot_seq[s] < best) {
            best = g_rgd.slot_seq[s];
            victim = s;
        }
    }
    if (victim < 0) {
        /* All slots are in the active list (shouldn't happen with 8 slots
         * and max 2-3 active maneuvers).  Fall back to true LRU. */
        best = UINT32_MAX;
        for (int s = 0; s < MANEUVER_CACHE_SIZE; s++) {
            if (g_rgd.slot_seq[s] < best) {
                best = g_rgd.slot_seq[s];
                victim = s;
            }
        }
    }
    g_rgd.slot_to_iap_idx[victim] = idx;
    g_rgd.slot_seq[victim] = ++g_rgd.seq_counter;
    memset(&g_rgd.slot_cache[victim], 0, sizeof(g_rgd.slot_cache[victim]));
    return victim;
}

static void rgd_update_session_active(void) {
    hook_context_t* ctx = hook_framework_get_context();
    if (ctx) {
        ctx->session_active = (g_rgd.active || g_rgd.got_520x);
    }
}

static uint8_t* rgd_resize_identify_buffer(hook_context_t* ctx, size_t len, size_t new_len, size_t* out_cap) {
    if (!ctx || !ctx->_priv) return NULL;
    void* out_array = ctx->_priv;
    uint8_t* base = *(uint8_t**)((char*)out_array + 0);
    unsigned int cap = *(unsigned int*)((char*)out_array + 8);

    if (cap >= new_len) {
        if (out_cap) *out_cap = cap;
        return base;
    }

    size_t alloc = new_len + 64;
    uint8_t* new_buf = (uint8_t*)realloc(base, alloc);
    if (!new_buf) {
        new_buf = (uint8_t*)malloc(alloc);
        if (!new_buf) return NULL;
        if (base && len > 0) {
            memcpy(new_buf, base, len);
        }
        if (base) {
            free(base);
        }
    }

    *(uint8_t**)((char*)out_array + 0) = new_buf;
    *(unsigned int*)((char*)out_array + 8) = (unsigned int)alloc;
    if (out_cap) *out_cap = alloc;
    return new_buf;
}

/* Message filter */
static uint16_t rgd_msg_filter[] = {
    IAP2_MSG_ROUTE_GUIDANCE_UPDATE,
    IAP2_MSG_ROUTE_GUIDANCE_MANEUVER,
    IAP2_MSG_ROUTE_GUIDANCE_LANE
};

/* Module definition */
static hook_module_def_t rgd_module_def = {
    .name = "routeguidance",
    .priority = HOOK_PRIORITY_NORMAL,
    .msg_filter = rgd_msg_filter,
    .msg_filter_count = sizeof(rgd_msg_filter) / sizeof(rgd_msg_filter[0]),
    .on_message = rgd_message_handler,
    .on_identify = rgd_identify_patcher,
    .on_state = rgd_state_handler,
    .on_transport_send = rgd_transport_handler,
    .user_data = NULL
};

/* PPS snapshot writer */
static void write_pps_update_partial(const rgd_update_t* upd) {
    if (!g_rgd.pps || !upd) return;
    if (upd->present == 0) return;
    if ((upd->present & RGD_UPD_WRITE_MASK) == 0) return;
    rgd_update_cache_merge(upd);
    write_pps_snapshot_from_cache(-1, NULL, upd);
}

/*
 * Write per-slot maneuver keys into an OPEN PPS transaction.
 * Caller must have called pps_begin()/pps_write_header() before and
 * pps_end() after.  This allows embedding slot data inside the
 * route-update PPS write so Java gets maneuver_list and slot data
 * in a single atomic delta.
 */
static void write_slot_data_keys(unsigned idx, const rgd_maneuver_t* man) {
    char key[64];

    if (man->present & RGD_MAN_TYPE) {
        snprintf(key, sizeof(key), "m%u_type", idx);
        pps_write_int(g_rgd.pps, key, man->maneuver_type);
    }
    if (man->present & RGD_MAN_EXIT_ANGLE) {
        snprintf(key, sizeof(key), "m%u_turn_angle", idx);
        pps_write_int(g_rgd.pps, key, (int)man->exit_angle);
        snprintf(key, sizeof(key), "m%u_exit_angle", idx);
        pps_write_int(g_rgd.pps, key, (int)man->exit_angle);
    }
    if (man->present & RGD_MAN_JUNCTION_TYPE) {
        snprintf(key, sizeof(key), "m%u_junction_type", idx);
        pps_write_int(g_rgd.pps, key, man->junction_type);
    }
    if (man->present & RGD_MAN_DRIVING_SIDE) {
        snprintf(key, sizeof(key), "m%u_driving_side", idx);
        pps_write_int(g_rgd.pps, key, man->driving_side);
    }
    if (man->present & RGD_MAN_DISTANCE_BETWEEN) {
        snprintf(key, sizeof(key), "m%u_distance", idx);
        pps_write_uint(g_rgd.pps, key, man->distance_between);
    }
    if (man->present & RGD_MAN_DISTANCE_STRING) {
        snprintf(key, sizeof(key), "m%u_distance_str", idx);
        pps_write_string(g_rgd.pps, key, man->distance_string);
    }
    if (man->present & RGD_MAN_DISTANCE_UNITS) {
        snprintf(key, sizeof(key), "m%u_distance_units", idx);
        pps_write_int(g_rgd.pps, key, man->distance_units);
    }
    if (man->present & RGD_MAN_DESCRIPTION) {
        snprintf(key, sizeof(key), "m%u_name", idx);
        pps_write_string(g_rgd.pps, key, man->description);
    }
    if (man->present & RGD_MAN_AFTER_ROAD) {
        snprintf(key, sizeof(key), "m%u_after_road", idx);
        pps_write_string(g_rgd.pps, key, man->after_road_name);
    }
    if (man->present & RGD_MAN_JUNCTION_ANGLES) {
        char buf[128];
        int off = 0;
        for (int j = 0; j < man->junction_angle_count; j++)
            off += snprintf(buf + off, sizeof(buf) - off, "%s%d",
                            j > 0 ? "," : "", (int)man->junction_angles[j]);
        snprintf(key, sizeof(key), "m%u_junction_angles", idx);
        pps_write_string(g_rgd.pps, key, buf);
    }
    /* Slot version: bumps when a different iOS maneuver is assigned to this slot.
     * Java uses this to detect maneuver transitions even when type/angles are identical.
     * Cycles 1..65535 to bound PPS value; internal slot_seq stays monotonic for LRU. */
    snprintf(key, sizeof(key), "m%u_ver", idx);
    pps_write_uint(g_rgd.pps, key, (g_rgd.slot_seq[idx] % 65535) + 1);
    if (man->present & RGD_MAN_LANE_GUIDANCE_RAW) {
        snprintf(key, sizeof(key), "m%u_lane_guidance_len", idx);
        pps_write_int(g_rgd.pps, key, man->lane_guidance_raw_len);
        snprintf(key, sizeof(key), "m%u_lane_guidance_hex", idx);
        pps_write_string(g_rgd.pps, key, man->lane_guidance_hex);
    }
    if (man->present & RGD_MAN_LINKED_LANE_INDEX) {
        snprintf(key, sizeof(key), "m%u_linked_lane_guidance_index", idx);
        pps_write_int(g_rgd.pps, key, (int)man->linked_lane_guidance_index);
        {
            int linked_slot = rgd_slot_for_iap_index(man->linked_lane_guidance_index, false);
            if (linked_slot >= 0) {
                snprintf(key, sizeof(key), "m%u_linked_lane_guidance_slot", idx);
                pps_write_int(g_rgd.pps, key, linked_slot);
            }
        }
    }
    if (man->present & RGD_MAN_LANE_GUIDANCE) {
        snprintf(key, sizeof(key), "m%u_lane_count", idx);
        pps_write_int(g_rgd.pps, key, man->lane_count);

        {
            char buf[128];
            int boff = 0;
            for (int j = 0; j < man->lane_count && j < MAX_LANE_GUIDANCE; j++)
                boff += snprintf(buf + boff, sizeof(buf) - (size_t)boff, "%s%u",
                                 j > 0 ? "," : "", (unsigned)man->lanes[j].position);
            if (man->lane_count == 0) buf[0] = '\0';
            snprintf(key, sizeof(key), "m%u_lane_positions", idx);
            pps_write_string(g_rgd.pps, key, buf);
        }

        {
            char buf[128];
            int boff = 0;
            for (int j = 0; j < man->lane_count && j < MAX_LANE_GUIDANCE; j++)
                boff += snprintf(buf + boff, sizeof(buf) - (size_t)boff, "%s%d",
                                 j > 0 ? "," : "", (int)man->lanes[j].direction);
            if (man->lane_count == 0) buf[0] = '\0';
            snprintf(key, sizeof(key), "m%u_lane_directions", idx);
            pps_write_string(g_rgd.pps, key, buf);
        }

        {
            char buf[128];
            int boff = 0;
            for (int j = 0; j < man->lane_count && j < MAX_LANE_GUIDANCE; j++)
                boff += snprintf(buf + boff, sizeof(buf) - (size_t)boff, "%s%u",
                                 j > 0 ? "," : "", (unsigned)man->lanes[j].status);
            if (man->lane_count == 0) buf[0] = '\0';
            snprintf(key, sizeof(key), "m%u_lane_status", idx);
            pps_write_string(g_rgd.pps, key, buf);
        }

        {
            char buf[600];
            int boff = 0;
            for (int j = 0; j < man->lane_count && j < MAX_LANE_GUIDANCE; j++)
                boff += snprintf(buf + boff, sizeof(buf) - (size_t)boff, "%s%s",
                                 j > 0 ? "|" : "", man->lanes[j].description);
            if (man->lane_count == 0) buf[0] = '\0';
            snprintf(key, sizeof(key), "m%u_lane_desc", idx);
            pps_write_string(g_rgd.pps, key, buf);
        }

        /*
         * Full lane-angle vectors from 0x5204 lane informations.
         * Encoding:
         *   lane0 angles comma-separated, lanes separated by '|'
         * Example:
         *   "1000|40,20|"
         */
        {
            char buf[1200];
            int boff = 0;
            for (int j = 0; j < man->lane_count && j < MAX_LANE_GUIDANCE; j++) {
                if (j > 0) {
                    boff += snprintf(buf + boff, sizeof(buf) - (size_t)boff, "|");
                }
                for (int k = 0; k < man->lanes[j].angle_count && k < MAX_LANE_ANGLES; k++) {
                    boff += snprintf(buf + boff, sizeof(buf) - (size_t)boff, "%s%d",
                                     k > 0 ? "," : "", (int)man->lanes[j].angles[k]);
                }
            }
            if (man->lane_count == 0) buf[0] = '\0';
            snprintf(key, sizeof(key), "m%u_lane_angles", idx);
            pps_write_string(g_rgd.pps, key, buf);
        }
    } else if (man->present & RGD_MAN_LANE_GUIDANCE_RAW) {
        /* Keep lane keys coherent when only raw linked-lane payload is available. */
        snprintf(key, sizeof(key), "m%u_lane_count", idx);
        pps_write_int(g_rgd.pps, key, 0);
        snprintf(key, sizeof(key), "m%u_lane_positions", idx);
        pps_write_string(g_rgd.pps, key, "");
        snprintf(key, sizeof(key), "m%u_lane_directions", idx);
        pps_write_string(g_rgd.pps, key, "");
        snprintf(key, sizeof(key), "m%u_lane_status", idx);
        pps_write_string(g_rgd.pps, key, "");
        snprintf(key, sizeof(key), "m%u_lane_desc", idx);
        pps_write_string(g_rgd.pps, key, "");
        snprintf(key, sizeof(key), "m%u_lane_angles", idx);
        pps_write_string(g_rgd.pps, key, "");
    }
    if (man->present & RGD_MAN_EXIT_INFO_RAW) {
        snprintf(key, sizeof(key), "m%u_exit_info_len", idx);
        pps_write_int(g_rgd.pps, key, man->exit_info_raw_len);
        snprintf(key, sizeof(key), "m%u_exit_info_hex", idx);
        pps_write_string(g_rgd.pps, key, man->exit_info_hex);
    }
    if (man->present & RGD_MAN_EXIT_INFO_STR) {
        snprintf(key, sizeof(key), "m%u_exit_info", idx);
        pps_write_string(g_rgd.pps, key, man->exit_info_str);
    }
}

/*
 * Resolve linked-lane guidance references.
 * When a maneuver has RGD_MAN_LINKED_LANE_INDEX but no RGD_MAN_LANE_GUIDANCE,
 * look up the referenced maneuver in the slot cache and copy its lane data.
 */
static void rgd_resolve_linked_lanes(int slot) {
    rgd_maneuver_t* man = &g_rgd.slot_cache[slot];
    if (!(man->present & RGD_MAN_LINKED_LANE_INDEX)) return;
    if (man->present & RGD_MAN_LANE_GUIDANCE) return; /* already has own lane data */

    uint16_t linked_iap_idx = man->linked_lane_guidance_index;
    int linked_slot = rgd_slot_for_iap_index(linked_iap_idx, false);
    if (linked_slot < 0) {
        LOG_DEBUG(LOG_MODULE, "Linked lane idx %u: ref maneuver not cached yet", linked_iap_idx);
        return;
    }

    const rgd_maneuver_t* src = &g_rgd.slot_cache[linked_slot];
    if (!(src->present & RGD_MAN_LANE_GUIDANCE) || src->lane_count == 0) {
        LOG_DEBUG(LOG_MODULE, "Linked lane idx %u (slot %d): source has no lane data",
                  linked_iap_idx, linked_slot);
        return;
    }

    man->lane_count = src->lane_count;
    memcpy(man->lanes, src->lanes, sizeof(man->lanes));
    man->present |= RGD_MAN_LANE_GUIDANCE;

    LOG_INFO(LOG_MODULE, "Resolved linked lanes: slot %d -> slot %d (iAP2 %u), %d lanes",
             slot, linked_slot, linked_iap_idx, man->lane_count);
}

/*
 * Back-propagate lane data: when a maneuver with lane data is cached,
 * check if any other cached maneuver has a linked-lane index pointing to
 * this one's iAP2 index, and resolve those references.
 */
static void rgd_backpropagate_linked_lanes(int source_slot) {
    const rgd_maneuver_t* src = &g_rgd.slot_cache[source_slot];
    if (!(src->present & RGD_MAN_LANE_GUIDANCE) || src->lane_count == 0) return;

    uint16_t src_iap_idx = g_rgd.slot_to_iap_idx[source_slot];
    if (src_iap_idx == 0xFFFF) return;

    for (int s = 0; s < MANEUVER_CACHE_SIZE; s++) {
        if (s == source_slot) continue;
        rgd_maneuver_t* man = &g_rgd.slot_cache[s];
        if (!(man->present & RGD_MAN_LINKED_LANE_INDEX)) continue;
        if (man->present & RGD_MAN_LANE_GUIDANCE) continue;
        if (man->linked_lane_guidance_index != src_iap_idx) continue;

        man->lane_count = src->lane_count;
        memcpy(man->lanes, src->lanes, sizeof(man->lanes));
        man->present |= RGD_MAN_LANE_GUIDANCE;

        LOG_INFO(LOG_MODULE, "Back-propagated linked lanes: slot %d <- slot %d (iAP2 %u), %d lanes",
                 s, source_slot, src_iap_idx, man->lane_count);
    }
}

static void write_pps_maneuver_partial(const rgd_maneuver_t* man) {
    if (!g_rgd.pps || !man) return;
    if (man->present == 0) return;
    if (!(man->present & RGD_MAN_INDEX)) return;

    uint64_t write_mask = RGD_MAN_TYPE |
                          RGD_MAN_EXIT_ANGLE |
                          RGD_MAN_JUNCTION_TYPE |
                          RGD_MAN_DRIVING_SIDE |
                          RGD_MAN_DISTANCE_BETWEEN |
                          RGD_MAN_DISTANCE_STRING |
                          RGD_MAN_DISTANCE_UNITS |
                          RGD_MAN_DESCRIPTION |
                          RGD_MAN_AFTER_ROAD |
                          RGD_MAN_JUNCTION_ANGLES |
                          RGD_MAN_LANE_GUIDANCE_RAW |
                          RGD_MAN_LINKED_LANE_INDEX |
                          RGD_MAN_LANE_GUIDANCE |
                          RGD_MAN_EXIT_INFO_RAW |
                          RGD_MAN_EXIT_INFO_STR;
    if ((man->present & write_mask) == 0) return;

    unsigned iap_idx = man->index;
    if (!rgd_can_process_maneuver_index((uint16_t)iap_idx)) return;
    int slot = rgd_slot_for_iap_index((uint16_t)iap_idx, true);
    if (slot < 0) return;

    /* Cache for re-publish on 0x5201 */
    g_rgd.slot_cache[slot] = *man;

    /* Resolve linked-lane guidance: if this maneuver references another's lanes, copy them */
    rgd_resolve_linked_lanes(slot);

    /* If this maneuver has lane data, back-propagate to any maneuver linking to it */
    if (g_rgd.slot_cache[slot].present & RGD_MAN_LANE_GUIDANCE) {
        rgd_backpropagate_linked_lanes(slot);
    }

    write_pps_snapshot_from_cache(slot, &g_rgd.slot_cache[slot], NULL);
}

static void write_pps_lane_guidance_partial(const rgd_lane_guidance_t* lane) {
    if (!g_rgd.pps || !lane) return;
    if ((lane->present & (RGD_LANE_GUIDANCE_INDEX | RGD_LANE_INFORMATIONS)) == 0) return;

    rgd_update_t upd;
    bool have_upd = false;
    memset(&upd, 0, sizeof(upd));

    uint16_t iap_idx = 0xFFFF;
    if (lane->present & RGD_LANE_GUIDANCE_INDEX) {
        upd.present |= RGD_UPD_LANE_INDEX;
        upd.lane_guidance_index = lane->lane_guidance_index;
        have_upd = true;
        iap_idx = lane->lane_guidance_index;
    } else if (g_rgd.current_list_present && g_rgd.current_list_count > 0) {
        /*
         * Fallback when 0x5204 omits laneGuidanceIndex: attach to current primary maneuver.
         * Native path usually provides index explicitly.
         */
        iap_idx = g_rgd.current_list[0];
    }

    bool have_slot = false;
    int slot = -1;
    if (lane->present & RGD_LANE_INFORMATIONS) {
        if (iap_idx == 0xFFFF) {
            LOG_DEBUG(LOG_MODULE, "0x5204 lane data without index and no active maneuver list");
        } else if (!rgd_can_process_maneuver_index(iap_idx) && !rgd_has_cached_maneuver(iap_idx)) {
            LOG_DEBUG(LOG_MODULE, "0x5204 lane idx=%u rejected by maneuver-index gating", iap_idx);
        } else {
            slot = rgd_slot_for_iap_index(iap_idx, true);
            if (slot >= 0) {
                rgd_maneuver_t* dst = &g_rgd.slot_cache[slot];
                dst->index = iap_idx;
                dst->present |= RGD_MAN_INDEX;
                dst->lane_count = lane->lane_count;
                memset(dst->lanes, 0, sizeof(dst->lanes));
                if (lane->lane_count > 0) {
                    memcpy(dst->lanes, lane->lanes, sizeof(dst->lanes));
                }

                if ((lane->present & RGD_LANE_GUIDANCE_DESC) && lane->lane_guidance_description[0] != '\0') {
                    for (int i = 0; i < dst->lane_count && i < MAX_LANE_GUIDANCE; i++) {
                        if (dst->lanes[i].description[0] == '\0') {
                            snprintf(dst->lanes[i].description, sizeof(dst->lanes[i].description), "%s",
                                     lane->lane_guidance_description);
                        }
                    }
                }

                dst->present |= RGD_MAN_LANE_GUIDANCE;
                rgd_resolve_linked_lanes(slot);
                if (dst->present & RGD_MAN_LANE_GUIDANCE) {
                    rgd_backpropagate_linked_lanes(slot);
                }
                have_slot = true;
            }
        }
    }

    if (!have_upd && !have_slot) return;

    if (have_upd) {
        rgd_update_cache_merge(&upd);
    }

    if (have_slot) {
        write_pps_snapshot_from_cache(slot, &g_rgd.slot_cache[slot], have_upd ? &upd : NULL);
    } else {
        write_pps_snapshot_from_cache(-1, NULL, &upd);
    }
}

static void write_pps_snapshot_from_cache(int extra_slot, const rgd_maneuver_t* extra_man,
                                          const rgd_update_t* current_upd) {
    if (!g_rgd.pps) return;

    const rgd_update_t* upd = &g_rgd.update_cache;
    uint64_t present = upd->present & RGD_UPD_WRITE_MASK;
    bool have_extra_slot = (extra_man && extra_man->present != 0 &&
                            extra_slot >= 0 && extra_slot < MANEUVER_CACHE_SIZE);
    if (present == 0 && !have_extra_slot) return;

    pps_begin(g_rgd.pps);
    pps_write_header(g_rgd.pps);

    if (present & RGD_UPD_ROUTE_STATE)
        pps_write_int(g_rgd.pps, "route_state", upd->route_state);
    if (present & RGD_UPD_MANEUVER_STATE)
        pps_write_int(g_rgd.pps, "maneuver_state", upd->maneuver_state);
    if (present & RGD_UPD_DISTANCE_REMAINING)
        pps_write_uint(g_rgd.pps, "dist_dest_m", upd->distance_remaining);
    if (present & RGD_UPD_DIST_TO_MANEUVER)
        pps_write_uint(g_rgd.pps, "dist_maneuver_m", upd->dist_to_maneuver);
    if (present & RGD_UPD_ETA)
        pps_write_uint(g_rgd.pps, "eta_seconds", upd->eta);
    if (present & RGD_UPD_TIME_REMAINING)
        pps_write_uint(g_rgd.pps, "time_remaining_seconds", upd->time_remaining);
    if (present & RGD_UPD_CURRENT_ROAD)
        pps_write_string(g_rgd.pps, "current_road", upd->current_road);
    if (present & RGD_UPD_DESTINATION)
        pps_write_string(g_rgd.pps, "destination", upd->destination);
    if (present & RGD_UPD_DISTANCE_UNITS)
        pps_write_int(g_rgd.pps, "dist_dest_units", upd->distance_units);
    if (present & RGD_UPD_DIST_TO_MANEUVER_UNI)
        pps_write_int(g_rgd.pps, "dist_maneuver_units", upd->dist_to_maneuver_units);
    if (present & RGD_UPD_DISTANCE_STRING)
        pps_write_string(g_rgd.pps, "dist_dest_str", upd->distance_string);
    if (present & RGD_UPD_DIST_TO_MANEUVER_STR)
        pps_write_string(g_rgd.pps, "dist_maneuver_str", upd->dist_to_maneuver_string);
    if (present & RGD_UPD_LANE_INDEX)
        pps_write_int(g_rgd.pps, "lane_guidance_index", upd->lane_guidance_index);
    if (present & RGD_UPD_LANE_TOTAL)
        pps_write_int(g_rgd.pps, "lane_guidance_total", upd->lane_guidance_total);
    if (present & RGD_UPD_LANE_SHOWING)
        pps_write_int(g_rgd.pps, "lane_guidance_showing", upd->lane_guidance_showing);
    if (present & RGD_UPD_SOURCE_NAME)
        pps_write_string(g_rgd.pps, "source_name", upd->source_name);
    if (present & RGD_UPD_SOURCE_SUPPORTS_RG)
        pps_write_int(g_rgd.pps, "source_supports_rg", upd->source_supports_route_guidance);
    if (present & RGD_UPD_VISIBLE_IN_APP)
        pps_write_int(g_rgd.pps, "visible_in_app", upd->visible_in_app);
    if (present & RGD_UPD_COMPONENT_IDS) {
        pps_write_int(g_rgd.pps, "component_count", upd->component_count);
        if (upd->component_count > 0) {
            char list_buf[128];
            int loff = 0;
            for (uint16_t i = 0; i < upd->component_count; i++) {
                loff += snprintf(list_buf + loff, sizeof(list_buf) - (size_t)loff,
                                 "%s%u", i > 0 ? "," : "", (unsigned)upd->component_ids[i]);
                if (loff >= (int)sizeof(list_buf) - 4) break;
            }
            pps_write_string(g_rgd.pps, "component_ids", list_buf);
        } else {
            pps_write_string(g_rgd.pps, "component_ids", "");
        }
    }
    if (present & RGD_UPD_MANEUVER_COUNT)
        pps_write_int(g_rgd.pps, "maneuver_count", upd->maneuver_count);

    int listed_slots[MAX_MANEUVER_LIST];
    int listed_count = 0;
    /*
     * Maneuver list persistence:
     * - 0x5201 with ManeuverList TLV present updates current_list (including explicit empty list)
     * - 0x5201 without ManeuverList TLV keeps previous current_list
     * - 0x5202 writes (current_upd == NULL) also keep previous current_list
     *
     * This avoids publishing a transient empty maneuver_list during partial 0x5201 updates.
     */
    if (current_upd && (current_upd->present & RGD_UPD_MANEUVER_LIST)) {
        g_rgd.current_list_present = true;
        g_rgd.current_list_count = 0;
        for (uint16_t i = 0; i < current_upd->maneuver_list_count && g_rgd.current_list_count < MAX_MANEUVER_LIST; i++) {
            g_rgd.current_list[g_rgd.current_list_count++] = current_upd->maneuver_list[i];
        }
    }

    if (g_rgd.current_list_present) {
        if (g_rgd.current_list_count > 0) {
            char list_buf[128];
            int loff = 0;
            int outc = 0;
            for (uint16_t i = 0; i < g_rgd.current_list_count; i++) {
                int slot = rgd_slot_for_iap_index(g_rgd.current_list[i], true);
                if (slot < 0) continue;
                if (!(g_rgd.slot_cache[slot].present & RGD_MAN_TYPE)) continue;
                if (listed_count < MAX_MANEUVER_LIST)
                    listed_slots[listed_count++] = slot;
                loff += snprintf(list_buf + loff, sizeof(list_buf) - (size_t)loff,
                                 "%s%u", outc > 0 ? "," : "", (unsigned)slot);
                outc++;
                if (loff >= (int)sizeof(list_buf) - 4) break;
            }
            if (outc > 0) {
                pps_write_string(g_rgd.pps, "maneuver_list", list_buf);
            } else {
                /* All ManeuverList indices lack cached type data (evicted).
                 * Clear stale maneuver_list so Java doesn't keep showing
                 * the previous maneuver. */
                pps_write_string(g_rgd.pps, "maneuver_list", "");
            }
        } else {
            /* Explicit empty list from source; propagate as a real clear. */
            pps_write_string(g_rgd.pps, "maneuver_list", "");
        }
    }

    for (int i = 0; i < listed_count; i++) {
        int s = listed_slots[i];
        if (s >= 0 && s < MANEUVER_CACHE_SIZE && g_rgd.slot_cache[s].present != 0) {
            write_slot_data_keys((unsigned)s, &g_rgd.slot_cache[s]);
        }
    }

    /*
     * Extra slot (from 0x5202) that is already in the listed maneuver_list
     * was written above.  Do NOT write non-listed extra slots: Java only
     * consumes slots present in maneuver_list, and the extra data bloats
     * the PPS payload (causing write() ENOSPC on small ramdisks).
     * The cached slot data will be included automatically when the next
     * 0x5201 adds the slot to maneuver_list.
     */

    pps_end(g_rgd.pps);
}

void rgd_clear_state(const char* reason) {
    /* Stop cluster renderer on disconnect/shutdown */
    renderer_stop();

    g_rgd.active = false;
    g_rgd.sent_5200 = false;
    g_rgd.sent_5203 = false;
    g_rgd.got_520x = false;
    g_rgd.component_valid = false;
    g_rgd.have_update = false;
    g_rgd.last_route_state = 0;
    rgd_maneuver_map_reset();
    rgd_update_cache_reset();

    if (g_rgd.pps) {
        pps_begin(g_rgd.pps);
        pps_write_header(g_rgd.pps);
        pps_write_int(g_rgd.pps, "route_state", RGD_STATE_NOT_ACTIVE);
        pps_write_int(g_rgd.pps, "maneuver_count", 0);
        if (reason) pps_write_string(g_rgd.pps, "disconnect_reason", reason);
        pps_end(g_rgd.pps);
    }

    rgd_update_session_active();

    LOG_INFO(LOG_MODULE, "State cleared: %s", reason ? reason : "unknown");
}

hook_result_t rgd_request_updates(void) {
    if (!g_rgd.component_valid) {
        LOG_WARN(LOG_MODULE, "Cannot request: component not valid");
        return HOOK_ERR_INIT;
    }

    /*
     * Mirror native libesoiap2 0x5200 shape:
     * - 0x0000 component ID (u16)
     * - 0x0001 sourceName request (presence TLV, empty payload)
     * - 0x0002 sourceSupportsRouteGuidance request (presence TLV)
     * - 0x0003 supportsExitInfo request (presence TLV)
     */
    uint8_t tlv[24];
    size_t off = 0;
    size_t n = 0;

    n = iap2_build_tlv_u16(tlv + off, sizeof(tlv) - off, RGD_START_TLV_COMPONENT_ID, g_rgd.component_id);
    if (n == 0) return HOOK_ERR_PARAM;
    off += n;

    n = iap2_build_tlv(tlv + off, sizeof(tlv) - off, RGD_START_TLV_SOURCE_NAME, NULL, 0);
    if (n == 0) return HOOK_ERR_PARAM;
    off += n;

    n = iap2_build_tlv(tlv + off, sizeof(tlv) - off, RGD_START_TLV_SOURCE_SUPPORTS_RG, NULL, 0);
    if (n == 0) return HOOK_ERR_PARAM;
    off += n;

    n = iap2_build_tlv(tlv + off, sizeof(tlv) - off, RGD_START_TLV_SUPPORTS_EXIT_INFO, NULL, 0);
    if (n == 0) return HOOK_ERR_PARAM;
    off += n;

    hook_result_t res = hook_inject_message(IAP2_MSG_ROUTE_GUIDANCE_START, tlv, off);
    if (res == HOOK_OK) {
        g_rgd.sent_5200 = true;
        g_rgd.active = true;
        rgd_update_session_active();
        LOG_INFO(LOG_MODULE, "Sent 0x5200 (component=0x%04X, opts=source_name+source_supports_rg+exit_info)", g_rgd.component_id);
    }
    return res;
}

hook_result_t rgd_stop_updates(void) {
    if (g_rgd.sent_5203 || !g_rgd.active) return HOOK_OK;

    uint8_t tlv[6];
    iap2_build_tlv_u16(tlv, sizeof(tlv), RGD_START_TLV_COMPONENT_ID, g_rgd.component_id);

    hook_result_t res = hook_inject_message(IAP2_MSG_ROUTE_GUIDANCE_STOP, tlv, 6);
    if (res == HOOK_OK) {
        g_rgd.sent_5203 = true;
        g_rgd.active = false;
        rgd_update_session_active();
        LOG_INFO(LOG_MODULE, "Sent 0x5203");
    }
    return res;
}

/* Raw packet full log before parsing (debug) */
#ifndef RGD_TRACE_RAW_FULL
#define RGD_TRACE_RAW_FULL 0
#endif

static void rgd_log_raw_packet(const char* label, const uint8_t* data, size_t len) {
    if (!data || len == 0 || !label) return;
    LOG_DEBUG(LOG_MODULE, "%s len=%zu", label, len);

    for (size_t off = 0; off < len; off += 16) {
        char line[128];
        int pos = snprintf(line, sizeof(line), "%s %04zx:", label, off);
        for (size_t i = 0; i < 16 && (off + i) < len && pos < (int)sizeof(line) - 4; i++) {
            pos += snprintf(line + pos, sizeof(line) - (size_t)pos, " %02X", data[off + i]);
        }
        log_write(LOG_LEVEL_DEBUG, LOG_MODULE, "%s", line);
    }
}

/* Message handler - incoming 0x5201/0x5202/0x5204 */
static bool rgd_message_handler(hook_context_t* ctx, const iap2_frame_t* frame) {
    rgd_lazy_init();  /* Ensure PPS is open */

    if (frame->msgid == IAP2_MSG_ROUTE_GUIDANCE_UPDATE) {
        if (!g_rgd.got_520x) {
            g_rgd.got_520x = true;
            rgd_update_session_active();
            LOG_INFO(LOG_MODULE, "*** FIRST RouteGuidance message received! msgid=0x%04X ***", frame->msgid);
        }
#if RGD_TRACE_RAW_FULL
        rgd_log_raw_packet("RGD 0x5201 raw", ctx->raw_buf, frame->frame_len);
#endif

        rgd_update_t upd;
        rgd_parse_update(ctx->raw_buf, frame->frame_len, &upd);

        LOG_INFO(LOG_MODULE, "Update: state=%u road=\"%s\" dest=\"%s\"",
                 upd.route_state, upd.current_road, upd.destination);

        /* Track update presence and reset on hard reset.
         *
         * Do NOT wipe slot_cache on route_state=0.  iOS sends rapid
         * state=0 transitions during route setup, often AFTER the 0x5202
         * maneuver data has arrived.  Wiping the cache here permanently
         * loses that data because iOS never resends 0x5202 for
         * already-sent indices.  The slot cache is only reset on actual
         * disconnect (rgd_clear_state).
         *
         * We still reset the update_cache (route-level fields like
         * distance, ETA, etc.) so that stale route-level values don't
         * leak across state transitions.
         */
        g_rgd.have_update = true;
        if (upd.present & RGD_UPD_ROUTE_STATE) {
            g_rgd.last_route_state = upd.route_state;
            if (upd.route_state == 0) {
                rgd_update_cache_reset();
            }
            /* Start renderer when route guidance becomes active */
            if (upd.route_state >= RGD_STATE_ROUTE_SET && g_renderer_pid <= 0) {
                renderer_start();
            }
        }
        write_pps_update_partial(&upd);
    }
    else if (frame->msgid == IAP2_MSG_ROUTE_GUIDANCE_MANEUVER) {
        if (!g_rgd.got_520x) {
            g_rgd.got_520x = true;
            rgd_update_session_active();
            LOG_INFO(LOG_MODULE, "*** FIRST RouteGuidance message received! msgid=0x%04X ***", frame->msgid);
        }
#if RGD_TRACE_RAW_FULL
        rgd_log_raw_packet("RGD 0x5202 raw", ctx->raw_buf, frame->frame_len);
#endif

        rgd_maneuver_t man;
        rgd_parse_maneuver(ctx->raw_buf, frame->frame_len, &man);

        LOG_INFO(LOG_MODULE, "Maneuver: idx=%u type=%u desc=\"%s\"",
                 man.index, man.maneuver_type, man.description);
        write_pps_maneuver_partial(&man);
    }
    else if (frame->msgid == IAP2_MSG_ROUTE_GUIDANCE_LANE) {
        if (!g_rgd.got_520x) {
            g_rgd.got_520x = true;
            rgd_update_session_active();
            LOG_INFO(LOG_MODULE, "*** FIRST RouteGuidance message received! msgid=0x%04X ***", frame->msgid);
        }
#if RGD_TRACE_RAW_FULL
        rgd_log_raw_packet("RGD 0x5204 raw", ctx->raw_buf, frame->frame_len);
#endif

        rgd_lane_guidance_t lane;
        rgd_parse_lane_guidance(ctx->raw_buf, frame->frame_len, &lane);

        LOG_INFO(LOG_MODULE, "Lane guidance: idx=%u lanes=%u desc=\"%s\"",
                 lane.lane_guidance_index, lane.lane_count, lane.lane_guidance_description);
        for (int li = 0; li < lane.lane_count && li < MAX_LANE_GUIDANCE; li++) {
            const rgd_lane_t* l = &lane.lanes[li];
            char abuf[128];
            int ao = 0;
            for (int ai = 0; ai < l->angle_count && ai < MAX_LANE_ANGLES; ai++)
                ao += snprintf(abuf + ao, sizeof(abuf) - (size_t)ao, "%s%d",
                               ai > 0 ? "," : "", (int)l->angles[ai]);
            if (l->angle_count == 0) abuf[0] = '\0';
            LOG_INFO(LOG_MODULE, "  lane[%d] pos=%u dir=%d status=%u angles(%u)=[%s]",
                     li, l->position, (int)l->direction, l->status,
                     l->angle_count, abuf);
        }
        write_pps_lane_guidance_partial(&lane);
    }

    return false;
}

/* Transport send callback - trigger 0x5200 injection on 0xFFFB */
static void rgd_transport_handler(hook_context_t* ctx, uint16_t msgid) {
    (void)ctx;

    /* Inject 0x5200 when we see LocationInfo (0xFFFB) going out */
    if (msgid == IAP2_MSG_LOCATION_INFO && !g_rgd.sent_5200 && hook_is_ready()) {
        rgd_request_updates();
    }
}

/* Identify patcher */
static size_t rgd_identify_patcher(hook_context_t* ctx, uint8_t* buf, size_t len, size_t max_len) {
    rgd_lazy_init();  /* Ensure initialized */
    if (!buf || len < 6) return len;

    uint16_t existing_len = 0;
    size_t existing_off = iap2_find_tlv_offset(buf + 6, len - 6, IDENT_TLV_ROUTE_GUIDANCE_COMPONENT, &existing_len);
    bool existing_found = (existing_len > 0);

    if (existing_found) {
        existing_off += 6;
        uint16_t comp_id = rgd_extract_component_id(buf + existing_off, existing_len);
        if (comp_id != 0) {
            g_rgd.component_id = comp_id;
            LOG_INFO(LOG_MODULE, "Found existing 0x001E component=0x%04X", comp_id);
        }
        g_rgd.component_valid = true;
    } else {
        g_rgd.component_valid = true;
        LOG_INFO(LOG_MODULE, "Appending 0x001E component=0x%04X", g_rgd.component_id);
    }

    uint8_t new_tlv[256];
    size_t new_tlv_len = rgd_build_component_tlv(new_tlv, sizeof(new_tlv), g_rgd.component_id);
    if (new_tlv_len == 0) return len;

    size_t new_len = existing_found ? (len - existing_len + new_tlv_len) : (len + new_tlv_len);
    if (new_len > max_len) {
        size_t new_cap = max_len;
        uint8_t* new_buf = rgd_resize_identify_buffer(ctx, len, new_len, &new_cap);
        if (!new_buf) {
            LOG_WARN(LOG_MODULE, "Identify patch failed: no buffer space");
            return len;
        }
        buf = new_buf;
        max_len = new_cap;
    }

    if (existing_found) {
        size_t after_off = existing_off + existing_len;
        size_t after_len = len - after_off;
        if (after_len > 0) memmove(buf + existing_off + new_tlv_len, buf + after_off, after_len);
        memcpy(buf + existing_off, new_tlv, new_tlv_len);
    } else {
        memcpy(buf + len, new_tlv, new_tlv_len);
    }

    write_be16(buf + 2, (uint16_t)new_len);
    if (ctx && ctx->_priv) {
        *(unsigned int*)((char*)ctx->_priv + 4) = (unsigned int)new_len;
    }

    ctx->rgd_component_id = g_rgd.component_id;
    ctx->rgd_component_valid = true;
    ctx->identify_patched = true;

    LOG_INFO(LOG_MODULE, "Identify patched: %zu -> %zu bytes", len, new_len);
    return new_len;
}

/* State handler */
static void rgd_state_handler(hook_context_t* ctx, int event, void* event_data) {
    (void)ctx;
    (void)event_data;

    switch (event) {
        case HOOK_EVENT_SHUTDOWN:
        case HOOK_EVENT_DISCONNECT:
            rgd_stop_updates();
            rgd_clear_state(event == HOOK_EVENT_SHUTDOWN ? "shutdown" : "disconnect");
            break;
        case HOOK_EVENT_IDENTIFY_END:
            if (!g_rgd.active && !g_rgd.got_520x) rgd_clear_state("identify_end");
            break;
    }
}

/* Public API */
static bool g_rgd_module_registered = false;

void rgd_init(void) {
    /* Just register module - no file ops in constructor */
    if (!g_rgd_module_registered) {
        hook_framework_register_module(&rgd_module_def);
        g_rgd_module_registered = true;
    }
}

/* Called lazily on first message */
static void rgd_lazy_init(void) {
    if (g_rgd.pps) return;  /* Already initialized */

    pps_config_t pps_cfg = PPS_CONFIG_DEFAULT;
    pps_cfg.path = RGD_PPS_PATH;
    pps_cfg.object_name = RGD_PPS_OBJECT;
    pps_cfg.create_dirs = true;

    g_rgd.pps = pps_open(&pps_cfg);
    if (g_rgd.pps) rgd_clear_state("init");
    LOG_INFO(LOG_MODULE, "Route Guidance module initialized");
}

void rgd_shutdown(void) {
    rgd_stop_updates();
    rgd_clear_state("module_shutdown");
    if (g_rgd.pps) {
        pps_close(g_rgd.pps);
        g_rgd.pps = NULL;
    }
}

__attribute__((constructor))
static void rgd_module_init(void) {
    rgd_init();
}

__attribute__((destructor))
static void rgd_module_fini(void) {
    rgd_shutdown();
}
