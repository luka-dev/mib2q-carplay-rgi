/*
 * The fine mask net on its own thread: see scene_mask_worker.h for the contract and the timing.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "scene_mask_worker.h"

#include <sched.h>
#include <string.h>
#include <time.h>

int (*spa_scene_mask_worker_test_control)(struct scene_mask *net, const float *in512,
                                          float *elig257, float *prot257);

static uint32_t smw_load(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void smw_store(volatile uint32_t *value, uint32_t next)
{
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
}

static void smw_increment(volatile uint32_t *value)
{
    (void)__atomic_add_fetch(value, 1u, __ATOMIC_RELAXED);
}

/* ---- the provider (audio thread) */

static int smw_submit(void *context, uint32_t generation, uint32_t sequence, const float *in512)
{
    struct spa_scene_mask_worker *w = (struct spa_scene_mask_worker *)context;
    uint32_t published, next;
    struct spa_scene_mask_job *job;

    if (!w || !in512 || !sequence || !smw_load(&w->_started)) return 0;
    published = smw_load(&w->_job_published);
    if ((uint32_t)(published - smw_load(&w->_job_consumed)) >= 2u) {
        smw_increment(&w->_busy);
        return 0;
    }
    next = published + 1u;
    job = &w->_jobs[next & 1u];
    job->generation = generation;
    job->sequence = sequence;
    memcpy(job->in, in512, sizeof(job->in));
    smw_store(&w->_job_published, next);
    smw_increment(&w->_submitted);
    if (w->_priority != SPA_SCENE_MASK_WORKER_NO_THREAD &&
        pthread_mutex_trylock(&w->_wake_lock) == 0) {
        (void)pthread_cond_signal(&w->_wake_cond);
        (void)pthread_mutex_unlock(&w->_wake_lock);
    }
    return 1;
}

static int smw_receive(void *context, uint32_t generation, struct spa_scene_mask_row *row)
{
    struct spa_scene_mask_worker *w = (struct spa_scene_mask_worker *)context;
    uint32_t published, consumed;
    int found = 0;

    if (!w || !row || !smw_load(&w->_started)) return 0;
    published = smw_load(&w->_result_published);
    consumed = smw_load(&w->_result_consumed);
    /* Drain to the newest row of this generation.  The worker never writes a slot while two are
     * unconsumed, so everything between consumed and published is stable until the store below. */
    while (consumed != published) {
        const struct spa_scene_mask_result *result = &w->_results[(consumed + 1u) & 1u];

        if (result->generation == generation) {
            if (found) smw_increment(&w->_superseded);
            memcpy(row, &result->row, sizeof(*row));
            found = 1;
        } else {
            smw_increment(&w->_stale_discarded);
        }
        consumed++;
    }
    smw_store(&w->_result_consumed, consumed);
    return found;
}

static void smw_reset(void *context, uint32_t generation)
{
    struct spa_scene_mask_worker *w = (struct spa_scene_mask_worker *)context;

    if (!w) return;
    if (!generation) generation = 1u;
    smw_store(&w->_requested_generation, generation);
}

/* ---- the worker */

static void smw_apply_reset(struct spa_scene_mask_worker *w)
{
    uint32_t requested = smw_load(&w->_requested_generation);

    if (requested == w->_active_generation) return;
    scene_mask_reset(&w->_net);
    w->_active_generation = requested;
    smw_increment(&w->_resets);
}

int spa_scene_mask_worker_step(struct spa_scene_mask_worker *w)
{
    uint32_t published, consumed, next, generation, sequence;
    const struct spa_scene_mask_job *job;
    struct spa_scene_mask_result *result;
    int have;

    if (!w) return 0;
    smw_apply_reset(w);
    published = smw_load(&w->_job_published);
    consumed = smw_load(&w->_job_consumed);
    if (published == consumed) return 0;

    next = consumed + 1u;
    job = &w->_jobs[next & 1u];
    generation = job->generation;
    sequence = job->sequence;
    /* The audio thread stores the generation before it publishes a job of it, so a job seen here
     * belongs to a generation already requested: apply that reset first, never drop the job. */
    smw_apply_reset(w);
    if (generation != w->_active_generation) {
        smw_store(&w->_job_consumed, next);
        smw_increment(&w->_stale_discarded);
        return 1;
    }
    /* The net runs in place on the slot (it copies the frames into its own history); the slot is
     * handed back right after, so one job computes while one waits. */
    have = spa_scene_mask_worker_test_control
        ? spa_scene_mask_worker_test_control(&w->_net, job->in, NULL, NULL)
        : scene_mask_control(&w->_net, job->in, NULL, NULL);
    smw_store(&w->_job_consumed, next);
    smw_increment(&w->_completed);
    if (!have) return 1;                                    /* priming: nothing to publish */
    if (smw_load(&w->_requested_generation) != generation) {
        smw_increment(&w->_stale_discarded);
        return 1;
    }
    published = smw_load(&w->_result_published);
    if ((uint32_t)(published - smw_load(&w->_result_consumed)) >= 2u) {
        /* Never overwrite a slot while the audio thread may copy it (it drains every boundary, so
         * this is a paused stream). */
        smw_increment(&w->_stale_discarded);
        return 1;
    }
    next = published + 1u;
    result = &w->_results[next & 1u];
    result->generation = generation;
    result->row.sequence = sequence;
    memcpy(result->row.elig, w->_net.elig, sizeof(result->row.elig));
    memcpy(result->row.prot, w->_net.prot, sizeof(result->row.prot));
    spa_scene_mask_lfe_posteriors(&w->_net, &result->row.lfe_bass, &result->row.lfe_drum);
    smw_store(&w->_result_published, next);
    return 1;
}

