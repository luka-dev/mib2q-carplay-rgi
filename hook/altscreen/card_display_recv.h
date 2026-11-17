/* Internal implementation, included by card_display.h (card display experiment): the one
 * capture owner of ALT_CARD_CAPTURE_PATH, the card's showUI and the capture-only
 * receiver of the card stream. */
#ifndef ALTSCREEN_HOOK_CARD_RECV_H
#define ALTSCREEN_HOOK_CARD_RECV_H

/* ── one capture writer per process ───────────────────────────────────────────
 * A card re-SETUP can start a receiver while the retired one is still in its
 * loop.  The newest receiver takes the file over: the old fd is closed and its
 * 'wrote' line logged first, so the 12 MB cap counts one writer and the last
 * 'cap111b armed' line gets its 'wrote' line only when that writer is done
 * (custom.sh cap111b_closed).  Leaf lock; logging under it keeps that order. */
static pthread_mutex_t g_card_cap_lock = PTHREAD_MUTEX_INITIALIZER;
static alt_capture_t g_card_cap = {-1, 0, 0, 0, 0};
static uint32_t g_card_cap_gen;            /* receiver owning the fd; 0 = none */
static unsigned long g_card_cap_frames;

static void card_cap_close_locked(const char *why)
{
    alt_capture_close(&g_card_cap);
    card_log(1, "cap111b wrote %u bytes frames=%lu skipped=%u gen=%u%s", (unsigned)g_card_cap.written,
             g_card_cap_frames, (unsigned)g_card_cap.skipped, (unsigned)g_card_cap_gen, why);
    g_card_cap_gen = 0;
}
/* 1: this receiver now owns the capture */
static int card_cap_take(uint32_t gen)
{
    int rc, err;
    pthread_mutex_lock(&g_card_cap_lock);
    if (g_card_cap_gen) {
        char why[48];
        snprintf(why, sizeof(why), " (taken over by gen=%u)", (unsigned)gen);
        card_cap_close_locked(why);
    }
    rc = alt_capture_open(&g_card_cap, ALT_CARD_CAPTURE_PATH, ALT_CARD_CAPTURE_LIMIT);
    err = errno;
    g_card_cap_frames = 0;
    if (rc == 0) {
        g_card_cap_gen = gen;
        card_log(1, "cap111b armed gen=%u existing=%llu bytes", (unsigned)gen,
                 (unsigned long long)g_card_cap.size);
    } else if (rc == 1) {
        card_log(1, "cap111b full (existing=%llu); capture off", (unsigned long long)g_card_cap.size);
    } else if (rc == -2) {
        card_log(1, "cap111b target is not a regular file; capture off");
    } else {
        card_log(1, "cap111b open FAILED errno=%d", err);
    }
    pthread_mutex_unlock(&g_card_cap_lock);
    return rc == 0;
}
/* 0 written or skipped once; -1 the capture is closed or no longer this receiver's */
static int card_cap_append(uint32_t gen, uint32_t seq, const uint8_t *p, uint32_t n, int frame,
                           uint64_t now_us)
{
    int rc;
    pthread_mutex_lock(&g_card_cap_lock);
    if (!gen || g_card_cap_gen != gen) {
        pthread_mutex_unlock(&g_card_cap_lock);
        return -1;
    }
    rc = alt_capture_append(&g_card_cap, p, n);
    if (rc == 0) {
        alt_card_note_card(seq, n, frame, now_us);
        if (frame) ++g_card_cap_frames;
    } else if (rc < 0) {
        card_cap_close_locked(" (closed: cap or write error)");
    }
    pthread_mutex_unlock(&g_card_cap_lock);
    return rc < 0 ? -1 : 0;
}
static void card_cap_release(uint32_t gen)
{
    pthread_mutex_lock(&g_card_cap_lock);
    if (gen && g_card_cap_gen == gen) card_cap_close_locked("");
    pthread_mutex_unlock(&g_card_cap_lock);
}

