#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

/*
 * Dense lower-bound benchmark for the generator core disclosed in:
 *
 *   C. Uhle, P. Weyers, J. Zimmermann,
 *   "Implementing Two Upmixing Concepts With One Conditional GAN",
 *   AES 2026 Automotive Audio Conference, paper 9.
 *
 * This is NOT a CUpGAN implementation.  The paper does not publish weights
 * and omits details needed to reproduce the IBN, spectrum reconstruction and
 * trained model exactly.  This file implements only the fully dimensioned
 * part of Figure 4: four causal ConvNeXt blocks over (21, 513, T), each with
 * 3x3 depthwise convolution, 21->84->21 pointwise convolutions, layer norm,
 * GELU, FiLM and a residual connection.
 *
 * The purpose is to measure the cost of the disclosed dense core with the
 * same compiler/ISA as the head unit.  It deliberately excludes STFT/iSTFT,
 * six-to-21 IBN, final six-channel projection and the control net, so every
 * reported result is a lower bound for a complete inference backend.
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__QNX__)
#include <sys/neutrino.h>
#include <sys/syspage.h>
#endif

#if defined(CUPGAN_USE_NEON)
#include <arm_neon.h>
#endif

enum {
    CUPGAN_BLOCKS = 4,
    CUPGAN_CHANNELS = 21,
    CUPGAN_HIDDEN = 84,
    CUPGAN_HIDDEN_GROUPS = CUPGAN_HIDDEN / 4,
    CUPGAN_CHANNEL_GROUPS = (CUPGAN_CHANNELS + 3) / 4,
    CUPGAN_BINS = 513,
    CUPGAN_TIME_TAPS = 3,
    CUPGAN_FREQ_TAPS = 3,
    CUPGAN_HOP = 512,
    CUPGAN_MAX_MEASURED_FRAMES = 1024,
    CUPGAN_STRIDE_WORDS = 1024 * 1024
};

#define CUPGAN_PAPER_MACS_PER_SECOND 264000000.0
#define CUPGAN_PAPER_SAMPLE_RATE 44100.0
#define CUPGAN_TARGET_SAMPLE_RATE 48000.0
#define CUPGAN_TARGET_CPU_HZ 1700000000.0

struct cupgan_block {
    float depthwise[CUPGAN_CHANNELS][CUPGAN_TIME_TAPS]
                   [CUPGAN_FREQ_TAPS];
    float depthwise_bias[CUPGAN_CHANNELS];
    float norm_scale[CUPGAN_CHANNELS];
    float norm_bias[CUPGAN_CHANNELS];
#if defined(CUPGAN_USE_NEON)
    /* Four output channels per vector.  Padding exists only in the final
     * 21-channel output group and is never exposed as model output. */
    float pointwise_up[CUPGAN_HIDDEN_GROUPS][CUPGAN_CHANNELS][4];
#else
    float pointwise_up[CUPGAN_HIDDEN][CUPGAN_CHANNELS];
#endif
    float pointwise_up_bias[CUPGAN_HIDDEN];
#if defined(CUPGAN_USE_NEON)
    float pointwise_down[CUPGAN_CHANNEL_GROUPS][CUPGAN_HIDDEN][4];
#else
    float pointwise_down[CUPGAN_CHANNELS][CUPGAN_HIDDEN];
#endif
    float pointwise_down_bias[CUPGAN_CHANNELS];
    float film_scale[CUPGAN_CHANNELS];
    float film_bias[CUPGAN_CHANNELS];
};

struct cupgan_model {
    struct cupgan_block block[CUPGAN_BLOCKS];
};

struct cupgan_state {
    float history[CUPGAN_BLOCKS][CUPGAN_TIME_TAPS]
                 [CUPGAN_BINS][CUPGAN_CHANNELS];
    float current[CUPGAN_BINS][CUPGAN_CHANNELS];
    float next[CUPGAN_BINS][CUPGAN_CHANNELS];
    float depthwise[CUPGAN_BINS][CUPGAN_CHANNELS];
    float normalized[CUPGAN_BINS][CUPGAN_CHANNELS];
    float hidden[CUPGAN_BINS][CUPGAN_HIDDEN];
    unsigned history_position;
};

static struct cupgan_model g_model;
static struct cupgan_state g_state;
static uint64_t g_frame_ticks[CUPGAN_MAX_MEASURED_FRAMES];
static volatile float g_checksum;

