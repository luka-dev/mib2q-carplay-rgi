/*
 * Route Guidance TLV Parsing and Building Implementation
 */

#include "rgd_tlv.h"
#include "../framework/logging.h"

DEFINE_LOG_MODULE(RGD);

static void hex_encode_preview(const uint8_t* data, int len, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!data || len <= 0) return;

    size_t max_bytes = (size_t)len;
    if (max_bytes > RGD_HEX_PREVIEW_MAX) max_bytes = RGD_HEX_PREVIEW_MAX;

    size_t off = 0;
    for (size_t i = 0; i < max_bytes && off + 4 < out_sz; i++) {
        off += (size_t)snprintf(out + off, out_sz - off,
                                "%02X%s", data[i], (i + 1 < max_bytes) ? " " : "");
    }
    out[off] = '\0';
}

/* Replace newlines (\r\n or \n) with " / " separator in-place.
 * iOS packs multi-part location info (street + bus stop, etc.) into a single
 * string separated by newlines.  " / " keeps it readable on the HUD. */
static void replace_newlines(char* s, size_t buf_sz) {
    if (!s || buf_sz == 0) return;
    const char sep[] = " / ";
    const size_t sep_len = 3;
    char tmp[256];
    size_t si = 0;
    for (size_t i = 0; s[i] != '\0' && si < sizeof(tmp) - sep_len - 1; i++) {
        if (s[i] == '\r' && s[i + 1] == '\n') {
            memcpy(tmp + si, sep, sep_len); si += sep_len; i++; /* skip \r\n */
        } else if (s[i] == '\n' || s[i] == '\r') {
            memcpy(tmp + si, sep, sep_len); si += sep_len;
        } else {
            tmp[si++] = s[i];
        }
    }
    tmp[si] = '\0';
    size_t copy = si < buf_sz - 1 ? si : buf_sz - 1;
    memcpy(s, tmp, copy);
    s[copy] = '\0';
}

static void copy_printable_string_preview(const uint8_t* data, int len, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!data || len <= 0) return;

    /*
     * Copy until NUL or max.
     *
     * Exit info on MHI3 is fed into util::UnicodeString8, so it can contain UTF-8.
     * Preserve bytes >= 0x80 (UTF-8 sequences) and replace only ASCII control bytes.
     */
    size_t max_bytes = (size_t)len;
    if (max_bytes > out_sz - 1) max_bytes = out_sz - 1;

    size_t n = 0;
    for (; n < max_bytes; n++) {
        uint8_t c = data[n];
        if (c == 0) break;
        if (c >= 0x80) {
            out[n] = (char)c;
        } else if (c >= 0x20 && c <= 0x7E) {
            out[n] = (char)c;
        } else {
            out[n] = '?';
        }
    }
    out[n] = '\0';
}

/*
 * Decode scalar MAN_TLV_LINKED_LANE_GUIDANCE (0x000C) payload.
 *
 * Native parser (sub_72C70 case 0x000C) consumes the first big-endian u16 only.
 */
static bool decode_linked_lane_index(const uint8_t* val, int val_len, uint16_t* out_idx) {
    if (!val || !out_idx || val_len < 2) return false;
    *out_idx = read_be16(val);
    return true;
}

static void rgd_lane_reset(rgd_lane_t* lane, uint16_t fallback_position) {
    if (!lane) return;
    memset(lane, 0, sizeof(*lane));
    lane->position = fallback_position;
    lane->direction = 1000;
}

/*
 * Parse one LaneInformations TLV (0x5204 / id=0x0002) payload.
 *
 * Native parser path:
 * processLaneGuidanceInformation -> parseLaneInformation
 * fields:
 *   0x0000 index
 *   0x0001 laneStatus
 *   0x0002 laneAngles (repeated)
 *   0x0003 laneAngleHighlight
 *
 * Returns number of parsed lanes (can be >1 if payload packs multiple index groups).
 */
