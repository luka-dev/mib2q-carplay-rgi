/*
 * HSSR Universal stereo upmixer for the fixed MHI2Q PCM6 rail.
 *
 * The output order is always L, R, C, LFE, Ls, Rs.  C is deliberately zero
 * because the Audi amplifier already derives its centre. Universal feeds the
 * phase-recovered LFE V2 assist.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef STEREO_UPMIX_H
#define STEREO_UPMIX_H

#include <stddef.h>
#include <stdint.h>

#include "hssr_dsp.h"
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
    SPA_STEREO_UPMIX_REAR_Q15_DEFAULT = 5898,
    /* Scene build: rear may run up to 0.5 (-6 dB) and is delayed 12 ms so the
     * front anchors the image (precedence) and the rear reads as a second scene. */
    SPA_STEREO_UPMIX_REAR_Q15_MAX = 16384,
    SPA_SCENE_REAR_DELAY_FRAMES = 576,
    /* Relative assist level, NOT the whole subwoofer feed: corrected BANK1
     * also feeds its sub bus from L/R/C/Ls/Rs. 0 dB re4096 does not mean off.
     * The input-matrix guard is not a downstream EQ/acoustic safety model. */
    SPA_STEREO_UPMIX_LFE_Q15_DEFAULT = 4096,
    SPA_STEREO_UPMIX_LFE_Q15_MAX = 16384,
    SPA_STEREO_UPMIX_FRONT_BASS_HZ = 200,
    SPA_STEREO_UPMIX_FRONT_TREBLE_HZ = 4000,
    SPA_STEREO_UPMIX_FRONT_BASS_DB10_MIN = -120,
    /* Scene rear low shelf @500 Hz ("body stays in front"), tenths of dB. */
    SPA_STEREO_UPMIX_REAR_BODY_DB10_DEFAULT = -40,
    /* Vocal gate strength, Q15 (0.95): see spa_hssr_dsp.vocal_gate. */
    SPA_STEREO_UPMIX_VOCAL_GATE_Q15_DEFAULT = 31130,
    /* Front retention taper defaults: the retention benefit measured on real
     * cuts is the 500-2000 Hz front/centre body, so keep it there and let the
     * treble object leave the front entirely. */
    SPA_STEREO_UPMIX_RETENTION_HF_LO_HZ = 1000,
    SPA_STEREO_UPMIX_RETENTION_HF_HI_HZ = 2500,
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
 * §18): it needs no model, emits the engine's own levels (L/R = the input minus
 * the transferred objects, Ls/Rs = the engine's rear, all at unity scale, as the
 * car kit renders), lags by SPA_SCENE_LATENCY_FRAMES and keeps the LFE V2
 * assist and the cabin guard.  rear_gain_q15, the front bass/treble shelves,
 * rear_body, retention, the bed and the HSSR mask knobs are Universal-only and
 * ignored; front_gain_q15 is only the LFE assist's paired front headroom (init
 * sets it to unity when the assist is off). */
enum spa_stereo_upmix_backend {
    SPA_STEREO_UPMIX_OFF = 0,
    SPA_STEREO_UPMIX_HSSR_UNIVERSAL = 1,
    SPA_STEREO_UPMIX_SCENE = 2
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
    uint32_t rear_gain_q15;
    uint32_t lfe_gain_q15;
    uint32_t lfe_enabled;
    /* Experimental independent front retention: 0 = existing source removal,
     * 32768 = no source removal. Does not change rear synthesis gain. */
    uint32_t front_retention_q15;
    /* Front-only bass trim: RBJ low shelf at SPA_STEREO_UPMIX_FRONT_BASS_HZ,
     * tenths of dB, 0 = off, never above 0. LFE/rear synthesis is unchanged,
     * but amp sub/rear outputs also receive L/R and can change. Linked safety
     * attenuation can affect all PCM channels. */
    int32_t front_bass_db10;
    /* Front-only treble trim: RBJ high shelf at SPA_STEREO_UPMIX_FRONT_TREBLE_HZ,
     * tenths of dB (-120..0, 0 = off).  A static tone control for the front
     * pair, the lever for "the front is too bright" that does not touch the
     * object decisions (retention and the taper do, and they flicker). */
    int32_t front_treble_db10;
    /* Rear object low shelf @500 Hz, tenths of dB (-120..0, default -40).  It
     * also shapes what is subtracted from the front, so it tilts the object's
     * front/rear split with frequency: low notes stay front, high notes go
     * back (tools/stereo-upmix-render/upmix_behavior_tests.py octave_jump). */
    int32_t rear_body_db10;
    /* Scene-bed distribution experiments, see
     * docs/audio/UPMIX_CALIBRATION_AND_TUNING.md.  All zero keeps the
     * legacy renderer bit-exact.  With bed_attack the front retention is
     * applied per band inside HSSR instead of the time-domain transfer. */
    uint32_t bed_delays;             /* 0/1: per-band delayed bed copies + cross term */
    uint32_t bed_attack;             /* enum spa_hssr_bed_attack */
    uint32_t attack_collapse_q15;    /* OWNER: object front share r -> r*(1-c*w) on a hit */
    /* Front retention taper, Hz: full retention below lo, none from hi up, so
     * the front keeps body without keeping treble.  hi <= lo = flat (no
     * taper).  Ignored unless bed_attack is set. */
    uint32_t retention_hf_lo_hz;
    uint32_t retention_hf_hi_hz;
    /* 0..32768: instant per-band vocal veto on the rendered object (0 = off). */
    uint32_t vocal_gate_q15;
    /* 0..32768: rear air bed gain (0 = off), see spa_hssr_dsp.rear_air. */
    uint32_t rear_air_q15;
    enum spa_stereo_upmix_backend backend;
    const struct spa_hssr_model *hssr_model;
    const struct spa_hssr_control_provider *hssr_control_provider;
    const struct spa_hssr_model *hssr_guard_model;   /* scene builds: V11 vocal safety net, optional */
    /* Phase 1 mask net (optional) and its drive gain, Q15 (0 = off, 32768 = 1.0). */
    const struct spa_mask_model *hssr_mask_model;
    uint32_t mask_drive_q15;
    int32_t mask_treble_db10;      /* -120..120: the mask drive's shelf above ~2.5 kHz, 0 = flat */
    int32_t mask_gamma_pct;        /* 10..100: eligibility exponent in percent, 100 = as the net says */
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
    /* +3 dB / 4-kHz rear presence shelf on the separated Ls/Rs objects. */
    float _rear_high_shelf_state[2][2];
    float _limiter_gain;
    float _scene_rear_delay[2][SPA_SCENE_REAR_DELAY_FRAMES];
    uint32_t _scene_rear_pos;
    float _scene_rear_shelf_state[2][2];
    float _scene_source_shelf_state[2][2];
    struct spa_upmix_biquad_coefficients _front_bass_shelf;
    struct spa_upmix_biquad_coefficients _front_treble_shelf;
    float _front_treble_state[2][2];
    struct spa_upmix_biquad_coefficients _scene_rear_shelf;
    float _front_bass_state[2][2];
    struct spa_hssr_dsp _hssr;
    /* SCENE backend: the engine (265 KB, so it lives here, never on a stack)
     * and its output FIFO in engine order L R C LFE Ls Rs, float [-1,1]. */
    struct scene_state _scene;
    float _scene_fifo[SPA_SCENE_FIFO_FRAMES * SPA_STEREO_UPMIX_OUTPUT_CHANNELS];
    uint32_t _scene_fifo_count;
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

