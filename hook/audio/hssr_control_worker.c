/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_control_worker.h"

#include <sched.h>
#include <string.h>
#include <time.h>

static uint32_t spa_hssr_atomic_load(const volatile uint32_t *value)
{
    return __atomic_load_n(value, __ATOMIC_ACQUIRE);
}

static void spa_hssr_atomic_store(volatile uint32_t *value, uint32_t next)
{
    __atomic_store_n(value, next, __ATOMIC_RELEASE);
}

static void spa_hssr_atomic_increment(volatile uint32_t *value)
{
    (void)__atomic_add_fetch(value, 1u, __ATOMIC_RELAXED);
}

static int spa_hssr_worker_submit(
    void *context, uint32_t generation, uint32_t sequence,
    const float features[SPA_HSSR_FEATURES],
    const float *mask_features)
{
    struct spa_hssr_control_worker *worker =
        (struct spa_hssr_control_worker *)context;
    uint32_t published;
    uint32_t next;
    struct spa_hssr_control_job *job;

    if (!worker || !features || !sequence ||
        !spa_hssr_atomic_load(&worker->_started))
        return 0;
    published = spa_hssr_atomic_load(&worker->_job_published);
    if ((uint32_t)(published -
                   spa_hssr_atomic_load(&worker->_job_consumed)) >= 2u) {
        spa_hssr_atomic_increment(&worker->_busy);
        return 0;
    }

    next = published + 1u;
    job = &worker->_jobs[next & 1u];
    job->generation = generation;
    job->sequence = sequence;
    memcpy(job->features, features, sizeof(job->features));
    job->has_mask = mask_features != NULL;
    if (mask_features)
        memcpy(job->mask_features, mask_features, sizeof(job->mask_features));
    spa_hssr_atomic_store(&worker->_job_published, next);
    spa_hssr_atomic_increment(&worker->_submitted);
    return 1;
}

static int spa_hssr_worker_receive(
    void *context, uint32_t generation, uint32_t maximum_sequence,
    float control[SPA_HSSR_OUTPUTS], uint32_t *sequence)
{
    struct spa_hssr_control_worker *worker =
        (struct spa_hssr_control_worker *)context;
    uint32_t published;
    uint32_t consumed;
    uint32_t next;
    const struct spa_hssr_control_result *result;

    if (!worker || !control || !sequence ||
        !spa_hssr_atomic_load(&worker->_started))
        return 0;
    published = spa_hssr_atomic_load(&worker->_result_published);
    consumed = spa_hssr_atomic_load(&worker->_result_consumed);
    if (published == consumed) return 0;

    next = consumed + 1u;
    result = &worker->_results[next & 1u];
    if (result->generation != generation) {
        spa_hssr_atomic_store(&worker->_result_consumed, next);
        spa_hssr_atomic_increment(&worker->_stale_discarded);
        return 0;
    }
    if (!result->sequence || result->sequence > maximum_sequence)
        return 0;

    memcpy(control, result->control, sizeof(result->control));
    *sequence = result->sequence;
    spa_hssr_atomic_store(&worker->_result_consumed, next);
    return 1;
}

static int spa_hssr_worker_receive_guard(void *context,
                                         float control[SPA_HSSR_OUTPUTS])
{
    struct spa_hssr_control_worker *worker =
        (struct spa_hssr_control_worker *)context;
    const struct spa_hssr_control_result *result;

    if (!worker || !control || !spa_hssr_atomic_load(&worker->_started))
        return 0;
    /* The slot receive() just consumed; the worker cannot reuse it before
     * the audio thread consumes the other slot as well. */
    result = &worker->_results[spa_hssr_atomic_load(&worker->_result_consumed) & 1u];
    if (!result->guard_valid) return 0;
    memcpy(control, result->guard_control, sizeof(result->guard_control));
    return 1;
}

static int spa_hssr_worker_receive_mask(void *context,
                                        float eligibility[SPA_MASK_BINS])
{
    struct spa_hssr_control_worker *worker =
        (struct spa_hssr_control_worker *)context;
    const struct spa_hssr_control_result *result;

    if (!worker || !eligibility || !spa_hssr_atomic_load(&worker->_started))
        return 0;
    result = &worker->_results[spa_hssr_atomic_load(&worker->_result_consumed) & 1u];
    if (!result->mask_valid) return 0;
    memcpy(eligibility, result->eligibility, sizeof(result->eligibility));
    return 1;
}

