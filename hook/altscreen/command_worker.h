/* Internal implementation, included by altscreen_hook.c after its session
 * retain/send seam. All stock calls and completions run outside command lock.
 */
#ifndef ALTSCREEN_COMMAND_WORKER_H
#define ALTSCREEN_COMMAND_WORKER_H
#include "../framework/ap_status.h"
static void alt_command_dispose(alt_cmd_item_t *item, int status, int notify) {
    if (notify && item->completion) {
        void (*done)(int, void *, void *) = (void (*)(int, void *, void *))item->completion;
        done(status, NULL, item->ctx);
    }
    if (item->cmd && cf_rel)
        cf_rel(item->cmd);
    memset(item, 0, sizeof(*item));
}
static void alt_command_revoke(uint32_t gen) {
    alt_cmd_item_t retired[ALT_COMMAND_CAPACITY];
    unsigned count, i;
    pthread_mutex_lock(&g_alt_cmd_lock);
    count = alt_command_generation(&g_alt_commands, gen, 0, retired);
    pthread_cond_broadcast(g_alt_cmd_clock.active);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    for (i = 0; i < count; ++i)
        alt_command_dispose(&retired[i], ALT_CMD_SUPERSEDED, 0);
}
static uint64_t alt_command_env_ms(const char *name, unsigned fallback, unsigned low,
                                   unsigned high) {
    const char *value = getenv(name);
    char *end;
    long parsed;
    if (!value || !*value)
        return (uint64_t)fallback * 1000u;
    errno = 0;
    parsed = strtol(value, &end, 10);
    if (errno || *end || parsed < (long)low || parsed > (long)high)
        return (uint64_t)fallback * 1000u;
    return (uint64_t)parsed * 1000u;
}
static int alt_dispatch_zoom(const alt_zoom_dispatch_t *dispatch) {
    void *params = alt_params_new(1), *cmd = NULL, *key = NULL, *session = NULL;
    int valid, rc = kNoMemoryErr;
    if (!params)
        return rc;
    if (set_i64(params, "zoomDirection", dispatch->direction) != 0) {
        cf_rel(params);
        return rc;
    }
    cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (cmd) {
        if (set_str(cmd, "type", "changeMapZoomLevel") != 0)
            goto done;
        key = cf_str(0, "params", CFENC_UTF8);
        if (!key)
            goto done;
        cf_dictset(cmd, key, params);
        /* Acquire the retained target before committing: acquisition may wait.
         * No session lock is held at the final queue-policy check or SDK call. */
        session = alt_retain_session_for_gen(dispatch->gen);
        if (!session) {
            rc = kUnsupportedErr;
            goto done;
        }
        pthread_mutex_lock(&g_alt_cmd_lock);
        /* Successful validation is the dispatch commit point. Reversal/stop
         * accepted before it cancels this step; afterwards this one committed
         * step is in flight. The SDK remains outside both mutexes. */
        valid = !g_alt_cmd_shutdown &&
                alt_zoom_dispatch_valid(&g_alt_commands, dispatch, alt_timing_now_us());
        pthread_mutex_unlock(&g_alt_cmd_lock);
        rc = valid ? sess_sendcmd(session, cmd, (void *)alt_cmd_done, (void *)"changeMapZoomLevel")
                   : ALT_CMD_SUPERSEDED;
        /* Pace from the completed SDK submission, avoiding catch-up bursts
         * after a slow send. A reversal/new owner keeps its immediate deadline. */
        pthread_mutex_lock(&g_alt_cmd_lock);
        if (valid && g_alt_commands.generation == dispatch->gen &&
            g_alt_commands.zoom_revision == dispatch->revision)
            g_alt_commands.zoom_due_us = alt_timing_now_us() + g_alt_commands.zoom_interval_us;
        pthread_mutex_unlock(&g_alt_cmd_lock);
    }
done:
    if (session && cf_rel)
        cf_rel(session);
    if (key && cf_rel)
        cf_rel(key);
    if (cmd && cf_rel)
        cf_rel(cmd);
    if (cf_rel)
        cf_rel(params);
    return rc;
}
static void *alt_command_worker_main(void *unused) {
    (void)unused;
    for (;;) {
        alt_cmd_item_t item = {0};
        alt_zoom_dispatch_t zoom = {0};
        int choice, rc;
        uint64_t now;
        pthread_mutex_lock(&g_alt_cmd_lock);
        for (;;) {
            now = alt_timing_now_us();
            choice = alt_command_select(&g_alt_commands, now);
            if (g_alt_cmd_shutdown || choice != ALT_COMMAND_NONE)
                break;
            if (g_alt_commands.zoom_steps && now) {
                uint64_t deadline = g_alt_commands.zoom_due_us;
                uint64_t expiry =
                    g_alt_commands.zoom_last_input_us + g_alt_commands.zoom_tail_us + 1u;
                if (deadline > expiry)
                    deadline = expiry;
                (void)alt_condition_wait_until(&g_alt_cmd_clock, &g_alt_cmd_lock, deadline);
            } else
                pthread_cond_wait(g_alt_cmd_clock.active, &g_alt_cmd_lock);
        }
        if (g_alt_cmd_shutdown) {
            g_alt_cmd_exited = 1;
            pthread_cond_broadcast(g_alt_cmd_clock.active);
            pthread_mutex_unlock(&g_alt_cmd_lock);
            return NULL;
        }
        if (choice == ALT_COMMAND_ZOOM)
            alt_zoom_take(&g_alt_commands, now, &zoom);
        else
            alt_command_take(&g_alt_commands, (unsigned)choice, &item);
        pthread_mutex_unlock(&g_alt_cmd_lock);
        if (choice == ALT_COMMAND_ZOOM) {
            (void)alt_dispatch_zoom(&zoom);
            continue;
        }
        rc = alt_send_locked_for_gen(item.gen, item.cmd, item.completion, item.ctx);
        alt_command_dispose(&item, rc, rc != 0);
    }
}
static int alt_command_start_locked(void) {
    int rc;
    if (g_alt_cmd_shutdown)
        return kUnsupportedErr;
    if (g_alt_cmd_created)
        return 0;
    rc = alt_condition_init(&g_alt_cmd_clock);
    if (rc)
        return kNoMemoryErr;
    g_alt_commands.zoom_interval_us = alt_command_env_ms("ALTSCREEN_ZOOM_INTERVAL_MS", 50, 20, 100);
    g_alt_commands.zoom_tail_us = alt_command_env_ms("ALTSCREEN_ZOOM_TAIL_MS", 250, 100, 1000);
    g_alt_cmd_exited = 0;
    rc = pthread_create(&g_alt_cmd_thread, NULL, alt_command_worker_main, NULL);
    if (!rc)
        g_alt_cmd_created = 1;
    return rc ? kNoMemoryErr : 0;
}
/* gen=0 snapshots the current session before admission, never at dispatch time.
 */
