/* Host test surface ONLY. Not a QNX ABI definition; shipping builds use SDP. */
#ifndef TEST_ASOUNDLIB_H
#define TEST_ASOUNDLIB_H
#include <stdint.h>
typedef struct { int id; } snd_pcm_t;
typedef struct { int interleave, format, rate, voices; } test_pcm_format_t;
typedef struct { int frag_size, frags_min, frags_max; uint32_t max_frag_size; } test_pcm_block_t;
typedef struct {
    int channel, mode, start_mode, stop_mode, why_failed;
    test_pcm_format_t format;
    union { test_pcm_block_t block; } buf;
} snd_pcm_channel_params_t;
typedef struct {
    int channel, mode;
    test_pcm_format_t format;
    union { test_pcm_block_t block; } buf;
} snd_pcm_channel_setup_t;
typedef struct {
    int channel, min_voices, max_voices, min_rate, max_rate;
    int min_fragment_size, max_fragment_size;
    unsigned formats;
} snd_pcm_channel_info_t;
typedef struct { int channel, status; unsigned underrun; } snd_pcm_channel_status_t;
#define SND_PCM_CHANNEL_PLAYBACK 0
#define SND_PCM_MODE_BLOCK 1
#define SND_PCM_SFMT_S16_LE 2
#define SND_PCM_OPEN_PLAYBACK 1
#define SND_PCM_START_FULL 1
#define SND_PCM_STOP_STOP 1
#define SND_PCM_STATUS_RUNNING 4
#define SND_PCM_STATUS_UNDERRUN 5
#define PLUGIN_DISABLE_MMAP 1
#endif
