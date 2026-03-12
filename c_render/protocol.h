/*
 * CarPlay Cluster Renderer - Control Protocol
 *
 * UDP protocol for Java <-> c_render communication.
 * Java sends commands; renderer sends status.
 */

#ifndef CR_PROTOCOL_H
#define CR_PROTOCOL_H

#include <stdint.h>

#define CR_UDP_PORT         19800
#define CR_UDP_ADDR         "127.0.0.1"
#define CR_MAX_PKT          512

/* Command IDs (Java -> renderer) */
#define CR_CMD_SHUTDOWN     0x01    /* Graceful shutdown */
#define CR_CMD_UPDATE       0x02    /* Maneuver data update (supplementary) */

/* Status IDs (renderer -> Java, via PPS) */
#define CR_STATUS_STARTING  0
#define CR_STATUS_READY     1
#define CR_STATUS_RUNNING   2
#define CR_STATUS_STOPPED   3

/* Renderer PPS paths */
#define CR_PPS_STATUS_PATH  "/ramdisk/pps/iap2/renderer"
#define CR_PPS_RGD_PATH     "/ramdisk/pps/iap2/routeguidance"

/* Display configuration */
#define CR_DISPLAYABLE_ID   200
#define CR_CONTEXT_ID       90
#define CR_DISPLAY_ID       0
#define CR_DEFAULT_WIDTH    284
#define CR_DEFAULT_HEIGHT   276
#define CR_TARGET_FPS       10

/*
 * CR_CMD_UPDATE packet layout (after cmd byte):
 *   u8  maneuver_type
 *   u8  distance_units
 *   u32 distance_to_maneuver  (big-endian)
 *   u8  turn_to_len
 *   char turn_to[turn_to_len] (UTF-8, no null)
 *   u8  dist_string_len
 *   char dist_string[dist_string_len]
 */

#endif /* CR_PROTOCOL_H */
