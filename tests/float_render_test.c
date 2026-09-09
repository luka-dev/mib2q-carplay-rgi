/* Offline next-upmix contract. The shipping S16 path remains a reference. */
#include "audio/stereo_upmix.h"
#include "audio/soundcheck.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct spa_stereo_upmix upmix, split;
static union { float alignment; unsigned char bytes[SPA_HSSR_MODEL_PAYLOAD_BYTES]; } payload;
static struct spa_hssr_model model;
static int16_t input[2048], quantized[6144], legacy[6144];
static float output[6144], reference[6144];

static void initialize_model(void)
{
    unsigned i;
    float *f = (float *)(void *)(payload.bytes + SPA_HSSR_WEIGHT_BYTES);
    for (i = 0; i < SPA_HSSR_FEATURES; i++) f[SPA_HSSR_FEATURES + i] = 1.0f;
    for (i = SPA_HSSR_FEATURES * 2; i < SPA_HSSR_FEATURES * 2 +
            SPA_HSSR_GATE_ROWS * 4 + SPA_HSSR_OUTPUTS; i++) f[i] = 1.0f / 127.0f;
    assert(spa_hssr_model_view_payload(&model, payload.bytes, sizeof(payload.bytes), 0) == SPA_HSSR_OK);
}

static void test_bypass_and_validation(void)
{
    struct spa_stereo_upmix_config c;
    unsigned i;
    spa_stereo_upmix_default_config(&c);
    assert(spa_stereo_upmix_init(&upmix, &c) == 0);
    for (i = 0; i < 2048; i++) input[i] = (int16_t)((i * 7919u) & 65535u);
    input[0] = -32768; input[1] = 32767;
    assert(spa_stereo_upmix_process_f32(&upmix, input, 1024, output, 1024) == 0);
    spa_soundcheck_quantize_s16(output, quantized, 1024, 6);
    assert(spa_stereo_upmix_process_s16(&upmix, input, 1024, legacy, 1024) == 0);
    assert(memcmp(quantized, legacy, sizeof(legacy)) == 0);
    memcpy(reference, output, sizeof(output));
    assert(spa_stereo_upmix_process_f32(&upmix, input, 1024, output, 1023) == SPA_STEREO_UPMIX_ERROR_OUTPUT_TOO_SMALL);
    assert(memcmp(output, reference, sizeof(output)) == 0);
    assert(spa_stereo_upmix_process_f32(&upmix, input, 1, (float *)(void *)&upmix, 1) == SPA_STEREO_UPMIX_ERROR_STATE_OVERLAP);
    assert(spa_stereo_upmix_process_f32(&upmix, NULL, 0, NULL, 0) == 0);
    /* Input staging supports widening into a shared, float-aligned buffer. */
    memcpy(output, input, sizeof(input));
    assert(spa_stereo_upmix_process_f32(&upmix, (int16_t *)(void *)output, 1024, output, 1024) == 0);
    assert(memcmp(output, reference, sizeof(output)) == 0);
}

static void test_inference_and_chunking(void)
{
    struct spa_stereo_upmix_config c;
    unsigned block, i;
    spa_stereo_upmix_default_config(&c);
    c.backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    c.hssr_model = &model;
    c.rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_MAX;
    spa_stereo_upmix_config_set_lfe(&c, 1);
    assert(spa_stereo_upmix_init(&upmix, &c) == 0);
    assert(spa_stereo_upmix_init(&split, &c) == 0);
    for (block = 0; block < 8; block++) {
        assert(spa_stereo_upmix_process_f32(&upmix, input, 1024, output, 1024) == 0);
        assert(spa_stereo_upmix_process_f32(&split, input, 137, reference, 137) == 0);
        assert(spa_stereo_upmix_process_f32(&split, input + 274, 887, reference + 822, 887) == 0);
        assert(memcmp(output, reference, sizeof(output)) == 0);
        for (i = 0; i < 6144; i++) assert(isfinite(output[i]));
        for (i = 0; i < 1024; i++) assert(output[i * 6 + 2] == 0.0f);
    }
    /* Identical analysis state despite a different rendering/limiting path. */
    assert(spa_stereo_upmix_reset(&upmix) == 0);
    assert(spa_stereo_upmix_reset(&split) == 0);
    for (block = 0; block < 8; block++) {
        assert(spa_stereo_upmix_process_f32(&upmix, input, 1024, output, 1024) == 0);
        assert(spa_stereo_upmix_process_s16(&split, input, 1024, legacy, 1024) == 0);
        assert(memcmp(&upmix._runtime._hssr, &split._runtime._hssr,
                      sizeof(upmix._runtime._hssr)) == 0);
    }
}