static uint8_t rgd_parse_lane_information_value(const uint8_t* val, size_t val_len,
                                                rgd_lane_t* out_lanes, uint8_t out_max) {
    if (!val || !out_lanes || out_max == 0 || val_len < 4) return 0;

    size_t off = 0;
    int lane_idx = -1;
    uint8_t lane_count = 0;
    bool lane_has_highlight = false;
    bool saw_any = false;

    while (off + 4 <= val_len) {
        uint16_t sub_len = read_be16(val + off);
        uint16_t sub_id = read_be16(val + off + 2);
        int sub_val_len = (int)sub_len - 4;
        const uint8_t* sub_val = val + off + 4;

        if (sub_len < 4 || off + sub_len > val_len) {
            LOG_WARN(LOG_MODULE, "Malformed 0x5204 lane-information TLV: len=%u off=%zu total=%zu",
                     sub_len, off, val_len);
            break;
        }

        if (sub_id == LANE_INFO_TLV_INDEX) {
            if (lane_count >= out_max) {
                LOG_WARN(LOG_MODULE, "0x5204 lane information exceeds MAX_LANE_GUIDANCE=%u",
                         (unsigned)MAX_LANE_GUIDANCE);
                break;
            }

            lane_idx = lane_count;
            rgd_lane_reset(&out_lanes[lane_idx], (uint16_t)lane_idx);
            if (sub_val_len >= 2) {
                out_lanes[lane_idx].position = read_be16(sub_val);
            } else if (sub_val_len >= 1) {
                out_lanes[lane_idx].position = (uint16_t)sub_val[0];
            }
            lane_count++;
            lane_has_highlight = false;
            saw_any = true;
            off += sub_len;
            continue;
        }

        if (sub_id == LANE_INFO_TLV_STATUS
            || sub_id == LANE_INFO_TLV_ANGLES
            || sub_id == LANE_INFO_TLV_ANGLE_HIGHLIGHT) {
            if (lane_idx < 0) {
                if (lane_count >= out_max) {
                    LOG_WARN(LOG_MODULE, "0x5204 lane information exceeds MAX_LANE_GUIDANCE=%u",
                             (unsigned)MAX_LANE_GUIDANCE);
                    break;
                }
                lane_idx = lane_count;
                rgd_lane_reset(&out_lanes[lane_idx], (uint16_t)lane_idx);
                lane_count++;
                lane_has_highlight = false;
            }
            saw_any = true;
        }

        switch (sub_id) {
            case LANE_INFO_TLV_STATUS:
                if (lane_idx >= 0 && sub_val_len >= 1) {
                    out_lanes[lane_idx].status = sub_val[0];
                }
                break;

            case LANE_INFO_TLV_ANGLES:
                if (lane_idx >= 0 && sub_val_len >= 1) {
                    int p = 0;
                    while (p < sub_val_len && out_lanes[lane_idx].angle_count < MAX_LANE_ANGLES) {
                        int16_t angle;
                        if (p + 1 < sub_val_len) {
                            angle = (int16_t)read_be16(sub_val + p);
                            p += 2;
                        } else {
                            angle = (int16_t)sub_val[p];
                            p++;
                        }

                        out_lanes[lane_idx].angles[out_lanes[lane_idx].angle_count++] = angle;

                        /*
                         * laneAngleHighlight (0x0003) is authoritative. If it is absent,
                         * use the first laneAngles value as directional fallback.
                         */
                        if (!lane_has_highlight && out_lanes[lane_idx].direction == 1000) {
                            out_lanes[lane_idx].direction = angle;
                        }
                    }
                }
                break;

            case LANE_INFO_TLV_ANGLE_HIGHLIGHT:
                if (lane_idx >= 0 && sub_val_len >= 1) {
                    out_lanes[lane_idx].direction = (sub_val_len >= 2)
                        ? (int16_t)read_be16(sub_val)
                        : (int16_t)sub_val[0];
                    lane_has_highlight = true;
                }
                break;

            default:
                LOG_WARN(LOG_MODULE, "Unknown 0x5204 lane-information sub-TLV: id=0x%04X len=%u",
                         sub_id, sub_len);
                if (sub_val_len > 0) {
                    LOG_HEXDUMP(LOG_MODULE, "RGD 0x5204 lane-information unknown TLV",
                                sub_val, (size_t)sub_val_len);
                }
                break;
        }

        off += sub_len;
    }

    return saw_any ? lane_count : 0;
}

