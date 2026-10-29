/* Actual native hook worker; CFLite, SDK send and bus are deterministic leaves.
 */
#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_hook.c"
#include <assert.h>

typedef struct fake_cf {
    int refs, session;
    char text[128], type[48], uuid[64], url[128];
    int64_t value;
    struct fake_cf *params;
} fake_cf_t;
static unsigned live_objects;
/* Fault switches are changed before thread start or while the SDK hold blocks
 * the only worker; there is no concurrent CF construction in these cases. */
static const char *fail_string;
static const char *fail_number;
static int fail_allocation_at, fault_hit, fail_append;
static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static int hold, entered, fail_view;
static struct {
    char type[48], url[128];
    int value, session;
    uint64_t time;
} sent[512];
static unsigned sent_count, ack_count;
static struct {
    int mode, status;
} acks[512];
static fake_cf_t *make_object(void) {
    if (fail_allocation_at > 0 && --fail_allocation_at == 0) {
        fault_hit = 1;
        return NULL;
    }
    fake_cf_t *p = calloc(1, sizeof(*p));
    assert(p);
    p->refs = 1;
    __sync_add_and_fetch(&live_objects, 1);
    return p;
}
static void *retain_object(void *p) {
    assert(p);
    assert(__sync_add_and_fetch(&((fake_cf_t *)p)->refs, 1) > 1);
    return p;
}
static void release_object(void *p) {
    if (!p)
        return;
    fake_cf_t *f = p;
    int refs = __sync_sub_and_fetch(&f->refs, 1);
    assert(refs >= 0);
    if (!refs) {
        release_object(f->params);
        free(f);
        __sync_sub_and_fetch(&live_objects, 1);
    }
}
static void *make_string(void *a, const char *text, unsigned encoding) {
    (void)a;
    (void)encoding;
    if (fail_string && !strcmp(fail_string, text))
        return NULL;
    fake_cf_t *p = make_object();
    if (!p)
        return NULL;
    snprintf(p->text, sizeof(p->text), "%s", text);
    return p;
}
static void *make_dict(void *a, long n, const void *k, const void *v) {
    (void)a;
    (void)n;
    (void)k;
    (void)v;
    return make_object();
}
static void *make_array(void *a, long n, const void *cb) {
    (void)a;
    (void)n;
    (void)cb;
    return make_object();
}
static int array_append(void *a, int64_t n) {
    (void)a;
    (void)n;
    return fail_append ? -1 : 0;
}
/* GCC/ASan keeps the hook's public module table and its unreachable receiver
 * callbacks. Provide fail-closed leaves rather than depending on linker GC. */
