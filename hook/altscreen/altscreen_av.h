/*
 * altscreen_av — AltScreen H.264 forward: hook side.
 *
 * Receives decrypted AVCC access units from the hook-owned stream-111 socket and
 * forwards them over localhost TCP to the separate `altscreen_render` process.
 *
 * Payload is Annex-B H.264, with SPS/PPS replayed before a connection's IDR.
 * Silent renderers retain [u32 BE length][payload]; negotiated v2 carries a
 * 40-byte metadata header (common/alt_video_wire.h). HELLO capability 3 also
 * offers presentation status; an explicit hook ACK opts in to its heartbeats.
 * The reader transfers owned buffers; length-size-4 prefixes change in place.
 *
 * No __thread anywhere (QNX 6.5 ARMv7 gcc 4.4 emutls trap) — globals + mutex.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef CARPLAY_ALTSCREEN_AV_H
#define CARPLAY_ALTSCREEN_AV_H

#include <stdint.h>
#include <stddef.h>
#include "../../common/alt_video_wire.h"
#include "avcc_frame.h"
#include "recovery_policy.h"

#define ALTAV_DEFAULT_HOST "127.0.0.1"
/* MUST match altscreen_render's listen port (altscreen_render/protocol.h ALTAV_PORT 19831).
 * Was 19830 — a mismatch that silently prevented the hook from ever connecting to the renderer. */
#define ALTAV_DEFAULT_PORT 19831

/* Configure the decoder-process endpoint (call once at init). */
void altav_init(const char *host, int port);

/* Enable/disable forwarding at runtime (default: enabled). */
void altav_set_enabled(int on);
int  altav_is_enabled(void);

/* Generation ownership.  The single global renderer transport (transport descriptor/config) belongs
 * to exactly ONE receiver generation at a time, so an old stream-111 receiver on a fast reconnect
 * cannot corrupt or close the new receiver's connection.  altav_claim() makes `gen` the owner only
 * when it is newer than the current owner (dropping the previous owner's connection + cached
 * config) and returns 1 when accepted, 0 when a newer owner already won.  Every
 * forward/config/shutdown checks ownership inside the transport mutex (no TOCTOU). A non-owner call
 * is a no-op.  altav_is_owner() lets a receiver notice it has been superseded and exit. */
int  altav_claim(uint32_t gen);
int  altav_is_owner(uint32_t gen);

/* Shipping receiver uses this bounded asynchronous queue, never socket send.
 * Negative result means continuity lost: drain/decrypt but wait for a fresh IDR.
 * Error callback runs without queue/transport locks and is generation-scoped. */
int altav_async_start(void (*failed)(uint32_t, void *), void *context);
int altav_enqueue_config(uint32_t gen, const uint8_t *config, uint32_t len);
int altav_take_failure(uint32_t gen);
/* Success transfers ownership and returns a spare buffer to the reader.
 * Failure leaves the caller's pointer/capacity untouched. info is obtained by
 * alt_avcc_prepare on this exact buffer before any UI or enqueue action. */
int altav_enqueue_owned(uint32_t gen, uint8_t **data, uint32_t *capacity,
                        const alt_avcc_frame_t *info, const alt_frame_meta_t *meta);
int altav_keyframe_permit(uint32_t gen, unsigned kind);
void altav_note_input_loss(uint32_t gen);

/* Close the transport ONLY if `gen` is the current owner (a superseded receiver's exit
 * must not close the new owner's connection). */
void altav_shutdown_if_owner(uint32_t gen);
/* Unconditional close — process-exit path only (altscreen_shutdown). */
void altav_shutdown(void);

#endif /* CARPLAY_ALTSCREEN_AV_H */
