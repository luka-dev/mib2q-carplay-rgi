/* Selected zoom must expire while target-session acquisition is delayed. */
#define main original_command_worker_main
#include "altscreen_command_worker_test.c"
#undef main
static int target_released, retain_action;
static void note_release(void *p) {
    if (p && ((fake_cf_t *)p)->session) {
        pthread_mutex_lock(&test_lock);
        target_released = 1;
        pthread_cond_broadcast(&test_cond);
        pthread_mutex_unlock(&test_lock);
    }
    release_object(p);
}
static void *delayed_session_retain(void *p) {
    fake_cf_t *obj = p;
    if (obj->session) {
        if (retain_action == 1)
            alt_command_revoke(1);
        else if (retain_action == 2) {
            pthread_mutex_lock(&g_alt_cmd_lock);
            g_alt_cmd_shutdown = 1;
            pthread_mutex_unlock(&g_alt_cmd_lock);
        } else {
            struct timespec pause = {.tv_sec = 0, .tv_nsec = 350000000};
            nanosleep(&pause, NULL);
        }
    }
    return retain_object(p);
}
int main(void) {
    alarm(10);
    cf_ok = 1;
    cf_ret = delayed_session_retain;
    cf_rel = note_release;
    cf_str = make_string;
    cf_dictm = make_dict;
    cf_dictset = dict_set;
    cf_dictseti = dict_integer;
    cf_arrm = make_array;
    cf_arrappi64 = array_append;
    dict_keycb = dict_valcb = arr_cb = (void *)1;
    sess_sendcmd = send_command;
    fake_cf_t *s = make_object();
    s->session = 1;
    publish_owner(s, 1);
    /* Direct dispatch cases run before worker start: retirement and shutdown
     * during retention must win over a previously selected valid stamp. */
    alt_cmd_item_t retired[ALT_COMMAND_CAPACITY];
    for (unsigned gen = 1; gen <= 2; ++gen) {
        alt_zoom_dispatch_t dispatch;
        publish_owner(s, gen);
        pthread_mutex_lock(&g_alt_cmd_lock);
        alt_command_generation(&g_alt_commands, gen, 1, retired);
        uint64_t now = alt_timing_now_us();
        assert(!alt_zoom_offer(&g_alt_commands, gen, 1, now));
        alt_zoom_take(&g_alt_commands, now, &dispatch);
        pthread_mutex_unlock(&g_alt_cmd_lock);
        retain_action = (int)gen;
        assert(alt_dispatch_zoom(&dispatch) == ALT_CMD_SUPERSEDED);
        assert(!sent_count && live_objects == 1);
    }
    pthread_mutex_lock(&g_alt_cmd_lock);
    g_alt_cmd_shutdown = 0;
    pthread_mutex_unlock(&g_alt_cmd_lock);
    retain_action = target_released = 0;
    publish_owner(s, 3);
    uint64_t input_time = alt_timing_now_us();
    assert(!alt_command_zoom(3, 1));
    struct timespec until;
    alt_deadline_after_ms(&until, 1500);
    pthread_mutex_lock(&test_lock);
    while (!target_released)
        assert(pthread_cond_timedwait(&test_cond, &test_lock, &until) != ETIMEDOUT);
    pthread_mutex_unlock(&test_lock);
    alt_command_worker_shutdown();
    assert(!sent_count && alt_timing_now_us() - input_time > ALT_ZOOM_TAIL_US);
    puts("altscreen_command_age_test: pre-commit retirement/stop and delayed "
         "target acquisition cancel zoom before SDK, CF refs balanced PASS");
    publish_owner(NULL, 0);
    release_object(s);
    assert(!live_objects);
    return 0;
}
