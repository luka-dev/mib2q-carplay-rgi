/* Test-only synchronous/copying seams. Include after the actual forwarder.
 * Production exposes owned async submission; these wrappers are not shipped. */
#ifndef ALTSCREEN_FORWARD_COMPAT_H
#define ALTSCREEN_FORWARD_COMPAT_H
static __attribute__((unused)) void altav_set_config(uint32_t gen, const uint8_t *cfg,
                                                     uint32_t len) {
    av_set_config(gen, cfg, len);
}
static int altav_forward_frame_meta(uint32_t gen, const uint8_t *avcc, uint32_t len, uint8_t ls,
                                    const alt_frame_meta_t *meta) {
    int result = -1, is_idr = 0;
    uint32_t need;
    if (!avcc || !len || len > ALT_WIRE_MAX_PAYLOAD)
        return -1;
    need = avcc_to_annexb(avcc, len, ls, NULL, 0, &is_idr);
    if (!need || need > ALT_WIRE_MAX_PAYLOAD)
        return -1;
    pthread_mutex_lock(&g_transport.lock);
    if (g_transport.enabled && g_transport.owner_gen == gen && g_transport.config_len &&
        (g_transport.fd >= 0 || is_idr)) {
        if (need > g_transport.annexb_cap) {
            uint32_t cap = need < 65536u ? 65536u : need;
            uint8_t *grown = (uint8_t *)realloc(g_transport.annexb, cap);
            if (grown) {
                g_transport.annexb = grown;
                g_transport.annexb_cap = cap;
            }
        }
        if (need <= g_transport.annexb_cap &&
            avcc_to_annexb(avcc, len, ls, g_transport.annexb, g_transport.annexb_cap, NULL))
            result = av_send_locked(g_transport.annexb, need, meta);
    }
    pthread_mutex_unlock(&g_transport.lock);
    return result;
}
static __attribute__((unused)) int altav_forward_avcc_frame(uint32_t gen, const uint8_t *avcc,
                                                            uint32_t len, uint8_t ls) {
    return altav_forward_frame_meta(gen, avcc, len, ls, NULL);
}

/* Compatibility entry point for callers that cannot transfer their input. The
 * shipping receiver uses enqueue_owned and has no per-frame payload copy here. */
static __attribute__((unused)) int altav_enqueue_frame(uint32_t gen, const uint8_t *avcc,
                                                       uint32_t len, uint8_t ls,
                                                       const alt_frame_meta_t *meta) {
    uint8_t *owned;
    uint32_t cap = len;
    alt_avcc_frame_t info;
    int rc;
    if (!avcc || !len || len > ALT_WIRE_MAX_PAYLOAD)
        return -1;
    owned = (uint8_t *)malloc(len);
    if (!owned)
        return -1;
    memcpy(owned, avcc, len);
    if (alt_avcc_prepare(owned, len, ls, ls == 4, &info) != 0) {
        free(owned);
        return -1;
    }
    rc = altav_enqueue_owned(gen, &owned, &cap, &info, meta);
    free(owned);
    return rc;
}

#endif
