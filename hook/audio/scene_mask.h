/* scene_mask.h — the fine mask net (mask_net.py class ConvNet, arch "conv2048", net c) as a streaming float32
 * C runtime: the port of stft_fine() + features() + standardize() + ConvNet._logits() + out_act() +
 * rear_eligibility() + protect_mask() (spec docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md §18 item 2).
 *
 * Contract:
 *  - The Python module is the reference. tools/stereo-upmix-render/scene_mask_fixtures.py writes the goldens
 *    (masknet_oracle.npz); tools/stereo-upmix-render/port/test_mask.py checks every layer against them.
 *  - float32 arithmetic, no FMA (host builds use -ffp-contract=off), no allocation after scene_mask_init(),
 *    no dependency on the rest of the hook (standalone testable). Everything lives in struct scene_mask.
 *  - Timing: torch.stft(center=True, hop 512) frame k covers x[512k-1024, 512k+1024) and the net reads frame
 *    k+1 for control k (features(...)[:, 1:]), so control k is the 2048 samples x[512k-512, 512k+1536): they
 *    are complete after the 512-sample call that delivers x[512(k+2), 512(k+3)). Call j (0-based since reset)
 *    therefore returns control j-2; calls 0 and 1 return nothing. torch's reflect padding at the start
 *    (x[-i] = x[i]) is reproduced exactly: after call 1 the history slots of x[-512..-1] hold x[512..1].
 *  - The GRU state per bin persists across calls (the streaming state); scene_mask_reset() clears it.
 *  - Arithmetic: NEON over four bins (ARMv7 -mfpu=neon and the arm64 host) with a scalar fallback that performs
 *    the same operations in the same order (bit-identical on the host). tanh and sigmoid are the clamped
 *    rational of hssr_mask.c (|err| < 1e-4), the two log features a mantissa series (|err| < 1e-6), so the
 *    posteriors sit ~1e-4 from torch's; the ARMv7 build takes its reciprocals from VRECPE + two Newton steps.
 */
#ifndef SCENE_MASK_H
#define SCENE_MASK_H

#include <stddef.h>
#include <stdint.h>

#define SMASK_NFFT  2048
#define SMASK_HOP   512
#define SMASK_FBINS 1025                 /* fine bins of the 2048-point analysis (23.4 Hz) */
#define SMASK_BINS  257                  /* output bins (the hook's 512-point grid): conv1 stride 4 */
#define SMASK_FEAT  4                    /* log|L|^2, log|R|^2, cos IPD, sin IPD */
#define SMASK_CH    24                   /* ConvNet ch (checkpoint "ctx") */
#define SMASK_HID   12                   /* GRU hidden */
#define SMASK_OUT   4                    /* vocals, drum_core, bass, other */
#define SMASK_K1    8                    /* conv1 kernel (stride 4, pad 4) */
#define SMASK_K2    5                    /* conv2 / conv3 kernel (pad 2) */
#define SMASK_MAGIC "SMSKNET1"
#define SMASK_NPARAM 8131                /* floats after the 20-byte header: 8116 weights + 15 constants */

/* Row layouts of the per-bin arrays (bins contiguous, four per NEON vector):
 *  - conv inputs and the GRU state keep SMASK_PAD zero columns on the left (the k-5 taps read two of them, the
 *    16-byte alignment of bin 0 wants four) and seven on the right: bin j of row r is r[SMASK_PAD + j];
 *  - the fine feature rows: fine bin f is r[SMASK_PAD + f], conv1 output o reads columns 4o .. 4o+7;
 *  - the head's rows (logits, posteriors, eligibility, protect) have no left pad: bin j is r[j] (the backend
 *    reads post[n][bin] and the first 257 entries of elig / prot directly).
 * The right pads are never written (the 4-bin tiles that would spill recompute the last bins instead), so
 * they stay the zeros of scene_mask_init(). */
#define SMASK_PAD   4
#define SMASK_ROW   (SMASK_PAD + 264)    /* 268 floats per 257-bin activation row */
#define SMASK_FROW  (SMASK_PAD + 1028)   /* 1032 floats per 1025-bin feature row (conv1's right pad of 3) */
#define SMASK_OROW  264                  /* the head's rows: 257 bins, written four at a time */

