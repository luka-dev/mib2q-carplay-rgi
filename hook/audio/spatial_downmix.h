/*
 * Deterministic PCM helpers for the CarPlay type-103 render path.
 *
 * Channel order is NOT self-evident on this path; two different orders are in
 * play and they were both measured, not assumed.
 *
 * 1. The order CarPlay DECLARES.  Pioneer's AirPlayAudioFormatIndexToASBD maps
 *    format index 65 to CoreAudio layout tag 0x00790006 and index 66 to
 *    0x00c20008.  Resolved against CoreAudio itself
 *    (kAudioFormatProperty_ChannelLayoutForTag) these are:
 *      index 65 -> L R C LFE Ls Rs                    (MPEG_5_1_A)
 *      index 66 -> L R C LFE Ls Rs LeftTopFront RightTopFront
 *    The fold below is written for exactly this order.
 *
 * 2. The order the DECODER hands back.  Apple's APAC decoder, driven through
 *    AudioConverter with the raw 33-byte CarPlay magic cookie and no output
 *    layout set, emits the six channels as
 *      C L R Ls Rs LFE
 *    Measured on 74.6 s of live iOS 27 index-65 traffic
 *    (captures/apac_5_1_live_20260831_0205): channel 5 carries 100 % of its
 *    energy below 120 Hz, channels 1/2 correlate at 0.61 and channels 3/4 at
 *    0.22.  The declared order would put LFE in channel 3, which measures 1 %
 *    sub-120 Hz and cannot be an LFE feed.  A locally encoded APAC round trip
 *    confirms the decoder preserves encoder slot order rather than re-ordering
 *    to the layout tag, so this is a property of the stream, not of the oracle.
 *
 * 3. The order the STREAM ITSELF declares.  Parsing the live 33-byte cookie's
 *    apac::GlobalConfig gives channel layout tag 0x007c0006, which CoreAudio
 *    resolves as C L R Ls Rs LFE.  That is the same order the decoder was
 *    measured to emit, arrived at independently: from the bitstream rather than
 *    from the audio.  Where it disagrees with Pioneer's format-index table
 *    (which maps index 65 to 0x00790006, L R C LFE Ls Rs), the cookie wins -
 *    it is what the sender actually encoded.
 *
 * So a decoder frame must be normalised into the declared order before it is
 * folded.  spa_reorder_5_1_decoder_to_declared_s16() does that for 5.1.
 *
 * UNVERIFIED: the equivalent native order for index 66 (eight channels).  Only
 * the six-channel case has been measured.  Do not assume the eight-channel
 * stream is "the same plus two heights" until an index-66 capture is measured
 * the same way.
 */
#ifndef SPATIAL_DOWNMIX_H
#define SPATIAL_DOWNMIX_H

#include <stddef.h>
#include <stdint.h>

enum {
    SPA_PCM_5_1_CHANNELS = 6,
    SPA_PCM_5_1_2_CHANNELS = 8
};

/* Permute interleaved S16 5.1 from the decoder's native order
 * (C L R Ls Rs LFE) into the declared CarPlay order (L R C LFE Ls Rs).
 *
 * Returns the number of output samples (frames * 6).  Exact in-place use
 * (input == output) is supported; other overlapping ranges are not.
 */
size_t spa_reorder_5_1_decoder_to_declared_s16(const int16_t *input, size_t frames,
                                               int16_t *output);

/* Fold interleaved S16 5.1.2 into interleaved S16 5.1.  Input must already be
 * in the declared order documented above.
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
