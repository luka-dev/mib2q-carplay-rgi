#include "audio/hssr_control_worker.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void make_zero_model(struct spa_hssr_model *model, void **payload_out)
{
    uint8_t *payload = (uint8_t *)calloc(1, SPA_HSSR_MODEL_PAYLOAD_BYTES);
    float *floats;
    unsigned i;

    assert(payload != NULL);
    floats = (float *)(void *)(payload + SPA_HSSR_WEIGHT_BYTES);
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

static int submit_until_ready(
    const struct spa_hssr_control_provider *provider,
    uint32_t generation, uint32_t sequence,
    const float features[SPA_HSSR_FEATURES])
{
    const struct timespec pause = { 0, 1000000 };
    unsigned attempt;

    for (attempt = 0; attempt < 2000u; ++attempt) {
        if (provider->submit(provider->context, generation, sequence,
                             features, NULL) > 0)
            return 1;
        (void)nanosleep(&pause, NULL);
    }
    return 0;
}

static int receive_until_ready(
    const struct spa_hssr_control_provider *provider,
    uint32_t generation, uint32_t maximum_sequence,
    float control[SPA_HSSR_OUTPUTS], uint32_t *sequence)
{
    const struct timespec pause = { 0, 1000000 };
    unsigned attempt;

    for (attempt = 0; attempt < 2000u; ++attempt) {
        if (provider->receive(provider->context, generation,
                              maximum_sequence, control, sequence) > 0)
            return 1;
        (void)nanosleep(&pause, NULL);
    }
    return 0;
}

int main(void)
{
    struct spa_hssr_model model;
    static struct spa_hssr_control_worker worker;
    struct spa_hssr_control_worker_stats stats;
    const struct spa_hssr_control_provider *provider;
    float features[SPA_HSSR_FEATURES];
    float control[SPA_HSSR_OUTPUTS];
    uint32_t sequence = 0;
    void *payload;
    unsigned i;

    make_zero_model(&model, &payload);
    for (i = 0; i < SPA_HSSR_FEATURES; ++i)
        features[i] = (float)((int)(i % 19u) - 9) * 0.0625f;

    assert(spa_hssr_control_worker_start(
               &worker, &model, 7u, 0) == SPA_HSSR_OK);
    provider = spa_hssr_control_worker_provider(&worker);
    assert(provider != NULL);
    assert(submit_until_ready(provider, 7u, 1u, features));
    assert(receive_until_ready(provider, 7u, 1u, control, &sequence));
    assert(sequence == 1u);
    for (i = 0; i < SPA_HSSR_OUTPUTS; ++i) assert(control[i] == 0.0f);

    /* Two completed results may be pending. They remain ordered and neither
     * slot may be overwritten while the consumer copies the older result. */
    assert(submit_until_ready(provider, 7u, 2u, features));
    assert(submit_until_ready(provider, 7u, 3u, features));
    for (i = 0; i < 2000u; ++i) {
        const struct timespec pause = { 0, 1000000 };

        spa_hssr_control_worker_get_stats(&worker, &stats);
        if (stats.completed >= 3u) break;
        (void)nanosleep(&pause, NULL);
    }
    assert(stats.completed >= 3u);
    assert(receive_until_ready(provider, 7u, 3u, control, &sequence));
    assert(sequence == 2u);
    assert(receive_until_ready(provider, 7u, 3u, control, &sequence));
    assert(sequence == 3u);

    /* An in-flight result from generation 7 may finish, but generation 8 is
     * the only one the consumer may ever accept after reset. */
    assert(submit_until_ready(provider, 7u, 4u, features));
    provider->reset(provider->context, 8u);
    assert(submit_until_ready(provider, 8u, 1u, features));
    sequence = 0;
    assert(receive_until_ready(provider, 8u, 1u, control, &sequence));
    assert(sequence == 1u);

    spa_hssr_control_worker_stop(&worker);
    assert(spa_hssr_control_worker_provider(&worker) == NULL);
    spa_hssr_control_worker_get_stats(&worker, &stats);
    assert(stats.submitted >= 5u);
    assert(stats.completed >= 4u);
    assert(stats.resets >= 1u);
    free(payload);
    puts("hssr_control_worker_test: OK (mailbox + generation reset)");
    return 0;
}
