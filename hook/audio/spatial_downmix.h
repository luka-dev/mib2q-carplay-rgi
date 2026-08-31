/*
 * Deterministic PCM helpers for the CarPlay type-103 render path.
 *
 * Both layouts use Apple's canonical interleaved speaker order:
 *   5.1.2 input: L R C LFE Ls Rs heightL heightR
 *   5.1 output:  L R C LFE Ls Rs
 */
#ifndef SPATIAL_DOWNMIX_H
#define SPATIAL_DOWNMIX_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_PCM_5_1_CHANNELS = 6,
    SPA_PCM_5_1_2_CHANNELS = 8
};

/* Fold interleaved S16 5.1.2 into interleaved S16 5.1.
 *
 * Matrix recovered from Pioneer's SpatialAudioDownMixer path:
 *   L/R/C stay unchanged; LFE is -6 dB; each height channel is folded into
 *   its matching surround at -6 dB.  Mixed surrounds saturate to S16.
 *
 * Returns the number of output samples (frames * 6).  Exact in-place use
 * (input == output) is supported; other overlapping ranges are not.
 */
size_t spa_downmix_5_1_2_s16(const int16_t *input, size_t frames, int16_t *output);

#endif /* SPATIAL_DOWNMIX_H */
