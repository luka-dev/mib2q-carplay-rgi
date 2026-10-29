#ifndef CP_WIRELESS_AUDIO_QUEUE_H
#define CP_WIRELESS_AUDIO_QUEUE_H
#include "audio_codec.h"
typedef struct cp_audio_queue cp_audio_queue;
cp_audio_queue *cp_audio_queue_create(uint64_t format, unsigned latency_ms);
void cp_audio_queue_destroy(cp_audio_queue *queue);
void cp_audio_queue_reset(cp_audio_queue *queue);
/* Timestamps are in negotiated sample-rate units, time in monotonic ns.
 * Calls are serialized by the owning stream, never by an audio device lock. */
int cp_audio_queue_put(cp_audio_queue *queue, uint32_t timestamp,
                       const void *au, size_t length, uint64_t now_ns);
int cp_audio_queue_read(cp_audio_queue *queue, int16_t *pcm, size_t samples,
                        uint64_t now_ns);
#endif
