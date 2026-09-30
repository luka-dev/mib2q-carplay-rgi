#ifndef UNIFIED_RUNTIME_H
#define UNIFIED_RUNTIME_H
#include <stdint.h>

/* Track B standalone runtime. No hook ABI, FFT, audio routing or device claim. */
#define UMX_BINS 1025
#define UMX_BANDS 48
#define UMX_SOURCES 4
#define UMX_FAMILIES 6

typedef struct UmxModel UmxModel;
typedef struct {
    float time[2][UMX_BANDS][48];
    float global[24];
    float level;
    float membership[UMX_BANDS][UMX_FAMILIES];
    float pan[UMX_BANDS][UMX_FAMILIES];
    uint64_t frames;
} UmxState;
typedef struct {
    float mask[UMX_SOURCES][UMX_BINS][2][2]; /* source,bin,L/R,real/imag */
    float presence[UMX_BANDS][UMX_FAMILIES];
    float membership[UMX_BANDS][UMX_FAMILIES];
    float pan[UMX_BANDS][UMX_FAMILIES];
    float update[UMX_BANDS][UMX_FAMILIES];
    uint64_t dense_macs; /* all four source heads and family head */
} UmxOutput;

UmxModel *umx_load(const char *path, char *error, unsigned error_size);
void umx_free(UmxModel *model);
void umx_reset(UmxState *state);
/* After applying any complex mask, zero imaginary DC/Nyquist before iFFT. */
int umx_predict(const UmxModel *model, UmxState *state,
                const float spectrum[UMX_BINS][2][2], UmxOutput *output);
/* Identical eligible/scene/state path, skip unused protected output rows.
 * Protected mask fields are explicitly zero; all heads remain in the blob.
 * Auxiliary training heads do not need inference when routing only eligible. */
int umx_predict_eligible(const UmxModel *model, UmxState *state,
                        const float spectrum[UMX_BINS][2][2], UmxOutput *output);
#endif
