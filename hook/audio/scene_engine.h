/* scene_engine.h — the scene upmix renderer in C (variant J), the port target of
 * tools/stereo-upmix-render/scene_upmix.py (spec docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md §4–§18).
 *
 * Contract:
 *  - The Python module is the reference. `scene_upmix._engine()` is the per-frame algorithm, `SceneStream`
 *    the block driver; `tools/stereo-upmix-render/scene_port_fixtures.py` writes the goldens (functions.npz,
 *    stream_spatial.npz, stream_mask.npz): every per-frame output here must match them (float32 port:
 *    intermediates within 1e-4 relative, the [n,6] output below -60 dB of its RMS against stream_render).
 *  - float32 arithmetic, no FMA (the QNX/ARMv7 build has none; host builds use -ffp-contract=off), no
 *    allocation after scene_engine_init(), no dependency on the rest of the hook (standalone testable).
 *  - One instance = one stereo stream. Sample rate 48 kHz, analysis 1024/256 (SCENE_NFFT/SCENE_HOP).
 *  - Masks: the hook's mask net emits one row per 512 input samples (SCENE_MASK_HOP) on the 257-bin grid
 *    of a 512-point analysis (net c analyses 2048 points and strides down to it). scene_engine_set_mask() installs the row that
 *    applies to every frame t with (t * SCENE_HOP) / SCENE_MASK_HOP == row index, exactly like the Python
 *    indexing of inst_mask[ctrl]; when no mask is set the semantic stage and the duck are off (spatial only).
 */
#ifndef SCENE_ENGINE_H
#define SCENE_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#define SCENE_SR            48000
#define SCENE_NFFT          1024
#define SCENE_HOP           256
#define SCENE_BINS          (SCENE_NFFT / 2 + 1)     /* 513 */
#define SCENE_GROUPS_MAX    160                       /* erb_groups(513, 1024) yields 134 */
#define SCENE_PLACES_MAX    8                         /* clusters kept per frame (places + dying) */
#define SCENE_MEMORY_MAX    16                        /* song memory of admitted pans */
#define SCENE_HIST_BINS     21
#define SCENE_MASK_HOP      512
#define SCENE_MASK_BINS_MAX 1025
#define SCENE_RING          (4 * SCENE_NFFT)          /* overlap-add ring, pad-index space */
#define SCENE_DELAY_MAX     2400                      /* 50 ms of signed rear/front delay */
#define SCENE_MAX_BLOCK     1024                      /* frames per push (the hook's packet) */
#define SCENE_RAMP_HOPS     32

typedef struct { float re, im; } scene_cpx;

/* Mirrors SceneConfig (scene_upmix.py) field for field; scene_config_defaults() fills the defaults
 * listed here (variant J: rear_delay_ms 0, inst_gamma 1, inst_alpha 0.9, duck_depth 0.25). */
