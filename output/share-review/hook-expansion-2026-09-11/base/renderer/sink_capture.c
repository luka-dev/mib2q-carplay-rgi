/* Copyright (c) 2026, original project author. */
/* Adapted host sink: counters, optional raw capture, no display claims. */
#include "sink.h"
#include <stdio.h>
#include <stdlib.h>

static FILE *output;
static unsigned long messages, bytes;
static int write_failed;

int av_sink_open(void)
{
    const char *path=getenv("VIDEO_CAPTURE_FILE");
    if (path && *path) {
        output=fopen(path,"wb");
        if (!output) { perror("video capture"); return -1; }
    }
    return 0;
}

void av_sink_frame(const uint8_t *data, uint32_t length)
{
    if (!data || !length || write_failed) return;
    if (output && fwrite(data,1,length,output)!=length) {
        write_failed=1;
        fprintf(stderr,"video_capture: write failed\n");
        return;
    }
    ++messages;
    bytes+=length;
}

void av_sink_session_end(void)
{
    if (output && fflush(output)!=0) write_failed=1;
    fprintf(stderr,"video_capture: session end messages=%lu bytes=%lu error=%d\n",
            messages,bytes,write_failed);
    messages=bytes=0;
}

void av_sink_close(void)
{
    if (output) { fclose(output); output=NULL; }
}