void rgd_parse_update(const uint8_t* buf, size_t len, rgd_update_t* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!buf || len < 6) return;

    size_t off = 6; /* Skip iAP2 header */
    while (off + 4 <= len) {
        uint16_t tlv_len = read_be16(buf + off);
        uint16_t tlv_id = read_be16(buf + off + 2);
        int val_len = (int)tlv_len - 4;
        const uint8_t* val = buf + off + 4;

        if (tlv_len < 4 || off + tlv_len > len) break;

        switch (tlv_id) {
            case RGD_TLV_COMPONENT_ID:
                out->component_count = 0;
                for (int i = 0; i + 1 < val_len && out->component_count < MAX_COMPONENT_LIST; i += 2) {
                    out->component_ids[out->component_count++] = read_be16(val + i);
                }
                out->present |= RGD_UPD_COMPONENT_IDS;
                break;

            case RGD_TLV_ROUTE_GUIDANCE_STATE:
                if (val_len >= 1) {
                    out->route_state = val[0];
                    out->present |= RGD_UPD_ROUTE_STATE;
                }
                break;

            case RGD_TLV_MANEUVER_STATE:
                if (val_len >= 1) {
                    out->maneuver_state = val[0];
                    out->present |= RGD_UPD_MANEUVER_STATE;
                }
                break;

            case RGD_TLV_CURRENT_ROAD_NAME:
                if (val_len >= 0) {
                    int n = val_len < 255 ? val_len : 255;
                    if (n > 0) memcpy(out->current_road, val, (size_t)n);
                    out->current_road[n] = '\0';
                    replace_newlines(out->current_road, sizeof(out->current_road));
                    out->present |= RGD_UPD_CURRENT_ROAD;
                }
                break;

            case RGD_TLV_DESTINATION_NAME:
                if (val_len >= 0) {
                    int n = val_len < 255 ? val_len : 255;
                    if (n > 0) memcpy(out->destination, val, (size_t)n);
                    out->destination[n] = '\0';
                    replace_newlines(out->destination, sizeof(out->destination));
                    out->present |= RGD_UPD_DESTINATION;
                }
                break;

            case RGD_TLV_ETA:
                if (val_len >= 8) {
                    out->eta = read_be64(val);
                    out->present |= RGD_UPD_ETA;
                }
                break;

            case RGD_TLV_TIME_REMAINING:
                if (val_len >= 8) {
                    out->time_remaining = read_be64(val);
                    out->present |= RGD_UPD_TIME_REMAINING;
                }
                break;

            case RGD_TLV_DISTANCE_REMAINING:
                if (val_len >= 4) {
                    out->distance_remaining = read_be32(val);
                    out->present |= RGD_UPD_DISTANCE_REMAINING;
                }
                break;

            case RGD_TLV_DISTANCE_STRING:
                if (val_len >= 0) {
                    int n = val_len < 63 ? val_len : 63;
                    if (n > 0) memcpy(out->distance_string, val, (size_t)n);
                    out->distance_string[n] = '\0';
                    out->present |= RGD_UPD_DISTANCE_STRING;
                }
                break;

            case RGD_TLV_DISTANCE_UNITS:
                if (val_len >= 1) {
                    out->distance_units = val[0];
                    out->present |= RGD_UPD_DISTANCE_UNITS;
                }
                break;

            case RGD_TLV_DIST_TO_MANEUVER:
                if (val_len >= 4) {
                    out->dist_to_maneuver = read_be32(val);
                    out->present |= RGD_UPD_DIST_TO_MANEUVER;
                }
                break;

            case RGD_TLV_DIST_TO_MANEUVER_STRING:
                if (val_len >= 0) {
                    int n = val_len < 63 ? val_len : 63;
                    if (n > 0) memcpy(out->dist_to_maneuver_string, val, (size_t)n);
                    out->dist_to_maneuver_string[n] = '\0';
                    out->present |= RGD_UPD_DIST_TO_MANEUVER_STR;
                }
                break;

            case RGD_TLV_DIST_TO_MANEUVER_UNITS:
                if (val_len >= 1) {
                    out->dist_to_maneuver_units = val[0];
                    out->present |= RGD_UPD_DIST_TO_MANEUVER_UNI;
                }
                break;

            case RGD_TLV_MANEUVER_COUNT:
                if (val_len >= 2) {
                    out->maneuver_count = read_be16(val);
                    out->present |= RGD_UPD_MANEUVER_COUNT;
                }
                break;

            case RGD_TLV_MANEUVER_LIST:
                out->maneuver_list_count = 0;
                for (int i = 0; i + 1 < val_len && out->maneuver_list_count < MAX_MANEUVER_LIST; i += 2) {
                    out->maneuver_list[out->maneuver_list_count++] = read_be16(val + i);
                }
                out->present |= RGD_UPD_MANEUVER_LIST;
                break;

            case RGD_TLV_VISIBLE_IN_APP:
                if (val_len >= 1) {
                    out->visible_in_app = val[0];
                    out->present |= RGD_UPD_VISIBLE_IN_APP;
                }
                break;

            case RGD_TLV_LANE_GUIDANCE_INDEX:
                if (val_len >= 2) {
                    out->lane_guidance_index = read_be16(val);
                    out->present |= RGD_UPD_LANE_INDEX;
                }
                break;

            case RGD_TLV_LANE_GUIDANCE_TOTAL:
                if (val_len >= 2) {
                    out->lane_guidance_total = read_be16(val);
                    out->present |= RGD_UPD_LANE_TOTAL;
                }
                break;

            case RGD_TLV_LANE_GUIDANCE_SHOWING:
                if (val_len >= 1) {
                    out->lane_guidance_showing = val[0];
                    out->present |= RGD_UPD_LANE_SHOWING;
                }
                break;

            case RGD_TLV_SOURCE_NAME:
                if (val_len >= 0) {
                    int n = val_len < 63 ? val_len : 63;
                    if (n > 0) memcpy(out->source_name, val, (size_t)n);
                    out->source_name[n] = '\0';
                    replace_newlines(out->source_name, sizeof(out->source_name));
                    out->present |= RGD_UPD_SOURCE_NAME;
                }
                break;

            case RGD_TLV_SOURCE_SUPPORTS_RG:
                if (val_len >= 1) {
                    out->source_supports_route_guidance = val[0];
                    out->present |= RGD_UPD_SOURCE_SUPPORTS_RG;
                }
                break;

            case 0x0015:
                /* Optional non-native extension: ignore and do not publish to the bus. */
                break;
            default:
                LOG_WARN(LOG_MODULE, "Unknown RGD TLV (0x5201): id=0x%04X len=%u", tlv_id, tlv_len);
                if (val_len > 0) {
                    LOG_HEXDUMP(LOG_MODULE, "RGD 0x5201 unknown TLV", val, (size_t)val_len);
                }
                break;
        }
        off += tlv_len;
    }
}