struct scene_config {
    int   n_fft                   ; /* default 1024 */
    int   hop                     ; /* default 256 */
    float tau_stats_ms            ; /* default 43.0f */
    float tau_fast_ms             ; /* default 8.0f */
    float tau_dir_ms              ; /* default 300.0f */
    int   erb_pool                ; /* default 1 */
    float pan_lo                  ; /* default 0.1f */
    float pan_hi                  ; /* default 0.35f */
    float tau_slow_ms             ; /* default 400.0f */
    float release_ms              ; /* default 2000.0f */
    int   fast_protect            ; /* default 1 */
    int   cluster                 ; /* default 1 */
    float hist_tau_s              ; /* default 3.0f */
    float pick_every_s            ; /* default 0.34f */
    float admit_s                 ; /* default 1.5f */
    float admit_mass              ; /* default 0.08f */
    float drop_mass               ; /* default 0.02f */
    float drop_s                  ; /* default 30.0f */
    float min_abs_pan             ; /* default 0.18f */
    float peak_sep                ; /* default 0.25f */
    int   k_max                   ; /* default 3 */
    float centre_tau_s            ; /* default 2.0f */
    float centre_slew_per_s       ; /* default 0.05f */
    float ramp_s                  ; /* default 0.6f */
    float sigma_min               ; /* default 0.08f */
    int   member_mass             ; /* default 1 */
    float sigma_max               ; /* default 0.25f */
    float front_floor             ; /* default 0.2f */
    float theta_max_deg           ; /* default 120.0f */
    int   unmix                   ; /* default 1 */
    float unmix_lambda            ; /* default 0.05f */
    float alpha                   ; /* default 0.7f */
    int   energy_conserving       ; /* default 1 */
    int   front_follows_slow      ; /* default 1 ("slow"; 0 = "fast") */
    float ambient_gain            ; /* default 1.0f */
    float ambient_gain_lowmid     ; /* default 0.7f */
    float surround                ; /* default 0.7f */
    float air_db                  ; /* default -9.0f */
    float hp_hz                   ; /* default 190.0f */
    float same_sign_below_hz      ; /* default 550.0f */
    float side_lp_hz              ; /* default 7000.0f */
    int   decorrelate_ambient     ; /* default 1 */
    float rear_delay_ms           ; /* default 0.0f */
    float bleed_cancel            ; /* default 0.0f */
    float bleed_cancel_offset_ms  ; /* default 0.0f */
    float front_keep_db           ; /* default 0.0f */
    int   wide_pair               ; /* default 0 */
    float wide_pair_alpha         ; /* default 0.5f */
    float wide_env_tau_ms         ; /* default 300.0f */
    float duck_depth              ; /* default 0.25f */
    float inst_alpha              ; /* default 0.9f */
    float inst_attack_ms          ; /* default 300.0f */
    float inst_release_ms         ; /* default 30.0f */
    int   inst_mask_nfft          ; /* default 512 */
    int   inst_mask_hop           ; /* default 512; must be SCENE_MASK_HOP (init refuses others) */
    float inst_gamma              ; /* default 1.0f */
    float inst_thresh             ; /* default 0.0f */
    float inst_gamma_lo           ; /* default 0.0f */
    float inst_gamma_split_hz     ; /* default 700.0f */
    float memory_boost            ; /* default 0.75f */
    float memory_s                ; /* default 120.0f */
    float inst_long_tau_s         ; /* default 0.0f */
    float inst_long_lo            ; /* default 0.15f */
    float inst_long_hi            ; /* default 0.4f */
    int   duck_from_mask          ; /* default 1 */
    float duck_attack_ms          ; /* default 30.0f */
    float duck_release_ms         ; /* default 300.0f */
    int   seed                    ; /* default 7 */
};

struct scene_place {              /* StageClusters.clusters[i] */
    float centre, sigma, mass, seen_s, absent_s, gain;
    int   admitted, present, dying;
};

struct scene_memory { float centre, last_s; };

struct scene_clusters {           /* StageClusters */
    float hist[SCENE_HIST_BINS];
    float centres_h[SCENE_HIST_BINS];
    float a_hist, a_centre, frame_s, dt_pick;
    int   pick_every, frame, n;
    struct scene_place c[SCENE_PLACES_MAX];
    struct scene_memory memory[SCENE_MEMORY_MAX];
    int   n_memory;
    float song_s, tot_ref;
    int   silent;
    /* membership() outputs of the last call, used by _shadow() and the unmix pan */
    float last_pc[SCENE_BINS];
    float last_p[SCENE_BINS], last_e[SCENE_BINS];
    int   have_last;
    struct scene_config cfg;      /* copy taken by scene_clusters_init(): update()/membership() take only this struct */
};

