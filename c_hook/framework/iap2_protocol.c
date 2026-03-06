/*
 * CarPlay Hook Framework - iAP2 Protocol Implementation
 */

#include "iap2_protocol.h"

bool iap2_find_frame(const uint8_t* buf, size_t len, iap2_frame_t* frame) {
    return iap2_find_frame_at(buf, len, 0, frame);
}

bool iap2_find_frame_at(const uint8_t* buf, size_t len, size_t start_off, iap2_frame_t* frame) {
    if (!buf || len < IAP2_MIN_FRAME || !frame) return false;

    size_t max = len - 5;
    for (size_t i = start_off; i < max; i++) {
        if (buf[i] != IAP2_SYNC1 || buf[i + 1] != IAP2_SYNC2) continue;

        uint16_t flen = read_be16(buf + i + 2);
        if (flen < IAP2_MIN_FRAME) continue;
        if (i + flen > len) continue;

        frame->offset = i;
        frame->frame_len = flen;
        frame->msgid = read_be16(buf + i + 4);
        frame->payload = (flen > 6) ? (buf + i + 6) : NULL;
        frame->payload_len = (flen > 6) ? (flen - 6) : 0;
        return true;
    }
    return false;
}

bool iap2_validate_frame(const uint8_t* buf, size_t len) {
    if (!buf || len < IAP2_MIN_FRAME) return false;
    if (buf[0] != IAP2_SYNC1 || buf[1] != IAP2_SYNC2) return false;
    uint16_t flen = read_be16(buf + 2);
    if (flen < IAP2_MIN_FRAME || flen > len) return false;
    return true;
}

int iap2_parse_msgid(const uint8_t* buf, size_t len) {
    if (!buf || len < IAP2_MIN_FRAME) return -1;
    if (buf[0] != IAP2_SYNC1 || buf[1] != IAP2_SYNC2) return -1;
    return (int)read_be16(buf + 4);
}

bool iap2_parse_tlv(const uint8_t* buf, size_t len, iap2_tlv_t* tlv) {
    if (!buf || len < 4 || !tlv) return false;

    tlv->len = read_be16(buf);
    tlv->id = read_be16(buf + 2);

    if (tlv->len < 4 || tlv->len > len) return false;

    tlv->value = (tlv->len > 4) ? (buf + 4) : NULL;
    tlv->value_len = (tlv->len > 4) ? (tlv->len - 4) : 0;
    return true;
}

void iap2_tlv_iter_init(iap2_tlv_iter_t* iter, const uint8_t* frame, size_t frame_len) {
    if (!iter) return;
    /* Skip 6-byte iAP2 header */
    if (frame && frame_len > 6) {
        iter->data = frame + 6;
        iter->len = frame_len - 6;
    } else {
        iter->data = NULL;
        iter->len = 0;
    }
    iter->offset = 0;
}

bool iap2_tlv_iter_next(iap2_tlv_iter_t* iter, iap2_tlv_t* tlv) {
    if (!iter || !tlv || !iter->data) return false;
    if (iter->offset + 4 > iter->len) return false;

    const uint8_t* p = iter->data + iter->offset;
    size_t remaining = iter->len - iter->offset;

    if (!iap2_parse_tlv(p, remaining, tlv)) return false;

    iter->offset += tlv->len;
    return true;
}

bool iap2_find_tlv(const uint8_t* payload, size_t len, uint16_t tlv_id, iap2_tlv_t* tlv) {
    if (!payload || len < 4 || !tlv) return false;

    size_t off = 0;
    while (off + 4 <= len) {
        uint16_t tlen = read_be16(payload + off);
        uint16_t tid = read_be16(payload + off + 2);

        if (tlen < 4 || off + tlen > len) return false;

        if (tid == tlv_id) {
            tlv->len = tlen;
            tlv->id = tid;
            tlv->value = (tlen > 4) ? (payload + off + 4) : NULL;
            tlv->value_len = (tlen > 4) ? (tlen - 4) : 0;
            return true;
        }
        off += tlen;
    }
    return false;
}

size_t iap2_find_tlv_offset(const uint8_t* payload, size_t len, uint16_t tlv_id, uint16_t* out_len) {
    if (!payload || len < 4) return 0;

    size_t off = 0;
    while (off + 4 <= len) {
        uint16_t tlen = read_be16(payload + off);
        uint16_t tid = read_be16(payload + off + 2);

        if (tlen < 4 || off + tlen > len) return 0;

        if (tid == tlv_id) {
            if (out_len) *out_len = tlen;
            return off;
        }
        off += tlen;
    }
    return 0;
}

size_t iap2_build_frame(uint8_t* out, size_t out_max, uint16_t msgid,
                        const uint8_t* payload, size_t payload_len) {
    size_t frame_len = 6 + payload_len;
    if (!out || out_max < frame_len) return 0;

    out[0] = IAP2_SYNC1;
    out[1] = IAP2_SYNC2;
    write_be16(out + 2, (uint16_t)frame_len);
    write_be16(out + 4, msgid);

    if (payload && payload_len > 0) {
        memcpy(out + 6, payload, payload_len);
    }

    return frame_len;
}

