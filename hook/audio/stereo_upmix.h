/*
 * SCENE stereo upmixer for the fixed MHI2Q PCM6 rail.
 *
 * The output order is always L, R, C, LFE, Ls, Rs.  C is deliberately zero
 * because the Audi amplifier already derives its centre.  The scene renderer
 * (scene_engine.h) makes the front and the rear; its mask net's bass/drum
 * posteriors steer the phase-recovered LFE V2 assist.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef STEREO_UPMIX_H
#define STEREO_UPMIX_H

#include <stddef.h>
#include <stdint.h>

#include "scene_engine.h"   /* before <math.h> everywhere: QNX 6.5 math.h size_t guard */
#include "scene_mask.h"
#include "scene_mask_worker.h"

enum {
    SPA_STEREO_UPMIX_INPUT_CHANNELS = 2,
    SPA_STEREO_UPMIX_OUTPUT_CHANNELS = 6,
    SPA_STEREO_UPMIX_MAX_FRAMES = 1024,
    SPA_STEREO_UPMIX_SAMPLE_RATE = 48000,

    /* Q15 gains.  32768 is unity and is intentionally representable here. */
    SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT = 32670,
    SPA_STEREO_UPMIX_FRONT_Q15_LFE_DEFAULT = 31392,
    /* Relative assist level, NOT the whole subwoofer feed: corrected BANK1
     * also feeds its sub bus from L/R/C/Ls/Rs. 0 dB re4096 does not mean off.
     * The input-matrix guard is not a downstream EQ/acoustic safety model. */
    SPA_STEREO_UPMIX_LFE_Q15_DEFAULT = 4096,
    SPA_STEREO_UPMIX_LFE_Q15_MAX = 16384,
    /* SCENE backend: the engine emits a sample only once its 1024-sample
     * analysis frame is complete, so process_s16 primes its output FIFO with
     * this many zero frames and from then on every call emits exactly the
     * frames it was given.  Output frame i is input frame i - 1024 rendered
     * (21.3 ms; the rear pair adds scene.rear_delay_ms on top). */
    SPA_SCENE_LATENCY_FRAMES = SCENE_NFFT,
    /* FIFO bound: content before a pop is at most SCENE_HOP + frames (the
     * engine never holds more than one hop back once primed); 2048 leaves slack. */
    SPA_SCENE_FIFO_FRAMES = SPA_STEREO_UPMIX_MAX_FRAMES + SCENE_NFFT,
    /* SCENE mask cadence: the net c runtime (scene_mask.h) runs on every
     * SCENE_MASK_HOP (512) input frames, and its row must be installed after
     * 512r-256 and before 512r pushed frames to apply to frames 2r and 2r+1
     * exactly like scene_upmix.py's inst_mask[(t*hop)//512] (scene_engine.c
     * header).  So the accumulator starts half full of zeros: the net's 512-frame
     * calls end at 256, 768, 1280, ... pushed frames, each call installs its row
     * before the next frame is pushed, and the latency stays SPA_SCENE_LATENCY_FRAMES
     * (nothing is held back).  Row r (Python index) is therefore the net's call
     * r-1, which returns its control r-3 (scene_mask.h timing); rows 0..2 do
     * not exist (spatial only until then, exactly as with no net).  With a
     * scene_mask_provider (the worker, scene_mask_worker.h) the same boundary
     * submits the 512 frames and installs the newest row the worker finished:
     * the row of a boundary applies from the next one on, one control (512
     * frames, 10.7 ms) later than here, one more per control the net needs
     * beyond its period; rows the worker could not take (busy) are skipped. */
    SPA_SCENE_MASK_PREFILL_FRAMES = SCENE_MASK_HOP / 2
};

enum spa_stereo_upmix_output_channel {
    SPA_STEREO_UPMIX_CHANNEL_L = 0,
    SPA_STEREO_UPMIX_CHANNEL_R = 1,
    SPA_STEREO_UPMIX_CHANNEL_C = 2,
    SPA_STEREO_UPMIX_CHANNEL_LFE = 3,
    SPA_STEREO_UPMIX_CHANNEL_LS = 4,
    SPA_STEREO_UPMIX_CHANNEL_RS = 5
};


/* OFF is a safety bypass, not an alternative upmix profile.  SCENE is the
 * scene renderer of scene_engine.h (docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md
 * §18): it needs no model beyond the optional mask blob, emits the engine's own
 * levels (L/R = the input minus the transferred objects, Ls/Rs = the engine's
 * rear, all at unity scale, as the car kit renders), lags by
 * SPA_SCENE_LATENCY_FRAMES and keeps the LFE V2 assist and the cabin guard.
 * front_gain_q15 is only the LFE assist's paired front headroom (init sets it
 * to unity when the assist is off). */