static void test_gain_headroom_and_metadata(void)
{
    const float plus_one[3] = {-18.0f, -1.8125f, -1.78125f};
    const float minus_nine[3] = {-6.5f, -1.0f, 0.96875f};
    spa_soundcheck_config_t c;
    spa_soundcheck_state_t a, b;
    float frame[6] = {30000, 31000, -12000, 4000, 9000, -6000};
    unsigned i;
    spa_soundcheck_default_config(&c);
    c.limiterMode = SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK;
    spa_soundcheck_init(&a, &c, 1, 1, -16);
    assert(spa_soundcheck_update_main(&a, 1, plus_one));
    assert(!spa_soundcheck_update_main(&a, 1, plus_one));
    assert(!spa_soundcheck_update_main(&a, 2, minus_nine));
    spa_soundcheck_apply_f32(&a, frame, 1, 6);
    assert(frame[0] > 32767 && frame[1] > 32767);
    spa_soundcheck_limit_f32(&a, frame, 1, 6);
    assert(fabsf(frame[0] / frame[1] - 30000.0f / 31000.0f) < 1e-6f);
    assert(fabsf(frame[2] / frame[1] + 12000.0f / 31000.0f) < 1e-6f);
    spa_soundcheck_quantize_s16(frame, quantized, 1, 6);
    assert(quantized[0] != quantized[1]);
    assert(fabsf(frame[1] - 32767 * powf(10, -0.05f)) < 0.01f);
    /* -9 dB must recover an over-FS float synthesis peak before conversion. */
    spa_soundcheck_init(&a, &c, 1, 1, -16);
    assert(spa_soundcheck_update_main(&a, 1, minus_nine));
    frame[0] = 50000; frame[1] = -40000;
    spa_soundcheck_apply_f32(&a, frame, 1, 2);
    assert(fabsf(frame[0] - 50000 * powf(10, -0.45f)) < 0.01f);
    spa_soundcheck_limit_f32(&a, frame, 1, 2);
    assert(a.limitedFrames == 0);
    /* Track/FLUSH gain ramps remain sample-counted, independent of chunks. */
    spa_soundcheck_discontinuity(&a);
    assert(spa_soundcheck_update_main(&a, 1, plus_one));
    b = a;
    for (i = 0; i < 6144; i++) output[i] = reference[i] = 1000;
    spa_soundcheck_apply_f32(&a, output, 1024, 6);
    spa_soundcheck_apply_f32(&b, reference, 111, 6);
    spa_soundcheck_apply_f32(&b, reference + 666, 913, 6);
    assert(memcmp(output, reference, sizeof(output)) == 0);
    assert(a.rampRemaining == 0 && a.currentGain == a.targetGain);
    frame[0] = NAN; frame[1] = 1000;
    spa_soundcheck_limit_f32(&a, frame, 1, 2);
    assert(frame[0] == 0 && frame[1] == 0 && isfinite(a.limiterGain));
    frame[0] = INFINITY; frame[1] = NAN;
    spa_soundcheck_quantize_s16(frame, quantized, 1, 2);
    assert(quantized[0] == 0 && quantized[1] == 0);
}