size_t iap2_build_tlv(uint8_t* out, size_t out_max, uint16_t tlv_id,
                      const uint8_t* value, size_t value_len) {
    size_t tlv_len = 4 + value_len;
    if (!out || out_max < tlv_len) return 0;

    write_be16(out, (uint16_t)tlv_len);
    write_be16(out + 2, tlv_id);

    if (value && value_len > 0) {
        memcpy(out + 4, value, value_len);
    }

    return tlv_len;
}

size_t iap2_build_tlv_u16(uint8_t* out, size_t out_max, uint16_t tlv_id, uint16_t value) {
    if (!out || out_max < 6) return 0;

    write_be16(out, 6);      /* len */
    write_be16(out + 2, tlv_id);
    write_be16(out + 4, value);

    return 6;
}

size_t iap2_build_tlv_u8(uint8_t* out, size_t out_max, uint16_t tlv_id, uint8_t value) {
    if (!out || out_max < 5) return 0;

    write_be16(out, 5);      /* len */
    write_be16(out + 2, tlv_id);
    out[4] = value;

    return 5;
}

size_t iap2_build_tlv_str(uint8_t* out, size_t out_max, uint16_t tlv_id, const char* str) {
    if (!str) str = "";
    size_t str_len = strlen(str) + 1; /* Include null terminator */
    size_t tlv_len = 4 + str_len;

    if (!out || out_max < tlv_len) return 0;

    write_be16(out, (uint16_t)tlv_len);
    write_be16(out + 2, tlv_id);
    memcpy(out + 4, str, str_len);

    return tlv_len;
}

uint8_t iap2_cksum_sum(const uint8_t* buf, size_t len) {
    uint8_t sum = 0;
    if (!buf) return 0;
    for (size_t i = 0; i < len; i++) {
        sum += buf[i];
    }
    return sum;
}

uint8_t iap2_cksum_neg(const uint8_t* buf, size_t len) {
    return (uint8_t)(0u - (unsigned)iap2_cksum_sum(buf, len));
}

uint8_t iap2_cksum_ones(const uint8_t* buf, size_t len) {
    return (uint8_t)(~iap2_cksum_sum(buf, len));
}

uint8_t iap2_cksum_xor(const uint8_t* buf, size_t len) {
    uint8_t x = 0;
    if (!buf) return 0;
    for (size_t i = 0; i < len; i++) {
        x ^= buf[i];
    }
    return x;
}

uint8_t iap2_calc_cksum(const uint8_t* buf, size_t len, iap2_cksum_algo_t algo) {
    switch (algo) {
        case IAP2_CKSUM_SUM:  return iap2_cksum_sum(buf, len);
        case IAP2_CKSUM_NEG:  return iap2_cksum_neg(buf, len);
        case IAP2_CKSUM_ONES: return iap2_cksum_ones(buf, len);
        case IAP2_CKSUM_XOR:  return iap2_cksum_xor(buf, len);
        default:             return iap2_cksum_sum(buf, len);
    }
}

iap2_cksum_algo_t iap2_detect_cksum_algo(const uint8_t* payload, size_t len, uint8_t expected) {
    uint8_t sum = iap2_cksum_sum(payload, len);
    uint8_t neg = (uint8_t)(0u - (unsigned)sum);
    uint8_t ones = (uint8_t)(~sum);
    uint8_t xored = iap2_cksum_xor(payload, len);

    if (expected == sum)   return IAP2_CKSUM_SUM;
    if (expected == neg)   return IAP2_CKSUM_NEG;
    if (expected == ones)  return IAP2_CKSUM_ONES;
    if (expected == xored) return IAP2_CKSUM_XOR;

    return IAP2_CKSUM_UNKNOWN;
}

bool iap2_parse_link_header(const uint8_t* buf, size_t len, iap2_link_header_t* hdr) {
    if (!buf || len < 9 || !hdr) return false;
    if (buf[0] != IAP2_LINK_SYNC1 || buf[1] != IAP2_LINK_SYNC2) return false;

    hdr->length = read_be16(buf + 2);
    hdr->ctrl = buf[4];
    hdr->seq = buf[5];
    hdr->ack = buf[6];
    hdr->session = buf[7];
    hdr->checksum = buf[8];

    return true;
}

void iap2_patch_link_header(uint8_t* buf, size_t prefix_len, size_t new_total_len) {
    if (!buf || prefix_len < 9) return;

    /* Find and update length field */
    int matches = 0;
    size_t match_off = 0;
    uint16_t old_len = 0;

    /* Scan for 2-byte value that might be the length */
    for (size_t i = 0; i + 1 < prefix_len; i++) {
        uint16_t v = read_be16(buf + i);
        /* Length field is typically at offset 2 in link header */
        if (i == 2 || v > 0) {
            if (i == 2) {
                old_len = v;
                match_off = i;
                matches = 1;
                break;
            }
        }
    }

    if (matches == 1 && old_len > 0) {
        write_be16(buf + match_off, (uint16_t)new_total_len);

        /* Recalculate header checksum (negated sum of bytes 0..prefix_len-2) */
        uint8_t sum = 0;
        for (size_t i = 0; i < prefix_len - 1; i++) {
            sum += buf[i];
        }
        buf[prefix_len - 1] = (uint8_t)(0u - (unsigned)sum);
    }
}
