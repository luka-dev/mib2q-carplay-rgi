/*
 * CarPlay Hook Framework - iAP2 Protocol Helpers
 *
 * iAP2 frame parsing, building, and checksum utilities
 */

#ifndef CARPLAY_IAP2_PROTOCOL_H
#define CARPLAY_IAP2_PROTOCOL_H

#include "common.h"

/* iAP2 sync bytes */
#define IAP2_SYNC1      0x40
#define IAP2_SYNC2      0x40

/* iAP2 link header sync bytes */
#define IAP2_LINK_SYNC1 0xFF
#define IAP2_LINK_SYNC2 0x5A

/* Minimum frame size (sync + len + msgid) */
#define IAP2_MIN_FRAME  6

/* Common iAP2 message IDs */
#define IAP2_MSG_IDENTIFY_START           0x1D00
#define IAP2_MSG_IDENTIFY                 0x1D01
#define IAP2_MSG_IDENTIFY_ACCEPTED        0x1D02
#define IAP2_MSG_IDENTIFY_END             0x1D03
#define IAP2_MSG_AUTH_REQ_CERT            0xAA00
#define IAP2_MSG_AUTH_REQ_CHALLENGE       0xAA02
#define IAP2_MSG_AUTH_COMPLETE            0xAA05
#define IAP2_MSG_AUTH_SUCCESS             IAP2_MSG_AUTH_COMPLETE
#define IAP2_MSG_NOW_PLAYING_START        0x5000
#define IAP2_MSG_NOW_PLAYING_UPDATE       0x5001
#define IAP2_MSG_CALL_STATE_START         0x4154
#define IAP2_MSG_CALL_STATE_UPDATE        0x4155
#define IAP2_MSG_COMMUNICATION_START      0x4157
#define IAP2_MSG_COMMUNICATION_UPDATE     0x4158
#define IAP2_MSG_ROUTE_GUIDANCE_START     0x5200
#define IAP2_MSG_ROUTE_GUIDANCE_UPDATE    0x5201
#define IAP2_MSG_ROUTE_GUIDANCE_MANEUVER  0x5202
#define IAP2_MSG_ROUTE_GUIDANCE_STOP      0x5203
#define IAP2_MSG_ROUTE_GUIDANCE_LANE      0x5204
#define IAP2_MSG_START_LOCATION           0xFFFA
#define IAP2_MSG_LOCATION_INFO            0xFFFB
#define IAP2_MSG_STOP_LOCATION            0xFFFC
#define IAP2_MSG_DEVICE_LANGUAGE_UPDATE   0x4E0A
#define IAP2_MSG_DEVICE_TIME_UPDATE       0x4E0B
#define IAP2_MSG_WIRELESS_UPDATE          0x4E0D
#define IAP2_MSG_TRANSPORT_NOTIFY         0x4E0E
#define IAP2_MSG_WIFI_CONFIG_REQ          0x5702
#define IAP2_MSG_WIFI_CONFIG_INFO         0x5703
#define IAP2_MSG_WIFI_CONFIG_REQ_ALT      0x56F2
#define IAP2_MSG_WIFI_CONFIG_INFO_ALT     0x56F3
#define IAP2_MSG_TRANSPORT_NOTIFY_ALT     0x4E1D

/* iAP2 link-layer checksum is fixed at "negated 8-bit sum" by Apple's
 * spec (R12+ USB Host transport).  No algo enum - every payload uses
 * iap2_cksum_neg() unconditionally. */

/* iAP2 frame info */
typedef struct {
    uint16_t msgid;             /* Message ID */
    uint16_t frame_len;         /* Frame length (from header) */
    size_t offset;              /* Offset to frame start in buffer */
    const uint8_t* payload;     /* Pointer to payload (after msgid) */
    size_t payload_len;         /* Payload length */
} iap2_frame_t;

/* TLV (Type-Length-Value) info */
typedef struct {
    uint16_t id;                /* TLV ID */
    uint16_t len;               /* Total TLV length (including header) */
    const uint8_t* value;       /* Pointer to value data */
    size_t value_len;           /* Value length */
} iap2_tlv_t;

/* Find iAP2 frame in buffer
 * Returns true if found, fills frame info */
bool iap2_find_frame(const uint8_t* buf, size_t len, iap2_frame_t* frame);

/* Find iAP2 frame starting at specific offset
 * Useful for finding multiple frames in a buffer */
bool iap2_find_frame_at(const uint8_t* buf, size_t len, size_t start_off, iap2_frame_t* frame);

/* Validate frame (check sync, length, etc.) */
bool iap2_validate_frame(const uint8_t* buf, size_t len);

/* Parse message ID from buffer */
int iap2_parse_msgid(const uint8_t* buf, size_t len);

/* Parse TLV from buffer
 * Returns true if valid TLV found, fills tlv info */
bool iap2_parse_tlv(const uint8_t* buf, size_t len, iap2_tlv_t* tlv);

/* TLV iterator */
typedef struct {
    const uint8_t* data;
    size_t len;
    size_t offset;
} iap2_tlv_iter_t;

/* Initialize TLV iterator for frame payload (skips 6-byte header) */
void iap2_tlv_iter_init(iap2_tlv_iter_t* iter, const uint8_t* frame, size_t frame_len);

/* Get next TLV, returns true if found */
bool iap2_tlv_iter_next(iap2_tlv_iter_t* iter, iap2_tlv_t* tlv);

/* Find TLV by ID in payload */
bool iap2_find_tlv(const uint8_t* payload, size_t len, uint16_t tlv_id, iap2_tlv_t* tlv);

/* Find TLV offset in payload (returns 0 if not found) */
size_t iap2_find_tlv_offset(const uint8_t* payload, size_t len, uint16_t tlv_id, uint16_t* out_len);

/* Build simple iAP2 frame (sync + len + msgid + payload)
 * Returns frame size, or 0 on error */
size_t iap2_build_frame(uint8_t* out, size_t out_max, uint16_t msgid,
                        const uint8_t* payload, size_t payload_len);

/* Build TLV
 * Returns TLV size, or 0 on error */
size_t iap2_build_tlv(uint8_t* out, size_t out_max, uint16_t tlv_id,
                      const uint8_t* value, size_t value_len);

/* Build TLV with uint16 value */
size_t iap2_build_tlv_u16(uint8_t* out, size_t out_max, uint16_t tlv_id, uint16_t value);

/* Build TLV with uint8 value */
size_t iap2_build_tlv_u8(uint8_t* out, size_t out_max, uint16_t tlv_id, uint8_t value);

/* Build TLV with string value (including null terminator) */
size_t iap2_build_tlv_str(uint8_t* out, size_t out_max, uint16_t tlv_id, const char* str);

/* iAP2 link-layer checksum: negated 8-bit sum of payload bytes. */
uint8_t iap2_cksum_neg(const uint8_t* buf, size_t len);

/* Link header helpers */
typedef struct {
    uint16_t length;
    uint8_t ctrl;
    uint8_t seq;
    uint8_t ack;
    uint8_t session;
    uint8_t checksum;
} iap2_link_header_t;

/* Parse link header (9 bytes: FF 5A len[2] ctrl seq ack sess cksum) */
bool iap2_parse_link_header(const uint8_t* buf, size_t len, iap2_link_header_t* hdr);

/* Update link header length and recalculate checksum */
void iap2_patch_link_header(uint8_t* buf, size_t prefix_len, size_t new_total_len);

#endif /* CARPLAY_IAP2_PROTOCOL_H */
