/*
 * altscreen_render — wire protocol (hook → renderer).
 *
 * The CarPlay hook (hook/altscreen/altscreen_av.c) receives the decrypted cluster
 * (111) stream, converts each frame AVCC→Annex-B, and forwards it here over a
 * localhost TCP connection.  Renderer is the SERVER; the hook is the client and
 * reconnects on stream generation or continuity loss.
 *
 * Legacy framing is [u32 BE payload_len][payload]; negotiated v2 carries the
 * 40-byte metadata header from common/alt_video_wire.h. HELLO capability 3
 * offers an opt-in presentation status channel, enabled by the hook's ACK.
 * payload is raw Annex-B H.264 (00 00 00 01 start codes).  The SPS/PPS header is
 * sent as its own message before the first frame; concatenating every payload in
 * arrival order yields a valid Annex-B elementary stream (feed straight to OMX).
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_RENDER_PROTOCOL_H
#define ALTSCREEN_RENDER_PROTOCOL_H

#define ALTAV_HOST        "127.0.0.1"
/* MUST match ALTAV_DEFAULT_PORT in hook/altscreen/altscreen_av.h.
 * Main/110 remains OMX-decoded in-process by stock libairplay. */
#define ALTAV_PORT        19831

/* A single cluster frame is a few tens of KB; cap generously and drop the link
 * on anything larger (framing desync / hostile length). */
#define ALTAV_MAX_MSG     (4u * 1024u * 1024u)

/* ---- Control channel (altscreen_render <-> Java/lsd) --------------------------------------
 * DATA (H.264) comes from the libairplay hook into ALTAV_PORT (above).  CONTROL is separate:
 * altscreen_render connects as a CLIENT to Java's control server on ALTCTL_PORT (mirrors how
 * maneuver_render connects to our TCP) and reports when the CarPlay 111 video is flowing, so
 * Java can flip AltScreenModule.setVideoAvailable() and select the video context.
 * Both directions use fixed 4-byte packets. */
#define ALTCTL_HOST       "127.0.0.1"
#define ALTCTL_PORT       19832
#define ALTCTL_PKT_SIZE   4

#define ALTEVT_VIDEO_ON   0x81   /* current stream has a successfully presented picture */
#define ALTEVT_VIDEO_OFF  0x82   /* the video client disconnected / stream ended */
#define ALTCMD_VIEW_AREA  0x01   /* Java -> renderer: b1 0=full/1=compact, b2 style, b3 raw preset */

#endif /* ALTSCREEN_RENDER_PROTOCOL_H */
