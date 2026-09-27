/* Host check of the async guard path: worker runs main + guard, receive + receive_guard return both blocks. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "hssr_control_worker.h"
int main(int argc, char **argv)
{
    static struct spa_hssr_model model, guard;
    static struct spa_hssr_control_worker worker;
    float features[SPA_HSSR_FEATURES], control[SPA_HSSR_OUTPUTS], gcontrol[SPA_HSSR_OUTPUTS];
    uint32_t seq = 0, got = 0, got_guard = 0, i;
    if (argc != 3 || spa_hssr_model_load_file(&model, argv[1]) != SPA_HSSR_OK || spa_hssr_model_load_file(&guard, argv[2]) != SPA_HSSR_OK) return 2;
    if (spa_hssr_control_worker_start(&worker, &model, 1u, 0) != SPA_HSSR_OK) return 3;
    spa_hssr_control_worker_set_guard(&worker, &guard);
    for (i = 0; i < SPA_HSSR_FEATURES; ++i) features[i] = 0.1f * (float)((i * 7) % 11) - 0.5f;
    for (i = 1; i <= 40; ++i) {
        const struct spa_hssr_control_provider *p = spa_hssr_control_worker_provider(&worker);
        if (p->submit(p->context, 1u, i, features, NULL) <= 0) { /* busy is fine */ }
        usleep(3000);
        if (p->receive(p->context, 1u, i, control, &seq) > 0) {
            got++;
            if (p->receive_guard && p->receive_guard(p->context, gcontrol) > 0) {
                float diff = 0.0f; unsigned k;
                for (k = 0; k < SPA_HSSR_OUTPUTS; ++k) diff += (control[k] - gcontrol[k]) * (control[k] - gcontrol[k]);
                if (diff > 1e-6f) got_guard++;
            }
        }
    }
    spa_hssr_control_worker_stop(&worker);
    printf("results=%u guard_results=%u (guard block differs from main)\n", got, got_guard);
    return (got > 0 && got_guard == got) ? 0 : 1;
}
