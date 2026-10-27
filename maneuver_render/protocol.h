/*
 * CarPlay Cluster Renderer - TCP Command Protocol
 *
 * Fixed 48-byte packets over TCP :19800.
 * External clients send commands; renderer acts autonomously.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef CR_PROTOCOL_H
#define CR_PROTOCOL_H

#include <stdint.h>

#define CR_TCP_PORT         19800
#define CR_PKT_SIZE         48

/* Command IDs (Java -> renderer, except where noted) */
#define CMD_MANEUVER     0x01    /* New maneuver -- engine transitions automatically */
#define CMD_SCREENSHOT   0x02    /* Save framebuffer as PPM */
#define CMD_SHUTDOWN     0x03    /* Graceful exit */
#define CMD_PERSPECTIVE  0x04    /* Perspective: payload[0] = 0 (off) / 1 (on) */
#define CMD_DEBUG        0x05    /* Toggle debug overlay */
#define CMD_PROGRESS     0x06    /* Arrow progress: payload[0]=remaining level(0-16), [1]=mode, [2]=phase */
#define CMD_CLEAR        0x07    /* Blank the popup (CarPlay/route off) — drop maneuver+progress,
                                  * render fully transparent.  Keeps the link; renderer stays alive. */
#define CMD_VISIBLE_AREA 0x08   /* payload: x,y,w,h as four BE u16; source pixels, top-left origin */

/* Renderer -> Java events (high bit set to distinguish from commands) */
#define EVT_HEARTBEAT    0x80    /* Renderer alive, sent every 1 s; empty payload */
#define EVT_READY        0x81    /* EGL/render initialized; safe to send first command */
#define EVT_FRAME_READY  0x82    /* At least one maneuver frame has been swapped */
#define EVT_FRAME_CLEARED 0x83   /* CMD_CLEAR processed; later FRAME_READY belongs to new content */

/* Independent lane guidance; 0x09..0x0b are retired scene-road commands.
 * BEGIN: token u32 [0..3], count [4], complete [5], showing [6], event i32 [8..11].
 * LANE: token [0..3], record [4], position u16 [5..6], status [7], angle count [8],
 *       primary i16 [9..10], up to 16 signed-degree angles [11..42].
 * COMMIT: token [0..3]. All integers are big-endian.
 * No MANEUVER record in this transaction. Unknown angles (including +/-1000),
 * missing status/position (255/65535) and incomplete lists remain explicit.
 */
#define CMD_LANES_BEGIN  0x0c
#define CMD_LANES_LANE   0x0d
#define CMD_LANES_COMMIT 0x0e

/* 48-byte command packet */
typedef struct {
    uint8_t  cmd;               /* CMD_* */
    uint8_t  flags;             /* CMD_MANEUVER: bit flags (MAN_FLAG_*) */
    uint8_t  payload[46];       /* command-specific data */
} cr_cmd_t;

/*
 * CMD_MANEUVER payload layout:
 *   [0]      u8   icon (ICON_* constant)
 *   [1]      i8   direction (-1, 0, +1)
 *   [2..3]   i16  exit_angle (big-endian; degrees, or half-degrees with BAP_GEOMETRY)
 *   [4]      u8   driving_side (0=RHT, 1=LHT)
 *   [5]      u8   junction_count (0..18)
 *   [6..41]  i16  junction_angles[] (big-endian, same angle units, up to 18)
 *
 * Optional progress extension (flags & 0x20):
 *   MANEUVER [42], PROGRESS [2]: 0=off, 1=fill, 2=blink low, 3=blink high.
 *   Legacy level/mode retained. Explicit blink phase comes from Java/HUD.
 *   Unknown state or inconsistent mode shows the quiet (off) arrow.
 *   Without this extension, mode 1 fills the arrow; all other modes are off.
 *
 * Optional (when MAN_FLAG_SET_PERSP set):
 *   [43]     u8   perspective (0=flat 2D, 1=perspective 3D)
 *
 * Optional (when MAN_FLAG_PROGRESS set):
 *   [44]     u8   remaining_level (0..16)
 *   [45]     u8   progress_mode  (0=off, 1=on; legacy mode 2 has no phase, treated as off)
 */
/* CMD_MANEUVER flags (in cr_cmd_t.flags) */
#define MAN_FLAG_SET_PERSP    0x01    /* Set perspective after transition: payload[43] = 0 (2D) / 1 (3D) */
#define MAN_FLAG_PROGRESS     0x02    /* Progress level/mode in payload[44..45] */
#define MAN_FLAG_REFRESH      0x08    /* replace latest maneuver geometry, preserve transition */
#define MAN_FLAG_BAP_GEOMETRY 0x04    /* signed half-degrees; no snap unless SNAP_TO_ROAD */
#define MAN_FLAG_SNAP_TO_ROAD 0x10    /* raw roundabout roads include active exit: allow snap */

#define CR_MAN_ICON(p)          ((p)[0])
#define CR_MAN_DIRECTION(p)     ((int8_t)(p)[1])
#define CR_MAN_EXIT_ANGLE(p)    ((int16_t)(((p)[2] << 8) | (p)[3]))
#define CR_MAN_DRIVING_SIDE(p)  ((p)[4])
#define CR_MAN_JUNC_COUNT(p)    ((p)[5])
#define CR_MAN_JUNC_ANGLE(p,i)  ((int16_t)(((p)[6 + (i)*2] << 8) | (p)[7 + (i)*2]))

/* Display configuration.
 *
 * CR_DISPLAYABLE_ID = 98 — OUR OWN displayable, NOT the stock route-guidance
 * slot (20).  Picked from the free range 61-99 (above the semantic enum whose
 * max is 60=HUD_MAP_VIEW, below the KDK ids 100-102, so still inside the DM's
 * supported id range).  We open a managed screen window with ID_STRING="98" (via
 * screen_manage_window into the DisplayManager group).  98 has no stock owner →
 * no collision war with native nav (the old id-20 takeover / flapping is gone).
 *
 * Context routing is NO LONGER done here.  The Java patch declares cluster
 * contexts dc[80]={98,101,102,99} (nav: maneuver over the KDK backings + CarPlay
 * map) / dc[81]={99} (map only) in DisplayManagerMIB2High and switches between them
 * (AltScreenModule).  This renderer only creates the managed window + draws; it
 * runs NO dmdt.  (altscreen_render draws the CarPlay map on id 99.)
 *
 * On shutdown screen_destroy_window vacates m_surfaceSources[98]; Java switches
 * the cluster back to the stock context (dc[74]). */
#define CR_DISPLAYABLE_ID   98  /* our own cluster displayable (managed window, ID_STRING="98") */
#define CR_CONTEXT_ID       80  /* Java-declared cluster context {98,102,101} (informational) */
#define CR_DISPLAY_ID       1   /* 0=main (LVDS1), 1=cluster (LVDS2) */
#define CR_DEFAULT_WIDTH    328
#define CR_DEFAULT_HEIGHT   181 /* 180px content + 1px ECC annotation row */
#define CR_TARGET_FPS       30

/* Big-screen popup crop within the 328x180 content area (excludes ECC row).
 * Also the safe default until Java sends the active stage's visible area. */
#define CR_POPUP_X      59
#define CR_POPUP_Y      27
#define CR_POPUP_W      210
#define CR_POPUP_H      153

#endif /* CR_PROTOCOL_H */
