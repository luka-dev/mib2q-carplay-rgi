/*
 * Vehicle data to the iPhone over iAP2 (docs/connect-and-usb/VEHICLE_DATA_TO_PHONE.md).
 *
 * - VehicleStatus (MFi R29 97.18): the Identify gets a VehicleStatusComponent (21) and,
 *   when stock has none, a VehicleInformationComponent (20) with the engine type; the
 *   phone's StartVehicleStatusUpdates 0xA100 picks range / outside temperature / low-range
 *   warning, the hook answers with VehicleStatusUpdate 0xA101 on change, at most 1 Hz.
 * - Wheel speed (MFi R32 41.1.2): when the phone's StartLocationInformation 0xFFFA asks for
 *   VehicleSpeedData, the hook adds $PASCD sentences, about 1 Hz, as extra LocationInformation
 *   0xFFFB messages beside stock's GPGGA/GPRMC.  Stock advertises the capability
 *   (dio_manager.json "nmea" bit 0x08, set by the installer).
 *
 * The values come from Java (CMD_VEHICLE, hook/framework/bus_protocol.h): CAN speed,
 * reverse gear, range, outside temperature and the fuel warning from stock DSI.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef VEHICLE_HOOK_H
#define VEHICLE_HOOK_H

#include "../framework/hook_framework.h"

extern const hook_module_def_t vehicle_module_def;

#define VEH_ENGINE_GASOLINE     0
#define VEH_ENGINE_DIESEL       1
#define VEH_ENGINE_ELECTRIC     2
#define VEH_ENGINE_CNG          3

/* CMD_VEHICLE valid bits and gear values. */
#define VEH_VALID_SPEED         0x01
#define VEH_VALID_GEAR          0x02
#define VEH_VALID_RANGE         0x04
#define VEH_VALID_TEMP          0x08
#define VEH_VALID_WARNING       0x10
#define VEH_GEAR_UNKNOWN        0
#define VEH_GEAR_DRIVE          1
#define VEH_GEAR_REVERSE        2
#define VEH_GEAR_NEUTRAL        3
#define VEH_GEAR_PARK           4
#define VEH_CMD_LEN             14

typedef struct {
    uint8_t valid;
    uint16_t speed_cms;         /* cm/s */
    uint8_t gear;
    uint16_t range_km;
    int16_t temp_dc;            /* 0.1 degC */
    bool range_warning;
} veh_cmd_t;

bool veh_parse_cmd(const uint8_t* p, size_t len, veh_cmd_t* out);

/* StartVehicleStatusUpdates params -> mask of what the phone wants. */
#define VEH_WANT_RANGE          0x01    /* param 3 */
#define VEH_WANT_TEMP           0x02    /* param 4 */
#define VEH_WANT_WARNING        0x04    /* param 6 */
uint8_t veh_parse_start_status(const uint8_t* payload, size_t len);

/* VehicleStatusUpdate 0xA101 body with the wanted, valid fields; 0 when there is none. */
size_t veh_build_status_update(uint8_t* out, size_t out_max, uint8_t want, const veh_cmd_t* st);

/* StartLocationInformation 0xFFFA asks for VehicleSpeedData (param 4). */
bool veh_location_wants_speed(const uint8_t* payload, size_t len);

/* Identify additions: VehicleInformationComponent (when add_info) + VehicleStatusComponent. */
size_t veh_build_identify_components(uint8_t* out, size_t out_max, bool add_info, uint8_t engine);

/* IdentificationRejected 0x1D03 names one of our components or messages. */
bool veh_rejection_is_ours(const uint8_t* payload, size_t len);

typedef struct {
    uint64_t t_ms;              /* monotonic */
    uint32_t speed_mms;         /* mm/s */
    uint8_t gear;
} veh_sample_t;

#define VEH_PASCD_MAX_SAMPLES   10

/* One NUL-terminated $PASCD sentence with its checksum; returns strlen, 0 on overflow. */
size_t veh_format_pascd(char* out, size_t out_max, const veh_sample_t* s, size_t n);

#endif /* VEHICLE_HOOK_H */
