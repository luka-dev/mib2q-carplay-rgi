/*
 * altctl — control channel from altscreen_render to Java/lsd.
 *
 * The H.264 DATA path is separate (the libairplay hook streams into ALTAV_PORT).  This is the
 * CONTROL path: a background thread connects (as a client) to Java's control server on ALTCTL_PORT
 * and samples current-generation presentation readiness even during a static scene,
 * so Java can update AltScreenModule.setVideoAvailable() and restore ctx 80/81.
 *
 * The current video-on state is resent on every (re)connect, so a transient lsd restart or a
 * control link that comes up after the video already started still converges.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_ALTCTL_H
#define ALTSCREEN_ALTCTL_H

/* Called by the existing control thread, without its own locks, about every 100 ms.
 * The provider must be quick and must reject ended/superseded decode generations. */
void altctl_start(int (*video_ready)(void)); /* provider stays valid until stop */
void altctl_stop(void);         /* join the control thread */

#endif /* ALTSCREEN_ALTCTL_H */