enum spa_stereo_upmix_backend {
    SPA_STEREO_UPMIX_OFF = 0,
    SPA_STEREO_UPMIX_SCENE = 1
};

enum spa_stereo_upmix_status {
    SPA_STEREO_UPMIX_OK = 0,
    SPA_STEREO_UPMIX_ERROR_ARGUMENT = -1,
    SPA_STEREO_UPMIX_ERROR_CONFIG = -2,
    SPA_STEREO_UPMIX_ERROR_UNSUPPORTED = -3,
    SPA_STEREO_UPMIX_ERROR_NOT_INITIALIZED = -4,
    SPA_STEREO_UPMIX_ERROR_BLOCK_TOO_LARGE = -5,
    SPA_STEREO_UPMIX_ERROR_OUTPUT_TOO_SMALL = -6,
    SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP = -7
};

struct spa_stereo_upmix_config {
    uint32_t sample_rate_hz;
    uint32_t front_gain_q15;
    uint32_t lfe_gain_q15;
    uint32_t lfe_enabled;
    enum spa_stereo_upmix_backend backend;
    /* SCENE backend: the engine's SceneConfig, variant J defaults from
     * scene_config_defaults().  The hook's AUDIO_SCENE_* knobs go through the
     * spa_stereo_upmix_config_set_scene_* setters below (clamped); init()
     * rejects n_fft/hop other than 1024/256 and out-of-range knobs. */
    struct scene_config scene;
    /* SCENE: the fine mask net's weights (scene_mask.h, 'SMSKNET1'), caller
     * owned and alive for the state's lifetime; NULL = spatial only (no mask,
     * duck off).  Set through spa_stereo_upmix_config_set_scene_mask_blob(). */
    const struct scene_mask_weights *scene_mask_blob;
    /* SCENE: the mask net on a worker (scene_mask_worker.h), caller owned and
     * started on the blob before init(); NULL = the net runs synchronously on
     * the audio thread from scene_mask_blob (tools, tests, AUDIO_SCENE_MASK_SYNC).
     * With a provider the blob is the worker's business and may be NULL here.
     * Set through spa_stereo_upmix_config_set_scene_mask_provider(). */
    const struct spa_scene_mask_provider *scene_mask_provider;
    /* SCENE: the starting level (enum spa_upmix_level, below), NORMAL by default;
     * spa_stereo_upmix_set_level() changes it on the running state. */
    int level;
};

/* Runtime details are public only so callers can embed the complete state and
 * the realtime path never has to allocate.  Callers must not inspect or alter
 * fields whose names start with an underscore. */
struct spa_upmix_biquad_coefficients {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
};

struct spa_stereo_upmix_runtime {
    /* Shared 60-Hz LR4 filters (kLfeLow); V2 adds phase recovery/dynamics. */
    float _lfe_low48_state[2][2][2];
    float _lfe_power_l;
    float _lfe_power_r;
    float _lfe_cross;
    float _lfe_polarity;
    float _lfe_target_polarity;
    float _lfe_fast_envelope;
    float _lfe_slow_envelope;
    float _lfe_dynamic_gain;
    uint32_t _lfe_correlation_tick;
    /* SCENE backend: the engine (265 KB, so it lives here, never on a stack)
     * and its output FIFO in engine order L R C LFE Ls Rs, float [-1,1]. */
    struct scene_state _scene;
    float _scene_fifo[SPA_SCENE_FIFO_FRAMES * SPA_STEREO_UPMIX_OUTPUT_CHANNELS];
    uint32_t _scene_fifo_count;
    /* the stereo input SPA_SCENE_LATENCY_FRAMES late: the bypass side of the level fade */
    int16_t _dry_ring[SPA_SCENE_LATENCY_FRAMES * SPA_STEREO_UPMIX_INPUT_CHANNELS];
    uint32_t _dry_pos;
    /* SCENE mask runtime (193 KB) and its 512-frame accumulator; _scene_mask_on
     * only when a blob was configured and accepted.  The LFE V2 confidences of
     * the current control are held for its frames. */
    struct scene_mask _scene_mask;
    float _scene_mask_acc[SCENE_MASK_HOP * SPA_STEREO_UPMIX_INPUT_CHANNELS];
    uint32_t _scene_mask_fill;
    uint32_t _scene_mask_on;
    uint64_t _scene_pushed;          /* input frames pushed since arm (cadence bookkeeping) */
    float _scene_lfe_bass;
    float _scene_lfe_drum;
    /* SCENE with a provider: the row received at the last boundary, the
     * worker's generation (bumped by reset(), forwarded through the provider)
     * and the sequence of the last submitted control. */
    struct spa_scene_mask_row _scene_mask_row;
    uint32_t _scene_mask_generation;
    uint32_t _scene_mask_sequence;
};

