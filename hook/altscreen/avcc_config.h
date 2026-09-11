#ifndef CARPLAY_AVCC_CONFIG_H
#define CARPLAY_AVCC_CONFIG_H
#include <stdint.h>
#include <string.h>

/* Parse the mandatory avcC SPS/PPS arrays transactionally. Optional high-profile
 * extension fields may follow. Zero means unusable configuration, never a
 * partially valid SPS/PPS prefix. The caller must not publish output on error. */
static inline uint8_t alt_avcc_config_to_annexb(const uint8_t *a, uint32_t len,
                                               uint8_t *out, uint32_t cap,
                                               uint32_t *outlen)
{
    uint32_t i = 6, o = 0;
    unsigned group, n, count;
    uint8_t ls;
    *outlen = 0;
    if (!a || !out || len < 7 || a[0] != 1) return 0;
    ls = (uint8_t)((a[4] & 3) + 1);
    count = a[5] & 31;
    for (group = 0; group < 2; ++group) {
        if (!count) return 0;
        for (n = 0; n < count; ++n) {
            uint32_t size;
            if (len - i < 2) return 0;
            size = ((uint32_t)a[i] << 8) | a[i + 1]; i += 2;
            if (!size || size > len - i || cap - o < 4 || size > cap - o - 4) return 0;
            out[o] = out[o + 1] = out[o + 2] = 0; out[o + 3] = 1;
            memcpy(out + o + 4, a + i, size); o += 4 + size; i += size;
        }
        if (group == 0) {
            if (i == len) return 0;
            count = a[i++];
        }
    }
    *outlen = o;
    return ls;
}
#endif
