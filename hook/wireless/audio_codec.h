#ifndef CP_WIRELESS_AUDIO_CODEC_H
#define CP_WIRELESS_AUDIO_CODEC_H
#include <stddef.h>
#include <stdint.h>

#define CP_AUDIO_AAC ((UINT64_C(1)<<22) | (UINT64_C(1)<<23))
#define CP_AUDIO_OPUS ((UINT64_C(1)<<28) | (UINT64_C(1)<<29) | (UINT64_C(1)<<30))
#define CP_AUDIO_AU_MAX 1416u
#define CP_AUDIO_SAMPLES_MAX 2048u

/* This is PCM geometry, deliberately separate from Apple's ASBD ABI. */
typedef struct { unsigned rate, channels, frames; int opus; } cp_audio_geometry;
typedef struct cp_audio_codec cp_audio_codec;
int cp_audio_geometry_for(uint64_t format, cp_audio_geometry *geometry);
cp_audio_codec *cp_audio_codec_create(uint64_t format, int encoder);
void cp_audio_codec_destroy(cp_audio_codec *codec);
void cp_audio_codec_reset(cp_audio_codec *codec);
int cp_audio_codec_decode(cp_audio_codec *codec, const void *au, size_t length,
                         int16_t *pcm, size_t samples, unsigned *frames);
int cp_audio_codec_encode(cp_audio_codec *codec, const int16_t *pcm, unsigned frames,
                         void *au, size_t capacity);
/* Probe the actual Cinemo plugin once, outside the loader lock. */
uint64_t cp_audio_codec_formats(void);
#endif