struct spa_stereo_upmix {
    struct spa_stereo_upmix_config config;
    uint32_t _initialized;
    struct spa_stereo_upmix_runtime _runtime;
    /* SCENE level (spa_stereo_upmix_set_level): requested by any thread, applied
     * by the audio thread at the next process call; _wet fades 0 (bypass) .. 1. */
    volatile int _level_req;
    int _level_applied;
    float _wet;
    /* the engine tuning (alpha, inst_alpha, duck_depth, side_lp_hz) now and the
     * level's target: a change glides over SPA_UPMIX_LEVEL_FADE_MS, not a step */
    float _tune[4];
    float _tune_from[4];
    float _tune_target[4];
    float _tune_t;                   /* 0..1 through the glide */

    /* A complete 1024-frame stereo packet is staged before output starts.
     * This makes widening safe even when the PCM2 and PCM6 ranges overlap. */
    int16_t _input_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                           SPA_STEREO_UPMIX_INPUT_CHANNELS];
    float _scene_input_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                               SPA_STEREO_UPMIX_INPUT_CHANNELS];
};

/* The reusable default is the exact OFF bypass with the variant J scene
 * defaults; production selects SCENE. */
void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config);

/* SCENE upmix level, the MMI "CarPlay UpMix" choice.  OFF is the bypass ([L, R, 0, 0,
 * 0, 0], still SPA_SCENE_LATENCY_FRAMES late so the switch never jumps in time); NORMAL
 * is the configured scene (the AUDIO_SCENE_* knobs); the others are fixed tunings, rear
 * delay always as configured.  Every change takes SPA_UPMIX_LEVEL_FADE_MS: to and from
 * OFF an ease in/out crossfade with the plain stereo, between the others a linear glide
 * of the tuning.  A new stream (init/reset) starts at its level directly. */
enum spa_upmix_level {
    SPA_UPMIX_LEVEL_OFF = 0,
    SPA_UPMIX_LEVEL_MODERATE = 1,
    SPA_UPMIX_LEVEL_NORMAL = 2,
    SPA_UPMIX_LEVEL_FULL = 3,
    SPA_UPMIX_LEVEL_AGGRESSIVE = 4,
    SPA_UPMIX_LEVEL_COUNT = 5
};
#define SPA_UPMIX_LEVEL_FADE_MS 300
/* Safe from any thread (one int store); out of range is ignored. */
void spa_stereo_upmix_set_level(struct spa_stereo_upmix *state, int level);

/* Front ~=0.958 with LFE V2, otherwise ~=0.997; the dynamic limiter still bounds output. */
void spa_stereo_upmix_config_set_lfe(
    struct spa_stereo_upmix_config *config, int enabled);

/* SCENE knobs (the AUDIO_SCENE_* environment), each clamped to its range:
 * alpha 0..1 (transfer fraction), inst_alpha 0..1, inst_gamma 0.1..4,
 * duck_depth 0..1, rear_delay_ms -50..50 (signed: < 0 delays the front),
 * sigma_min 0.01..1, side_lp_hz 0 (off) or 1000..24000, bleed_cancel 0..0.5. */