struct scene_state {
    struct scene_config cfg;
    int   F, G;                                   /* bins, ERB groups */
    /* tables (filled at init) */
    float     freqs[SCENE_BINS];
    int       group[SCENE_BINS];                  /* bin -> ERB group */
    int       group_count[SCENE_GROUPS_MAX];      /* bins per group */
    float     group_freq[SCENE_GROUPS_MAX];       /* mean frequency per group */
    float     hp[SCENE_BINS], GA[SCENE_BINS], air[SCENE_BINS];
    int       same_sign[SCENE_BINS];
    scene_cpx phL[SCENE_BINS], phR[SCENE_BINS];
    float     window[SCENE_NFFT];
    float     a_s, a_fast, a_dir, a_slow, a_rel, a_env, a_iatt, a_irel, a_long, a_datt, a_drel, theta_max, cos_theta_max;
    float     a_anc_meas, a_anc_att, a_anc_rel;      /* front anchor: demand statistics, gain down, gain back */
    int       mbin[SCENE_BINS];                   /* bin -> mask bin (nearest), both masks */
    float     gamma_vec[SCENE_BINS];              /* per-bin eligibility exponent */
    /* per-bin statistics (three EMA sets, slow pan, holds, mask smoothers) */
    float     PL[SCENE_BINS], PR[SCENE_BINS]; scene_cpx C[SCENE_BINS];
    float     fPL[SCENE_BINS], fPR[SCENE_BINS]; scene_cpx fC[SCENE_BINS];
    float     dPL[SCENE_BINS], dPR[SCENE_BINS]; scene_cpx dC[SCENE_BINS];
    float     pb_num[SCENE_BINS], pb_den[SCENE_BINS], p_slow_bin[SCENE_BINS];
    float     w_hold[SCENE_BINS], q_hold[SCENE_BINS], a_hold[SCENE_BINS], pc_hold[SCENE_BINS];
    float     e_inst[SCENE_BINS], e_long[SCENE_BINS], d_prot[SCENE_BINS];
    /* per-group statistics */
    float     p_slow[SCENE_GROUPS_MAX], ps_num[SCENE_GROUPS_MAX], ps_den[SCENE_GROUPS_MAX];
    float     envL[SCENE_GROUPS_MAX], envR[SCENE_GROUPS_MAX], envLR[SCENE_GROUPS_MAX], envLL[SCENE_GROUPS_MAX], envRR[SCENE_GROUPS_MAX];
    struct scene_clusters clusters;
    /* masks: the current control row (installed by scene_engine_set_mask) */
    int       mask_bins, have_mask;
    float     elig_row[SCENE_MASK_BINS_MAX], prot_row[SCENE_MASK_BINS_MAX];
    /* front anchor (front_keep_db): 1 s sums of the input, the unscaled front demand and their cross term */
    float     anc_ex, anc_rxs, anc_es, anchor;
    /* frame counter and control */
    int       t, pending_reset;
    /* streaming: analysis buffer in pad-index space, OLA ring, delay lines */
    float     inbuf[SCENE_RING][2];
    int       in_head, in_count;                  /* ring head sample index (pad space) and fill */
    int       next_frame;                         /* frame t to compute next (covers pad[tH, tH+N)) */
    float     ola[SCENE_RING][4];                 /* subL, subR, rearL, rearR, pad-index space */
    /* centre (scene_engine_set_vocal): the centred lead vocal leaves L/R for C */
    float     voc_row[SCENE_MASK_BINS_MAX], e_voc[SCENE_BINS], ola_c[SCENE_RING];
    float     centre_frac; int have_voc, centre_on;
    scene_cpx ctrX[SCENE_BINS];
    float     dline[SCENE_DELAY_MAX][2]; int dpos, dlen; int delay_rear, delay_front;
    float     bline[SCENE_DELAY_MAX][2]; int bpos, blen;
    /* scratch for one frame */
    scene_cpx XL[SCENE_BINS], XR[SCENE_BINS], subL[SCENE_BINS], subR[SCENE_BINS], rearL[SCENE_BINS], rearR[SCENE_BINS];
    float     fft_scratch[8 * SCENE_NFFT];           /* FFT work [0,2N), time-domain pairs: in [2N,4N), sub out [4N,6N), rear out [6N,8N) */
};

/* defaults = SceneConfig() of the Python module (variant J) */
void scene_config_defaults(struct scene_config *cfg);

/* Builds every table from cfg and zeroes the state. Returns 0, or -1 on an unsupported config
 * (n_fft != 1024, hop != 256, erb_pool 0, cluster 0 with unmix 1). */
int  scene_engine_init(struct scene_state *s, const struct scene_config *cfg);

/* Stream start: the state of a fresh scene_engine_init() with the tables kept (no trig, no table
 * build, no mask row: cheap enough for the hook's realtime period callback, where a reset runs). */
void scene_engine_reset_state(struct scene_state *s);

/* Live level change between pushes (the audio thread only): the transfer fractions, the duck depth and
 * the side-layer low-pass (its per-bin table is rebuilt, 513 bins, no trig).  Everything else stays. */
