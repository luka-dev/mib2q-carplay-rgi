/*
 * Persistent off-audio-thread worker for the HSSR recurrent controller.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef HSSR_CONTROL_WORKER_H
#define HSSR_CONTROL_WORKER_H

#include <pthread.h>
#include <stdint.h>

#include "hssr_dsp.h"

struct spa_hssr_control_job {
    uint32_t generation;
    uint32_t sequence;
    float features[SPA_HSSR_FEATURES];
    uint32_t has_mask;
    float mask_features[SPA_MASK_BINS * SPA_MASK_FEATURES];
};

struct spa_hssr_control_result {
    uint32_t generation;
    uint32_t sequence;
    float control[SPA_HSSR_OUTPUTS];
    float guard_control[SPA_HSSR_OUTPUTS];
    uint32_t guard_valid;
    float eligibility[SPA_MASK_BINS];
    uint32_t mask_valid;
};

struct spa_hssr_control_worker_stats {
    uint32_t submitted;
    uint32_t completed;
    uint32_t busy;
    uint32_t stale_discarded;
    uint32_t resets;
};

/* Fields are public so the hook can embed the complete object without a heap
 * allocation.  Callers must treat every field except provider as private. */
struct spa_hssr_control_worker {
    pthread_t _thread;
    const struct spa_hssr_model *_model;
    struct spa_hssr_state _network;
    const struct spa_hssr_model *_guard_model;   /* optional second model, see set_guard */
    struct spa_hssr_state _guard_network;
    const struct spa_mask_model *_mask_model;    /* optional mask net, see set_mask */
    struct spa_mask_state _mask_network;
    struct spa_hssr_control_provider provider;
    struct spa_hssr_control_job _jobs[2];
    struct spa_hssr_control_result _results[2];
    volatile uint32_t _stop;
    volatile uint32_t _started;
    volatile uint32_t _requested_generation;
    volatile uint32_t _job_published;
    volatile uint32_t _job_consumed;
    volatile uint32_t _result_published;
    volatile uint32_t _result_consumed;
    volatile uint32_t _submitted;
    volatile uint32_t _completed;
    volatile uint32_t _busy;
    volatile uint32_t _stale_discarded;
    volatile uint32_t _resets;
    int _priority;
};

/* priority <= 0 keeps the platform default.  Production uses a bounded RR
 * priority below the APAP receive/decode thread. */
int spa_hssr_control_worker_start(
    struct spa_hssr_control_worker *worker,
    const struct spa_hssr_model *model,
    uint32_t initial_generation, int priority);
void spa_hssr_control_worker_stop(struct spa_hssr_control_worker *worker);
/* Optional guard model run after the main one on every job; NULL disables. */
void spa_hssr_control_worker_set_guard(
    struct spa_hssr_control_worker *worker,
    const struct spa_hssr_model *guard);
/* Optional mask net run after the controllers on every job that carries mask
 * features; NULL disables. */
void spa_hssr_control_worker_set_mask(
    struct spa_hssr_control_worker *worker,
    const struct spa_mask_model *mask);
const struct spa_hssr_control_provider *spa_hssr_control_worker_provider(
    struct spa_hssr_control_worker *worker);
void spa_hssr_control_worker_get_stats(
    const struct spa_hssr_control_worker *worker,
    struct spa_hssr_control_worker_stats *stats);

#endif /* HSSR_CONTROL_WORKER_H */