#if defined(__QNX__)
static volatile uint32_t g_stride_buffer[CUPGAN_STRIDE_WORDS];
#endif

static uint32_t cupgan_lcg(uint32_t *state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static float cupgan_random_small(uint32_t *state)
{
    uint32_t bits = cupgan_lcg(state) >> 8;
    float unit = (float)bits * (1.0f / 16777215.0f);
    return (unit - 0.5f) * 0.04f;
}

static void cupgan_initialize_model(struct cupgan_model *model)
{
    uint32_t random_state = UINT32_C(0x43555047);
    unsigned block;

    memset(model, 0, sizeof(*model));
    for (block = 0; block < CUPGAN_BLOCKS; ++block) {
        struct cupgan_block *parameters = &model->block[block];
        unsigned channel;
        unsigned hidden;
        unsigned time_tap;
        unsigned frequency_tap;

        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
            for (time_tap = 0; time_tap < CUPGAN_TIME_TAPS; ++time_tap) {
                for (frequency_tap = 0;
                     frequency_tap < CUPGAN_FREQ_TAPS; ++frequency_tap) {
                    parameters->depthwise[channel][time_tap][frequency_tap] =
                        cupgan_random_small(&random_state);
                }
            }
            parameters->depthwise_bias[channel] =
                cupgan_random_small(&random_state);
            parameters->norm_scale[channel] = 1.0f;
            parameters->norm_bias[channel] =
                cupgan_random_small(&random_state);
            parameters->pointwise_down_bias[channel] =
                cupgan_random_small(&random_state);
            parameters->film_scale[channel] = 1.0f;
            parameters->film_bias[channel] =
                cupgan_random_small(&random_state);
        }

        for (hidden = 0; hidden < CUPGAN_HIDDEN; ++hidden) {
            parameters->pointwise_up_bias[hidden] =
                cupgan_random_small(&random_state);
            for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
#if defined(CUPGAN_USE_NEON)
                parameters->pointwise_up[hidden / 4][channel][hidden % 4] =
                    cupgan_random_small(&random_state);
                parameters->pointwise_down[channel / 4][hidden][channel % 4] =
                    cupgan_random_small(&random_state);
#else
                parameters->pointwise_up[hidden][channel] =
                    cupgan_random_small(&random_state);
                parameters->pointwise_down[channel][hidden] =
                    cupgan_random_small(&random_state);
#endif
            }
        }
    }
}

static void cupgan_prepare_input(struct cupgan_state *state,
                                 unsigned frame_index)
{
    unsigned bin;

    for (bin = 0; bin < CUPGAN_BINS; ++bin) {
        unsigned channel;
        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
            uint32_t value = (uint32_t)(frame_index + 1) * UINT32_C(1315423911) ^
                             (uint32_t)(bin + 3) * UINT32_C(2654435761) ^
                             (uint32_t)(channel + 7) * UINT32_C(2246822519);
            int32_t centered = (int32_t)((value >> 16) & UINT32_C(0xffff)) -
                               INT32_C(32768);
            state->current[bin][channel] = (float)centered *
                                           (1.0f / 65536.0f);
        }
    }
}

/*
 * A bounded rational approximation of the tanh-based GELU.  The paper names
 * GELU but not the inference approximation.  Calling libm tanhf once per
 * latent value would benchmark the library rather than the convolution core;
 * this explicit approximation makes the measured kernel reproducible on QNX.
 */
static float cupgan_fast_gelu(float input)
{
    const float kSqrtTwoOverPi = 0.7978845608028654f;
    float input_squared = input * input;
    float argument = kSqrtTwoOverPi *
                     (input + 0.044715f * input * input_squared);
    float argument_squared;
    float tanh_approximation;

    if (argument > 3.0f)
        tanh_approximation = 1.0f;
    else if (argument < -3.0f)
        tanh_approximation = -1.0f;
    else {
        argument_squared = argument * argument;
        tanh_approximation = argument * (27.0f + argument_squared) /
                             (27.0f + 9.0f * argument_squared);
    }
    return 0.5f * input * (1.0f + tanh_approximation);
}