static int alt_send_async_for_gen_key(uint32_t gen, void *cmd, void *completion, void *ctx,
                                      unsigned key) {
    alt_cmd_item_t retired[ALT_COMMAND_CAPACITY], replaced, item = {0};
    unsigned count = 0, i;
    int rc, has_replaced = 0;
    if (!cmd || !cf_ret || !cf_rel)
        return kUnsupportedErr;
    if (!gen)
        gen = alt_generation_for_session(NULL);
    if (!alt_generation_is_current(gen))
        return kUnsupportedErr;
    item.gen = gen;
    item.cmd = cf_ret(cmd);
    item.completion = completion;
    item.ctx = ctx;
    item.key = key;
    if (!item.cmd)
        return kNoMemoryErr;
    pthread_mutex_lock(&g_alt_cmd_lock);
    rc = alt_command_start_locked();
    if (!rc) {
        count = alt_command_generation(&g_alt_commands, gen, 1, retired);
        rc = alt_command_offer(&g_alt_commands, item, &replaced, &has_replaced);
        rc = rc == -1 ? kUnsupportedErr : rc == -2 ? kNoResourcesErr : rc;
    }
    pthread_cond_signal(g_alt_cmd_clock.active);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    for (i = 0; i < count; ++i)
        alt_command_dispose(&retired[i], ALT_CMD_SUPERSEDED, 0);
    if (has_replaced)
        alt_command_dispose(&replaced, ALT_CMD_SUPERSEDED, 1);
    if (rc)
        alt_command_dispose(&item, rc, 0);
    return rc;
}
static int alt_command_zoom(uint32_t gen, int steps) {
    alt_cmd_item_t retired[ALT_COMMAND_CAPACITY];
    unsigned count = 0, i;
    int rc;
    if (!gen)
        gen = alt_generation_for_session(NULL);
    if (!alt_generation_is_current(gen))
        return kUnsupportedErr;
    pthread_mutex_lock(&g_alt_cmd_lock);
    rc = alt_command_start_locked();
    if (!rc) {
        count = alt_command_generation(&g_alt_commands, gen, 1, retired);
        if (alt_zoom_offer(&g_alt_commands, gen, steps, alt_timing_now_us()) < 0)
            rc = kUnsupportedErr;
    }
    pthread_cond_signal(g_alt_cmd_clock.active);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    for (i = 0; i < count; ++i)
        alt_command_dispose(&retired[i], ALT_CMD_SUPERSEDED, 0);
    return rc;
}
static void alt_command_worker_shutdown(void) {
    alt_cmd_item_t retired[ALT_COMMAND_CAPACITY];
    unsigned count, i;
    int collect = 0;
    pthread_t thread;
    uint64_t deadline;
    pthread_mutex_lock(&g_alt_cmd_lock);
    g_alt_cmd_shutdown = 1;
    count = alt_command_generation(&g_alt_commands, g_alt_commands.generation, 0, retired);
    pthread_cond_broadcast(g_alt_cmd_clock.active);
    if (g_alt_cmd_created) {
        thread = g_alt_cmd_thread;
        deadline = alt_timing_now_us() + ALT_CMD_SHUTDOWN_WAIT_MS * UINT64_C(1000);
        while (!g_alt_cmd_exited) {
            int rc = alt_condition_wait_until(&g_alt_cmd_clock, &g_alt_cmd_lock, deadline);
            if (rc != 0)
                break;
        }
        if (g_alt_cmd_exited) {
            g_alt_cmd_created = 0;
            collect = 1;
        }
    }
    pthread_mutex_unlock(&g_alt_cmd_lock);
    for (i = 0; i < count; ++i)
        alt_command_dispose(&retired[i], ALT_CMD_SUPERSEDED, 0);
    if (collect)
        pthread_join(thread, NULL);
    else if (g_alt_cmd_created)
        LOG_WARN(LOG_MODULE, "alt command worker stop timed out; process exit retains live work");
}
#endif