static void spa_hssr_worker_reset(void *context, uint32_t generation)
{
    struct spa_hssr_control_worker *worker =
        (struct spa_hssr_control_worker *)context;

    if (!worker) return;
    if (!generation) generation = 1u;
    spa_hssr_atomic_store(&worker->_requested_generation, generation);
}

static void spa_hssr_worker_set_priority(int priority)
{
    struct sched_param parameter;
    int policy = SCHED_RR;
    int minimum;
    int maximum;

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

static void *spa_hssr_worker_main(void *opaque)
{
    struct spa_hssr_control_worker *worker =
        (struct spa_hssr_control_worker *)opaque;
    struct spa_hssr_control_job local_job;
    float local_control[SPA_HSSR_OUTPUTS];
    float local_guard[SPA_HSSR_OUTPUTS];
    uint32_t local_guard_valid = 0;
    float local_eligibility[SPA_MASK_BINS];
    uint32_t local_mask_valid = 0;
    uint32_t active_generation = 0;
    /* Busy: poll every 250 us (jobs arrive every 4 hops = 10.7 ms).  Parked
     * (no job for ~10 ms, e.g. the realtime upmixer while type 103 plays):
     * back off to 4 ms so an idle worker costs ~250 wakeups/s, not 4000. */
    const struct timespec idle = { 0, 250000 };
    const struct timespec parked = { 0, 4000000 };
    uint32_t idle_polls = 0;

    spa_hssr_worker_set_priority(worker->_priority);
    while (!spa_hssr_atomic_load(&worker->_stop)) {
        uint32_t requested =
            spa_hssr_atomic_load(&worker->_requested_generation);
        uint32_t published;
        uint32_t consumed;
        uint32_t next;

        if (requested != active_generation) {
            spa_hssr_state_reset(&worker->_network);
            spa_hssr_state_reset(&worker->_guard_network);
            spa_mask_state_reset(&worker->_mask_network);
            active_generation = requested;
            spa_hssr_atomic_increment(&worker->_resets);
        }

        published = spa_hssr_atomic_load(&worker->_job_published);
        consumed = spa_hssr_atomic_load(&worker->_job_consumed);
        if (published == consumed) {
            if (idle_polls < 40u) idle_polls++;
            (void)nanosleep(idle_polls >= 40u ? &parked : &idle, NULL);
            continue;
        }
        idle_polls = 0;

        next = consumed + 1u;
        memcpy(&local_job, &worker->_jobs[next & 1u],
               sizeof(local_job));
        /* The slot may be reused as soon as the private copy is complete. */
        spa_hssr_atomic_store(&worker->_job_consumed, next);
        requested = spa_hssr_atomic_load(&worker->_requested_generation);
        if (local_job.generation != active_generation ||
            local_job.generation != requested) {
            spa_hssr_atomic_increment(&worker->_stale_discarded);
            continue;
        }

        if (spa_hssr_infer(worker->_model, &worker->_network,
                           local_job.features, local_control) != SPA_HSSR_OK) {
            spa_hssr_atomic_increment(&worker->_stale_discarded);
            continue;
        }
        {
            const struct spa_hssr_model *guard = worker->_guard_model;
            local_guard_valid = guard &&
                spa_hssr_infer(guard, &worker->_guard_network,
                               local_job.features, local_guard) == SPA_HSSR_OK;
        }
        {
            const struct spa_mask_model *mask = worker->_mask_model;
            local_mask_valid = mask && local_job.has_mask &&
                spa_mask_infer(mask, &worker->_mask_network,
                               local_job.mask_features, local_eligibility, NULL) == 0;
        }
        if (spa_hssr_atomic_load(&worker->_requested_generation) !=
                local_job.generation) {
            spa_hssr_atomic_increment(&worker->_stale_discarded);
            continue;
        }

        published = spa_hssr_atomic_load(&worker->_result_published);
        if ((uint32_t)(published - spa_hssr_atomic_load(
                &worker->_result_consumed)) >= 2u) {
            /* Never overwrite a slot while the audio thread may copy it. */
            spa_hssr_atomic_increment(&worker->_stale_discarded);
            continue;
        }
        next = published + 1u;
        worker->_results[next & 1u].generation =
            local_job.generation;
        worker->_results[next & 1u].sequence = local_job.sequence;
        memcpy(worker->_results[next & 1u].control,
               local_control, sizeof(local_control));
        worker->_results[next & 1u].guard_valid = local_guard_valid;
        if (local_guard_valid)
            memcpy(worker->_results[next & 1u].guard_control,
                   local_guard, sizeof(local_guard));
        worker->_results[next & 1u].mask_valid = local_mask_valid;
        if (local_mask_valid)
            memcpy(worker->_results[next & 1u].eligibility,
                   local_eligibility, sizeof(local_eligibility));
        spa_hssr_atomic_store(&worker->_result_published, next);
        spa_hssr_atomic_increment(&worker->_completed);
    }
    return NULL;
}

int spa_hssr_control_worker_start(
    struct spa_hssr_control_worker *worker,
    const struct spa_hssr_model *model,
    uint32_t initial_generation, int priority)
{
    if (!worker || !spa_hssr_model_ready(model))
        return SPA_HSSR_ERROR_ARGUMENT;
    if (!initial_generation) initial_generation = 1u;

    memset(worker, 0, sizeof(*worker));
    worker->_model = model;
    worker->_priority = priority;
    worker->provider.context = worker;
    worker->provider.submit = spa_hssr_worker_submit;
    worker->provider.receive = spa_hssr_worker_receive;
    worker->provider.reset = spa_hssr_worker_reset;
    worker->provider.receive_guard = spa_hssr_worker_receive_guard;
    worker->provider.receive_mask = spa_hssr_worker_receive_mask;
    spa_hssr_atomic_store(&worker->_requested_generation,
                          initial_generation);
    if (pthread_create(&worker->_thread, NULL,
                       spa_hssr_worker_main, worker) != 0) {
        memset(worker, 0, sizeof(*worker));
        return SPA_HSSR_ERROR_MEMORY;
    }
    spa_hssr_atomic_store(&worker->_started, 1u);
    return SPA_HSSR_OK;
}

void spa_hssr_control_worker_set_guard(
    struct spa_hssr_control_worker *worker,
    const struct spa_hssr_model *guard)
{
    if (!worker) return;
    if (guard && !spa_hssr_model_ready(guard)) guard = NULL;
    spa_hssr_state_reset(&worker->_guard_network);
    worker->_guard_model = guard;
}

void spa_hssr_control_worker_set_mask(
    struct spa_hssr_control_worker *worker,
    const struct spa_mask_model *mask)
{
    if (!worker) return;
    worker->_mask_model = mask && spa_mask_model_ready(mask) ? mask : NULL;
}

void spa_hssr_control_worker_stop(struct spa_hssr_control_worker *worker)
{
    if (!worker || !spa_hssr_atomic_load(&worker->_started)) return;
    spa_hssr_atomic_store(&worker->_stop, 1u);
    (void)pthread_join(worker->_thread, NULL);
    spa_hssr_atomic_store(&worker->_started, 0u);
}

const struct spa_hssr_control_provider *spa_hssr_control_worker_provider(
    struct spa_hssr_control_worker *worker)
{
    if (!worker || !spa_hssr_atomic_load(&worker->_started)) return NULL;
    return &worker->provider;
}

void spa_hssr_control_worker_get_stats(
    const struct spa_hssr_control_worker *worker,
    struct spa_hssr_control_worker_stats *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    if (!worker) return;
    stats->submitted = spa_hssr_atomic_load(&worker->_submitted);
    stats->completed = spa_hssr_atomic_load(&worker->_completed);
    stats->busy = spa_hssr_atomic_load(&worker->_busy);
    stats->stale_discarded =
        spa_hssr_atomic_load(&worker->_stale_discarded);
    stats->resets = spa_hssr_atomic_load(&worker->_resets);
}
