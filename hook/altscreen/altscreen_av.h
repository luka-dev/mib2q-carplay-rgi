/*
 * altscreen_av — AltScreen H.264 forward: hook side.
 *
 * Receives decrypted AVCC access units from the hook-owned stream-111 socket and
 * forwards them over localhost TCP to the separate `altscreen_render` process.
 *
 * Wire protocol (hook -> decoder), all lengths big-endian:
 *   per message:  [u32 payload_len][payload]
 *   payload is ANNEX-B H.264 (00 00 00 01 start codes), fed straight to OMX.
 * The hook does the AVCC->Annex B conversion (it has the lengthSize), so the
 * decoder process is trivial: read message, feed bytes to OMX.  SPS/PPS are sent
 * as their own Annex-B message (converted from opcode-1 avcC) before frames.
 *
 * No __thread anywhere (QNX 6.5 ARMv7 gcc 4.4 emutls trap) — globals + mutex.
 */
#ifndef CARPLAY_ALTSCREEN_AV_H
#define CARPLAY_ALTSCREEN_AV_H

#include <stdint.h>
#include <stddef.h>

#define ALTAV_DEFAULT_HOST "127.0.0.1"
/* MUST match altscreen_render's listen port (altscreen_render/protocol.h ALTAV_PORT 19831).
 * Was 19830 — a mismatch that silently prevented the hook from ever connecting to the renderer. */
#define ALTAV_DEFAULT_PORT 19831

/* Configure the decoder-process endpoint (call once at init). */
void altav_init(const char *host, int port);

/* Enable/disable forwarding at runtime (default: enabled). */
void altav_set_enabled(int on);
int  altav_is_enabled(void);

/* Forward one already-Annex-B buffer verbatim (used for the SPS/PPS header). */
void altav_forward_frame(const uint8_t *annexb, uint32_t len);
/* cache + send Annex-B SPS/PPS; replayed on every reconnect so the decoder always gets config first */
void altav_set_config(const uint8_t *annexb_cfg, uint32_t len);

/* Convert one AVCC frame (lengthSize-byte BE prefixes from the AirPlay screen
 * opcode-0 body) to Annex-B and forward it. */
void altav_forward_avcc_frame(const uint8_t *avcc, uint32_t len, uint8_t length_size);

void altav_shutdown(void);

#endif /* CARPLAY_ALTSCREEN_AV_H */
