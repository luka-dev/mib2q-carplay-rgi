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

/* Command IDs */
#define CMD_MANEUVER     0x01    /* New maneuver -- engine transitions automatically */
#define CMD_SCREENSHOT   0x02    /* Save framebuffer as PPM */
#define CMD_SHUTDOWN     0x03    /* Graceful exit */
#define CMD_PERSPECTIVE  0x04    /* Perspective: payload[0] = 0 (off) / 1 (on) */
#define CMD_DEBUG        0x05    /* Toggle debug overlay */
#define CMD_BARGRAPH     0x06    /* Bargraph: payload[0]=level(0-16), payload[1]=on/off */

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

/* Display configuration */
#define CR_DISPLAYABLE_ID   20  /* DISPLAYABLE_MAP_ROUTE_GUIDANCE (328x181, native widget slot) */
#define CR_CONTEXT_ID       74  /* Native context: RG(20) + images(101,102) + map(33) */
#define CR_DISPLAY_ID       1   /* 0=main screen, 1=cluster (LVDS2) */
#define CR_DEFAULT_WIDTH    328
#define CR_DEFAULT_HEIGHT   240
#define CR_TARGET_FPS       10

#endif /* CR_PROTOCOL_H */
