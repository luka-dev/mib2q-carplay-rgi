/*
 * Route Guidance TLV Definitions and Parsing
 *
 * Defines TLV IDs for iAP2 Route Guidance messages:
 * - 0x5200 StartRouteGuidanceUpdates
 * - 0x5201 RouteGuidanceUpdate
 * - 0x5202 RouteGuidanceManeuverUpdate
 * - 0x5203 StopRouteGuidanceUpdates
 * - 0x5204 RouteGuidanceLaneGuidanceInformation
 */

#ifndef RGD_TLV_H
#define RGD_TLV_H

#include "../framework/common.h"

/* ============================================================
 * Identify TLV for RouteGuidanceDisplayComponent (0x001E)
 * ============================================================ */
#define IDENT_TLV_ROUTE_GUIDANCE_COMPONENT  0x001E

/* RouteGuidanceDisplayComponent inner TLV IDs
 *
 * Matches MHI3 libesoiap2.so component layout (9 TLVs, IDs 0-8).
 * Verified via CIOSTraceHelper::toString(RouteGuidanceDisplayComponent):
 *   compId, name, maxCurrentRoadNameLength, maxDestinationNameLength,
 *   maxAfterManeuverRoadNameLength, maxManeuverDescriptionLength,
 *   maxGuidanceManeuverStorageCapacity, maxLaneGuidanceDescriptionLength,
 *   maxLaneGuidanceStorageCapacity
 */
#define RGD_COMP_COMPONENT_ID               0x0000
#define RGD_COMP_NAME                       0x0001
#define RGD_COMP_MAX_CURRENT_ROAD_NAME      0x0002
#define RGD_COMP_MAX_DEST_NAME              0x0003
#define RGD_COMP_MAX_AFTER_MANEUVER_NAME    0x0004
#define RGD_COMP_MAX_MANEUVER_DESC          0x0005
#define RGD_COMP_MAX_MANEUVER_CAPACITY      0x0006
#define RGD_COMP_MAX_LANE_GUIDANCE_DESC     0x0007
#define RGD_COMP_MAX_LANE_GUIDANCE_CAPACITY 0x0008

/* ============================================================
 * StartRouteGuidanceUpdates (0x5200) TLV IDs
 * ============================================================ */
#define RGD_START_TLV_COMPONENT_ID       0x0000
#define RGD_START_TLV_SOURCE_NAME        0x0001
#define RGD_START_TLV_SOURCE_SUPPORTS_RG 0x0002
#define RGD_START_TLV_SUPPORTS_EXIT_INFO 0x0003

/* ============================================================
 * RouteGuidanceUpdate (0x5201) TLV IDs
 * ============================================================ */
#define RGD_TLV_COMPONENT_ID            0x0000
#define RGD_TLV_ROUTE_GUIDANCE_STATE    0x0001
#define RGD_TLV_MANEUVER_STATE          0x0002
#define RGD_TLV_CURRENT_ROAD_NAME       0x0003
#define RGD_TLV_DESTINATION_NAME        0x0004
#define RGD_TLV_ETA                     0x0005
#define RGD_TLV_TIME_REMAINING          0x0006
#define RGD_TLV_DISTANCE_REMAINING      0x0007
#define RGD_TLV_DISTANCE_STRING         0x0008
#define RGD_TLV_DISTANCE_UNITS          0x0009
#define RGD_TLV_DIST_TO_MANEUVER        0x000A
#define RGD_TLV_DIST_TO_MANEUVER_STRING 0x000B
#define RGD_TLV_DIST_TO_MANEUVER_UNITS  0x000C
#define RGD_TLV_MANEUVER_LIST           0x000D
#define RGD_TLV_MANEUVER_COUNT          0x000E
#define RGD_TLV_VISIBLE_IN_APP          0x000F
#define RGD_TLV_LANE_GUIDANCE_INDEX     0x0010
#define RGD_TLV_LANE_GUIDANCE_TOTAL     0x0011
#define RGD_TLV_LANE_GUIDANCE_SHOWING   0x0012
#define RGD_TLV_SOURCE_NAME             0x0013
#define RGD_TLV_SOURCE_SUPPORTS_RG      0x0014