/* The weight blob written by tools/stereo-upmix-render/port/export_masknet_weights.py is exactly this
 * struct's memory image: little-endian, every field 4 bytes wide, no padding, sizeof == 20 + 4 * 8131.
 * Tensors are the PyTorch state_dict of ConvNet.modules in their native order ([out][in][k] for Conv1d,
 * [3*hidden][in] for the GRU with gate rows r, z, n, [out][in] for Linear). The hook reads the file into
 * a struct scene_mask_weights and scene_mask_init() validates magic and dimensions. */
struct scene_mask_weights {
    char     magic[8];                                                 /* SMASK_MAGIC */
    uint32_t ch, hidden, n_out;                                        /* must equal SMASK_CH, SMASK_HID, SMASK_OUT */
    float c1_w[SMASK_CH][SMASK_FEAT][SMASK_K1], c1_b[SMASK_CH];        /* 0.weight, 0.bias */
    float c2_w[SMASK_CH][SMASK_CH][SMASK_K2],   c2_b[SMASK_CH];        /* 1.weight, 1.bias */
    float c3_w[SMASK_CH][SMASK_CH][SMASK_K2],   c3_b[SMASK_CH];        /* 2.weight, 2.bias */
    float gru_w_ih[3 * SMASK_HID][SMASK_CH];                           /* 3.weight_ih_l0 (rows r, z, n) */
    float gru_w_hh[3 * SMASK_HID][SMASK_HID];                          /* 3.weight_hh_l0 */
    float gru_b_ih[3 * SMASK_HID], gru_b_hh[3 * SMASK_HID];            /* 3.bias_ih_l0, 3.bias_hh_l0 */
    float head_w[SMASK_OUT][SMASK_CH + SMASK_HID], head_b[SMASK_OUT];  /* 4.weight (cols: conv ch, then GRU), 4.bias */
    float feat_mean[SMASK_FEAT], feat_std[SMASK_FEAT];                 /* standardize(); 0 / 1 when the checkpoint has none */
    float elig_w[SMASK_OUT];                                           /* 1 for every roles.eligible output, else 0 */
    float has_calib, calib_a, calib_b;                                 /* net.calib: elig = sigmoid(a logit(elig) + b) */
};

typedef struct { float re, im; } smask_cpx;

#if defined(__GNUC__)
#define SMASK_ALIGN16 __attribute__((aligned(16)))
#else
#define SMASK_ALIGN16
#endif

struct scene_mask {
    /* the weights as the kernels read them, built from the blob by scene_mask_init(): the output channels
     * (gates, outputs) are grouped in tiles (8 for the k-5 convolutions, 4 elsewhere) and every
     * tile's weights are contiguous in the order the kernel consumes them, one float per output channel, so
     * a tile multiplies its input vector by the lanes of consecutive weight pairs (VFP vldr with an
     * immediate offset: no address arithmetic, no lane-register copies) */
    float c1_wt[SMASK_CH / 4][SMASK_FEAT * SMASK_K1][4] SMASK_ALIGN16;   /* [c / 4][i * 8 + k][c % 4] = c1_w[c][i][k] */
    float c2_wt[SMASK_CH / 8][SMASK_CH * SMASK_K2][8] SMASK_ALIGN16;     /* [c / 8][i * 5 + k][c % 8] = c2_w[c][i][k] */
    float c3_wt[SMASK_CH / 8][SMASK_CH * SMASK_K2][8] SMASK_ALIGN16;
    float c1_b[SMASK_CH], c2_b[SMASK_CH], c3_b[SMASK_CH];
    float gru_wt[3 * SMASK_HID / 4][SMASK_CH][4] SMASK_ALIGN16;          /* [g / 4][i][g % 4] = gru_w_ih[g][i] */
    float gru_ut[3 * SMASK_HID / 4][SMASK_HID][4] SMASK_ALIGN16;         /* [g / 4][l][g % 4] = gru_w_hh[g][l] */
    float gru_b[3 * SMASK_HID];                            /* r, z: b_ih + b_hh; n: b_ih */
    float gru_bn[SMASK_HID];                               /* n: b_hh (inside the reset gate's product) */
    float head_wt[SMASK_CH + SMASK_HID][SMASK_OUT] SMASK_ALIGN16;        /* [i][n] = head_w[n][i] (one tile of four) */
    float head_b[SMASK_OUT];
    float feat_mean[SMASK_FEAT], feat_istd[SMASK_FEAT];    /* standardize: (f - mean) * (1 / std) */
    float elig_w[SMASK_OUT];
    float has_calib, calib_a, calib_b;

