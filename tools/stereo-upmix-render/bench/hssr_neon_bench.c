/*
 * Small ARMv7/NEON throughput probe for the proposed HSSR control network.
 *
 * This deliberately benchmarks only dense signed-int8 GEMV work.  It is not
 * an inference engine and does not include GRU nonlinearities, requantisation,
 * feature extraction, or STFT processing.  The matrix shapes and MAC counts do
 * match a two-layer GRU controller with a 264-value output head.
 */
#include <arm_neon.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define INPUT_FEATURES 368
#define OUTPUT_FEATURES 264
#define MAX_HIDDEN 160
#define MAX_COLS (INPUT_FEATURES + MAX_HIDDEN)
#define MAX_ROWS (3 * MAX_HIDDEN)
#define MAX_WEIGHTS 450504

static int8_t g_weights[MAX_WEIGHTS];
static int8_t g_input[MAX_COLS];
static int32_t g_output[MAX_ROWS];
static volatile uint32_t g_sink;

static uint64_t now_ns(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

static int32_t dot_s8_neon(const int8_t *a, const int8_t *b, int cols)
{
    int32x4_t sum = vdupq_n_s32(0);
    int c = 0;
    int32_t lanes[4];
    int32_t scalar;

    for (; c + 15 < cols; c += 16) {
        const int8x16_t va = vld1q_s8(a + c);
        const int8x16_t vb = vld1q_s8(b + c);
        const int16x8_t lo = vmull_s8(vget_low_s8(va), vget_low_s8(vb));
        const int16x8_t hi = vmull_s8(vget_high_s8(va), vget_high_s8(vb));
        sum = vpadalq_s16(sum, lo);
        sum = vpadalq_s16(sum, hi);
    }

    vst1q_s32(lanes, sum);
    scalar = lanes[0] + lanes[1] + lanes[2] + lanes[3];
    for (; c < cols; ++c) {
        scalar += (int32_t)a[c] * (int32_t)b[c];
    }
    return scalar;
}

static void gemv_s8_neon(const int8_t *weights, const int8_t *input,
                         int rows, int cols, int32_t *output)
{
    int r;
    for (r = 0; r < rows; ++r) {
        output[r] = dot_s8_neon(weights + (size_t)r * (size_t)cols,
                                input, cols);
    }
}

static uint64_t controller_pass(int hidden)
{
    const int layer1_rows = 3 * hidden;
    const int layer1_cols = INPUT_FEATURES + hidden;
    const int layer2_rows = 3 * hidden;
    const int layer2_cols = 2 * hidden;
    const int head_rows = OUTPUT_FEATURES;
    const int head_cols = hidden;
    const size_t layer1_weights =
        (size_t)layer1_rows * (size_t)layer1_cols;
    const size_t layer2_weights =
        (size_t)layer2_rows * (size_t)layer2_cols;
    const size_t head_weights =
        (size_t)head_rows * (size_t)head_cols;

    gemv_s8_neon(g_weights, g_input,
                 layer1_rows, layer1_cols, g_output);
    gemv_s8_neon(g_weights + layer1_weights, g_input,
                 layer2_rows, layer2_cols, g_output);
    gemv_s8_neon(g_weights + layer1_weights + layer2_weights, g_input,
                 head_rows, head_cols, g_output);

    g_sink += (uint32_t)g_output[0] + (uint32_t)g_output[head_rows - 1];
    return (uint64_t)(layer1_weights + layer2_weights + head_weights);
}

static void run_shape(int hidden, int iterations)
{
    uint64_t macs;
    uint64_t begin;
    uint64_t elapsed;
    double us_per_control;
    double throughput_mmac_s;
    double worker_core_percent;
    int i;

    for (i = 0; i < 8; ++i) {
        (void)controller_pass(hidden);
    }

    macs = controller_pass(hidden);
    begin = now_ns();
    for (i = 0; i < iterations; ++i) {
        (void)controller_pass(hidden);
    }
    elapsed = now_ns() - begin;

    us_per_control = (double)elapsed / (double)iterations / 1000.0;
    throughput_mmac_s =
        ((double)macs * (double)iterations) / (double)elapsed * 1000.0;
    worker_core_percent = us_per_control * 93.75 / 10000.0;

    printf("H=%d macs/control=%" PRIu64
           " weights=%.3fMiB time=%.3fus throughput=%.1fMMAC/s"
           " worker_core@93.75Hz=%.2f%%\n",
           hidden, macs, (double)macs / (1024.0 * 1024.0),
           us_per_control, throughput_mmac_s, worker_core_percent);
}

int main(void)
{
    size_t i;

    for (i = 0; i < MAX_WEIGHTS; ++i) {
        g_weights[i] = (int8_t)(((i * 29u + 17u) % 255u) - 127);
    }
    for (i = 0; i < MAX_COLS; ++i) {
        g_input[i] = (int8_t)(((i * 13u + 5u) % 255u) - 127);
    }

    puts("HSSR ARMv7 NEON int8 GEMV microbenchmark");
    puts("Note: matrix core only; excludes GRU gates/state, STFT and routing.");
    run_shape(96, 512);
    run_shape(128, 512);
    run_shape(160, 512);
    printf("checksum=%" PRIu32 "\n", g_sink);
    return 0;
}