int altav_async_start(void (*failed)(uint32_t, void *), void *context) {
    assert(!"unexpected forwarding worker start");
    return -1;
}
hook_result_t bus_on(uint16_t type, bus_handler_t handler, void *ctx) {
    assert(!"unexpected bus registration");
    return -1;
}
void altav_shutdown(void) { assert(!"unexpected forwarding shutdown"); }
int altav_claim(uint32_t gen) {
    assert(!"unexpected receiver claim");
    return 0;
}
int altav_is_owner(uint32_t gen) {
    assert(!"unexpected receiver owner check");
    return 0;
}
int altav_enqueue_config(uint32_t gen, const uint8_t *config, uint32_t len) {
    assert(!"unexpected receiver config");
    return -1;
}
int altav_enqueue_owned(uint32_t gen, uint8_t **data, uint32_t *capacity,
                        const alt_avcc_frame_t *info, const alt_frame_meta_t *meta) {
    assert(!"unexpected receiver frame");
    return -1;
}
int altav_take_failure(uint32_t gen) {
    assert(!"unexpected receiver failure check");
    return 0;
}
void altav_note_input_loss(uint32_t gen) { assert(!"unexpected receiver input loss"); }
void altav_shutdown_if_owner(uint32_t gen) { assert(!"unexpected receiver owner shutdown"); }
static void dict_set(void *dict, const void *key, const void *value) {
    fake_cf_t *d = dict;
    const fake_cf_t *k = key, *v = value;
    if (!strcmp(k->text, "params")) {
        release_object(d->params);
        d->params = retain_object((void *)v);
    } else if (!strcmp(k->text, "type"))
        snprintf(d->type, sizeof(d->type), "%.*s", (int)sizeof(d->type) - 1, v->text);
    else if (!strcmp(k->text, "uuid"))
        snprintf(d->uuid, sizeof(d->uuid), "%.*s", (int)sizeof(d->uuid) - 1, v->text);
    else if (!strcmp(k->text, "url"))
        snprintf(d->url, sizeof(d->url), "%.*s", (int)sizeof(d->url) - 1, v->text);
}
static int fake_getcstr(const void *cf, char *buf, long max, uint32_t enc) {
    const fake_cf_t *f = cf;
    (void)enc;
    if (!f || !buf || max <= 0)
        return 0;
    snprintf(buf, (size_t)max, "%s", f->text);
    return 1;
}
static int dict_integer(void *dict, const void *key, int64_t value) {
    fake_cf_t *d = dict;
    const fake_cf_t *k = key;
    if (fail_number && !strcmp(fail_number, k->text))
        return -6728;
    fake_cf_t *number = make_object();
    if (!number)
        return -6728;
    if (!strcmp(k->text, "zoomDirection") || !strcmp(k->text, "viewAreaIndex") ||
        !strcmp(k->text, "nightMode"))
        d->value = value;
    release_object(number);
    return 0;
}
int altav_keyframe_permit(uint32_t gen, unsigned kind) {
    (void)gen;
    (void)kind;
    return 0;
}
hook_result_t bus_send(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len) {
    (void)flags;
    assert(type == CMD_ALT_ZONE_ACK && len == 5);
    pthread_mutex_lock(&g_alt_cmd_lock);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    const uint8_t *p = payload;
    int status = (int)((uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) |
                       ((uint32_t)p[4] << 24));
    pthread_mutex_lock(&test_lock);
    assert(ack_count < 512);
    acks[ack_count].mode = p[0];
    acks[ack_count++].status = status;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    return 0;
}
static int send_command(void *session, void *command, void *completion, void *ctx) {
    fake_cf_t *s = session, *c = command;
    pthread_mutex_lock(&g_alt_cmd_lock);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    pthread_mutex_lock(&test_lock);
    assert(sent_count < 512);
    unsigned n = sent_count++;
    snprintf(sent[n].type, sizeof(sent[n].type), "%s", c->type);
    snprintf(sent[n].url, sizeof(sent[n].url), "%s", c->params ? c->params->url : "");
    sent[n].value = c->params ? (int)c->params->value : 0;
    sent[n].session = s->session;
    sent[n].time = alt_timing_now_us();
    if (!strcmp(c->type, "changeMapZoomLevel") || !strcmp(c->type, "forceKeyFrame") ||
        !strcmp(c->type, "showUI"))
        assert(c->params && !strcmp(c->params->uuid, CLUSTER_DISPLAY_UUID));
    if (!strcmp(c->type, "hold")) {
        entered = 1;
        pthread_cond_broadcast(&test_cond);
        while (hold)
            pthread_cond_wait(&test_cond, &test_lock);
    }
    int result = fail_view && !strcmp(c->type, "updateViewArea") ? -6729 : 0;
    if (result)
        fail_view = 0;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    if (!result && completion)
        ((void (*)(int, void *, void *))completion)(0, NULL, ctx);
    return result;
}
static void publish_owner(fake_cf_t *session, uint32_t gen) {
    pthread_mutex_lock(&alt_lock);
    memset(alt_slots, 0, sizeof(alt_slots));
    alt_slots[0].inUse = alt_slots[0].threadCreated = 1;
    alt_slots[0].session = session;
    alt_slots[0].avGeneration = gen;
    g_alt_session = session;
    g_alt_session_gen = gen;
    pthread_mutex_unlock(&alt_lock);
}
static void block_worker(void) {
    fake_cf_t *cmd = make_object();
    snprintf(cmd->type, sizeof(cmd->type), "hold");
    pthread_mutex_lock(&test_lock);
    hold = 1;
    entered = 0;
    pthread_mutex_unlock(&test_lock);
    assert(!alt_send_async_for_gen_key(0, cmd, NULL, NULL, ALT_COMMAND_GENERIC));
    release_object(cmd);
    pthread_mutex_lock(&test_lock);
    while (!entered)
        pthread_cond_wait(&test_cond, &test_lock);
    pthread_mutex_unlock(&test_lock);
}
static void unblock_worker(void) {
    pthread_mutex_lock(&test_lock);
    hold = 0;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
}
static unsigned count_zoom(void) {
    unsigned count = 0;
    for (unsigned i = 0; i < sent_count; ++i)
        if (!strcmp(sent[i].type, "changeMapZoomLevel"))
            ++count;
    return count;
}
static void wait_zooms(unsigned target) {
    struct timespec until;
    alt_deadline_after_ms(&until, 1500);
    pthread_mutex_lock(&test_lock);
    while (count_zoom() < target)
        assert(pthread_cond_timedwait(&test_cond, &test_lock, &until) != ETIMEDOUT);
    pthread_mutex_unlock(&test_lock);
}
static void schedule_state(unsigned kind) {
    if (kind == 0)
        (void)zone_send_mode(1, 2, 250, NULL);
    else if (kind == 1)
        (void)alt_send_cluster_night_mode(1, 1, "fault-test");
    else if (kind == 2)
        (void)alt_show_ui_for_mode(1, 0, "fault-test");
    else
        (void)alt_send_cluster_idr_raw(1, "fault-test");
}
static void assert_preserved(void *previous, unsigned objects) {
    pthread_mutex_lock(&g_alt_cmd_lock);
    assert(g_alt_commands.count == 1 && g_alt_commands.items[0].cmd == previous);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    assert(live_objects == objects);
}
/* Exercise every construction allocation plus required field keys. The real
 * worker is blocked inside SDK send, so pending refs can be inspected safely.
 */
