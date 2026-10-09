/* Force reversal after producer validation and during target-session retain. */
#define _GNU_SOURCE
#include <pthread.h>
static int review_mutex_lock(pthread_mutex_t *lock);
#define pthread_mutex_lock review_mutex_lock
#define main original_command_worker_main
#include "altscreen_command_worker_test.c"
#undef main
#undef pthread_mutex_lock
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static pthread_key_t producer_role;
static int construction_waiting, allow_construction, producer_validated;
static int allow_admission, reversal_accepted, guarded_once;
static int review_mutex_lock(pthread_mutex_t *lock) {
    if (lock == &g_alt_cmd_lock && pthread_getspecific(producer_role)) {
        pthread_mutex_lock(&gate_lock);
        producer_validated = 1;
        pthread_cond_broadcast(&gate_cond);
        while (!allow_admission)
            pthread_cond_wait(&gate_cond, &gate_lock);
        pthread_mutex_unlock(&gate_lock);
    }
    return pthread_mutex_lock(lock);
}
static void gated_dict_set(void *d, const void *k, const void *v) {
    dict_set(d, k, v);
    fake_cf_t *cmd = d;
    const fake_cf_t *key = k;
    if (!strcmp(cmd->type, "changeMapZoomLevel") && !strcmp(key->text, "params")) {
        pthread_mutex_lock(&gate_lock);
        if (!guarded_once) {
            construction_waiting = 1;
            pthread_cond_broadcast(&gate_cond);
            while (!allow_construction)
                pthread_cond_wait(&gate_cond, &gate_lock);
        }
        pthread_mutex_unlock(&gate_lock);
    }
}
static void *gated_retain(void *p) {
    fake_cf_t *obj = p;
    if (obj->session) {
        pthread_mutex_lock(&gate_lock);
        if (!guarded_once) {
            guarded_once = 1;
#if defined(ALT_ZOOM_REFRESH_TEST)
            struct timespec pause = {.tv_sec = 0, .tv_nsec = 350000000};
            nanosleep(&pause, NULL);
#endif
            allow_admission = 1;
            pthread_cond_broadcast(&gate_cond);
            while (!reversal_accepted)
                pthread_cond_wait(&gate_cond, &gate_lock);
        }
        pthread_mutex_unlock(&gate_lock);
    }
    return retain_object(p);
}
static void *reverse_input(void *unused) {
    (void)unused;
    assert(!pthread_setspecific(producer_role, (void *)1));
#if defined(ALT_ZOOM_REFRESH_TEST)
    assert(!alt_command_zoom(1, 1));
#else
    assert(!alt_command_zoom(1, -1));
#endif
    pthread_mutex_lock(&gate_lock);
    reversal_accepted = 1;
    pthread_cond_broadcast(&gate_cond);
    pthread_mutex_unlock(&gate_lock);
    return NULL;
}
static int observed_old_after_reversal;
static int review_send(void *s, void *c, void *done, void *ctx) {
    fake_cf_t *cmd = c;
    if (!strcmp(cmd->type, "changeMapZoomLevel") && cmd->params->value == 1) {
        pthread_mutex_lock(&gate_lock);
        observed_old_after_reversal = 1;
        pthread_mutex_unlock(&gate_lock);
    }
    return send_command(s, c, done, ctx);
}
int main(void) {
    alarm(10);
    assert(!pthread_key_create(&producer_role, NULL));
    cf_ok = 1;
    cf_ret = gated_retain;
    cf_rel = release_object;
    cf_str = make_string;
    cf_dictm = make_dict;
    cf_dictset = gated_dict_set;
    cf_dictseti = dict_integer;
    cf_arrm = make_array;
    cf_arrappi64 = array_append;
    dict_keycb = dict_valcb = arr_cb = (void *)1;
    sess_sendcmd = review_send;
    fake_cf_t *s = make_object();
    s->session = 1;
    publish_owner(s, 1);
    assert(!alt_command_zoom(1, 1));
    pthread_mutex_lock(&gate_lock);
    while (!construction_waiting)
        pthread_cond_wait(&gate_cond, &gate_lock);
    pthread_mutex_unlock(&gate_lock);
    pthread_t producer;
    assert(!pthread_create(&producer, NULL, reverse_input, NULL));
    pthread_mutex_lock(&gate_lock);
    while (!producer_validated)
        pthread_cond_wait(&gate_cond, &gate_lock);
    allow_construction = 1;
    pthread_cond_broadcast(&gate_cond);
    pthread_mutex_unlock(&gate_lock);
    pthread_join(producer, NULL);
    wait_zooms(1);
#if defined(ALT_ZOOM_REFRESH_TEST)
    /* Quiescence barrier: do not hide a duplicate by shutting down after the
     * first send. If an old step was sent, fresh intent is pending or was sent
     * before this SDK hold; both outcomes fail the checks below. */
    block_worker();
    pthread_mutex_lock(&g_alt_cmd_lock);
    assert(!g_alt_commands.zoom_steps);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    pthread_mutex_lock(&test_lock);
    assert(count_zoom() == 1 && sent[0].value == 1);
    pthread_mutex_unlock(&test_lock);
    unblock_worker();
#endif
    alt_command_worker_shutdown();
#if defined(ALT_ZOOM_REFRESH_TEST)
    assert(sent_count == 2 && !strcmp(sent[1].type, "hold"));
    puts("altscreen_command_refresh_test: expired selection is not revived by "
         "fresh same-direction input, one zoom reaches SDK, CF refs balanced PASS");
#else
    assert(!observed_old_after_reversal && sent_count == 1 && sent[0].value == 0);
    puts("altscreen_command_reversal_test: accepted pre-commit reversal cancels "
         "selected old zoom, SDK remains unlocked, CF refs balanced PASS");
#endif
    publish_owner(NULL, 0);
    release_object(s);
    assert(!live_objects);
    assert(!pthread_key_delete(producer_role));
    return 0;
}