void rgd_parse_maneuver(const uint8_t* buf, size_t len, rgd_maneuver_t* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->linked_lane_guidance_index = 0xFFFF;
    if (!buf || len < 6) return;

    size_t off = 6; /* Skip iAP2 header */
    while (off + 4 <= len) {
        uint16_t tlv_len = read_be16(buf + off);
        uint16_t tlv_id = read_be16(buf + off + 2);
        int val_len = (int)tlv_len - 4;
        const uint8_t* val = buf + off + 4;

        if (tlv_len < 4 || off + tlv_len > len) break;

        switch (tlv_id) {
            case MAN_TLV_COMPONENT_ID:
                out->component_count = 0;
                for (int i = 0; i + 1 < val_len && out->component_count < MAX_COMPONENT_LIST; i += 2) {
                    out->component_ids[out->component_count++] = read_be16(val + i);
                }
                out->present |= RGD_MAN_COMPONENT_IDS;
                break;

            case MAN_TLV_INDEX:
                if (val_len >= 2) {
                    out->index = read_be16(val);
                    out->present |= RGD_MAN_INDEX;
                }
                break;

            case MAN_TLV_DESCRIPTION:
                if (val_len >= 0) {
                    int n = val_len < 255 ? val_len : 255;
                    if (n > 0) memcpy(out->description, val, (size_t)n);
                    out->description[n] = '\0';
                    replace_newlines(out->description, sizeof(out->description));
                    out->present |= RGD_MAN_DESCRIPTION;
                }
                break;

            case MAN_TLV_TYPE:
                if (val_len >= 1) {
                    out->maneuver_type = val[0];
                    out->present |= RGD_MAN_TYPE;
                }
                break;

            case MAN_TLV_AFTER_ROAD_NAME:
                if (val_len >= 0) {
                    int n = val_len < 255 ? val_len : 255;
                    if (n > 0) memcpy(out->after_road_name, val, (size_t)n);
                    out->after_road_name[n] = '\0';
                    replace_newlines(out->after_road_name, sizeof(out->after_road_name));
                    out->present |= RGD_MAN_AFTER_ROAD;
                }
                break;

            case MAN_TLV_DISTANCE_BETWEEN:
                if (val_len >= 4) {
                    out->distance_between = read_be32(val);
                    out->present |= RGD_MAN_DISTANCE_BETWEEN;
                }
                break;

            case MAN_TLV_DISTANCE_STRING:
                if (val_len >= 0) {
                    int n = val_len < 63 ? val_len : 63;
                    if (n > 0) memcpy(out->distance_string, val, (size_t)n);
                    out->distance_string[n] = '\0';
                    out->present |= RGD_MAN_DISTANCE_STRING;
                }
                break;

            case MAN_TLV_DISTANCE_UNITS:
                if (val_len >= 1) {
                    out->distance_units = val[0];
                    out->present |= RGD_MAN_DISTANCE_UNITS;
                }
                break;

            case MAN_TLV_DRIVING_SIDE:
                if (val_len >= 1) {
                    out->driving_side = val[0];
                    out->present |= RGD_MAN_DRIVING_SIDE;
                }
                break;

            case MAN_TLV_JUNCTION_TYPE:
                if (val_len >= 1) {
                    out->junction_type = val[0];
                    out->present |= RGD_MAN_JUNCTION_TYPE;
                }
                break;

            case MAN_TLV_JUNCTION_ANGLES:
                /* iAP2 sends one angle per repeated TLV - don't reset count
                 * (memset at function entry already initializes to 0) */
                for (int i = 0; i + 1 < val_len && out->junction_angle_count < MAX_MANEUVER_LIST; i += 2) {
                    out->junction_angles[out->junction_angle_count++] = (int16_t)read_be16(val + i);
                }
                out->present |= RGD_MAN_JUNCTION_ANGLES;
                break;

            case MAN_TLV_EXIT_ANGLE:
                if (val_len >= 2) {
                    out->exit_angle = (int16_t)read_be16(val);
                    out->present |= RGD_MAN_EXIT_ANGLE;
                }
                break;

            case MAN_TLV_LINKED_LANE_GUIDANCE:
                /* Always capture raw hex for debugging */
                out->lane_guidance_raw_len = (uint16_t)val_len;
                hex_encode_preview(val, val_len, out->lane_guidance_hex, sizeof(out->lane_guidance_hex));
                out->present |= RGD_MAN_LANE_GUIDANCE_RAW;
                {
                    uint16_t linked_idx = 0xFFFF;
                    if (decode_linked_lane_index(val, val_len, &linked_idx)) {
                        out->linked_lane_guidance_index = linked_idx;
                        out->present |= RGD_MAN_LINKED_LANE_INDEX;
                    }
                }
                break;

            case MAN_TLV_EXIT_INFO:
                out->exit_info_raw_len = (uint16_t)val_len;
                hex_encode_preview(val, val_len, out->exit_info_hex, sizeof(out->exit_info_hex));
                copy_printable_string_preview(val, val_len, out->exit_info_str, sizeof(out->exit_info_str));
                out->present |= RGD_MAN_EXIT_INFO_RAW;
                out->present |= RGD_MAN_EXIT_INFO_STR;
                break;
            default:
                LOG_WARN(LOG_MODULE, "Unknown MAN TLV (0x5202): id=0x%04X len=%u", tlv_id, tlv_len);
                if (val_len > 0) {
                    LOG_HEXDUMP(LOG_MODULE, "RGD 0x5202 unknown TLV", val, (size_t)val_len);
                }
                break;
        }
        off += tlv_len;
    }

    /*
     * MHI3 dio_manager (CRouteGuidanceUpdateProcessorImpl::getRouteGuidanceManeuverInformation)
     * explicitly sets a default exit angle when the field is not present.
     * Mirror that so Java-side mapping doesn't see "-1"/unset for valid maneuvers.
     */
    if ( (out->present & RGD_MAN_INDEX) && !(out->present & RGD_MAN_EXIT_ANGLE) ) {
        out->exit_angle = 1000;
        out->present |= RGD_MAN_EXIT_ANGLE;
    }
}

