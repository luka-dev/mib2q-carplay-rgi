/*
 * altscreen_render — wire protocol (hook → renderer).
 *
 * The CarPlay hook (c_hook/altscreen/altscreen_av.c) taps the decrypted cluster
 * (111) stream, converts each frame AVCC→Annex-B, and forwards it here over a
 * localhost TCP connection.  Renderer is the SERVER; the hook is the client and
 * (re)connects once per CarPlay session.
 *
 * Message framing (matches altscreen_av.c av_send_locked):
 *     [u32 be payload_len][payload]
 * payload is raw Annex-B H.264 (00 00 00 01 start codes).  The SPS/PPS header is
 * sent as its own message before the first frame; concatenating every payload in
 * arrival order yields a valid Annex-B elementary stream (feed straight to OMX).
 */
#ifndef ALTSCREEN_RENDER_PROTOCOL_H
#define ALTSCREEN_RENDER_PROTOCOL_H

#define ALTAV_HOST        "127.0.0.1"
#define ALTAV_PORT        19830

/* A single cluster frame is a few tens of KB; cap generously and drop the link
 * on anything larger (framing desync / hostile length). */
#define ALTAV_MAX_MSG     (4u * 1024u * 1024u)

#endif /* ALTSCREEN_RENDER_PROTOCOL_H */