void scene_engine_set_tuning(struct scene_state *s, float alpha, float inst_alpha, float duck_depth, float side_lp_hz);

/* Track change (Now Playing): the next frame fades every place (1.5 s), clears the histogram, the
 * song memory and the long gate; statistics and holds are untouched (they release by themselves). */
void scene_engine_reset(struct scene_state *s);

/* Install the mask row for the control period that starts now: elig[mask_bins] in 0..1 (the
 * instrument share), prot[mask_bins] in 0..1 (vocal + drum core + bass posterior); either may be
 * NULL (prot = 1 - elig when only one is given). mask_bins = 257 (512-point grid) or 1025 (2048). */
void scene_engine_set_mask(struct scene_state *s, const float *elig, const float *prot, int mask_bins);
void scene_engine_clear_mask(struct scene_state *s);
/* Centre: voc[mask_bins] = the net's vocal posterior row (after set_mask, same grid); frac 0..1 = how much
 * of the centred lead vocal leaves L/R for C (0 = off, C stays silent). Not part of the Python reference. */
void scene_engine_set_vocal(struct scene_state *s, const float *voc, int mask_bins, float frac);

/* One analysis frame: XL/XR = rfft of the sine-windowed 1024-sample frame t; writes the frame's
 * subL/subR (what leaves the front) and rearL/rearR (Ls/Rs) spectra into the state scratch.
 * Exposed for the fixture tests; scene_engine_push() calls it. */
void scene_engine_frame(struct scene_state *s, const scene_cpx *XL, const scene_cpx *XR, int t);

/* Block driver = SceneStream.push(): in[frames][2] float in [-1,1] -> out[frames][6] (L R C LFE Ls Rs,
 * LFE zero, C zero unless scene_engine_set_vocal() turned the centre on). Returns the number of output
 * frames written (the output lags the input by SCENE_NFFT samples plus the delay of the delayed pair;
 * the first call(s) return fewer frames).
 * frames <= SCENE_MAX_BLOCK. Never allocates, never looks ahead. */
size_t scene_engine_push(struct scene_state *s, const float *in, size_t frames, float *out, size_t out_capacity);

/* Flush = push SCENE_NFFT zeros (end of a finite stream, tests only). */
size_t scene_engine_flush(struct scene_state *s, float *out, size_t out_capacity);

/* Pure helpers exposed for the function fixtures (functions.npz). */
float scene_smoothstep(float x, float lo, float hi);
float scene_fast_atan2f(float y, float x);                                 /* ~2e-7 rad, no libm */
void  scene_fast_sincosf(float x, float *sn, float *cs);                   /* x in [-pi, pi], ~1.5e-7 */
float scene_ema_alpha(float tau_ms, int hop);
void  scene_decompose_frame(const float *PL, const float *PR, const scene_cpx *C, int n,
                            float *PN, float *PS, float *a, float *b, float *theta, float *Pa, float *Pb);
void  scene_centre_removed_pan(const float *PL, const float *PR, const scene_cpx *C, int n, float eps,
                               const float *PN, float *p, float *e, int *valid);
int   scene_erb_groups(int n_bins, int n_fft, int *group, float *group_freq, int *group_count);
void  scene_phase_table(int n_bins, uint32_t seed, float sign, scene_cpx *out);
/* 1024-point real transforms (numpy rfft/irfft: irfft scales by 1/N and ignores the imaginary parts of bins 0
 * and 512). scratch[4 * SCENE_NFFT]. The pair forms run two real transforms through one complex FFT
 * (scene_fft.h, Stockham radix-4, NEON): the engine's three complex FFTs per hop instead of six.
 * rfft pair: xa/xb may be scratch[2N, 3N) / [3N, 4N). irfft pair: xa/xb must not overlap scratch[0, 4N). */
void  scene_rfft1024(const float *x, scene_cpx *X, float *scratch);     /* x[1024] -> X[513] */
void  scene_irfft1024(const scene_cpx *X, float *x, float *scratch);    /* X[513] -> x[1024] */
void  scene_rfft1024_pair(const float *xa, const float *xb, scene_cpx *XA, scene_cpx *XB, float *scratch);
void  scene_irfft1024_pair(const scene_cpx *XA, const scene_cpx *XB, float *xa, float *xb, float *scratch);

#endif
