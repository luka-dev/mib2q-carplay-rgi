#ifndef CP_WIRELESS_AUDIO_STREAM_H
#define CP_WIRELESS_AUDIO_STREAM_H
#include "audio_codec.h"
int cp_wireless_audio_enabled(void);
/* Stock210 takes a nine-uint32 PCM ASBD. Decode happens before its PCM reader. */
int cp_wireless_audio_asbd(uint64_t format,uint32_t *asbd,uint32_t *bits);
int cp_wireless_audio_receive(void *jitter,uint32_t timestamp,const void *au,size_t length);
typedef int (*cp_audio_emit)(void *,const uint8_t *,size_t,uint32_t);
/* RX and TX can progress concurrently. emit runs with TX pinned: it may send
 * but must not synchronously init/free/reset jitter or mutate media sessions.
 * Native stop/join still precedes destruction of the caller's jitter memory. */
int cp_wireless_audio_encode(void *jitter,uint32_t timestamp,const uint8_t *pcm_be,
                             size_t length,cp_audio_emit emit,void *context);
#endif
