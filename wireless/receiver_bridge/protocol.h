#ifndef MHI2Q_RECEIVER_BRIDGE_PROTOCOL_H
#define MHI2Q_RECEIVER_BRIDGE_PROTOCOL_H

/* Exact on-wire contract used by daemon/src/receiver_control.rs and the
 * guarded-handoff owner helpers. Requests carry the receiver port; the bridge
 * rejects a mismatch before acknowledging START, STOP, QUIESCE, or the
 * wired-owner DRAIN request. */
#include <stdint.h>
#include <string.h>

enum {
    MHI2Q_RBR_VERSION = 2,
    MHI2Q_RBR_START = 1,
    MHI2Q_RBR_STOP = 2,
    MHI2Q_RBR_QUIESCE = 3,
    MHI2Q_RBR_WIRED_DRAIN = 4,
    MHI2Q_RBR_IDENTITY = 5,
    MHI2Q_RBR_STATUS = 6,
    MHI2Q_RBR_HEALTH = 7,
    MHI2Q_RBR_NOW_PLAYING = 8, /* v3 SI only; u16 LE size + iAP2 CSM */
    MHI2Q_RBR_IAP2 = 9, /* v3 SI: u16 size + kind + u32 LE sequence + payload */
    MHI2Q_RBR_IAP2_POLL = 10, /* v3 SI: u32 ACK; reply u32 sequence/u16 size/CSM */
    MHI2Q_RBR_ID_MAX = 252,
    MHI2Q_RBR_OK = 0,
    MHI2Q_RBR_NOT_READY = 1,
    MHI2Q_RBR_BAD_REQUEST = 2,
    MHI2Q_RBR_BUSY = 3,
    MHI2Q_RBR_REQUEST_SIZE = 16,
    MHI2Q_RBR_RESPONSE_SIZE = 8
};

typedef struct {
    uint8_t operation;
    uint32_t generation;
    uint16_t airplay_port;
} mhi2q_rbr_request;

/* Decode the receiver's published pi, never pk. Clear output on every error
 * so a malformed later registration cannot leave a previous identity live. */
static inline int mhi2q_rbr_copy_pairing_id(const void *txt, uint16_t length,
                                          char out[MHI2Q_RBR_ID_MAX + 1])
{
    const uint8_t *bytes = txt;
    size_t offset = 0;
    int seen = 0;
    if (!out) return -1;
    memset(out, 0, MHI2Q_RBR_ID_MAX + 1);
    if (!bytes) return -1;
    while (offset < length) {
        size_t n = bytes[offset++], i;
        if (!n || n > (size_t)length - offset) goto invalid;
        if (n >= 3 && (bytes[offset] == 'p' || bytes[offset] == 'P') &&
            (bytes[offset + 1] == 'i' || bytes[offset + 1] == 'I') && bytes[offset + 2] == '=') {
            if (seen++ || n == 3 || n - 3 > MHI2Q_RBR_ID_MAX) goto invalid;
            for (i = 3; i < n; ++i)
                if (bytes[offset + i] <= 32 || bytes[offset + i] == 127) goto invalid;
            memcpy(out, bytes + offset + 3, n - 3);
        }
        offset += n;
    }
    return seen == 1 ? 0 : -1;
invalid:
    memset(out, 0, MHI2Q_RBR_ID_MAX + 1);
    return -1;
}

static inline int mhi2q_rbr_decode_request(
    const uint8_t bytes[MHI2Q_RBR_REQUEST_SIZE], mhi2q_rbr_request *out)
{
    if (!bytes || !out || memcmp(bytes, "CPRX", 4) != 0 ||
        bytes[4] != MHI2Q_RBR_VERSION ||
        (bytes[5] != MHI2Q_RBR_START && bytes[5] != MHI2Q_RBR_STOP &&
         bytes[5] != MHI2Q_RBR_QUIESCE &&
         bytes[5] != MHI2Q_RBR_WIRED_DRAIN && bytes[5] != MHI2Q_RBR_IDENTITY &&
         bytes[5] != MHI2Q_RBR_STATUS && bytes[5] != MHI2Q_RBR_HEALTH &&
         bytes[5] != MHI2Q_RBR_NOW_PLAYING && bytes[5] != MHI2Q_RBR_IAP2 &&
         bytes[5] != MHI2Q_RBR_IAP2_POLL) ||
        bytes[6] != 0 || bytes[7] != 0 ||
        bytes[14] != 0 || bytes[15] != 0)
        return -1;
    out->operation = bytes[5];
    out->generation = (uint32_t)bytes[8] |
        ((uint32_t)bytes[9] << 8) |
        ((uint32_t)bytes[10] << 16) |
        ((uint32_t)bytes[11] << 24);
    out->airplay_port = (uint16_t)bytes[12] |
        ((uint16_t)bytes[13] << 8);
    if (out->airplay_port == 0)
        return -1;
    return 0;
}

static inline void mhi2q_rbr_encode_response(
    uint8_t bytes[MHI2Q_RBR_RESPONSE_SIZE], uint8_t status)
{
    memcpy(bytes, "CPRA", 4);
    bytes[4] = MHI2Q_RBR_VERSION;
    bytes[5] = status;
    bytes[6] = 0;
    bytes[7] = 0;
}

#endif