/* ── the card's showUI, sent once per card stream ─────────────────────────────
 * It rides the MAP's committed generation: the command worker holds one
 * generation, and admitting another would retire the map's queued commands.
 * SHOW_UI is not replaceable, so it never replaces the map's showUI.  Its ack is
 * a WARN line (production logs drop the INFO-level alt_cmd_done). */
static void alt_card_showui_done(int status, void *resp, void *ctx)
{
    (void)resp;
#if ENABLE_LOGGING
    card_log(1, "showUI ack status=%d%s gen=%u", status, ap_status(status),
             (unsigned)(uintptr_t)ctx);
#else
    card_log(1, "showUI ack status=%d gen=%u", status, (unsigned)(uintptr_t)ctx);
#endif
}
static int alt_card_show_ui(void *session, const char *reason)
{
    uint32_t gen = alt_generation_for_session(session);
    void *p = NULL, *cmd = NULL, *pk = NULL;
    int rc = kNoMemoryErr;
    if (!gen) {
        card_log(1, "showUI deferred (no committed map generation) reason=%s", reason);
        return kUnsupportedErr;
    }
    if (cf_init() != 1 || !cf_dictm || !dict_keycb || !dict_valcb || !cf_dictset || !cf_str)
        return kUnsupportedErr;
    p = cf_dictm(0, 0, dict_keycb, dict_valcb);
    cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!p || !cmd) goto done;
    if (set_str(p, "uuid", CARD_DISPLAY_UUID) != 0 ||
        set_str(p, "url", ALT_CARD_DISPLAY_URL) != 0 ||
        set_str(cmd, "type", "showUI") != 0)
        goto done;
    pk = cf_str(0, "params", CFENC_UTF8);
    if (!pk) goto done;
    cf_dictset(cmd, pk, p);
    rc = alt_send_async_for_gen_key(gen, cmd, (void *)alt_card_showui_done, (void *)(uintptr_t)gen,
                                    ALT_COMMAND_SHOW_UI);
done:
    if (cf_rel) {
        if (pk) cf_rel(pk);
        if (p) cf_rel(p);
        if (cmd) cf_rel(cmd);
    }
    card_log(1, "showUI uuid=%s url=%s gen=%u reason=%s rc=%d", CARD_DISPLAY_UUID,
             ALT_CARD_DISPLAY_URL, (unsigned)gen, reason, rc);
    return rc;
}

/* ── capture-only receiver for the card stream ────────────────────────────────
 * Same wire, key derivation and AES-CTR as alt_recv_loop.  Never calls altav_*,
 * never sends cluster commands other than its one showUI, never touches
 * g_alt_session* or the map slot.  A parse, decrypt or allocation error ends it
 * and disarms: a capture-only stream must never be why a session drops twice. */
