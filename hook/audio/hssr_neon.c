/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "hssr_neon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SPA_HSSR_HAVE_NEON 1
#else
#define SPA_HSSR_HAVE_NEON 0
#endif

#define SPA_HSSR_MODEL_MAGIC0 UINT32_C(0x52535348) /* "HSSR" */
#define SPA_HSSR_MODEL_MAGIC1 UINT32_C(0x314f454e) /* "NEO1" */
#define SPA_HSSR_MODEL_VERSION UINT32_C(1)
#define SPA_HSSR_MODEL_VALID UINT32_C(0x48535231)

enum {
    SPA_HSSR_GRU_RESET = 0,
    SPA_HSSR_GRU_UPDATE = SPA_HSSR_HIDDEN,
    SPA_HSSR_GRU_NEW = 2 * SPA_HSSR_HIDDEN
};

static uint32_t spa_hssr_read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint32_t spa_hssr_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t i;
    unsigned bit;

    for (i = 0; i < size; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static float spa_hssr_bound(float value, float low, float high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int spa_hssr_float_is_finite(float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}

/* Max error is acceptable for a control network and, unlike expf/tanhf, this
 * rational approximation has bounded cost on the old QNX libm. */
static float spa_hssr_tanh(float value)
{
    float square;

    value = spa_hssr_bound(value, -3.0f, 3.0f);
    square = value * value;
    return value * (27.0f + square) / (27.0f + 9.0f * square);
}

static float spa_hssr_sigmoid(float value)
{
    return 0.5f * (spa_hssr_tanh(0.5f * value) + 1.0f);
}

static int8_t spa_hssr_quantize(float value, float multiplier)
{
    float scaled = value * multiplier;
    int rounded;

    if (scaled >= 127.0f) return INT8_C(127);
    if (scaled <= -127.0f) return -INT8_C(127);
    rounded = scaled >= 0.0f ? (int)(scaled + 0.5f)
                             : (int)(scaled - 0.5f);
    return (int8_t)rounded;
}

static int32_t spa_hssr_dot_s8(const int8_t *left, const int8_t *right,
                               unsigned count)
{
#if SPA_HSSR_HAVE_NEON
    int32x4_t sum = vdupq_n_s32(0);
    int32_t lanes[4];
    unsigned at = 0;
    int32_t scalar;

    for (; at + 15u < count; at += 16u) {
        int8x16_t a = vld1q_s8(left + at);
        int8x16_t b = vld1q_s8(right + at);
        int16x8_t lo = vmull_s8(vget_low_s8(a), vget_low_s8(b));
        int16x8_t hi = vmull_s8(vget_high_s8(a), vget_high_s8(b));
        sum = vpadalq_s16(sum, lo);
        sum = vpadalq_s16(sum, hi);
    }
    vst1q_s32(lanes, sum);
    scalar = lanes[0] + lanes[1] + lanes[2] + lanes[3];
    for (; at < count; ++at)
        scalar += (int32_t)left[at] * (int32_t)right[at];
    return scalar;
#else
    int32_t sum = 0;
    unsigned at;

    for (at = 0; at < count; ++at)
        sum += (int32_t)left[at] * (int32_t)right[at];
    return sum;
#endif
}

static void spa_hssr_gemv(const int8_t *weights, const int8_t *input,
                          unsigned rows, unsigned columns, int32_t *output)
{
    unsigned row;

    for (row = 0; row < rows; ++row) {
        output[row] = spa_hssr_dot_s8(
            weights + (size_t)row * columns, input, columns);
    }
}

static void spa_hssr_assign_payload(struct spa_hssr_model *model,
                                    uint8_t *payload)
{
    const size_t w1_ih_count =
        (size_t)SPA_HSSR_GATE_ROWS * SPA_HSSR_FEATURES;
    const size_t gru_matrix_count =
        (size_t)SPA_HSSR_GATE_ROWS * SPA_HSSR_HIDDEN;
    const size_t head_count =
        (size_t)SPA_HSSR_OUTPUTS * SPA_HSSR_HIDDEN;
    const int8_t *weights = (const int8_t *)payload;
    const float *floats = (const float *)(const void *)(
        payload + SPA_HSSR_WEIGHT_BYTES);

    model->w1_ih = weights;
    weights += w1_ih_count;
    model->w1_hh = weights;
    weights += gru_matrix_count;
    model->w2_ih = weights;
    weights += gru_matrix_count;
    model->w2_hh = weights;
    weights += gru_matrix_count;
    model->w_head = weights;
    weights += head_count;

    model->feature_mean = floats;
    floats += SPA_HSSR_FEATURES;
    model->feature_inv_std = floats;
    floats += SPA_HSSR_FEATURES;
    model->s1_ih = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->s1_hh = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->s2_ih = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->s2_hh = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->s_head = floats;
    floats += SPA_HSSR_OUTPUTS;
    model->b1_ih = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->b1_hh = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->b2_ih = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->b2_hh = floats;
    floats += SPA_HSSR_GATE_ROWS;
    model->b_head = floats;

    (void)weights;
}

static int spa_hssr_validate_payload(const struct spa_hssr_model *model,
                                     const uint8_t *payload)
{
    const float *floats = (const float *)(const void *)(
        payload + SPA_HSSR_WEIGHT_BYTES);
    size_t index;

    for (index = 0; index < SPA_HSSR_FLOAT_COUNT; ++index)
        if (!spa_hssr_float_is_finite(floats[index])) return 0;
    for (index = 0; index < SPA_HSSR_FEATURES; ++index)
        if (!(model->feature_inv_std[index] > 0.0f)) return 0;
    for (index = 0; index < SPA_HSSR_GATE_ROWS; ++index) {
        if (!(model->s1_ih[index] > 0.0f) ||
            !(model->s1_hh[index] > 0.0f) ||
            !(model->s2_ih[index] > 0.0f) ||
            !(model->s2_hh[index] > 0.0f))
            return 0;
    }
    for (index = 0; index < SPA_HSSR_OUTPUTS; ++index)
        if (!(model->s_head[index] > 0.0f)) return 0;
    return 1;
}

int spa_hssr_model_view_payload(struct spa_hssr_model *model,
                                void *payload, size_t payload_bytes,
                                uint32_t payload_crc32)
{
    if (!model || !payload) return SPA_HSSR_ERROR_ARGUMENT;
    if (payload_bytes != SPA_HSSR_MODEL_PAYLOAD_BYTES)
        return SPA_HSSR_ERROR_FORMAT;

    memset(model, 0, sizeof(*model));
    spa_hssr_assign_payload(model, (uint8_t *)payload);
    if (!spa_hssr_validate_payload(model, (const uint8_t *)payload)) {
        memset(model, 0, sizeof(*model));
        return SPA_HSSR_ERROR_FORMAT;
    }
    model->payload_crc32 = payload_crc32;
    model->valid = SPA_HSSR_MODEL_VALID;
    return SPA_HSSR_OK;
}

int spa_hssr_model_load_file(struct spa_hssr_model *model,
                             const char *path)
{
    uint8_t header[SPA_HSSR_MODEL_HEADER_BYTES];
    uint8_t *payload = NULL;
    FILE *file = NULL;
    uint32_t expected_crc;
    int status = SPA_HSSR_ERROR_IO;

    if (!model || !path || !path[0]) return SPA_HSSR_ERROR_ARGUMENT;
    memset(model, 0, sizeof(*model));
    file = fopen(path, "rb");
    if (!file) goto done;
    if (fread(header, 1, sizeof(header), file) != sizeof(header)) goto done;
    if (spa_hssr_read_u32_le(header + 0) != SPA_HSSR_MODEL_MAGIC0 ||
        spa_hssr_read_u32_le(header + 4) != SPA_HSSR_MODEL_MAGIC1 ||
        spa_hssr_read_u32_le(header + 8) != SPA_HSSR_MODEL_VERSION ||
        spa_hssr_read_u32_le(header + 12) != SPA_HSSR_MODEL_HEADER_BYTES ||
        spa_hssr_read_u32_le(header + 16) != SPA_HSSR_FEATURES ||
        spa_hssr_read_u32_le(header + 20) != SPA_HSSR_HIDDEN ||
        spa_hssr_read_u32_le(header + 24) != SPA_HSSR_OUTPUTS ||
        spa_hssr_read_u32_le(header + 28) != SPA_HSSR_WEIGHT_BYTES ||
        spa_hssr_read_u32_le(header + 32) != SPA_HSSR_FLOAT_COUNT ||
        spa_hssr_read_u32_le(header + 36) != SPA_HSSR_MODEL_PAYLOAD_BYTES) {
        status = SPA_HSSR_ERROR_FORMAT;
        goto done;
    }
    expected_crc = spa_hssr_read_u32_le(header + 40);
    payload = (uint8_t *)malloc(SPA_HSSR_MODEL_PAYLOAD_BYTES);
    if (!payload) {
        status = SPA_HSSR_ERROR_MEMORY;
        goto done;
    }
    if (fread(payload, 1, SPA_HSSR_MODEL_PAYLOAD_BYTES, file) !=
            SPA_HSSR_MODEL_PAYLOAD_BYTES || fgetc(file) != EOF) {
        status = SPA_HSSR_ERROR_FORMAT;
        goto done;
    }
    if (spa_hssr_crc32(payload, SPA_HSSR_MODEL_PAYLOAD_BYTES) !=
        expected_crc) {
        status = SPA_HSSR_ERROR_FORMAT;
        goto done;
    }
    status = spa_hssr_model_view_payload(
        model, payload, SPA_HSSR_MODEL_PAYLOAD_BYTES, expected_crc);
    if (status == SPA_HSSR_OK) {
        model->_allocation = payload;
        model->_allocation_bytes = SPA_HSSR_MODEL_PAYLOAD_BYTES;
        payload = NULL;
    }

done:
    if (file) fclose(file);
    free(payload);
    return status;
}

void spa_hssr_model_unload(struct spa_hssr_model *model)
{
    void *allocation;

    if (!model) return;
    allocation = model->_allocation;
    memset(model, 0, sizeof(*model));
    free(allocation);
}

int spa_hssr_model_ready(const struct spa_hssr_model *model)
{
    return model && model->valid == SPA_HSSR_MODEL_VALID &&
           model->w1_ih && model->w1_hh && model->w2_ih &&
           model->w2_hh && model->w_head && model->feature_mean &&
           model->feature_inv_std;
}

void spa_hssr_state_reset(struct spa_hssr_state *state)
{
    if (state) memset(state, 0, sizeof(*state));
}

static void spa_hssr_quantize_hidden(const float *hidden, int8_t *quantized)
{
    unsigned i;

    for (i = 0; i < SPA_HSSR_HIDDEN; ++i)
        quantized[i] = spa_hssr_quantize(
            spa_hssr_bound(hidden[i], -1.0f, 1.0f), 127.0f);
}

static void spa_hssr_gru_layer(struct spa_hssr_state *state,
                               const int8_t *input, unsigned input_size,
                               float input_step, const int8_t *w_ih,
                               const int8_t *w_hh, const float *s_ih,
                               const float *s_hh, const float *b_ih,
                               const float *b_hh, float *hidden)
{
    unsigned row;
    unsigned unit;

    spa_hssr_quantize_hidden(hidden, state->hidden_q);
    spa_hssr_gemv(w_ih, input, SPA_HSSR_GATE_ROWS, input_size,
                  state->input_acc);
    spa_hssr_gemv(w_hh, state->hidden_q, SPA_HSSR_GATE_ROWS,
                  SPA_HSSR_HIDDEN, state->hidden_acc);
    for (row = 0; row < SPA_HSSR_GATE_ROWS; ++row) {
        state->gate_input[row] =
            (float)state->input_acc[row] * input_step * s_ih[row] +
            b_ih[row];
        state->gate_hidden[row] =
            (float)state->hidden_acc[row] * (1.0f / 127.0f) *
            s_hh[row] + b_hh[row];
    }

    /* PyTorch-compatible GRU gate order and reset-after candidate equation:
     * r=sigmoid(i_r+h_r), z=sigmoid(i_z+h_z),
     * n=tanh(i_n+r*h_n), h=(1-z)n+z*h. */
    for (unit = 0; unit < SPA_HSSR_HIDDEN; ++unit) {
        float reset = spa_hssr_sigmoid(
            state->gate_input[SPA_HSSR_GRU_RESET + unit] +
            state->gate_hidden[SPA_HSSR_GRU_RESET + unit]);
        float update = spa_hssr_sigmoid(
            state->gate_input[SPA_HSSR_GRU_UPDATE + unit] +
            state->gate_hidden[SPA_HSSR_GRU_UPDATE + unit]);
        float candidate = spa_hssr_tanh(
            state->gate_input[SPA_HSSR_GRU_NEW + unit] +
            reset * state->gate_hidden[SPA_HSSR_GRU_NEW + unit]);
        hidden[unit] = spa_hssr_bound(
            (1.0f - update) * candidate + update * hidden[unit],
            -1.0f, 1.0f);
    }
}

int spa_hssr_infer(const struct spa_hssr_model *model,
                   struct spa_hssr_state *state,
                   const float features[SPA_HSSR_FEATURES],
                   float output[SPA_HSSR_OUTPUTS])
{
    unsigned i;

    if (!model || !state || !features || !output)
        return SPA_HSSR_ERROR_ARGUMENT;
    if (!spa_hssr_model_ready(model)) return SPA_HSSR_ERROR_NOT_READY;

    for (i = 0; i < SPA_HSSR_FEATURES; ++i) {
        float normalized = (features[i] - model->feature_mean[i]) *
                           model->feature_inv_std[i];
        state->input_q[i] = spa_hssr_quantize(
            spa_hssr_bound(normalized, -6.0f, 6.0f), 127.0f / 6.0f);
    }

    spa_hssr_gru_layer(
        state, state->input_q, SPA_HSSR_FEATURES, 6.0f / 127.0f,
        model->w1_ih, model->w1_hh, model->s1_ih, model->s1_hh,
        model->b1_ih, model->b1_hh, state->hidden1);
    /* Keep layer-two input separate from hidden_q: gru_layer quantizes that
     * layer's previous recurrent state into hidden_q before GEMV.  Aliasing
     * both vectors would silently replace hidden1 with the old hidden2. */
    spa_hssr_quantize_hidden(state->hidden1, state->input_q);
    spa_hssr_gru_layer(
        state, state->input_q, SPA_HSSR_HIDDEN, 1.0f / 127.0f,
        model->w2_ih, model->w2_hh, model->s2_ih, model->s2_hh,
        model->b2_ih, model->b2_hh, state->hidden2);

    spa_hssr_quantize_hidden(state->hidden2, state->hidden_q);
    spa_hssr_gemv(model->w_head, state->hidden_q,
                  SPA_HSSR_OUTPUTS, SPA_HSSR_HIDDEN, state->input_acc);
    for (i = 0; i < SPA_HSSR_OUTPUTS; ++i) {
        output[i] = (float)state->input_acc[i] * (1.0f / 127.0f) *
                    model->s_head[i] + model->b_head[i];
    }
    state->inference_count++;
    return SPA_HSSR_OK;
}
