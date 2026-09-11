/* Copyright (c) 2026, original project author. */
/* Synthetic host demonstration; no production entry points. */
#include "interpose_example.h"
#include <errno.h>

int demo_video_submit(void *context, const uint8_t *data, size_t size)
{
    if (errno != EDOM || !context || *(int *)context != 17 || !data || size != 1)
        return 91;
    errno = data[0] ? EIO : EAGAIN;
    return data[0] ? 23 : 0;
}