static void cupgan_run_block(const struct cupgan_block *restrict parameters,
                             struct cupgan_state *restrict state,
                             unsigned block_index)
{
    const unsigned position = state->history_position;
    unsigned bin;

    memcpy(state->history[block_index][position], state->current,
           sizeof(state->current));

    for (bin = 0; bin < CUPGAN_BINS; ++bin) {
        unsigned channel;
        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
            float accumulator = parameters->depthwise_bias[channel];
            unsigned time_tap;

            for (time_tap = 0; time_tap < CUPGAN_TIME_TAPS; ++time_tap) {
                unsigned history_index =
                    (position + CUPGAN_TIME_TAPS - time_tap) %
                    CUPGAN_TIME_TAPS;
                int frequency_offset;

                for (frequency_offset = -1; frequency_offset <= 1;
                     ++frequency_offset) {
                    int source_bin = (int)bin + frequency_offset;
                    if (source_bin >= 0 && source_bin < CUPGAN_BINS) {
                        accumulator +=
                            state->history[block_index][history_index]
                                          [source_bin][channel] *
                            parameters->depthwise[channel][time_tap]
                                                 [frequency_offset + 1];
                    }
                }
            }
            state->depthwise[bin][channel] = accumulator;
        }
    }

    for (bin = 0; bin < CUPGAN_BINS; ++bin) {
        float mean = 0.0f;
        float variance = 0.0f;
        float inverse_standard_deviation;
        unsigned channel;
        unsigned hidden;

        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel)
            mean += state->depthwise[bin][channel];
        mean *= 1.0f / (float)CUPGAN_CHANNELS;

        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
            float centered = state->depthwise[bin][channel] - mean;
            variance += centered * centered;
        }
        variance *= 1.0f / (float)CUPGAN_CHANNELS;
        inverse_standard_deviation = 1.0f / sqrtf(variance + 1.0e-5f);

        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
            state->normalized[bin][channel] =
                (state->depthwise[bin][channel] - mean) *
                    inverse_standard_deviation *
                    parameters->norm_scale[channel] +
                parameters->norm_bias[channel];
        }

#if defined(CUPGAN_USE_NEON)
        for (hidden = 0; hidden < CUPGAN_HIDDEN; hidden += 4) {
            float gelu_input[4];
            float32x4_t accumulator =
                vld1q_f32(parameters->pointwise_up_bias + hidden);

            for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
                float32x4_t weights = vld1q_f32(
                    parameters->pointwise_up[hidden / 4][channel]);
                accumulator = vmlaq_n_f32(
                    accumulator, weights,
                    state->normalized[bin][channel]);
            }
            vst1q_f32(gelu_input, accumulator);
            state->hidden[bin][hidden] = cupgan_fast_gelu(gelu_input[0]);
            state->hidden[bin][hidden + 1] =
                cupgan_fast_gelu(gelu_input[1]);
            state->hidden[bin][hidden + 2] =
                cupgan_fast_gelu(gelu_input[2]);
            state->hidden[bin][hidden + 3] =
                cupgan_fast_gelu(gelu_input[3]);
        }

        for (channel = 0; channel < CUPGAN_CHANNELS; channel += 4) {
            unsigned group = channel / 4;
            unsigned valid = CUPGAN_CHANNELS - channel;
            float result[4];
            float bias[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float32x4_t accumulator;
            unsigned lane;

            if (valid > 4)
                valid = 4;
            for (lane = 0; lane < valid; ++lane)
                bias[lane] = parameters->pointwise_down_bias[channel + lane];
            accumulator = vld1q_f32(bias);
            for (hidden = 0; hidden < CUPGAN_HIDDEN; ++hidden) {
                float32x4_t weights =
                    vld1q_f32(parameters->pointwise_down[group][hidden]);
                accumulator = vmlaq_n_f32(
                    accumulator, weights, state->hidden[bin][hidden]);
            }
            vst1q_f32(result, accumulator);
            for (lane = 0; lane < valid; ++lane) {
                unsigned output_channel = channel + lane;
                state->next[bin][output_channel] =
                    state->current[bin][output_channel] +
                    result[lane] *
                        parameters->film_scale[output_channel] +
                    parameters->film_bias[output_channel];
            }
        }
#else
        for (hidden = 0; hidden < CUPGAN_HIDDEN; ++hidden) {
            float accumulator = parameters->pointwise_up_bias[hidden];
            for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
                accumulator += state->normalized[bin][channel] *
                               parameters->pointwise_up[hidden][channel];
            }
            state->hidden[bin][hidden] = cupgan_fast_gelu(accumulator);
        }

        for (channel = 0; channel < CUPGAN_CHANNELS; ++channel) {
            float accumulator = parameters->pointwise_down_bias[channel];
            for (hidden = 0; hidden < CUPGAN_HIDDEN; ++hidden) {
                accumulator += state->hidden[bin][hidden] *
                               parameters->pointwise_down[channel][hidden];
            }
            state->next[bin][channel] =
                state->current[bin][channel] +
                accumulator * parameters->film_scale[channel] +
                parameters->film_bias[channel];
        }
