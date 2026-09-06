/*
 * Quantized HSSR control-network runtime.
 *
 * Model loading may allocate and perform file I/O.  Inference and state reset
 * never allocate, lock, log, or touch the filesystem.  The model produces
 * control values only; waveform routing remains deterministic DSP.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef HSSR_NEON_H
#define HSSR_NEON_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_HSSR_FEATURES = 368,
    SPA_HSSR_HIDDEN = 160,
    SPA_HSSR_OUTPUTS = 264,
    SPA_HSSR_GATES = 3,
    SPA_HSSR_GATE_ROWS = SPA_HSSR_GATES * SPA_HSSR_HIDDEN,
    SPA_HSSR_WEIGHT_BYTES =
        SPA_HSSR_GATE_ROWS * SPA_HSSR_FEATURES +
        SPA_HSSR_GATE_ROWS * SPA_HSSR_HIDDEN +
        SPA_HSSR_GATE_ROWS * SPA_HSSR_HIDDEN +
        SPA_HSSR_GATE_ROWS * SPA_HSSR_HIDDEN +
        SPA_HSSR_OUTPUTS * SPA_HSSR_HIDDEN,
    SPA_HSSR_FLOAT_COUNT =
        SPA_HSSR_FEATURES * 2 +
        SPA_HSSR_GATE_ROWS * 4 + SPA_HSSR_OUTPUTS +
        SPA_HSSR_GATE_ROWS * 4 + SPA_HSSR_OUTPUTS,
    SPA_HSSR_MODEL_HEADER_BYTES = 64,
    SPA_HSSR_MODEL_PAYLOAD_BYTES =
        SPA_HSSR_WEIGHT_BYTES + SPA_HSSR_FLOAT_COUNT * 4
};

enum spa_hssr_status {
    SPA_HSSR_OK = 0,
    SPA_HSSR_ERROR_ARGUMENT = -1,
    SPA_HSSR_ERROR_IO = -2,
    SPA_HSSR_ERROR_FORMAT = -3,
    SPA_HSSR_ERROR_MEMORY = -4,
    SPA_HSSR_ERROR_NOT_READY = -5
};

/* All pointers address one validated, owned model allocation after load. */
struct spa_hssr_model {
    void *_allocation;
    size_t _allocation_bytes;

    const int8_t *w1_ih;
    const int8_t *w1_hh;
    const int8_t *w2_ih;
    const int8_t *w2_hh;
    const int8_t *w_head;

    const float *feature_mean;
    const float *feature_inv_std;
    const float *s1_ih;
    const float *s1_hh;
    const float *s2_ih;
    const float *s2_hh;
    const float *s_head;
    const float *b1_ih;
    const float *b1_hh;
    const float *b2_ih;
    const float *b2_hh;
    const float *b_head;

    uint32_t payload_crc32;
    uint32_t valid;
};

struct spa_hssr_state {
    float hidden1[SPA_HSSR_HIDDEN];
    float hidden2[SPA_HSSR_HIDDEN];
    int8_t input_q[SPA_HSSR_FEATURES];
    int8_t hidden_q[SPA_HSSR_HIDDEN];
    int32_t input_acc[SPA_HSSR_GATE_ROWS];
    int32_t hidden_acc[SPA_HSSR_GATE_ROWS];
    float gate_input[SPA_HSSR_GATE_ROWS];
    float gate_hidden[SPA_HSSR_GATE_ROWS];
    uint32_t inference_count;
};

/* Load and validate one little-endian HSSRNEO1 model blob. */
int spa_hssr_model_load_file(struct spa_hssr_model *model,
                             const char *path);
void spa_hssr_model_unload(struct spa_hssr_model *model);
int spa_hssr_model_ready(const struct spa_hssr_model *model);

/* Test/export helper: initialise pointer views over a complete payload.  The
 * caller retains ownership and must keep it alive until model_unload/reset. */
int spa_hssr_model_view_payload(struct spa_hssr_model *model,
                                void *payload, size_t payload_bytes,
                                uint32_t payload_crc32);

void spa_hssr_state_reset(struct spa_hssr_state *state);

/* Raw output layout: 32 groups of [six class logits, rearability logit,
 * lateral logit], followed by eight global logits. */
int spa_hssr_infer(const struct spa_hssr_model *model,
                   struct spa_hssr_state *state,
                   const float features[SPA_HSSR_FEATURES],
                   float output[SPA_HSSR_OUTPUTS]);

#endif /* HSSR_NEON_H */
