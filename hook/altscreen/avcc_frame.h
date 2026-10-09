/* Whole-AU validation/classification. A length-size-4 owned buffer can be
 * rewritten without copying payloads. Never publish a rejected partial AU. */
#ifndef ALT_AVCC_FRAME_H
#define ALT_AVCC_FRAME_H
#include "../../common/alt_video_wire.h"
typedef struct {
    uint32_t input_len, annexb_len;
    uint8_t length_size, annexb, idr;
} alt_avcc_frame_t;
static inline int alt_avcc_prepare(uint8_t *p, uint32_t len, uint8_t ls, int rewrite,
                                   alt_avcc_frame_t *info) {
    uint32_t i = 0, total = 0;
    int key = 0;
    if (!info)
        return -1;
    memset(info, 0, sizeof(*info));
    if (!p || !len || len > ALT_WIRE_MAX_PAYLOAD || ls < 1 || ls > 4 || (rewrite && ls != 4))
        return -1;
    while (len - i >= ls) {
        uint32_t n = 0, k, prefix = i;
        for (k = 0; k < ls; ++k)
            n = (n << 8) | p[i + k];
        i += ls;
        if (!n || n > len - i || n > ALT_WIRE_MAX_PAYLOAD - 4u ||
            total > ALT_WIRE_MAX_PAYLOAD - 4u - n)
            return -1;
        if ((p[i] & 31u) == 5u)
            key = 1;
        if (rewrite) {
            p[prefix] = p[prefix + 1] = p[prefix + 2] = 0;
            p[prefix + 3] = 1;
        }
        total += 4u + n;
        i += n;
    }
    if (i != len)
        return -1;
    info->input_len = len;
    info->annexb_len = total;
    info->length_size = ls;
    info->annexb = rewrite ? 1 : 0;
    info->idr = (uint8_t)key;
    return 0;
}
#endif
