/* scene_mask_worker.h — the fine mask net (scene_mask.h) on a persistent thread off the audio thread: the
 * pattern of the retired HSSR control worker (hssr_control_worker.c at tag upmix-universal-last-2026-10-09; spec
 * docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md §18 stage 4 item 4).
 * Two-slot job and result mailboxes, a generation (reset) and a sequence (control) number, stale results
 * discarded, a busy counter; the audio thread never blocks, never allocates, never logs.
 *
 * Timing.  The SCENE backend (stereo_upmix.c spa_scene_process) hands the net its 512 newest stereo frames
 * at every mask control boundary (pushed = 256 + 512 k) and, at the same boundary before submitting,
 * installs the newest completed row through scene_engine_set_mask().  The row of control k is therefore
 * applied from boundary k+1 on: ONE CONTROL (512 frames, 10.7 ms) later than the synchronous path, which
 * installs it at boundary k (stereo_upmix.h SPA_SCENE_MASK_PREFILL_FRAMES), plus one more control for every
 * 10.7 ms the net needs beyond its period.  The engine's instrument smoother (inst_attack_ms 300) makes a
 * control of lateness inaudible; the synchronous path stays selectable (no provider: tools, tests, the unit
 * with AUDIO_SCENE_MASK_SYNC=1) and is the reference the port tests check.
 *
 * Slower than realtime.  One job is in flight and one may wait; a submit while both slots are full is
 * refused (busy++) and its 512 frames never reach the net, which then sees a splice in its 2048-sample
 * history: the next two or three rows are perturbed, the rows that do come are applied later, nothing
 * waits and the engine keeps rendering with the last row.  The stats say how often (busy = rows skipped).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef SCENE_MASK_WORKER_H
#define SCENE_MASK_WORKER_H

#include <pthread.h>
#include <stdint.h>

#include "scene_mask.h"

/* What the audio thread takes from a completed control: the two 257-bin rows and the raw LFE V2
 * posteriors (spa_scene_mask_lfe_posteriors below; the backend smooths them). */
struct spa_scene_mask_row {
    uint32_t sequence;
    float elig[SMASK_BINS];
    float prot[SMASK_BINS];
    float lfe_bass;
    float lfe_drum;
};

/* The backend's view of a mask source (stereo_upmix.h spa_stereo_upmix_config.scene_mask_provider).
 * All three run on the audio thread. */
struct spa_scene_mask_provider {
    void *context;
    /* in512: the newest SMASK_HOP stereo frames, interleaved, in [-1, 1].  1 = queued, 0 = refused
     * (both slots full, or the worker is not running). */
    int (*submit)(void *context, uint32_t generation, uint32_t sequence, const float *in512);
    /* The newest completed row of this generation; older unconsumed rows are dropped (superseded),
     * rows of another generation discarded.  0 = nothing new. */
    int (*receive)(void *context, uint32_t generation, struct spa_scene_mask_row *row);
    /* A new stream: the net resets (history, GRU state, priming) before the first job of this
     * generation runs; results of older generations are discarded. */
    void (*reset)(void *context, uint32_t generation);
};

struct spa_scene_mask_job {
    uint32_t generation;
    uint32_t sequence;
    float in[SMASK_HOP * 2];
};

struct spa_scene_mask_result {
    uint32_t generation;
    struct spa_scene_mask_row row;
};

struct spa_scene_mask_worker_stats {
    uint32_t submitted;        /* jobs accepted into a slot */
    uint32_t completed;        /* jobs the net ran (the first two after a reset prime it and yield no row) */
    uint32_t busy;             /* submits refused with both slots full: rows skipped */
    uint32_t stale_discarded;  /* jobs or rows of a generation the audio thread had already left */
    uint32_t superseded;       /* completed rows dropped because a newer one was ready at the same receive */
    uint32_t resets;
};

/* Fields are public so the hook can embed the complete object (the net is 193 KB) without a heap
 * allocation.  Callers must treat every field except provider as private. */
struct spa_scene_mask_worker {
    pthread_t _thread;
    pthread_mutex_t _wake_lock;
    pthread_cond_t _wake_cond;
    struct scene_mask _net;             /* the worker thread's instance; the audio thread never touches it */
    struct spa_scene_mask_provider provider;
    struct spa_scene_mask_job _jobs[2];
    struct spa_scene_mask_result _results[2];
    uint32_t _active_generation;        /* worker thread only */
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
    volatile uint32_t _superseded;
    volatile uint32_t _resets;
    int _priority;
};

/* No thread: the caller drives spa_scene_mask_worker_step() itself (deterministic tests, the bench). */
#define SPA_SCENE_MASK_WORKER_NO_THREAD (-1)

/* Validates the blob (scene_mask_init), starts the thread.  priority <= 0 keeps the platform default;
 * production uses the retired HSSR controller's bounded SCHED_RR priority (17, below the APAP receive/decode
 * thread).  Returns 0, or -1 on a bad blob or no thread (the worker is then not started). */
int  spa_scene_mask_worker_start(struct spa_scene_mask_worker *worker,
                                 const struct scene_mask_weights *blob,
                                 uint32_t initial_generation, int priority);
void spa_scene_mask_worker_stop(struct spa_scene_mask_worker *worker);
const struct spa_scene_mask_provider *spa_scene_mask_worker_provider(struct spa_scene_mask_worker *worker);
void spa_scene_mask_worker_get_stats(const struct spa_scene_mask_worker *worker,
                                     struct spa_scene_mask_worker_stats *stats);

/* One iteration of the worker loop on the calling thread: a pending reset, then at most one job through the
 * net and into the result mailbox.  Returns 1 when a job was consumed, 0 when there was nothing to do.
 * The thread calls it; a caller that started with SPA_SCENE_MASK_WORKER_NO_THREAD calls it instead. */
int  spa_scene_mask_worker_step(struct spa_scene_mask_worker *worker);

/* Test hook (worker thread): when set, runs in place of scene_mask_control() with the same contract
 * (a test sleeps here before calling the real net, a bench stubs the net).  NULL in production. */
extern int (*spa_scene_mask_worker_test_control)(struct scene_mask *net, const float *in512,
                                                 float *elig257, float *prot257);

/* The LFE V2 steering of a control (spec §18 item 4): the net's bass (post[2]) and drum-core (post[1])
 * posteriors of the 512-point bins 0..1 (0..141 Hz) weighted by the control's fine-spectrum energy in
 * each bin's 93.75-Hz band.  Read on the thread that ran scene_mask_control(), right after it returned
 * 1; the backend smooths the pair per control (spa_scene_lfe_smooth).  Header-only so the synchronous
 * path and the worker compute the identical value without a link dependency. */
static inline void spa_scene_mask_lfe_posteriors(const struct scene_mask *m, float *bass_out, float *drum_out)
{
    float wsum = 0.0f, bass = 0.0f, drum = 0.0f;
    int b, k;

    for (b = 0; b < 2; b++) {
        float w = 1.0e-12f;

        for (k = b ? 4 * b - 2 : 0; k < 4 * b + 2; k++)
            w += m->L[k].re * m->L[k].re + m->L[k].im * m->L[k].im +
                 m->R[k].re * m->R[k].re + m->R[k].im * m->R[k].im;
        bass += w * m->post[2][b];
        drum += w * m->post[1][b];
        wsum += w;
    }
    *bass_out = bass / wsum;
    *drum_out = drum / wsum;
}

#endif /* SCENE_MASK_WORKER_H */
