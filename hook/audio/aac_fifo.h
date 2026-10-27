#ifndef CARPLAY_AAC_FIFO_H
#define CARPLAY_AAC_FIFO_H

#include <stddef.h>
#include <stdint.h>

/* Cinemo callbacks are whole interleaved PCM frames.  Refuse malformed or
 * over-capacity callbacks atomically: accepting a prefix would make the codec
 * call look successful while silently deleting the tail of the access unit. */
static inline int spa_aac_fifo_callback_frames(uint32_t pcmBytes,
                                                uint32_t channels,
                                                uint32_t *frames)
{
    uint32_t frameBytes;

    if (!frames || !channels || channels > UINT32_MAX / sizeof(int16_t))
        return 0;
    frameBytes = channels * (uint32_t)sizeof(int16_t);
    if (!frameBytes || pcmBytes % frameBytes != 0)
        return 0;
    *frames = pcmBytes / frameBytes;
    return 1;
}

static inline int spa_aac_fifo_can_append(uint32_t usedFrames,
                                           uint32_t incomingFrames,
                                           uint32_t capacityFrames)
{
    return usedFrames <= capacityFrames &&
           incomingFrames <= capacityFrames - usedFrames;
}

#endif /* CARPLAY_AAC_FIFO_H */