/*
 * RouteGuidanceState values (as observed in MHI3 dio_manager type-info strings).
 *
 * mediabase::CDIONavigationMetadataTypeInfo::toString(ERouteGuidanceState):
 *   0=NO_ROUTE_SET, 1=ROUTE_SET, 2=ARRIVED, 3=LOADING, 4=LOCATING,
 *   5=REROUTING, 6=PROCEED_TO_ROUTE
 */
#define RGD_STATE_NO_ROUTE_SET      0
#define RGD_STATE_ROUTE_SET         1
#define RGD_STATE_ARRIVED           2
#define RGD_STATE_LOADING           3
#define RGD_STATE_LOCATING          4
#define RGD_STATE_REROUTING         5
#define RGD_STATE_PROCEED_TO_ROUTE  6

/* Backward-compatible aliases used throughout the hook code. */
#define RGD_STATE_NOT_SET           RGD_STATE_NO_ROUTE_SET
#define RGD_STATE_NOT_ACTIVE        RGD_STATE_NO_ROUTE_SET
#define RGD_STATE_ACTIVE            RGD_STATE_ROUTE_SET

/* ============================================================
 * RouteGuidanceManeuverUpdate (0x5202) TLV IDs
 * ============================================================ */
#define MAN_TLV_COMPONENT_ID            0x0000
#define MAN_TLV_INDEX                   0x0001
#define MAN_TLV_DESCRIPTION             0x0002
#define MAN_TLV_TYPE                    0x0003
#define MAN_TLV_AFTER_ROAD_NAME         0x0004
#define MAN_TLV_DISTANCE_BETWEEN        0x0005
#define MAN_TLV_DISTANCE_STRING         0x0006
#define MAN_TLV_DISTANCE_UNITS          0x0007
#define MAN_TLV_DRIVING_SIDE            0x0008
#define MAN_TLV_JUNCTION_TYPE           0x0009
#define MAN_TLV_JUNCTION_ANGLES         0x000A /* JunctionElementAngle (iOS) */
#define MAN_TLV_EXIT_ANGLE              0x000B /* JunctionElementExitAngle (iOS) */
#define MAN_TLV_LINKED_LANE_GUIDANCE    0x000C
#define MAN_TLV_EXIT_INFO               0x000D

/* ============================================================
 * RouteGuidanceLaneGuidanceInformation (0x5204) TLV IDs
 * ============================================================ */
#define LANE_MSG_TLV_COMPONENT_ID            0x0000
#define LANE_MSG_TLV_LANE_GUIDANCE_INDEX     0x0001
#define LANE_MSG_TLV_LANE_INFORMATIONS       0x0002
#define LANE_MSG_TLV_LANE_GUIDANCE_DESC      0x0003

/*
 * Maneuver types.
 *
 * Verified against MHI3 dio_manager type-info strings
 * (CDIONavigationMetadataTypeInfo::toString(EManeuverType)):
 *   0=NO_TURN, 1=LEFT_TURN, 2=RIGHT_TURN, ... , 53=CHANGE_HIGHWAY_RIGHT
 */
