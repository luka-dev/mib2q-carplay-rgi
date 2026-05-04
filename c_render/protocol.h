/*
 * CarPlay Cluster Renderer - TCP Command Protocol
 *
 * Fixed 48-byte packets over TCP :19800.
 * External clients send commands; renderer acts autonomously.
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
#define CMD_BARGRAPH     0x06    /* Bargraph: payload[0]=level(0-16), payload[1]=on/off */

/* Renderer -> Java events (high bit set to distinguish from commands) */
#define EVT_HEARTBEAT    0x80    /* Renderer alive, sent every 1 s; empty payload */
#define EVT_READY        0x81    /* EGL/render initialized; safe to send first command */
#define EVT_FRAME_READY  0x82    /* At least one maneuver frame has been swapped */

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
 *   [2..3]   i16  exit_angle (big-endian, signed degrees)
 *   [4]      u8   driving_side (0=RHT, 1=LHT)
 *   [5]      u8   junction_count (0..18)
 *   [6..41]  i16  junction_angles[] (big-endian, up to 18)
 *
 * Optional (when MAN_FLAG_SET_PERSP set):
 *   [43]     u8   perspective (0=flat 2D, 1=perspective 3D)
 *
 * Optional (when MAN_FLAG_BARGRAPH set):
 *   [44]     u8   bargraph_level (0..16)
 *   [45]     u8   bargraph_mode  (0=off, 1=on, 2=blink)
 */
/* CMD_MANEUVER flags (in cr_cmd_t.flags) */
#define MAN_FLAG_SET_PERSP    0x01    /* Set perspective after transition: payload[43] = 0 (2D) / 1 (3D) */
#define MAN_FLAG_BARGRAPH     0x02    /* Bargraph data in payload[44..45] */

#define CR_MAN_ICON(p)          ((p)[0])
#define CR_MAN_DIRECTION(p)     ((int8_t)(p)[1])
#define CR_MAN_EXIT_ANGLE(p)    ((int16_t)(((p)[2] << 8) | (p)[3]))
#define CR_MAN_DRIVING_SIDE(p)  ((p)[4])
#define CR_MAN_JUNC_COUNT(p)    ((p)[5])
#define CR_MAN_JUNC_ANGLE(p,i)  ((int16_t)(((p)[6 + (i)*2] << 8) | (p)[7 + (i)*2]))

/* Display configuration.
 *
 * CR_DISPLAYABLE_ID = 20 — DISPLAYABLE_MAP_ROUTE_GUIDANCE, the native KOMO
 * RG widget slot in cluster context 74.  We *take it over*:
 *   - display_create_window(displayable_id=20) creates a screen window in
 *     our process with SCREEN_PROPERTY_ID_STRING="20".  Display manager's
 *     m_surfaceSources[20] gets re-bound to our window (the native widget's
 *     screen window still exists in its own process, just no longer the
 *     active source for displayable 20).
 *   - dmdt dc 74 20 102 101 33 re-declares context 74 with the original
 *     composition order, since display_create_window strips other
 *     displayables from context 74 as a side effect.
 *   - setActiveDisplayable(4, 20) (called by stock cluster firmware in
 *     preContextSwitchHook) makes the MOST encoder read our window for
 *     the LVDS video stream that lands on the VC's MAP tab.
 *   - On shutdown EGL surface release destroys our window, dmdt's
 *     m_surfaceSources[20] naturally falls back to the native widget,
 *     and restore_display() forces the cluster back to context 74.
 *
 * In production native navigation is idle while CarPlay is active, so the
 * native widget process never tries to re-bind displayable 20 back during
 * our session — there is no live competition. */
#define CR_DISPLAYABLE_ID   20  /* DISPLAYABLE_MAP_ROUTE_GUIDANCE (native widget slot) */
#define CR_CONTEXT_ID       74  /* Cluster MAP context (LVDS encoder reads from here) */
#define CR_DISPLAY_ID       1   /* 0=main (LVDS1), 1=cluster (LVDS2) */
#define CR_DEFAULT_WIDTH    328
#define CR_DEFAULT_HEIGHT   181 /* 180px content + 1px ECC annotation row */
#define CR_TARGET_FPS       30

/* Popup crop geometry within the 328x180 content area (for debug grid only) */
#define CR_POPUP_X      59
#define CR_POPUP_Y      27
#define CR_POPUP_W      210
#define CR_POPUP_H      153

#endif /* CR_PROTOCOL_H */