#endif
    }

    memcpy(state->current, state->next, sizeof(state->current));
}

static void cupgan_run_frame(const struct cupgan_model *restrict model,
                             struct cupgan_state *restrict state)
{
    unsigned block;

    for (block = 0; block < CUPGAN_BLOCKS; ++block)
        cupgan_run_block(&model->block[block], state, block);
    state->history_position =
        (state->history_position + 1) % CUPGAN_TIME_TAPS;
}

#if defined(__QNX__)
static uint64_t cupgan_now(void)
{
    return (uint64_t)ClockCycles();
}

__attribute__((noinline)) static void cupgan_spin2(uint32_t iterations)
{
    __asm__ volatile("1:\n\tsubs %0, %0, #1\n\tbne 1b\n"
                     : "+r"(iterations) : : "cc");
}

__attribute__((noinline)) static void cupgan_spin4(uint32_t iterations)
{
    __asm__ volatile("1:\n\tnop\n\tnop\n\tsubs %0, %0, #1\n\tbne 1b\n"
                     : "+r"(iterations) : : "cc");
}

__attribute__((noinline)) static uint32_t
cupgan_stride_walk(uint32_t iterations)
{
    uint32_t accumulator = 0;
    uint32_t position = 0;
    uint32_t index;

    for (index = 0; index < iterations; ++index) {
        accumulator += g_stride_buffer[position];
        position += 16;
        if (position >= CUPGAN_STRIDE_WORDS)
            position = 0;
    }
    return accumulator;
}

static double cupgan_calibrate_instruction_clock(void)
{
    const uint32_t iterations = UINT32_C(500000);
    uint64_t start;
    uint64_t end;
    double spin2_ticks;
    double spin4_ticks;
    double walk_ticks;
    double ticks_per_instruction;
    double spin_agreement;
    double walk_ratio;
    uint32_t index;
    uint32_t checksum;

    for (index = 0; index < CUPGAN_STRIDE_WORDS; ++index)
        g_stride_buffer[index] = index;

    start = cupgan_now();
    cupgan_spin2(iterations);
    end = cupgan_now();
    spin2_ticks = (double)(end - start);

    start = cupgan_now();
    cupgan_spin4(iterations);
    end = cupgan_now();
    spin4_ticks = (double)(end - start);

    start = cupgan_now();
    checksum = cupgan_stride_walk(iterations);
    end = cupgan_now();
    walk_ticks = (double)(end - start);

    ticks_per_instruction =
        (spin4_ticks - spin2_ticks) / (2.0 * (double)iterations);
    spin_agreement = ticks_per_instruction > 0.0
                         ? (spin2_ticks / (2.0 * (double)iterations)) /
                               ticks_per_instruction
                         : 0.0;
    walk_ratio = ticks_per_instruction > 0.0
                     ? (walk_ticks / (4.0 * (double)iterations)) /
                           ticks_per_instruction
                     : 0.0;

    printf("clock: ClockCycles, cycles_per_sec=%llu\n",
           (unsigned long long)SYSPAGE_ENTRY(qtime)->cycles_per_sec);
    printf("clock calibration: spin2=%.0f spin4=%.0f agreement=%.3f "
           "stride_ratio=%.2f checksum=%u\n",
           spin2_ticks, spin4_ticks, spin_agreement, walk_ratio, checksum);

    if (ticks_per_instruction > 0.0 && spin_agreement > 0.98 &&
        spin_agreement < 1.02 && walk_ratio < 5.0) {
        printf("clock classification: instruction count; "
               "1 tick = %.3f ARMv7 instructions\n",
               1.0 / ticks_per_instruction);
        return ticks_per_instruction;
    }

    printf("clock classification: target cycles/time, not instruction count\n");
    return 0.0;
}
#elif defined(__APPLE__)
static uint64_t cupgan_now(void)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}
#else
static uint64_t cupgan_now(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
           (uint64_t)now.tv_nsec;
}
#endif