void rgd_parse_lane_guidance(const uint8_t* buf, size_t len, rgd_lane_guidance_t* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!buf || len < 6) return;

    size_t off = 6; /* Skip iAP2 header */
    while (off + 4 <= len) {
        uint16_t tlv_len = read_be16(buf + off);
        uint16_t tlv_id = read_be16(buf + off + 2);
        int val_len = (int)tlv_len - 4;
        const uint8_t* val = buf + off + 4;

        if (tlv_len < 4 || off + tlv_len > len) break;

        switch (tlv_id) {
            case LANE_MSG_TLV_COMPONENT_ID:
                if (val_len > 0) {
                    for (int i = 0; i + 1 < val_len && out->component_count < MAX_COMPONENT_LIST; i += 2) {
                        out->component_ids[out->component_count++] = read_be16(val + i);
                    }
                    if ((val_len & 1) && out->component_count < MAX_COMPONENT_LIST) {
                        out->component_ids[out->component_count++] = val[val_len - 1];
                    }
                }
                out->present |= RGD_LANE_COMPONENT_IDS;
                break;

            case LANE_MSG_TLV_LANE_GUIDANCE_INDEX:
                if (val_len >= 2) {
                    out->lane_guidance_index = read_be16(val);
                    out->present |= RGD_LANE_GUIDANCE_INDEX;
                } else if (val_len >= 1) {
                    out->lane_guidance_index = val[0];
                    out->present |= RGD_LANE_GUIDANCE_INDEX;
                }
                break;

            case LANE_MSG_TLV_LANE_INFORMATIONS:
                out->present |= RGD_LANE_INFORMATIONS;
                if (val_len > 0) {
                    LOG_HEXDUMP(LOG_MODULE, "0x5204 LaneInformations raw", val, (size_t)val_len);
                }
                if (val_len > 0 && out->lane_count < MAX_LANE_GUIDANCE) {
                    rgd_lane_t parsed[MAX_LANE_GUIDANCE];
                    uint8_t free_slots = (uint8_t)(MAX_LANE_GUIDANCE - out->lane_count);
                    uint8_t parsed_count = rgd_parse_lane_information_value(
                        val, (size_t)val_len, parsed, free_slots);
                    for (uint8_t i = 0; i < parsed_count && out->lane_count < MAX_LANE_GUIDANCE; i++) {
                        out->lanes[out->lane_count++] = parsed[i];
                    }
                }
                break;

            case LANE_MSG_TLV_LANE_GUIDANCE_DESC:
                if (val_len >= 0) {
                    int n = val_len < (int)(sizeof(out->lane_guidance_description) - 1)
                        ? val_len
                        : (int)(sizeof(out->lane_guidance_description) - 1);
                    if (n > 0) memcpy(out->lane_guidance_description, val, (size_t)n);
                    out->lane_guidance_description[n] = '\0';
                    out->present |= RGD_LANE_GUIDANCE_DESC;
                }
                break;

            default:
                LOG_WARN(LOG_MODULE, "Unknown RGD TLV (0x5204): id=0x%04X len=%u", tlv_id, tlv_len);
                if (val_len > 0) {
                    LOG_HEXDUMP(LOG_MODULE, "RGD 0x5204 unknown TLV", val, (size_t)val_len);
                }
                break;
        }

        off += tlv_len;
    }
}