static void alt_card_recv_loop(altslot_t *slot, int fd, uint32_t av_gen)
{
    uint8_t aesctx[512] __attribute__((aligned(16)));
    uint8_t ls = 4;
    uint8_t hdr[ALT_HDR_SIZE];
    uint8_t cfg[4096];
    uint8_t *body = NULL;
    uint32_t bodycap = 0;
    unsigned long nframes = 0;
    unsigned invalid = 0;
    int has_idr = 0, kicked = 0, armed, avcc_logged = 0, ls_logged = 0;
    uint64_t last_try = 0;
    uint32_t seq = slot->cardSeq;
    const char *err = NULL;
    char why[96];

    if (!aes_ctr_init || !aes_ctr_update || aes_ctr_init(aesctx, slot->key, slot->iv) != 0) {
        LOG_ERROR(LOG_MODULE, "card display: AES-CTR initialization failed");
        alt_card_disarm("card receiver error: AES-CTR initialization failed");
        return;
    }
    armed = card_cap_take(av_gen);

    for (;;) {
        uint32_t bodySize;
        uint8_t opcode;
        if (alt_receiver_read_exact(fd, hdr, ALT_HDR_SIZE, &slot->stop, NULL) != 0) break;
        bodySize = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                   ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
        opcode = hdr[4];
        if (bodySize > ALT_MAX_BODY) {
            LOG_ERROR(LOG_MODULE, "card display: invalid bodySize=%u opcode=%u", bodySize, opcode);
            err = "invalid bodySize";
            break;
        }
        if (bodySize > bodycap) {
            uint8_t *nb = (uint8_t *)realloc(body, bodySize);
            if (!nb) { err = "out of memory"; break; }
            body = nb;
            bodycap = bodySize;
        }
        if (bodySize && alt_receiver_read_exact(fd, body, bodySize, &slot->stop, NULL) != 0) break;

        if (opcode == 1) {                                /* avcC config (not encrypted) */
            uint32_t clen = 0;
            int r = alt_video_config_to_annexb(body, bodySize, cfg, sizeof(cfg), &clen);
            if (r == 0) continue;                         /* screen-state notification */
            if (r < 0) {
                LOG_ERROR(LOG_MODULE, "card display: invalid avcC config (%u bytes)", bodySize);
                err = "invalid avcC config";
                break;
            }
            ls = (uint8_t)r;
            if (!avcc_logged) {
                float w = 0.0f, h = 0.0f;
                memcpy(&w, hdr + 16, sizeof(w));
                memcpy(&h, hdr + 20, sizeof(h));
                card_log(1, "avcC encoded=%dx%d lengthSize=%u gen=%u", (int)(w + 0.5f),
                         (int)(h + 0.5f), (unsigned)ls, (unsigned)av_gen);
                avcc_logged = 1;
            }
            if (armed && card_cap_append(av_gen, seq, cfg, clen, 0, 0) < 0) armed = 0;
        } else if (opcode == 0) {                         /* encrypted AVCC access unit */
            alt_avcc_frame_t prep;
            uint64_t now = alt_timing_now_us();
            /* only opcode-0 bodies advance the CTR stream */
            if (aes_ctr_update(aesctx, body, bodySize, body) != 0) {
                LOG_ERROR(LOG_MODULE, "card display: decryption failed");
                err = "decryption failed";
                break;
            }
            if (alt_avcc_prepare(body, bodySize, ls, ls == 4, &prep) != 0) {
                ++invalid;
                ++nframes;
                continue;
            }
            if (prep.idr && !has_idr) {
                has_idr = 1;
                card_log(1, "first IDR frame#%lu body=%u gen=%u", nframes, bodySize, (unsigned)av_gen);
            }
            if (has_idr) {   /* pre-IDR AUs are not captured: the file stays decodable */
                if (prep.annexb) {
                    if (armed && card_cap_append(av_gen, seq, body, prep.annexb_len, 1, now) < 0)
                        armed = 0;
                } else if (!ls_logged) {
                    ls_logged = 1;
                    card_log(1, "lengthSize=%u not captured", (unsigned)ls);
                }
                if (!kicked && (!last_try || now < last_try || now - last_try >= UINT64_C(1000000))) {
                    last_try = now ? now : 1;
                    if (alt_card_show_ui(slot->session, "card-first-IDR") == 0) kicked = 1;
                }
            }
            ++nframes;
        }
        /* opcodes 2/4/5: body consumed, nothing to do for a capture-only stream */
    }

    card_cap_release(av_gen);
    card_log(1, "receiver exit gen=%u frames=%lu invalid=%u first_idr=%d showUI=%d stop=%d%s%s",
             (unsigned)av_gen, nframes, invalid, has_idr, kicked, slot->stop,
             err ? " error=" : "", err ? err : "");
    if (err) {
        snprintf(why, sizeof(why), "card receiver error: %s", err);
        alt_card_disarm(why);
    }
    if (aes_ctr_final) aes_ctr_final(aesctx);
    free(body);
}
#endif