static int cupgan_compare_u64(const void *left, const void *right)
{
    uint64_t lhs = *(const uint64_t *)left;
    uint64_t rhs = *(const uint64_t *)right;
    return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

static long cupgan_parse_count(const char *text, const char *name)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value < 1 ||
        value > CUPGAN_MAX_MEASURED_FRAMES) {
        fprintf(stderr, "cupgan_bench: %s must be in 1..%d\n", name,
                CUPGAN_MAX_MEASURED_FRAMES);
        exit(2);
    }
    return value;
}

static void cupgan_print_static_costs(void)
{
    const uint64_t depthwise_macs_per_block =
        (uint64_t)CUPGAN_CHANNELS * CUPGAN_TIME_TAPS *
        ((uint64_t)CUPGAN_BINS * CUPGAN_FREQ_TAPS - 2);
    const uint64_t pointwise_macs_per_block =
        UINT64_C(2) * CUPGAN_BINS * CUPGAN_CHANNELS * CUPGAN_HIDDEN;
    const uint64_t implemented_macs_per_frame =
        CUPGAN_BLOCKS *
        (depthwise_macs_per_block + pointwise_macs_per_block);
    const uint64_t padded_macs_per_frame =
        (uint64_t)CUPGAN_BLOCKS * CUPGAN_BINS *
        (CUPGAN_CHANNELS * CUPGAN_TIME_TAPS * CUPGAN_FREQ_TAPS +
         UINT64_C(2) * CUPGAN_CHANNELS * CUPGAN_HIDDEN);
    const uint64_t disclosed_weight_parameters =
        (uint64_t)CUPGAN_BLOCKS *
        (CUPGAN_CHANNELS * CUPGAN_TIME_TAPS * CUPGAN_FREQ_TAPS +
         UINT64_C(2) * CUPGAN_CHANNELS * CUPGAN_HIDDEN);
    const double target_frames_per_second =
        CUPGAN_TARGET_SAMPLE_RATE / (double)CUPGAN_HOP;
    const double paper_scaled_macs =
        CUPGAN_PAPER_MACS_PER_SECOND * CUPGAN_TARGET_SAMPLE_RATE /
        CUPGAN_PAPER_SAMPLE_RATE;
    const double disclosed_core_macs =
        (double)padded_macs_per_frame * target_frames_per_second;

    printf("scope: disclosed four-block ConvNeXt core only "
           "(no STFT, IBN, output projection or control net)\n");
    printf("shape: blocks=%d channels=%d hidden=%d bins=%d hop=%d\n",
           CUPGAN_BLOCKS, CUPGAN_CHANNELS, CUPGAN_HIDDEN, CUPGAN_BINS,
           CUPGAN_HOP);
    printf("weights represented: %llu convolution weights; model=%lu bytes, "
           "state=%lu bytes\n",
           (unsigned long long)disclosed_weight_parameters,
           (unsigned long)sizeof(g_model), (unsigned long)sizeof(g_state));
    printf("dense MAC count: implemented=%llu/frame, conventional padded=%llu/frame\n",
           (unsigned long long)implemented_macs_per_frame,
           (unsigned long long)padded_macs_per_frame);
    printf("48 kHz dense core: %.3f M MAC/s; paper total scaled from 44.1 kHz: "
           "%.3f M MAC/s; ratio=%.3fx\n",
           disclosed_core_macs / 1.0e6, paper_scaled_macs / 1.0e6,
           disclosed_core_macs / paper_scaled_macs);
}

