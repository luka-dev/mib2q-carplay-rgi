/* Copyright (c) 2026, original project author. */
#ifndef SHARE_VIDEO_SINK_H
#define SHARE_VIDEO_SINK_H
#include <stdint.h>

/* Lifecycle used by the real receiver worker. One decoder instance per process.
 * open/close run before/after that worker. frame/session_end are serialized.
 * Input starts with one record containing all required SPS/PPS, followed by
 * complete video access units in decode order. Parameter changes require a
 * new session. Mixed configuration+picture records are not supported.
 */
int av_sink_open(void);
void av_sink_frame(const uint8_t *annexb, uint32_t length);
void av_sink_session_end(void);
void av_sink_close(void);
#endif