#define MAN_TYPE_NOT_SET                    255
#define MAN_TYPE_NO_TURN                    0
#define MAN_TYPE_LEFT_TURN                  1
#define MAN_TYPE_RIGHT_TURN                 2
#define MAN_TYPE_STRAIGHT_AHEAD             3
#define MAN_TYPE_U_TURN                     4
#define MAN_TYPE_FOLLOW_ROAD                5
#define MAN_TYPE_ENTER_ROUNDABOUT           6
#define MAN_TYPE_EXIT_ROUNDABOUT            7
#define MAN_TYPE_OFF_RAMP                   8
#define MAN_TYPE_ON_RAMP                    9
#define MAN_TYPE_ARRIVE_END_OF_NAVIGATION   10
#define MAN_TYPE_START_ROUTE                11
#define MAN_TYPE_ARRIVE_AT_DESTINATION      12
#define MAN_TYPE_KEEP_LEFT                  13
#define MAN_TYPE_KEEP_RIGHT                 14
#define MAN_TYPE_ENTER_FERRY                15
#define MAN_TYPE_EXIT_FERRY                 16
#define MAN_TYPE_CHANGE_FERRY               17
#define MAN_TYPE_START_ROUTE_WITH_U_TURN    18
#define MAN_TYPE_U_TURN_AT_ROUNDABOUT       19
#define MAN_TYPE_LEFT_TURN_AT_END           20
#define MAN_TYPE_RIGHT_TURN_AT_END          21
#define MAN_TYPE_HIGHWAY_OFF_RAMP_LEFT      22
#define MAN_TYPE_HIGHWAY_OFF_RAMP_RIGHT     23
#define MAN_TYPE_ARRIVE_DESTINATION_LEFT    24
#define MAN_TYPE_ARRIVE_DESTINATION_RIGHT   25
#define MAN_TYPE_U_TURN_WHEN_POSSIBLE       26
#define MAN_TYPE_ARRIVE_END_OF_DIRECTIONS   27
#define MAN_TYPE_ROUNDABOUT_EXIT_1          28
#define MAN_TYPE_ROUNDABOUT_EXIT_2          29
#define MAN_TYPE_ROUNDABOUT_EXIT_3          30
#define MAN_TYPE_ROUNDABOUT_EXIT_4          31
#define MAN_TYPE_ROUNDABOUT_EXIT_5          32
#define MAN_TYPE_ROUNDABOUT_EXIT_6          33
#define MAN_TYPE_ROUNDABOUT_EXIT_7          34
#define MAN_TYPE_ROUNDABOUT_EXIT_8          35
#define MAN_TYPE_ROUNDABOUT_EXIT_9          36
#define MAN_TYPE_ROUNDABOUT_EXIT_10         37
#define MAN_TYPE_ROUNDABOUT_EXIT_11         38
#define MAN_TYPE_ROUNDABOUT_EXIT_12         39
#define MAN_TYPE_ROUNDABOUT_EXIT_13         40
#define MAN_TYPE_ROUNDABOUT_EXIT_14         41
#define MAN_TYPE_ROUNDABOUT_EXIT_15         42
#define MAN_TYPE_ROUNDABOUT_EXIT_16         43
#define MAN_TYPE_ROUNDABOUT_EXIT_17         44
#define MAN_TYPE_ROUNDABOUT_EXIT_18         45
#define MAN_TYPE_ROUNDABOUT_EXIT_19         46
#define MAN_TYPE_SHARP_LEFT_TURN            47
#define MAN_TYPE_SHARP_RIGHT_TURN           48
#define MAN_TYPE_SLIGHT_LEFT_TURN           49
#define MAN_TYPE_SLIGHT_RIGHT_TURN          50
#define MAN_TYPE_CHANGE_HIGHWAY             51
#define MAN_TYPE_CHANGE_HIGHWAY_LEFT        52
#define MAN_TYPE_CHANGE_HIGHWAY_RIGHT       53

/*
 * Distance units (as observed in MHI3 dio_manager type-info strings).
 *
 * mediabase::CDIONavigationMetadataTypeInfo::toString(EDistanceRemainingDisplayUnits):
 *   0=KM, 1=MILES, 2=M, 3=YARDS, 4=FT
 */
#define DIST_UNIT_KM        0
#define DIST_UNIT_MILES     1
#define DIST_UNIT_METERS    2
#define DIST_UNIT_YARDS     3
#define DIST_UNIT_FEET      4

/*
 * Driving side (as observed in MHI3 tbt_renderer logging and captured iAP2 packets):
 *   0 = right-hand traffic
 *   1 = left-hand traffic
 *
 * Note: this field is optional; if the TLV is missing, we don't publish the bus key.
 */
#define DRIVING_SIDE_RIGHT   0
#define DRIVING_SIDE_LEFT    1

/* ============================================================
 * Data Structures
 * ============================================================ */

#define MAX_MANEUVER_LIST   16
#define MAX_COMPONENT_LIST  8
#define MAX_LANE_GUIDANCE   8
#define MAX_LANE_ANGLES     16
/* Must align with Java RouteGuidance.MAX_MANEUVERS. */
#define MANEUVER_CACHE_SIZE 16
#define RGD_HEX_PREVIEW_MAX 96
/*
 * Exit info (MAN_TLV_EXIT_INFO) in MHI3 dio_manager is treated as a string and fed into util::UnicodeString8
 * without an explicit length (CRouteGuidanceUpdateProcessorImpl::getRouteGuidanceManeuverInformation, a1+240).
 * We keep the raw+hex preview for debugging, but also publish a decoded string key to the bus.
 */
#define RGD_EXIT_INFO_MAX   256

/*
 * MAN_TLV_LINKED_LANE_GUIDANCE (0x000C) is treated as native scalar linked-lane index
 * (u16, with occasional component+index compact forms).
 * Detailed per-lane content is parsed from 0x5204 LaneInformations.
 */

