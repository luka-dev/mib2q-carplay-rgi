#ifndef UNIFIED_AUDIO_STREAM_H
#define UNIFIED_AUDIO_STREAM_H
#include "unified_runtime.h"
#define UMX_HOP 512
#define UMX_FFT 2048
#define UMX_DELAY (UMX_FFT-UMX_HOP)
typedef struct UmxAudioStream UmxAudioStream;
/* Model must outlive stream; NULL model is the transform identity diagnostic.
 * One allocation at creation; none during reset/process. One stream per caller.
 * No hook ABI, renderer, scene routing, deadline or device qualification. */
UmxAudioStream *umx_audio_create(const UmxModel *model);
void umx_audio_free(UmxAudioStream *stream);
void umx_audio_reset(UmxAudioStream *stream);
/* Exactly one stereo hop in and out [sample,L/R]. Output has fixed1536sample
 * array delay. Caller handles incomplete hop and three-hop end flush. Never
 * compensate delay by advancing real-time output; align front accordingly. */
int umx_audio_process(UmxAudioStream *stream,const float input[UMX_HOP][2],float output[UMX_HOP][2]);
/* Family fields remain untrained in current assets. */
const UmxOutput *umx_audio_prediction(const UmxAudioStream *stream);
unsigned umx_audio_state_bytes(void);
#endif