static void test_matrix_guard_and_extremes(void)
{
    const float *matrix = spa_stereo_upmix_input_matrix();
    const int db[6][8] = {
        {-2,-99,-6,-99,-99,-99,-99,-99}, {-99,-2,-6,-99,-99,-99,-99,-99},
        {-8,-8,-3,-99,-99,-99,-99,-99}, {-8,-8,-10,3,-14,-14,-99,-99},
        {-3,-99,-99,-99,6,-99,-99,-99}, {-99,-3,-99,-99,-99,6,-99,-99}
    };
    spa_soundcheck_config_t c;
    spa_soundcheck_state_t a, b;
    float extreme[3] = {3e38f, 0, -3e38f}, gain;
    unsigned frame, channel, bus;
    int rounded;
    for (channel = 0; channel < 6; channel++)
        for (bus = 0; bus < 8; bus++)
            assert(fabsf(matrix[channel * 8 + bus] - powf(10, db[channel][bus] / 20.0f)) < 3e-7f);
    spa_soundcheck_default_config(&c);
    c.limiterMode = SPA_SOUNDCHECK_LIMITER_SAMPLE_PEAK;
    spa_soundcheck_init(&a, &c, 0, 0, 0);
    b = a;
    for (frame = 0; frame < 1024; frame++)
        for (channel = 0; channel < 6; channel++)
            output[frame * 6 + channel] = reference[frame * 6 + channel] =
                (float)input[frame * 2 + channel % 2] * (channel + 1);
    spa_soundcheck_limit_matrix_f32(&a, output, 1024, 6, matrix, 8);
    spa_soundcheck_limit_matrix_f32(&b, reference, 333, 6, matrix, 8);
    spa_soundcheck_limit_matrix_f32(&b, reference + 1998, 691, 6, matrix, 8);
    assert(memcmp(output, reference, sizeof(output)) == 0);
    assert(a.limitedFrames > 0);
    for (frame = 0; frame < 1024; frame++) {
        for (channel = 0; channel < 6; channel++)
            assert(fabsf(output[frame * 6 + channel]) <= 29203.63f);
        for (bus = 0; bus < 8; bus++) {
            float value = 0;
            for (channel = 0; channel < 6; channel++)
                value += output[frame * 6 + channel] * matrix[channel * 8 + bus];
            assert(fabsf(value) <= 29203.65f);
        }
        /* One gain across channels, even when a downstream bus triggers it. */
        assert(fabsf(output[frame * 6 + 4] - 5 * output[frame * 6]) < 0.02f);
    }
    assert(spa_soundcheck_main_gain(-16, extreme, &rounded, &gain) && rounded == -128);
    extreme[0] = -3e38f;
    assert(spa_soundcheck_main_gain(-16, extreme, &rounded, &gain) && rounded == 20);
}

static void test_synthesis_does_not_clip_before_gain(void)
{
    struct spa_stereo_upmix_config c;
    spa_stereo_upmix_default_config(&c);
    c.backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    c.hssr_model = &model;
    c.rear_gain_q15 = SPA_STEREO_UPMIX_REAR_Q15_MAX;
    assert(spa_stereo_upmix_init(&upmix, &c) == 0);
    /* Inject one known analysis result to isolate synthesis from inference. */
    upmix._runtime._hssr.frames_seen = SPA_HSSR_LATENCY_FRAMES;
    upmix._runtime._hssr.front_delay[0][0] = 30000;
    upmix._runtime._hssr.ola[0][0] = -1.0f;
    assert(spa_stereo_upmix_process_f32(&upmix, input, 1, output, 1) == 0);
    assert(output[0] > 32767);
    assert(fabsf(output[0] - (30000.0f * c.front_gain_q15 / 32768.0f +
                            2.5f * c.rear_gain_q15)) < 0.01f);
}

static void test_cabin_source_transfer_and_reset(void)
{
    struct spa_stereo_upmix_config c;
    unsigned block, i;
    spa_stereo_upmix_default_config(&c);
    c.backend = SPA_STEREO_UPMIX_HSSR_UNIVERSAL;
    c.hssr_model = &model;
    spa_stereo_upmix_config_set_lfe(&c, 1);
    spa_stereo_upmix_config_set_cabin(&c, 1);
    assert(c.front_gain_q15 == SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT);
    assert(spa_stereo_upmix_init(&upmix, &c) == 0);
    upmix._runtime._hssr.frames_seen = SPA_HSSR_LATENCY_FRAMES;
    upmix._runtime._hssr.front_delay[0][0] = 24000;
    upmix._runtime._hssr.ola[0][0] = .5f;
    upmix._runtime._hssr.source_ola[0][0] = .1f;
    assert(spa_stereo_upmix_process_f32(&upmix,input,1,output,1)==0);
    assert(fabsf(output[0]-(24000.0f*c.front_gain_q15/32768-.1f*c.rear_gain_q15))<.01f);
    assert(fabsf(output[4]-.5f*c.rear_gain_q15)<.01f); /* No shelf. */
    assert(spa_stereo_upmix_reset(&upmix)==0);
    assert(upmix._runtime._hssr.cabin_policy==1);
    assert(upmix._runtime._hssr.source_ola[0][0]==0);
    assert(spa_stereo_upmix_init(&split,&c)==0);
    for(block=0;block<8;block++) {
        assert(spa_stereo_upmix_process_f32(&upmix,input,1024,output,1024)==0);
        assert(spa_stereo_upmix_process_f32(&split,input,137,reference,137)==0);
        assert(spa_stereo_upmix_process_f32(&split,input+274,887,reference+822,887)==0);
        assert(memcmp(output,reference,sizeof(output))==0);
        assert(upmix._runtime._hssr.universal_makeup==1.0f);
        for(i=0;i<SPA_HSSR_BINS;i++) assert(upmix._runtime._hssr.bin_universal_mask[i]==0);
    }
}