/*
 * Lane-information sub-TLV IDs inside 0x5204 / TLV 0x0002.
 *
 * Native parser (libesoiap2 processLaneGuidanceInformation + parseLaneInformation):
 * 0x0000=index, 0x0001=laneStatus, 0x0002=laneAngles (repeated), 0x0003=laneAngleHighlight.
 */
#define LANE_INFO_TLV_INDEX           0x0000  /* u16 */
#define LANE_INFO_TLV_STATUS          0x0001  /* u8 */
#define LANE_INFO_TLV_ANGLES          0x0002  /* repeated u16 */
#define LANE_INFO_TLV_ANGLE_HIGHLIGHT 0x0003  /* u16, sentinel 1000 is valid */

/* Lane guidance per-lane info */
typedef struct {
    uint16_t position;       /* lane position from TLV 0x0000 (fallback: parse index) */
    int16_t  direction;      /* raw u16/s16 lane direction/angle; preserves sentinel 1000 */
    uint8_t  status;         /* 0=NOT_GOOD, 1=GOOD, 2=PREFERRED */
    int16_t  angles[MAX_LANE_ANGLES]; /* full laneAngles vector from 0x5204/0x0002 */
    uint8_t  angle_count;
    char     description[64];
} rgd_lane_t;

/* Lane guidance data (from 0x5204) */
typedef struct {
    uint64_t present;
    uint16_t component_ids[MAX_COMPONENT_LIST];
    uint16_t component_count;
    uint16_t lane_guidance_index;
    rgd_lane_t lanes[MAX_LANE_GUIDANCE];
    uint8_t lane_count;
    char lane_guidance_description[64];
} rgd_lane_guidance_t;

/* Route update data (from 0x5201) */
typedef struct {
    uint64_t present;
    uint16_t component_ids[MAX_COMPONENT_LIST];
    uint16_t component_count;
    uint8_t route_state;
    uint8_t maneuver_state;
    uint8_t distance_units;
    uint8_t dist_to_maneuver_units;
    uint8_t visible_in_app;
    uint8_t lane_guidance_showing;
    uint8_t source_supports_route_guidance;
    uint16_t maneuver_count;
    uint16_t lane_guidance_index;
    int16_t lane_guidance_slot;  /* Bus-only remapped slot for lane_guidance_index */
    uint16_t lane_guidance_total;
    uint32_t distance_remaining;
    uint32_t dist_to_maneuver;
    uint64_t eta;
    uint64_t time_remaining;
    char current_road[256];
    char destination[256];
    char distance_string[64];
    char dist_to_maneuver_string[64];
    char source_name[64];
    uint16_t maneuver_list[MAX_MANEUVER_LIST];
    uint16_t maneuver_list_count;
} rgd_update_t;

/* rgd_update_t present bits */
#define RGD_UPD_COMPONENT_IDS        (1ULL << 0)
#define RGD_UPD_ROUTE_STATE          (1ULL << 1)
#define RGD_UPD_MANEUVER_STATE       (1ULL << 2)
#define RGD_UPD_CURRENT_ROAD         (1ULL << 3)
#define RGD_UPD_DESTINATION          (1ULL << 4)
#define RGD_UPD_ETA                  (1ULL << 5)
#define RGD_UPD_TIME_REMAINING       (1ULL << 6)
#define RGD_UPD_DISTANCE_REMAINING   (1ULL << 7)
#define RGD_UPD_DISTANCE_STRING      (1ULL << 8)
#define RGD_UPD_DISTANCE_UNITS       (1ULL << 9)
#define RGD_UPD_DIST_TO_MANEUVER     (1ULL << 10)
#define RGD_UPD_DIST_TO_MANEUVER_STR (1ULL << 11)
#define RGD_UPD_DIST_TO_MANEUVER_UNI (1ULL << 12)
#define RGD_UPD_MANEUVER_COUNT       (1ULL << 13)
#define RGD_UPD_MANEUVER_LIST        (1ULL << 14)
#define RGD_UPD_VISIBLE_IN_APP       (1ULL << 15)
#define RGD_UPD_LANE_INDEX           (1ULL << 16)
#define RGD_UPD_LANE_TOTAL           (1ULL << 17)
#define RGD_UPD_LANE_SHOWING         (1ULL << 18)
#define RGD_UPD_SOURCE_NAME          (1ULL << 19)
#define RGD_UPD_SOURCE_SUPPORTS_RG   (1ULL << 20)
#define RGD_UPD_LANE_SLOT            (1ULL << 21)  /* Bus-only remapped lane slot */