/* A bounded SCHED_RR priority, the result not checked (as the retired HSSR
 * control worker did; tag upmix-universal-last-2026-10-09). */
static void smw_set_priority(int priority)
{
    struct sched_param parameter;
    int policy = SCHED_RR;
    int minimum, maximum;

    if (priority <= 0) return;
    minimum = sched_get_priority_min(policy);
    maximum = sched_get_priority_max(policy);
    if (minimum < 0 || maximum < minimum) return;
    if (priority < minimum) priority = minimum;
    if (priority > maximum) priority = maximum;
    memset(&parameter, 0, sizeof(parameter));
    parameter.sched_priority = priority;
    (void)pthread_setschedparam(pthread_self(), policy, &parameter);
}

static void *smw_main(void *opaque)
{
    struct spa_scene_mask_worker *w = (struct spa_scene_mask_worker *)opaque;

    smw_set_priority(w->_priority);
    while (!smw_load(&w->_stop)) {
        struct timespec ts;

        if (spa_scene_mask_worker_step(w)) continue;
        if (pthread_mutex_lock(&w->_wake_lock) == 0) {
            if (!smw_load(&w->_stop) &&
                smw_load(&w->_job_published) == smw_load(&w->_job_consumed)) {
                (void)clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += 20000000L; /* 20 ms fallback wake */
                if (ts.tv_nsec >= 1000000000L) {
                    ts.tv_sec += 1;
                    ts.tv_nsec -= 1000000000L;
                }
                (void)pthread_cond_timedwait(&w->_wake_cond, &w->_wake_lock, &ts);
            }
            (void)pthread_mutex_unlock(&w->_wake_lock);
        }
    }
    return NULL;
}

int spa_scene_mask_worker_start(struct spa_scene_mask_worker *w,
                                const struct scene_mask_weights *blob,
                                uint32_t initial_generation, int priority)
{
    if (!w || !blob) return -1;
    if (!initial_generation) initial_generation = 1u;
    memset(w, 0, sizeof(*w));
    if (scene_mask_init(&w->_net, blob) != 0) return -1;
    w->_priority = priority;
    w->provider.context = w;
    w->provider.submit = smw_submit;
    w->provider.receive = smw_receive;
    w->provider.reset = smw_reset;
    smw_store(&w->_requested_generation, initial_generation);
    if (priority != SPA_SCENE_MASK_WORKER_NO_THREAD) {
        if (pthread_mutex_init(&w->_wake_lock, NULL) != 0) {
            memset(w, 0, sizeof(*w));
            return -1;
        }
        if (pthread_cond_init(&w->_wake_cond, NULL) != 0) {
            (void)pthread_mutex_destroy(&w->_wake_lock);
            memset(w, 0, sizeof(*w));
            return -1;
        }
        if (pthread_create(&w->_thread, NULL, smw_main, w) != 0) {
            (void)pthread_cond_destroy(&w->_wake_cond);
            (void)pthread_mutex_destroy(&w->_wake_lock);
            memset(w, 0, sizeof(*w));
            return -1;
        }
    }
    smw_store(&w->_started, 1u);
    return 0;
}

void spa_scene_mask_worker_stop(struct spa_scene_mask_worker *w)
{
    if (!w || !smw_load(&w->_started)) return;
    smw_store(&w->_stop, 1u);
    if (w->_priority != SPA_SCENE_MASK_WORKER_NO_THREAD) {
        if (pthread_mutex_lock(&w->_wake_lock) == 0) {
            (void)pthread_cond_broadcast(&w->_wake_cond);
            (void)pthread_mutex_unlock(&w->_wake_lock);
        }
        (void)pthread_join(w->_thread, NULL);
        (void)pthread_cond_destroy(&w->_wake_cond);
        (void)pthread_mutex_destroy(&w->_wake_lock);
    }
    smw_store(&w->_started, 0u);
}

const struct spa_scene_mask_provider *spa_scene_mask_worker_provider(struct spa_scene_mask_worker *w)
{
    if (!w || !smw_load(&w->_started)) return NULL;
    return &w->provider;
}

void spa_scene_mask_worker_get_stats(const struct spa_scene_mask_worker *w,
                                     struct spa_scene_mask_worker_stats *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    if (!w) return;
    stats->submitted = smw_load(&w->_submitted);
    stats->completed = smw_load(&w->_completed);
    stats->busy = smw_load(&w->_busy);
    stats->stale_discarded = smw_load(&w->_stale_discarded);
    stats->superseded = smw_load(&w->_superseded);
    stats->resets = smw_load(&w->_resets);
}