int main(int argc, char **argv)
{
#if defined(__QNX__)
    long measured_frames = 1;
    long warmup_frames = 2;
    double ticks_per_instruction;
#else
    long measured_frames = 64;
    long warmup_frames = 4;
#endif
    uint64_t sorted_ticks[CUPGAN_MAX_MEASURED_FRAMES];
    uint64_t total_ticks = 0;
    long frame;
    int argument;

    for (argument = 1; argument < argc; ++argument) {
        if (!strcmp(argv[argument], "--frames") && argument + 1 < argc) {
            measured_frames =
                cupgan_parse_count(argv[++argument], "--frames");
        } else if (!strcmp(argv[argument], "--warmup") &&
                   argument + 1 < argc) {
            warmup_frames =
                cupgan_parse_count(argv[++argument], "--warmup");
        } else {
            fprintf(stderr,
                    "usage: %s [--frames 1..%d] [--warmup 1..%d]\n",
                    argv[0], CUPGAN_MAX_MEASURED_FRAMES,
                    CUPGAN_MAX_MEASURED_FRAMES);
            return 2;
        }
    }

    cupgan_print_static_costs();
#if defined(__QNX__)
    ticks_per_instruction = cupgan_calibrate_instruction_clock();
#endif

    cupgan_initialize_model(&g_model);
    memset(&g_state, 0, sizeof(g_state));

    for (frame = 0; frame < warmup_frames; ++frame) {
        cupgan_prepare_input(&g_state, (unsigned)frame);
        cupgan_run_frame(&g_model, &g_state);
    }

    for (frame = 0; frame < measured_frames; ++frame) {
        uint64_t start;
        uint64_t end;

        cupgan_prepare_input(&g_state, (unsigned)(frame + warmup_frames));
        start = cupgan_now();
        cupgan_run_frame(&g_model, &g_state);
        end = cupgan_now();
        g_frame_ticks[frame] = end - start;
        total_ticks += g_frame_ticks[frame];
    }

    g_checksum = g_state.current[CUPGAN_BINS / 2][CUPGAN_CHANNELS / 2];
    memcpy(sorted_ticks, g_frame_ticks,
           (size_t)measured_frames * sizeof(sorted_ticks[0]));
    qsort(sorted_ticks, (size_t)measured_frames, sizeof(sorted_ticks[0]),
          cupgan_compare_u64);

    printf("measured: frames=%ld warmup=%ld checksum=%.9g\n",
           measured_frames, warmup_frames, (double)g_checksum);
#if defined(__QNX__)
    printf("target ticks/frame: mean=%.1f p50=%llu p99=%llu max=%llu\n",
           (double)total_ticks / (double)measured_frames,
           (unsigned long long)sorted_ticks[measured_frames / 2],
           (unsigned long long)sorted_ticks[
               ((uint64_t)measured_frames * 99) / 100],
           (unsigned long long)sorted_ticks[measured_frames - 1]);
    if (ticks_per_instruction > 0.0) {
        double mean_instructions =
            ((double)total_ticks / (double)measured_frames) /
            ticks_per_instruction;
        double instructions_per_audio_second =
            mean_instructions * CUPGAN_TARGET_SAMPLE_RATE /
            (double)CUPGAN_HOP;
        printf("ARMv7 instructions: %.0f/frame, %.3f G/audio-second\n",
               mean_instructions, instructions_per_audio_second / 1.0e9);
        printf("estimated one-core load at 1.7 GHz: IPC0.6=%.1f%% "
               "IPC1.0=%.1f%% IPC1.4=%.1f%%\n",
               100.0 * instructions_per_audio_second /
                   (CUPGAN_TARGET_CPU_HZ * 0.6),
               100.0 * instructions_per_audio_second /
                   CUPGAN_TARGET_CPU_HZ,
               100.0 * instructions_per_audio_second /
                   (CUPGAN_TARGET_CPU_HZ * 1.4));
    }
#else
    {
        double mean_nanoseconds =
            (double)total_ticks / (double)measured_frames;
        double packet_budget_nanoseconds =
            1.0e9 * (double)CUPGAN_HOP / CUPGAN_TARGET_SAMPLE_RATE;
        printf("host time/frame: mean=%.3f us p50=%.3f us p99=%.3f us "
               "max=%.3f us\n",
               mean_nanoseconds / 1.0e3,
               (double)sorted_ticks[measured_frames / 2] / 1.0e3,
               (double)sorted_ticks[
                   ((uint64_t)measured_frames * 99) / 100] / 1.0e3,
               (double)sorted_ticks[measured_frames - 1] / 1.0e3);
        printf("host one-core load versus 48 kHz/512-hop deadline: %.2f%%\n",
               100.0 * mean_nanoseconds / packet_budget_nanoseconds);
    }
#endif
    return 0;
}
