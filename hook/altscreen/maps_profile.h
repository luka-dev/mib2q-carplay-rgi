/* Maps source-rate profile selection through iAP2 IdentificationInfo.
 * CarExperienceAssets v40 maps this PPID to userInfo.mapsRefreshRate=30.
 * This selects a phone-side policy; it does not guarantee display cadence.
 */
#ifndef CARPLAY_ALTSCREEN_MAPS_PROFILE_H
#define CARPLAY_ALTSCREEN_MAPS_PROFILE_H

#include "../framework/hook_framework.h"
#include "../framework/logging.h"

#define ALT_MAPS_PROFILE_PPID "bf889dfdaea54096"
#define ALT_MAPS_IDENTIFY_MAX 8192u

static size_t alt_maps_profile_identify(hook_context_t *ctx, uint8_t *buf,
                                      size_t len, size_t max_len)
{
    static const char ppid[] = ALT_MAPS_PROFILE_PPID;
    const size_t extra = 4u + sizeof(ppid);
    size_t offset, ppid_count = 0, new_len;

    /* Parse the complete top-level parameter list before changing any bytes.
     * A nested component parameter numbered 34 is not an accessory PPID. */
    if (!buf || len < IAP2_MIN_FRAME || len > max_len ||
        len > ALT_MAPS_IDENTIFY_MAX || buf[0] != IAP2_SYNC1 ||
        buf[1] != IAP2_SYNC2 || read_be16(buf + 2) != len ||
        read_be16(buf + 4) != IAP2_MSG_IDENTIFY)
        return len;

    for (offset = IAP2_MIN_FRAME; offset < len; ) {
        uint16_t size;
        if (len - offset < 4u) {
            LOG_WARN("altscreen", "Identify: truncated parameter; Maps30 PPID skipped");
            return len;
        }
        size = read_be16(buf + offset);
        if (size < 4u || size > len - offset) {
            LOG_WARN("altscreen", "Identify: invalid parameter; Maps30 PPID skipped");
            return len;
        }
        if (read_be16(buf + offset + 2u) == 34u) ++ppid_count;
        offset += size;
    }
    if (ppid_count) {
        LOG_WARN("altscreen", "Identify: preserving %zu existing PPID parameter(s); Maps30 PPID not appended", ppid_count);
        return len;
    }

    new_len = len + extra;
    if (new_len > ALT_MAPS_IDENTIFY_MAX) {
        LOG_WARN("altscreen", "Identify: no room for Maps30 PPID within size bound");
        return len;
    }
    if (new_len > max_len) {
        size_t capacity = 0;
        uint8_t *grown = hook_identify_grow(ctx, len, new_len, &capacity);
        if (!grown || capacity < new_len) {
            LOG_WARN("altscreen", "Identify: buffer growth failed; Maps30 PPID skipped");
            return len;
        }
        buf = grown;
    }

    write_be16(buf + len, (uint16_t)extra);
    write_be16(buf + len + 2u, 34u);
    memcpy(buf + len + 4u, ppid, sizeof(ppid));
    write_be16(buf + 2, (uint16_t)new_len);
    /* The Encode dispatcher updates NmeArray.len from this return value. */
    LOG_WARN("altscreen", "Identify: +PPID %s (Maps target 30; %zu -> %zu bytes)", ppid, len, new_len);
    return new_len;
}

#endif