static void state_construction_faults(void) {
    const char *keys[4][8] = {
        {"uuid", "viewAreaIndex", "animationDurationMillis", "adjacentViewAreas", "type", "params",
         NULL},
        {"uuid", "nightMode", "type", "params", NULL},
        {"uuid", "url", "type", "params", NULL},
        {"uuid", "type", "params", NULL}};
    for (unsigned kind = 0; kind < 4; ++kind) {
        schedule_state(kind);
        pthread_mutex_lock(&g_alt_cmd_lock);
        assert(g_alt_commands.count == 1);
        void *previous = g_alt_commands.items[0].cmd;
        pthread_mutex_unlock(&g_alt_cmd_lock);
        unsigned objects = live_objects;
        for (unsigned k = 0; keys[kind][k]; ++k) {
            fail_string = keys[kind][k];
            schedule_state(kind);
            fail_string = NULL;
            assert_preserved(previous, objects);
        }
        const char *numbers[2][3] = {{"viewAreaIndex", "animationDurationMillis", NULL},
                                     {"nightMode", NULL}};
        if (kind < 2)
            for (unsigned k = 0; numbers[kind][k]; ++k) {
                fail_number = numbers[kind][k];
                schedule_state(kind);
                fail_number = NULL;
                assert_preserved(previous, objects);
            }
        if (kind == 0) {
            fail_append = 1;
            schedule_state(kind);
            fail_append = 0;
            assert_preserved(previous, objects);
            unsigned ack_before = ack_count;
            uint8_t area[] = {1, 0, 0};
            fail_string = "params";
            zone_on_cmd(0, 0, area, sizeof(area), NULL);
            fail_string = NULL;
            assert_preserved(previous, objects);
            assert(ack_count == ack_before + 1 && acks[ack_before].mode == 1 &&
                   acks[ack_before].status == -6728);
        }
        /* Stop at the first allocation beyond this complete construction. */
        unsigned n;
        for (n = 1; n < 32; ++n) {
            fault_hit = 0;
            fail_allocation_at = (int)n;
            schedule_state(kind);
            fail_allocation_at = 0;
            if (!fault_hit)
                break;
            assert_preserved(previous, objects);
        }
        assert(n < 32);
        alt_cmd_item_t retired[ALT_COMMAND_CAPACITY];
        unsigned count = 0;
        pthread_mutex_lock(&g_alt_cmd_lock);
        while (g_alt_commands.count)
            alt_command_take(&g_alt_commands, 0, &retired[count++]);
        g_alt_commands.urgent_streak = g_alt_commands.normal_streak = 0;
        pthread_mutex_unlock(&g_alt_cmd_lock);
        for (unsigned i = 0; i < count; ++i)
            alt_command_dispose(&retired[i], -1, 0);
    }
}
int main(void) {
    alarm(10);
    cf_ok = 1;
    alt_enabled = 1;
    alt_syms_ok = 1;
    cf_ret = retain_object;
    cf_rel = release_object;
    cf_str = make_string;
    cf_getcstr = fake_getcstr;
    cf_dictm = make_dict;
    cf_dictset = dict_set;
    cf_dictseti = dict_integer;
    cf_arrm = make_array;
    cf_arrappi64 = array_append;
    dict_keycb = dict_valcb = arr_cb = (void *)1;
    sess_sendcmd = send_command;
    fake_cf_t *one = make_object(), *two = make_object();
    one->session = 1;
    two->session = 2;
    publish_owner(one, 1);
    const char *fault_keys[] = {"uuid", "zoomDirection", "type", "params"};
    alt_zoom_dispatch_t fault_dispatch = {.gen = 1, .direction = 1};
    for (unsigned i = 0; i < sizeof(fault_keys) / sizeof(fault_keys[0]); ++i) {
        fail_string = fault_keys[i];
        assert(alt_dispatch_zoom(&fault_dispatch) == -6728);
        assert(!sent_count && live_objects == 2);
    }
    fail_string = NULL;
    fail_number = "zoomDirection";
    assert(alt_dispatch_zoom(&fault_dispatch) == -6728);
    assert(!sent_count && live_objects == 2);
    fail_number = NULL;
    assert(alt_command_env_ms("ALTSCREEN_ZOOM_INTERVAL_MS", 50, 20, 100) == 50000);
    setenv("ALTSCREEN_ZOOM_INTERVAL_MS", "invalid", 1);
    assert(alt_command_env_ms("ALTSCREEN_ZOOM_INTERVAL_MS", 50, 20, 100) == 50000);
    unsetenv("ALTSCREEN_ZOOM_INTERVAL_MS");
    block_worker();
    state_construction_faults();
    unsigned ack_base = ack_count;
    uint8_t steps = 8;
    zoom_on_cmd(0, 0, &steps, 1, NULL);
    for (int i = 0; i < 3; ++i)
        assert(!alt_send_cluster_night_mode(1, i == 2, "test"));
    for (int i = 1; i <= 3; ++i) {
        uint8_t area[3] = {(uint8_t)(i % 3), 250, 0};
        zone_on_cmd(0, 0, area, 3, NULL);
    }
    assert(!alt_send_cluster_idr_raw(1, "test"));
    assert(!alt_show_ui_for_mode(1, 0, "test"));
    pthread_mutex_lock(&test_lock);
    assert(ack_count == ack_base + 2 && acks[ack_base].status == ALT_CMD_SUPERSEDED &&
           acks[ack_base + 1].status == ALT_CMD_SUPERSEDED);
    pthread_mutex_unlock(&test_lock);
    pthread_mutex_lock(&g_alt_cmd_lock);
    assert(g_alt_commands.count == 4 && g_alt_commands.zoom_steps == 8);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    unblock_worker();
    wait_zooms(2);
    pthread_mutex_lock(&test_lock);
    assert(!strcmp(sent[1].type, "forceKeyFrame"));
    assert(!strcmp(sent[2].type, "updateViewArea") && sent[2].value == 0);
    uint64_t previous = 0;
    unsigned night = 0;
    for (unsigned i = 0; i < sent_count; ++i) {
        if (!strcmp(sent[i].type, "setNightMode")) {
            ++night;
            assert(sent[i].value == 1);
        }
        if (!strcmp(sent[i].type, "changeMapZoomLevel")) {
            if (previous)
                assert(sent[i].time - previous >= 49000u);
            previous = sent[i].time;
        }
    }
    assert(night == 1);
    unsigned before = count_zoom();
    pthread_mutex_unlock(&test_lock);
    steps = (uint8_t)-1;
    zoom_on_cmd(0, 0, &steps, 1, NULL);
    wait_zooms(before + 1);
    pthread_mutex_lock(&test_lock);
    for (unsigned i = sent_count; i-- > 0;)
        if (!strcmp(sent[i].type, "changeMapZoomLevel")) {
            assert(sent[i].value == 0);
            break;
        }
    pthread_mutex_unlock(&test_lock);
    /* An SDK failure has no completion; the worker must publish one error ACK. */
    pthread_mutex_lock(&test_lock);
    fail_view = 1;
    unsigned ack_before = ack_count;
    pthread_mutex_unlock(&test_lock);
    uint8_t failed_area[3] = {1, 0, 0};
    zone_on_cmd(0, 0, failed_area, 3, NULL);
    struct timespec until;
    alt_deadline_after_ms(&until, 1500);
    pthread_mutex_lock(&test_lock);
    while (ack_count == ack_before)
        assert(pthread_cond_timedwait(&test_cond, &test_lock, &until) != ETIMEDOUT);
    assert(acks[ack_count - 1].mode == 1 && acks[ack_count - 1].status == -6729);
    pthread_mutex_unlock(&test_lock);
    /* suggestUI control command interception and showUI dispatch. */
    {
        fake_cf_t *cmd_other = make_string(NULL, "updateFeedback", 0);
        fake_cf_t *cmd_suggest = make_string(NULL, "suggestUI", 0);
        void *resp_ptr = (void *)0xdeadbeef;
        hook_control_ctx_t ctrl = {0};

        ctrl.session = one;
        ctrl.command = cmd_other;
        ctrl.response_out = &resp_ptr;
        alt_on_control(&ctrl);
        assert(!ctrl.handled && resp_ptr == (void *)0xdeadbeef);

        /* Uncommitted session two: suggestUI is claimed/acked, no showUI queued. */
        memset(&ctrl, 0, sizeof(ctrl));
        resp_ptr = (void *)0xdeadbeef;
        ctrl.session = two;
        ctrl.command = cmd_suggest;
        ctrl.response_out = &resp_ptr;
        block_worker();
        alt_on_control(&ctrl);
        assert(ctrl.handled && ctrl.result == 0 && resp_ptr == NULL);
        pthread_mutex_lock(&g_alt_cmd_lock);
        assert(g_alt_commands.count == 0);
        pthread_mutex_unlock(&g_alt_cmd_lock);

        /* Active session one: suggestUI claims/acks and queues showUI with current RGI URL. */
        alt_rgi_active_set(1);
        memset(&ctrl, 0, sizeof(ctrl));
        resp_ptr = (void *)0xdeadbeef;
        ctrl.session = one;
        ctrl.command = cmd_suggest;
        ctrl.response_out = &resp_ptr;
        alt_on_control(&ctrl);
        assert(ctrl.handled && ctrl.result == 0 && resp_ptr == NULL);
        pthread_mutex_lock(&g_alt_cmd_lock);
        assert(g_alt_commands.count == 1 &&
               g_alt_commands.items[0].key == ALT_COMMAND_SHOW_UI);
        pthread_mutex_unlock(&g_alt_cmd_lock);
        unblock_worker();

        alt_deadline_after_ms(&until, 1500);
        pthread_mutex_lock(&test_lock);
        for (;;) {
            int found = 0;
            for (unsigned i = 0; i < sent_count; ++i) {
                if (!strcmp(sent[i].type, "showUI") &&
                    !strcmp(sent[i].url, ALT_CLUSTER_URL_RGI_ON)) {
                    found = 1;
                    break;
                }
            }
            if (found)
                break;
            assert(pthread_cond_timedwait(&test_cond, &test_lock, &until) != ETIMEDOUT);
        }
        pthread_mutex_unlock(&test_lock);
        alt_rgi_active_set(0);
        release_object(cmd_other);
        release_object(cmd_suggest);
    }
    /* Concurrent rapid area changes exercise real completion/state reads. */
    for (unsigned i = 0; i < 24; ++i) {
        uint8_t area[3] = {(uint8_t)(i % 3), 0, 0};
        zone_on_cmd(0, 0, area, 3, NULL);
    }
    /* A blocked old SDK call retains its session; queued work/zoom is revoked. */
    block_worker();
    steps = 8;
    zoom_on_cmd(0, 0, &steps, 1, NULL);
    assert(!alt_send_cluster_night_mode(1, 0, "old"));
    publish_owner(two, 2);
    alt_command_revoke(1);
    steps = 1;
    zoom_on_cmd(0, 0, &steps, 1, NULL);
    pthread_mutex_lock(&test_lock);
    before = count_zoom();
    unsigned boundary = sent_count;
    pthread_mutex_unlock(&test_lock);
    unblock_worker();
    wait_zooms(before + 1);
    alt_command_worker_shutdown();
    pthread_mutex_lock(&test_lock);
    for (unsigned i = boundary; i < sent_count; ++i)
        assert(sent[i].session == 2);
    pthread_mutex_unlock(&test_lock);
    publish_owner(NULL, 0);
    release_object(one);
    release_object(two);
    assert(!__sync_add_and_fetch(&live_objects, 0));
    puts("altscreen_command_worker_test: real worker priority, latest view/night "
         "state, "
         "canceled/actual ACK, pacing/reversal, generation fencing, unlocked "
         "callbacks and "
         "balanced CF ownership PASS");
    return 0;
}