    float hist[SMASK_NFFT][2] SMASK_ALIGN16;           /* the newest 2048 stereo samples (interleaved L, R) */
    int   calls;                                       /* 512-sample calls since reset */
    float h[SMASK_HID][SMASK_ROW] SMASK_ALIGN16;       /* GRU state per bin: the streaming state */
    /* per-control intermediates, kept here (no stack use) and read back by scene_mask_debug() */
    float     fft[2][2 * SMASK_NFFT] SMASK_ALIGN16;    /* split re[N] im[N]: the transform (L in re, R in im) and its ping-pong block */
    smask_cpx L[SMASK_FBINS] SMASK_ALIGN16;            /* the two fine spectra of the control */
    smask_cpx R[SMASK_FBINS] SMASK_ALIGN16;
    float     feat[SMASK_FEAT][SMASK_FROW] SMASK_ALIGN16;   /* standardized, clamped +-6 */
    float     h1[SMASK_CH][SMASK_ROW] SMASK_ALIGN16;
    float     h2[SMASK_CH][SMASK_ROW] SMASK_ALIGN16;
    float     h3[SMASK_CH][SMASK_ROW] SMASK_ALIGN16;
    float     logits[SMASK_OUT][SMASK_OROW] SMASK_ALIGN16;
    float     post[SMASK_OUT][SMASK_OROW] SMASK_ALIGN16;    /* sigmoid(logits): post[1][bin] = drum core, post[2][bin] = bass (LFE V2) */
    float     elig[SMASK_OROW] SMASK_ALIGN16;
    float     prot[SMASK_OROW] SMASK_ALIGN16;
};

/* Validates the blob (magic, dims), lays the weights out for the kernels, builds the FFT/window tables (static,
 * identical values if two instances race), resets. Returns 0, or -1 on a bad blob (m untouched). */
int  scene_mask_init(struct scene_mask *m, const struct scene_mask_weights *w);

/* Stream start / track change: clears the sample history, the GRU state and the call counter. */
void scene_mask_reset(struct scene_mask *m);

/* One control: in512[512][2] = the newest 512 stereo samples in [-1, 1]. Returns 1 and writes the control's
 * rows (elig257 = rear eligibility in 0..1 = min(1, sum of the eligible posteriors) through the calibration,
 * prot257 = 1 - elig: exactly protect_mask()'s row; either pointer may be NULL) for control (calls - 3), or 0
 * while priming (the first two calls after reset). The posteriors stay in m->post. */
int  scene_mask_control(struct scene_mask *m, const float *in512, float *elig257, float *prot257);

/* Test accessors: sizeof(struct scene_mask) for a foreign-language caller, and the per-control intermediates
 * of the last call as flat, unpadded float arrays ([rows][bins]; complex spectra interleaved re, im). The
 * padded rows are compacted into the dead FFT block on each call, so copy the result before the next one. */
enum { SMASK_DBG_L, SMASK_DBG_R, SMASK_DBG_FEAT, SMASK_DBG_H1, SMASK_DBG_H2, SMASK_DBG_H3,
       SMASK_DBG_GRU, SMASK_DBG_LOGITS, SMASK_DBG_POST, SMASK_DBG_ELIG, SMASK_DBG_PROT };
size_t       scene_mask_size(void);
const float *scene_mask_debug(struct scene_mask *m, int what);

#endif
