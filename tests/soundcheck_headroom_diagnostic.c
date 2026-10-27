/* Research diagnostic: documents the CURRENT S16 intermediate clipping.
 * This is not an acceptance test for the proposed float rendering path.
 * Synthetic near-full-scale output, not a claim of clipping in a captured song.
 */
#include "audio/soundcheck.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    const float metadata[3] = { -18.0f, -1.8125f, -1.78125f };
    spa_soundcheck_config_t config;
    spa_soundcheck_state_t state;
    int16_t pcm[2] = { 30000, 31000 };
    float float_limited[2];
    float ceiling, gain;

    spa_soundcheck_default_config(&config);
    config.limiterMode = SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK;
    spa_soundcheck_init(&state, &config, 1, 1, -16);
    assert(spa_soundcheck_update_main(&state, 1, metadata));
    gain = state.currentGain;
    assert(fabsf(gain - 1.1220185f) < 0.00001f);

    spa_soundcheck_apply_s16(&state, pcm, 1, 2);
    printf("after +1 dB S16: %d %d\n", pcm[0], pcm[1]);
    assert(pcm[0] == 32767 && pcm[1] == 32767);
    spa_soundcheck_limit_s16(&state, pcm, 1, 2);
    printf("after current limiter: %d %d, ratio %.8f\n",
           pcm[0], pcm[1], (float)pcm[0] / pcm[1]);
    assert(pcm[0] == pcm[1]);

    ceiling = 32767.0f * powf(10.0f, config.limiterCeilingDbfs / 20.0f);
    float_limited[0] = 30000.0f * gain * (ceiling / (31000.0f * gain));
    float_limited[1] = 31000.0f * gain * (ceiling / (31000.0f * gain));
    printf("float linked reference: %.3f %.3f, ratio %.8f\n",
           float_limited[0], float_limited[1],
           float_limited[0] / float_limited[1]);
    assert(fabsf(float_limited[0] / float_limited[1] - 30000.0f / 31000.0f) < 1e-6f);
    puts("CONFIRMED: a limiter after S16 gain clipping cannot restore channel ratios");
    return 0;
}