    /* A complete 1024-frame stereo packet is staged before output starts.
     * This makes widening safe even when the PCM2 and PCM6 ranges overlap. */
    int16_t _input_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                           SPA_STEREO_UPMIX_INPUT_CHANNELS];
    int16_t _hssr_front_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                                SPA_STEREO_UPMIX_INPUT_CHANNELS];
    float _hssr_rear_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                             SPA_STEREO_UPMIX_INPUT_CHANNELS];
    float _hssr_lfe_control_scratch[SPA_STEREO_UPMIX_MAX_FRAMES * 2];
    float _hssr_source_scratch[SPA_STEREO_UPMIX_MAX_FRAMES * 2];
    float _scene_input_scratch[SPA_STEREO_UPMIX_MAX_FRAMES *
                               SPA_STEREO_UPMIX_INPUT_CHANNELS];
};

/* The reusable default is the exact OFF bypass.  Production selects Universal
 * after attaching its validated model. */
void spa_stereo_upmix_default_config(struct spa_stereo_upmix_config *config);

/* Front ~=0.958 with LFE V2, otherwise ~=0.997; the dynamic limiter still bounds output. */
void spa_stereo_upmix_config_set_lfe(
    struct spa_stereo_upmix_config *config, int enabled);

/* The caller owns the model and keeps it alive for the upmix state lifetime. */
void spa_stereo_upmix_config_set_hssr_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model);

/* Attach a hook-owned asynchronous controller.  NULL retains the exact
 * synchronous renderer used by tools and deterministic regression tests. */
void spa_stereo_upmix_config_set_hssr_guard_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_model *model);
void spa_stereo_upmix_config_set_hssr_mask_gamma(struct spa_stereo_upmix_config *config,
                                                 int32_t gamma_pct);
void spa_stereo_upmix_config_set_hssr_mask_treble(struct spa_stereo_upmix_config *config,
                                                  int32_t treble_db10);
void spa_stereo_upmix_config_set_hssr_mask_model(
    struct spa_stereo_upmix_config *config,
    const struct spa_mask_model *model, uint32_t drive_q15);
void spa_stereo_upmix_config_set_hssr_control_provider(
    struct spa_stereo_upmix_config *config,
    const struct spa_hssr_control_provider *provider);

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

/* The exact OFF bypass, the production Universal renderer (needs a validated
 * model) and the SCENE renderer (needs none). */
int spa_stereo_upmix_backend_supported(enum spa_stereo_upmix_backend backend);

int spa_stereo_upmix_init(struct spa_stereo_upmix *state,
                          const struct spa_stereo_upmix_config *config);
int spa_stereo_upmix_reset(struct spa_stereo_upmix *state);

/* Convert interleaved PCM2/S16 into interleaved PCM6/S16.  OFF emits
 * [input L, input R, 0, 0, 0, 0] bit-exactly regardless of configured gains.
 * Universal emits [front L, front R, 0, optional LFE, Ls, Rs].  SCENE emits
 * the same layout SPA_SCENE_LATENCY_FRAMES late: the first 1024 output frames
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
/* Test-only access to the exact shipping rear shelf. */
float spa_stereo_upmix_test_rear_high_shelf(float input, float state[2]);
void spa_stereo_upmix_test_guard_s16(int16_t sample[6], int adjust_lfe);
int16_t spa_stereo_upmix_test_transfer(int16_t front, int16_t rear,
                                      uint32_t transfer, uint32_t limit);
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
