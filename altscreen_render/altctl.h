/*
 * altctl — control channel from altscreen_render to Java/lsd.
 *
 * The H.264 DATA path is separate (the libairplay hook streams into ALTAV_PORT).  This is the
 * CONTROL path: a background thread connects (as a client) to Java's control server on ALTCTL_PORT
 * and reports whether the CarPlay 111 video is currently flowing, so Java can flip
 * AltScreenModule.setVideoAvailable() and switch the cluster to ctx 82/83.
 *
 * The current video-on state is resent on every (re)connect, so a transient lsd restart or a
 * control link that comes up after the video already started still converges.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_ALTCTL_H
#define ALTSCREEN_ALTCTL_H

void altctl_start(void);        /* spawn the control thread (idempotent) */
void altctl_set_video(int on);  /* main thread: 1 = video client connected/streaming, 0 = off */
void altctl_stop(void);         /* join the control thread */

#endif /* ALTSCREEN_ALTCTL_H */
