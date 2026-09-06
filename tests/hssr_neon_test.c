#include "audio/hssr_neon.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_zero_model(struct spa_hssr_model *model, void **payload_out)
{
    uint8_t *payload = (uint8_t *)calloc(1, SPA_HSSR_MODEL_PAYLOAD_BYTES);
    float *floats;
    unsigned i;

    assert(payload != NULL);
    floats = (float *)(void *)(payload + SPA_HSSR_WEIGHT_BYTES);

    /* mean=0, inverse std=1.  Matrix scales are deliberately nonzero even
     * though this fixture's weights are zero. */
    for (i = 0; i < SPA_HSSR_FEATURES; ++i)
        floats[SPA_HSSR_FEATURES + i] = 1.0f;
    for (i = SPA_HSSR_FEATURES * 2;
         i < SPA_HSSR_FEATURES * 2 +
             SPA_HSSR_GATE_ROWS * 4 + SPA_HSSR_OUTPUTS;
         ++i)
        floats[i] = 1.0f / 127.0f;
    assert(spa_hssr_model_view_payload(
               model, payload, SPA_HSSR_MODEL_PAYLOAD_BYTES, 0) ==
           SPA_HSSR_OK);
    *payload_out = payload;
}

static void test_contract_and_zero_model(void)
{
    struct spa_hssr_model model;
    static struct spa_hssr_state state;
    float features[SPA_HSSR_FEATURES];
    float output[SPA_HSSR_OUTPUTS];
    void *payload;
    unsigned i;

    assert(SPA_HSSR_WEIGHT_BYTES == 449280);
    assert(SPA_HSSR_FLOAT_COUNT == 5104);
    assert(SPA_HSSR_MODEL_PAYLOAD_BYTES == 469696);
    assert(spa_hssr_model_view_payload(&model, NULL, 0, 0) ==
           SPA_HSSR_ERROR_ARGUMENT);
    make_zero_model(&model, &payload);
    assert(spa_hssr_model_ready(&model));

    for (i = 0; i < SPA_HSSR_FEATURES; ++i)
        features[i] = (float)((int)(i % 17u) - 8) * 0.125f;
    memset(output, 0x7f, sizeof(output));
    assert(spa_hssr_infer(&model, &state, features, output) == SPA_HSSR_OK);
    assert(state.inference_count == 1);
    for (i = 0; i < SPA_HSSR_HIDDEN; ++i) {
        assert(state.hidden1[i] == 0.0f);
        assert(state.hidden2[i] == 0.0f);
    }
    for (i = 0; i < SPA_HSSR_OUTPUTS; ++i)
        assert(output[i] == 0.0f);

    spa_hssr_state_reset(&state);
    assert(state.inference_count == 0);
    assert(spa_hssr_model_view_payload(
               &model, payload, SPA_HSSR_MODEL_PAYLOAD_BYTES - 1u, 0) ==
           SPA_HSSR_ERROR_FORMAT);
    free(payload);
}

static void test_bias_and_state_reset(void)
{
    struct spa_hssr_model model;
    static struct spa_hssr_state state;
    float features[SPA_HSSR_FEATURES] = {0};
    float first[SPA_HSSR_OUTPUTS];
    float second[SPA_HSSR_OUTPUTS];
    float *head_bias;
    void *payload;
    unsigned i;

    make_zero_model(&model, &payload);
    head_bias = (float *)(uintptr_t)model.b_head;
    for (i = 0; i < SPA_HSSR_OUTPUTS; ++i)
        head_bias[i] = ((float)(int)i - 100.0f) * 0.01f;

    assert(spa_hssr_infer(&model, &state, features, first) == SPA_HSSR_OK);
    assert(spa_hssr_infer(&model, &state, features, second) == SPA_HSSR_OK);
    assert(memcmp(first, second, sizeof(first)) == 0);
    for (i = 0; i < SPA_HSSR_OUTPUTS; ++i)
        assert(fabsf(first[i] - head_bias[i]) < 1.0e-7f);

    spa_hssr_state_reset(&state);
    assert(spa_hssr_infer(&model, &state, features, second) == SPA_HSSR_OK);
    assert(memcmp(first, second, sizeof(first)) == 0);
    free(payload);
}

static void test_argument_fail_closed(void)
{
    struct spa_hssr_model empty;
    struct spa_hssr_state state;
    float features[SPA_HSSR_FEATURES] = {0};
    float output[SPA_HSSR_OUTPUTS];

    memset(&empty, 0, sizeof(empty));
    spa_hssr_state_reset(&state);
    assert(!spa_hssr_model_ready(&empty));
    assert(spa_hssr_infer(&empty, &state, features, output) ==
           SPA_HSSR_ERROR_NOT_READY);
    assert(spa_hssr_infer(NULL, &state, features, output) ==
           SPA_HSSR_ERROR_ARGUMENT);
    assert(spa_hssr_infer(&empty, NULL, features, output) ==
           SPA_HSSR_ERROR_ARGUMENT);
    assert(spa_hssr_infer(&empty, &state, NULL, output) ==
           SPA_HSSR_ERROR_ARGUMENT);
    assert(spa_hssr_infer(&empty, &state, features, NULL) ==
           SPA_HSSR_ERROR_ARGUMENT);
    spa_hssr_model_unload(NULL);
}

static void test_second_layer_input_does_not_alias_recurrent_state(void)
{
    struct spa_hssr_model model;
    static struct spa_hssr_state state;
    float features[SPA_HSSR_FEATURES] = {0};
    float output[SPA_HSSR_OUTPUTS];
    int8_t *w1_ih;
    int8_t *w2_ih;
    int8_t *w_head;
    void *payload;

    make_zero_model(&model, &payload);
    w1_ih = (int8_t *)(uintptr_t)model.w1_ih;
    w2_ih = (int8_t *)(uintptr_t)model.w2_ih;
    w_head = (int8_t *)(uintptr_t)model.w_head;
    w1_ih[(size_t)(2 * SPA_HSSR_HIDDEN) * SPA_HSSR_FEATURES] = 127;
    w2_ih[(size_t)(2 * SPA_HSSR_HIDDEN) * SPA_HSSR_HIDDEN] = 127;
    w_head[0] = 127;
    features[0] = 6.0f;

    spa_hssr_state_reset(&state);
    assert(spa_hssr_infer(&model, &state, features, output) == SPA_HSSR_OK);
    assert(state.hidden1[0] > 0.45f);
    assert(state.hidden2[0] > 0.20f);
    assert(output[0] > 0.20f);
    free(payload);
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        struct spa_hssr_model model;
        static struct spa_hssr_state state;
        float features[SPA_HSSR_FEATURES] = {0};
        float output[SPA_HSSR_OUTPUTS];
        double checksum = 0.0;
        unsigned i;

        assert(spa_hssr_model_load_file(&model, argv[1]) == SPA_HSSR_OK);
        assert(spa_hssr_infer(&model, &state, features, output) == SPA_HSSR_OK);
        for (i = 0; i < SPA_HSSR_OUTPUTS; ++i)
            checksum += output[i] * (double)(i + 1u);
        printf("hssr_model_probe: crc=%08x inference=%u checksum=%.9f\n",
               model.payload_crc32, state.inference_count, checksum);
        spa_hssr_model_unload(&model);
        return 0;
    }
    assert(argc == 1);
    test_contract_and_zero_model();
    test_bias_and_state_reset();
    test_argument_fail_closed();
    test_second_layer_input_does_not_alias_recurrent_state();
    puts("hssr_neon_test: OK");
    return 0;
}
