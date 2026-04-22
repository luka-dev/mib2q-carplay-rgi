/*
 * CarPlay Hook Bus - Wire Protocol Specification
 *
 * Full-duplex TCP bus between libcarplay_hook.so (inside dio_manager) and the
 * Java side (lsd process, carplay_hook.jar).  Single persistent connection.
 *
 * Transport: TCP 127.0.0.1:19810 (hook = server, Java = client).
 *
 * Frame layout (big-endian wire order):
 *
 *   offset  size   field
 *    0       4     magic = 'CPHB' (0x43504842)
 *    4       4     seq   (u32, monotonically increasing per side)
 *    8       2     type  (u16, see EVT_* / CMD_*)
 *   10       1     flags (u8, BUS_FLAG_*)
 *   11       1     rsvd  (u8, must be 0)
 *   12       4     len   (u32, payload size in bytes, 0..BUS_MAX_PAYLOAD)
 *   16     <len>   payload (text or binary, type-dependent)
 *
 * Header is 16 bytes.  Maximum payload is 128 KB.
 *
 * Payload convention:
 *   - Default: text in QNX PPS style "key:type:value\n" repeated.
 *     Compatible with the existing PPS.java Data parser so migration of
 *     RouteGuidance consumers is mechanical.  No chunking needed — TCP
 *     delivers the whole frame atomically.
 *   - With BUS_FLAG_BINARY: type-specific packed struct.  Used by cursor
 *     and any future low-latency binary channel.
 *
 * Sticky state:
 *   - Types marked BUS_FLAG_STICKY on send are cached on the server side
 *     (one last-value slot per type).  When a new client connects and
 *     issues CMD_SYNC_REQ, the server replays every cached frame wrapped
 *     between EVT_SYNC_BEGIN and EVT_SYNC_END before resuming live traffic.
 *   - Replayed frames carry BUS_FLAG_REPLAY so the client can tell them
 *     apart from live events if needed.  Original type/flags/payload are
 *     preserved; seq is re-assigned from the server's current counter.
 *   - Non-sticky frames are fire-and-forget: new clients only see new ones.
 *   - **No publish yet = empty replay**: if the hook never emitted a given
 *     sticky type during its lifetime, new clients see the SYNC_BEGIN/END
 *     frames with no entry for that type.  Consumers must treat this as
 *     "no state yet" and not as a clear/reset.  Publishers that need a
 *     known baseline at connect-time should emit an initial frame at
 *     their module init (see rgd_lazy_init → rgd_clear_state("init") for
 *     the pattern).
 */

#ifndef CARPLAY_BUS_PROTOCOL_H
#define CARPLAY_BUS_PROTOCOL_H

#include <stdint.h>

/* ============================================================
 * Transport
 * ============================================================ */
#define BUS_TCP_HOST            "127.0.0.1"
#define BUS_TCP_PORT            19810
#define BUS_MAGIC               0x43504842u  /* 'CPHB' */
#define BUS_HEADER_SIZE         16
#define BUS_MAX_PAYLOAD         (128 * 1024)

/* ============================================================
 * Flags
 * ============================================================ */
#define BUS_FLAG_STICKY         0x01  /* server caches, replays on sync     */
#define BUS_FLAG_BINARY         0x02  /* payload is binary, not text k:t:v  */
#define BUS_FLAG_REPLAY         0x04  /* set by server during snapshot replay */

/* ============================================================
 * Event types (Hook -> Java)   range 0x0000 .. 0x00FF
 * ============================================================ */
#define EVT_HELLO               0x0001  /* server greet on connect, payload: text version info */
#define EVT_SYNC_BEGIN          0x0002  /* snapshot replay starts            */
#define EVT_SYNC_END            0x0003  /* snapshot replay finished          */
#define EVT_PONG                0x0004  /* reply to CMD_PING                 */

#define EVT_COVERART            0x0010  /* text: crc:n:<u32> path:s:<path>   */
#define EVT_RGD_UPDATE          0x0020  /* text: route_state + all fields    */
#define EVT_DEVICE_STATE        0x0030  /* reserved                          */

/* ============================================================
 * Command types (Java -> Hook)
 *   0x0100 .. 0x01FF : meta / control
 *   0x0200 .. 0x02FF : cursor / input
 *   0x0300 ..        : reserved for future categories
 * ============================================================ */
#define CMD_SYNC_REQ            0x0100  /* request sticky-state snapshot     */
#define CMD_PING                0x0101  /* echo test                         */

#define CMD_CURSOR_POS          0x0200  /* binary: i32 x, i32 y (BE)         */
#define CMD_CURSOR_HIDE         0x0201  /* no payload                        */

/* ============================================================
 * Classification helpers (header range checks, optional)
 * ============================================================ */
#define BUS_TYPE_IS_EVENT(t)    (((t) & 0xFF00u) == 0x0000)
#define BUS_TYPE_IS_COMMAND(t)  (((t) & 0xF000u) == 0x0000 && ((t) & 0xFF00u) >= 0x0100)

#endif /* CARPLAY_BUS_PROTOCOL_H */
