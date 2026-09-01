/*
 * Route Guidance Hook Module
 *
 * Enables CarPlay Route Guidance (TBT navigation) on MU1316:
 * - Patches Identify to advertise RouteGuidanceDisplayComponent
 * - Injects StartRouteGuidanceUpdates (0x5200) after auth
 * - Parses RouteGuidanceUpdate (0x5201), ManeuverUpdate (0x5202),
 *   and LaneGuidanceInformation (0x5204)
 * - Writes only fields present in each packet to the bus (stateless)
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef RGD_HOOK_H
#define RGD_HOOK_H

#include "../framework/hook_framework.h"
#include "rgd_tlv.h"

/* Lane direction bitmask (BAP NavSD format) */
#define LANE_DIR_LEFT           0x01
#define LANE_DIR_SLIGHT_LEFT    0x02
#define LANE_DIR_STRAIGHT       0x04
#define LANE_DIR_SLIGHT_RIGHT   0x08
#define LANE_DIR_RIGHT          0x10
#define LANE_DIR_SHARP_LEFT     0x21
#define LANE_DIR_SHARP_RIGHT    0x30

/* Registered by the framework from hook_module_table (hook/main.c). */
extern const hook_module_def_t rgd_module_def;

/* Shutdown route guidance module (module def's on_shutdown) */
void rgd_shutdown(void);

/* Force send 0x5200 request */
hook_result_t rgd_request_updates(void);

/* Force send 0x5203 stop */
hook_result_t rgd_stop_updates(void);

/* Clear cache and bus state */
void rgd_clear_state(const char* reason);

#endif /* RGD_HOOK_H */
