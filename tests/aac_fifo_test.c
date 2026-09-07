#include "audio/aac_fifo.h"

#include <stdio.h>

int main(void)
{
    uint32_t frames = 99;

    if (!spa_aac_fifo_callback_frames(4096, 2, &frames) || frames != 1024)
        return 1;
    if (spa_aac_fifo_callback_frames(4097, 2, &frames))
        return 2;
    if (spa_aac_fifo_callback_frames(4096, 0, &frames))
        return 3;
    if (!spa_aac_fifo_can_append(3072, 1024, 4096))
        return 4;
    if (spa_aac_fifo_can_append(3072, 1025, 4096))
        return 5;
    if (spa_aac_fifo_can_append(4097, 0, 4096))
        return 6;

    puts("aac_fifo_test: all tests passed");
    return 0;
}