static void test_cabin_tracker_cannot_bypass_learned_veto(void)
{
    struct spa_stereo_upmix_config c;
    struct spa_hssr_dsp *dsp;
    unsigned i,b;
    spa_stereo_upmix_default_config(&c);
    c.backend=SPA_STEREO_UPMIX_HSSR_UNIVERSAL;c.hssr_model=&model;
    spa_stereo_upmix_config_set_cabin(&c,1);
    assert(spa_stereo_upmix_init(&upmix,&c)==0);
    dsp=&upmix._runtime._hssr;
    dsp->object_slot[0].pan=.4f;dsp->object_slot[0].confidence=1;
    dsp->object_slot[0].hold_hops=100;
    for(b=0;b<SPA_HSSR_BANDS;b++) {
        dsp->object_slot[0].band_weight[b]=1;
        dsp->band_focus_rescue[b]=1;
        dsp->band_universal_safety[b]=1;
        dsp->band_mask[b]=0;
    }
    for(i=0;i<128;i++) {
        input[2*i]=(int16_t)(12000*sin(6.283185307179586*1000*i/48000));
        input[2*i+1]=input[2*i]/2;
    }
    assert(spa_stereo_upmix_process_f32(&upmix,input,128,output,128)==0);
    assert(dsp->object_slot[0].band_weight[16]>.5f);
    for(b=0;b<SPA_HSSR_BINS;b++) assert(dsp->bin_mask[b]==0);
}

static void test_cabin_bass_is_quiet_fixed_mono(int bass_only)
{
    struct spa_stereo_upmix_config c;
    double old_power=0,new_power=0,anti_power=0;
    unsigned block,i;
    spa_stereo_upmix_default_config(&c);
    c.backend=SPA_STEREO_UPMIX_HSSR_UNIVERSAL;c.hssr_model=&model;
    spa_stereo_upmix_config_set_lfe(&c,1);
    assert(spa_stereo_upmix_init(&split,&c)==0);
    if (bass_only) spa_stereo_upmix_config_set_quiet_lfe(&c,1);
    else spa_stereo_upmix_config_set_cabin(&c,1);
    assert(c.front_gain_q15==SPA_STEREO_UPMIX_FRONT_Q15_DEFAULT);
    assert(c.cabin_policy==(bass_only ? 0u : 1u));
    assert(spa_stereo_upmix_init(&upmix,&c)==0);
    for(block=0;block<60;block++) {
        for(i=0;i<1024;i++) input[2*i]=input[2*i+1]=(int16_t)(12000*sin(6.283185307179586*40*(block*1024+i)/48000));
        assert(spa_stereo_upmix_process_f32(&upmix,input,1024,output,1024)==0);
        assert(spa_stereo_upmix_process_f32(&split,input,1024,reference,1024)==0);
        if(block>30)for(i=0;i<1024;i++) {
            old_power+=reference[6*i+3]*reference[6*i+3];
            new_power+=output[6*i+3]*output[6*i+3];
        }
    }
    assert(new_power/old_power>.19 && new_power/old_power<.29);
    assert(spa_stereo_upmix_reset(&upmix)==0);
    for(i=0;i<1024;i++) input[2*i+1]=-input[2*i];
    for(block=0;block<8;block++) {
        assert(spa_stereo_upmix_process_f32(&upmix,input,1024,output,1024)==0);
        for(i=0;i<1024;i++) anti_power+=output[6*i+3]*output[6*i+3];
    }
    assert(anti_power==0); /* No new anti-phase bass synthesis. */
}

int main(void)
{
    initialize_model();
    test_bypass_and_validation();
    test_inference_and_chunking();
    test_gain_headroom_and_metadata();
    test_matrix_guard_and_extremes();
    test_synthesis_does_not_clip_before_gain();
    test_cabin_source_transfer_and_reset();
    test_cabin_tracker_cannot_bypass_learned_veto();
    test_cabin_bass_is_quiet_fixed_mono(0);
    test_cabin_bass_is_quiet_fixed_mono(1);
    puts("OK: float synthesis, analysis invariance, exact bypass, linked headroom, metadata/FLUSH ramps");
    return 0;
}