size_t rgd_build_component_tlv(uint8_t* out, size_t max_len, uint16_t component_id) {
    if (!out) return 0;

    /*
     * Match MHI3 libesoiap2.so RouteGuidanceDisplayComponent layout (9 TLVs).
     *
     * ID 0: Identifier (uint16)
     * ID 1: Name (utf8)
     * ID 2: MaxCurrentRoadNameLength (uint16)
     * ID 3: MaxDestinationNameLength (uint16)
     * ID 4: MaxAfterManeuverRoadNameLength (uint16)
     * ID 5: MaxManeuverDescriptionLength (uint16)
     * ID 6: MaxGuidanceManeuverStorageCapacity (uint16)
     * ID 7: MaxLaneGuidanceDescriptionLength (uint16)
     * ID 8: MaxLaneGuidanceStorageCapacity (uint16)
     *
     * NOTE: Previously we had a spurious GuidanceDisplayCapability at ID 7
     * that pushed lane guidance IDs to 8/9 and added non-existent boolean
     * flags at 0x000A-0x000F.  iOS accepted the identify but silently
     * refused to send 0x5204 LaneGuidance messages because the lane
     * capacity TLVs were at the wrong IDs.
     */
    const char* name = "RouteGuidanceDisplayComponent";
    size_t name_len = strlen(name) + 1;

    /* Calculate inner TLV sizes (each u16 TLV = 6 bytes: len(2)+id(2)+val(2)) */
    size_t tlv0_len = 6;              /* Identifier */
    size_t tlv1_len = 4 + name_len;   /* Name (string) */
    size_t tlv2_len = 6;              /* MaxCurrentRoadNameLength */
    size_t tlv3_len = 6;              /* MaxDestinationNameLength */
    size_t tlv4_len = 6;              /* MaxAfterManeuverRoadNameLength */
    size_t tlv5_len = 6;              /* MaxManeuverDescriptionLength */
    size_t tlv6_len = 6;              /* MaxGuidanceManeuverStorageCapacity */
    size_t tlv7_len = 6;              /* MaxLaneGuidanceDescriptionLength */
    size_t tlv8_len = 6;              /* MaxLaneGuidanceStorageCapacity */

    size_t inner_len = tlv0_len + tlv1_len + tlv2_len + tlv3_len + tlv4_len +
                       tlv5_len + tlv6_len + tlv7_len + tlv8_len;
    size_t outer_len = 4 + inner_len;

    if (max_len < outer_len) return 0;

    size_t off = 0;

    /* Outer TLV header */
    write_be16(out + off, (uint16_t)outer_len); off += 2;
    write_be16(out + off, IDENT_TLV_ROUTE_GUIDANCE_COMPONENT); off += 2;

    /* 0x0000 Identifier */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_COMPONENT_ID); off += 2;
    write_be16(out + off, component_id); off += 2;

    /* 0x0001 Name */
    write_be16(out + off, (uint16_t)tlv1_len); off += 2;
    write_be16(out + off, RGD_COMP_NAME); off += 2;
    memcpy(out + off, name, name_len); off += name_len;

    /* 0x0002 MaxCurrentRoadNameLength */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_CURRENT_ROAD_NAME); off += 2;
    write_be16(out + off, 0x0100); off += 2;

    /* 0x0003 MaxDestinationNameLength */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_DEST_NAME); off += 2;
    write_be16(out + off, 0x0100); off += 2;

    /* 0x0004 MaxAfterManeuverRoadNameLength */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_AFTER_MANEUVER_NAME); off += 2;
    write_be16(out + off, 0x0100); off += 2;

    /* 0x0005 MaxManeuverDescriptionLength */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_MANEUVER_DESC); off += 2;
    write_be16(out + off, 0x0100); off += 2;

    /* 0x0006 MaxGuidanceManeuverStorageCapacity */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_MANEUVER_CAPACITY); off += 2;
    write_be16(out + off, 0x0006); off += 2;

    /* 0x0007 MaxLaneGuidanceDescriptionLength */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_LANE_GUIDANCE_DESC); off += 2;
    write_be16(out + off, 0x0100); off += 2;

    /* 0x0008 MaxLaneGuidanceStorageCapacity */
    write_be16(out + off, 6); off += 2;
    write_be16(out + off, RGD_COMP_MAX_LANE_GUIDANCE_CAPACITY); off += 2;
    write_be16(out + off, 0x0006); off += 2;

    return off;
}

uint16_t rgd_extract_component_id(const uint8_t* tlv_start, size_t tlv_len) {
    if (!tlv_start || tlv_len < 10) return 0;

    /* TLV structure: outer_len(2) + outer_id(2) + inner TLVs */
    size_t inner_off = 4;
    while (inner_off + 4 <= tlv_len) {
        uint16_t inner_len = read_be16(tlv_start + inner_off);
        uint16_t inner_id = read_be16(tlv_start + inner_off + 2);

        if (inner_len < 4 || inner_off + inner_len > tlv_len) break;

        if (inner_id == RGD_COMP_COMPONENT_ID && inner_len >= 6) {
            return read_be16(tlv_start + inner_off + 4);
        }
        inner_off += inner_len;
    }
    return 0;
}