void spa_stereo_upmix_config_set_scene_alpha(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_inst_alpha(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_inst_gamma(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_duck_depth(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_rear_delay_ms(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_sigma_min(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_side_lp_hz(struct spa_stereo_upmix_config *config, float value);
void spa_stereo_upmix_config_set_scene_bleed_cancel(struct spa_stereo_upmix_config *config, float value);

/* SCENE only: install the mask row of the control period that starts now
 * (scene_engine_set_mask: elig/prot[bins] in 0..1, bins 257 or 1025, either
 * NULL).  With a blob configured the backend's own mask runtime does this once
 * per SCENE_MASK_HOP (512) input frames (SPA_SCENE_MASK_PREFILL_FRAMES above);
 * this entry is for tools and tests feeding external rows.  Not realtime-safe
 * against a concurrent process_s16: call it from the audio thread between
 * calls.  Returns OK, NOT_INITIALIZED, or UNSUPPORTED for other backends. */
int spa_stereo_upmix_scene_set_mask(struct spa_stereo_upmix *state,
                                    const float *elig, const float *prot,
                                    int bins);

/* SCENE: the mask net's weight blob (the file written by
 * tools/stereo-upmix-render/port/export_masknet_weights.py, read verbatim),
 * caller owned.  Stored only when bytes == sizeof(struct scene_mask_weights)
 * and the magic and dimensions are the runtime's (scene_mask.h); anything else
 * (NULL included) leaves the config without a mask. */
void spa_stereo_upmix_config_set_scene_mask_blob(struct spa_stereo_upmix_config *config,
                                                 const void *blob, size_t bytes);

/* SCENE: hand the mask net to a worker (spa_scene_mask_worker_provider()).
 * NULL keeps the exact synchronous runtime the tools and the port tests use.
 * Timing with a worker: scene_mask_worker.h. */
void spa_stereo_upmix_config_set_scene_mask_provider(struct spa_stereo_upmix_config *config,
                                                     const struct spa_scene_mask_provider *provider);

/* SCENE: a track change (Now Playing) -> scene_engine_reset(): the next frame
 * fades every place and clears the song memory; statistics, holds and the mask
 * net stream carry on.  Audio-thread only (it touches the engine state): the
 * hook calls it from the thread that also calls process_s16, under the same
 * lock -- audio_hook.c: the type-103 path on a changed APAP transition id
 * inside spa103_observe_extension_locked() (render lock held), the realtime
 * path from spa_rt_upmix_period_locked() (sink lock held) on a flag that the
 * iAP2 NowPlayingUpdate handler (Cinemo's receive thread) raises when the
 * MediaItemPersistentIdentifier changes.  Returns OK, NOT_INITIALIZED, or
 * UNSUPPORTED for other backends (a no-op there). */
int spa_stereo_upmix_scene_track_change(struct spa_stereo_upmix *state);

/* The exact OFF bypass and the SCENE renderer. */
int spa_stereo_upmix_backend_supported(enum spa_stereo_upmix_backend backend);

int spa_stereo_upmix_init(struct spa_stereo_upmix *state,
                          const struct spa_stereo_upmix_config *config);
int spa_stereo_upmix_reset(struct spa_stereo_upmix *state);

/* Convert interleaved PCM2/S16 into interleaved PCM6/S16.  OFF emits
 * [input L, input R, 0, 0, 0, 0] bit-exactly regardless of configured gains.
 * SCENE emits [front L, front R, 0, optional LFE, Ls, Rs]
 * SPA_SCENE_LATENCY_FRAMES late: the first 1024 output frames
 * of a stream are zeros, then output frame i is the engine's render of input
 * frame i - 1024; every call still emits exactly `frames` frames.
 *
 * frames must be <= SPA_STEREO_UPMIX_MAX_FRAMES and pcm6_capacity_frames is a
 * frame count.  The complete PCM input may overlap output, including exact
 * in-place use.  Neither PCM range may overlap the state object.  Validation
 * errors leave output and DSP state unchanged.  Processing allocates nothing. */
int spa_stereo_upmix_process_s16(struct spa_stereo_upmix *state,
                                 const int16_t *pcm2,
                                 size_t frames,
                                 int16_t *pcm6,
                                 size_t pcm6_capacity_frames);


/* Profile-0/bank-1 J525 input matrix, first six inputs x eight buses, row-major.
 * Static storage. Includes the verified -99 dB effective input-matrix floor.
 * For downstream peak detection only; not an acoustic model or an EQ inverse. */
const float *spa_stereo_upmix_input_matrix(void);

#ifdef SPA_STEREO_UPMIX_TESTING
/* Test-only access to the cabin guard. */
void spa_stereo_upmix_test_guard_s16(int16_t sample[6], int adjust_lfe);
/* Test-only: observes every SCENE mask control boundary, right after the
 * net ran on its 512 frames and before any further frame is pushed: pushed =
 * input frames pushed so far (256, 768, 1280, ...), elig/prot = the 257-bin
 * rows installed for the frames that follow, both NULL while the net is
 * priming (nothing installed).  NULL = off.  tools/stereo-upmix-render/
 * scene_render_hook.c --mask-rows writes them for port/test_hook_render.py. */
extern void (*spa_stereo_upmix_test_scene_mask_observer)(uint64_t pushed,
                                                         const float *elig,
                                                         const float *prot);
#endif

#endif /* STEREO_UPMIX_H */