/* rgd_lane_guidance_t present bits */
#define RGD_LANE_COMPONENT_IDS       (1ULL << 0)
#define RGD_LANE_GUIDANCE_INDEX      (1ULL << 1)
#define RGD_LANE_INFORMATIONS        (1ULL << 2)
#define RGD_LANE_GUIDANCE_DESC       (1ULL << 3)

/* Maneuver data (from 0x5202) */
typedef struct {
    uint64_t present;
    uint16_t component_ids[MAX_COMPONENT_LIST];
    uint16_t component_count;
    uint16_t index;
    uint8_t maneuver_type;
    uint8_t distance_units;
    uint8_t driving_side;
    uint8_t junction_type;
    int16_t exit_angle;
    int16_t junction_angles[MAX_MANEUVER_LIST];
    uint16_t junction_angle_count;
    uint32_t distance_between;
    char description[256];
    char after_road_name[256];
    char distance_string[64];
    rgd_lane_t lanes[MAX_LANE_GUIDANCE];
    uint8_t lane_count;
    uint16_t lane_guidance_raw_len;
    char lane_guidance_hex[(RGD_HEX_PREVIEW_MAX * 3) + 1];
    uint16_t linked_lane_guidance_index;
    uint16_t exit_info_raw_len;
    char exit_info_hex[(RGD_HEX_PREVIEW_MAX * 3) + 1];
    char exit_info_str[RGD_EXIT_INFO_MAX + 1];
    uint8_t valid;
} rgd_maneuver_t;

/* rgd_maneuver_t present bits */
#define RGD_MAN_COMPONENT_IDS        (1ULL << 0)
#define RGD_MAN_INDEX                (1ULL << 1)
#define RGD_MAN_DESCRIPTION          (1ULL << 2)
#define RGD_MAN_TYPE                 (1ULL << 3)
#define RGD_MAN_AFTER_ROAD           (1ULL << 4)
#define RGD_MAN_DISTANCE_BETWEEN     (1ULL << 5)
#define RGD_MAN_DISTANCE_STRING      (1ULL << 6)
#define RGD_MAN_DISTANCE_UNITS       (1ULL << 7)
#define RGD_MAN_DRIVING_SIDE         (1ULL << 8)
#define RGD_MAN_JUNCTION_TYPE        (1ULL << 9)
#define RGD_MAN_JUNCTION_ANGLES      (1ULL << 10)
#define RGD_MAN_EXIT_ANGLE           (1ULL << 11)
#define RGD_MAN_LANE_GUIDANCE_RAW    (1ULL << 12)
#define RGD_MAN_EXIT_INFO_RAW        (1ULL << 13)
#define RGD_MAN_EXIT_INFO_STR        (1ULL << 14)
#define RGD_MAN_LANE_GUIDANCE        (1ULL << 15)  /* Parsed lane data in lanes[] */
#define RGD_MAN_LINKED_LANE_INDEX    (1ULL << 16)  /* Scalar linked lane guidance index */

/* ============================================================
 * Parsing Functions
 * ============================================================ */

/* Parse RouteGuidanceUpdate (0x5201) payload */
void rgd_parse_update(const uint8_t* buf, size_t len, rgd_update_t* out);

/* Parse RouteGuidanceManeuverUpdate (0x5202) payload */
void rgd_parse_maneuver(const uint8_t* buf, size_t len, rgd_maneuver_t* out);

/* Parse RouteGuidanceLaneGuidanceInformation (0x5204) payload */
void rgd_parse_lane_guidance(const uint8_t* buf, size_t len, rgd_lane_guidance_t* out);

/* ============================================================
 * Building Functions (for Identify patching)
 * ============================================================ */

/* Build RouteGuidanceDisplayComponent TLV for Identify
 * Returns total TLV length, or 0 on error */
size_t rgd_build_component_tlv(uint8_t* out, size_t max_len, uint16_t component_id);

/* Extract component ID from existing RouteGuidanceDisplayComponent
 * Returns component ID, or 0 if not found */
uint16_t rgd_extract_component_id(const uint8_t* tlv_start, size_t tlv_len);

#endif /* RGD_TLV_H */
