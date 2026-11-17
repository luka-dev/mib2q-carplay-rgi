/* Internal implementation, included by card_display.h (card display experiment): the
 * card-session SETUP/TEARDOWN stream descriptor dump, the type-111 role binding
 * (map / card) and per-stream TEARDOWN naming.  Reached only while a card record
 * is open for the session. */
#ifndef ALTSCREEN_HOOK_CARD_STREAMS_H
#define ALTSCREEN_HOOK_CARD_STREAMS_H

/* ── SETUP/TEARDOWN stream descriptor dump (card sessions only) ───────────────
 * Optional CF symbols, resolved once by dlsym while NULL (tests preset them and
 * the flag, so a host CoreFoundation is never called on fake objects).  Key
 * material never reaches the log: CFData prints its length only and any key
 * whose name looks secret prints <redacted>. */
typedef long (*fn_cfdict_count_t)(const void *);
typedef void (*fn_cfdict_kv_t)(const void *, const void **, const void **);
typedef long (*fn_cfdata_len_t)(const void *);
static fn_cfdict_count_t card_cf_dict_count;
static fn_cfdict_kv_t    card_cf_dict_kv;
static fn_cf_typeid_t    card_cf_numtype, card_cf_booltype, card_cf_datatype;
static fn_cfdata_len_t   card_cf_datalen;
static int               card_cf_resolved;
#define CARD_KEYS_MAX 32
#define CARD_LINE_SPLIT 700

static void card_cf_resolve(void)
{
    pthread_mutex_lock(&cf_lock);
    if (!card_cf_resolved) {
        if (!card_cf_dict_count)
            card_cf_dict_count = (fn_cfdict_count_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetCount");
        if (!card_cf_dict_kv)
            card_cf_dict_kv = (fn_cfdict_kv_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetKeysAndValues");
        if (!card_cf_numtype)
            card_cf_numtype = (fn_cf_typeid_t)dlsym(RTLD_DEFAULT, "CFNumberGetTypeID");
        if (!card_cf_booltype)
            card_cf_booltype = (fn_cf_typeid_t)dlsym(RTLD_DEFAULT, "CFBooleanGetTypeID");
        if (!card_cf_datatype)
            card_cf_datatype = (fn_cf_typeid_t)dlsym(RTLD_DEFAULT, "CFDataGetTypeID");
        if (!card_cf_datalen)
            card_cf_datalen = (fn_cfdata_len_t)dlsym(RTLD_DEFAULT, "CFDataGetLength");
        card_cf_resolved = 1;
    }
    pthread_mutex_unlock(&cf_lock);
}

static int card_key_secret(const char *name)
{
    static const char *const bad[] = {"key", "shk", "iv", "secret", "salt", "seed", NULL};
    char low[64];
    size_t i;
    int j;
    for (i = 0; name[i] && i + 1 < sizeof(low); ++i)   /* ASCII only: no libc ctype table */
        low[i] = (name[i] >= 'A' && name[i] <= 'Z') ? (char)(name[i] - 'A' + 'a') : name[i];
    low[i] = 0;
    for (j = 0; bad[j]; ++j)
        if (strstr(low, bad[j])) return 1;
    return 0;
}
static int card_is_type(const void *v, fn_cf_typeid_t typeid_fn)
{
    return v && cf_typeof && typeid_fn && cf_typeof(v) == typeid_fn();
}
static int card_cstr(const void *v, char *buf, long n)
{
    buf[0] = 0;
    return card_is_type(v, cf_strtype) && cf_getcstr && cf_getcstr(v, buf, n, CFENC_UTF8);
}
/* scalars: strings (quoted, at most `maxs` chars) and numbers/booleans; 0 if not */
static int card_fmt_scalar(const void *v, char *out, size_t n, int maxs)
{
    char s[200];
    int err = 0;
    int64_t num;
    if (card_is_type(v, cf_strtype)) {
        if (!card_cstr(v, s, sizeof(s))) { snprintf(out, n, "<string>"); return 1; }
        snprintf(out, n, "\"%.*s%s\"", maxs, s, (int)strlen(s) > maxs ? "..." : "");
        return 1;
    }
    if (card_is_type(v, cf_arrtype) || card_is_type(v, cf_dicttype) ||
        card_is_type(v, card_cf_datatype) || !cf_geti64_value)
        return 0;
    /* numbers and booleans; CoreUtils CFGetInt64 type-checks anything else */
    num = cf_geti64_value((void *)v, &err);
    if (err) return 0;
    snprintf(out, n, "%lld", (long long)num);
    return 1;
}
static void card_fmt_value(const void *v, char *out, size_t n)
{
    if (!v) { snprintf(out, n, "<null>"); return; }
    if (!cf_typeof) { snprintf(out, n, "<untyped>"); return; }
    if (card_is_type(v, card_cf_datatype)) {
        snprintf(out, n, "<data %ld bytes>", card_cf_datalen ? card_cf_datalen(v) : -1L);
    } else if (card_is_type(v, cf_arrtype) && cf_arrcount && cf_arridx) {
        long c = cf_arrcount((void *)v), i;
        size_t w = (size_t)snprintf(out, n, "[%ld", c);
        for (i = 0; i < c && i < 4 && w < n; ++i) {
            char e[64];
            if (!card_fmt_scalar(cf_arridx((void *)v, i), e, sizeof(e), 32))
                snprintf(e, sizeof(e), "<%s>",
                         card_is_type(cf_arridx((void *)v, i), card_cf_datatype) ? "data" : "obj");
            w += (size_t)snprintf(out + w, n - w, "%s%s", i ? "," : ": ", e);
        }
        if (w < n) snprintf(out + w, n - w, "%s]", c > 4 ? ",..." : "");
    } else if (card_is_type(v, cf_dicttype)) {
        long c = card_cf_dict_count ? card_cf_dict_count(v) : -1;
        size_t w = (size_t)snprintf(out, n, "{%ld", c);
        if (c > 0 && c <= CARD_KEYS_MAX && card_cf_dict_kv) {
            const void *keys[CARD_KEYS_MAX], *vals[CARD_KEYS_MAX];
            long i;
            card_cf_dict_kv(v, keys, vals);
            for (i = 0; i < c && w < n; ++i) {
                char kn[48];
                if (!card_cstr(keys[i], kn, sizeof(kn))) snprintf(kn, sizeof(kn), "<key>");
                w += (size_t)snprintf(out + w, n - w, "%s%s", i ? "," : ": ", kn);
            }
        }
        if (w < n) snprintf(out + w, n - w, "}");
    } else if (!card_fmt_scalar(v, out, n, 96)) {
        snprintf(out, n, "<type %lu>", cf_typeof(v));
    }
}

typedef struct { char buf[CARD_LINE_SPLIT + 300]; size_t w; int part; } card_line_t;
static void card_line_flush(card_line_t *l, const char *head)
{
    card_log(1, "%s%s%s", head, l->part ? " (cont)" : "", l->buf);
    ++l->part;
    l->w = 0;
    l->buf[0] = 0;
}
static void card_line_kv(card_line_t *l, const char *head, const char *name, const void *v)
{
    char val[260], kv[340];
    if (card_key_secret(name)) snprintf(val, sizeof(val), "<redacted>");
    else card_fmt_value(v, val, sizeof(val));
    snprintf(kv, sizeof(kv), " %s=%s", name, val);
    if (l->w && l->w + strlen(kv) > CARD_LINE_SPLIT) card_line_flush(l, head);
    l->w += (size_t)snprintf(l->buf + l->w, sizeof(l->buf) - l->w, "%s", kv);
    if (l->w >= sizeof(l->buf)) l->w = sizeof(l->buf) - 1;
}

static void card_log_one_stream(void *sd, const char *what, uint32_t seq, long i, long n)
{
    static const char *const probe[] = {
        "type", "streamConnectionID", "uuid", "displayUUID", "uid", "streamID", "timestampInfo",
        "latencyMs", "width", "height", "widthPixels", "heightPixels", "audioType", "ct", "sr",
        "spf", NULL};
    card_line_t line;
    char head[96];
    long cnt = -1;
    int walk, j;
    int64_t type = -1;
    void *kt;
    if (!sd || (cf_typeof && cf_dicttype && cf_typeof(sd) != cf_dicttype())) {
        card_log(1, "%s seq=%u stream[%ld/%ld] is not a dictionary", what, (unsigned)seq, i, n);
        return;
    }
    kt = cf_str(0, "type", CFENC_UTF8);
    if (kt) {
        int err = 0;
        type = cf_geti64(sd, kt, &err);
        if (err) type = -1;
        if (cf_rel) cf_rel(kt);
    }
    if (card_cf_dict_count) cnt = card_cf_dict_count(sd);
    walk = card_cf_dict_kv && cnt >= 0 && cnt <= CARD_KEYS_MAX;
    snprintf(head, sizeof(head), "%s seq=%u stream[%ld/%ld]", what, (unsigned)seq, i, n);
    memset(&line, 0, sizeof(line));
    line.w = (size_t)snprintf(line.buf, sizeof(line.buf), " type=%lld keys=%s count=%ld",
                              (long long)type, walk ? "walk" : "probe", cnt);
    if (walk) {
        const void *keys[CARD_KEYS_MAX], *vals[CARD_KEYS_MAX];
        long k;
        card_cf_dict_kv(sd, keys, vals);
        for (k = 0; k < cnt; ++k) {
            char name[64];
            if (!card_cstr(keys[k], name, sizeof(name)))
                snprintf(name, sizeof(name), "<key type %lu>", cf_typeof ? cf_typeof(keys[k]) : 0UL);
            card_line_kv(&line, head, name, vals[k]);
        }
    } else {
        for (j = 0; probe[j]; ++j) {
            void *k = cf_str(0, probe[j], CFENC_UTF8);
            void *v = k ? cf_getval(sd, k) : NULL;
            if (k && cf_rel) cf_rel(k);
            if (v) card_line_kv(&line, head, probe[j], v);
        }
    }
    card_line_flush(&line, head);
}

/* Every stream of every type in a card session's SETUP or TEARDOWN request. */
static void alt_card_log_streams(void *request, const char *what, uint32_t seq)
{
    void *ks, *arr;
    long n, i;
    card_cf_resolve();
    ks = cf_str(0, "streams", CFENC_UTF8);
    arr = ks ? cf_getval(request, ks) : NULL;
    if (ks && cf_rel) cf_rel(ks);
    if (!arr) {
        card_log(1, "%s seq=%u: no streams", what, (unsigned)seq);
        return;
    }
    if (cf_typeof && cf_arrtype && cf_typeof(arr) != cf_arrtype()) {
        card_log(1, "%s seq=%u: streams is not an array", what, (unsigned)seq);
        return;
    }
    n = cf_arrcount(arr);
    for (i = 0; i < n; ++i) card_log_one_stream(cf_arridx(arr, i), what, seq, i, n);
}

/* ── role binding ─────────────────────────────────────────────────────────── */
/* type-111 dictionaries of request["streams"] in request order; returns the total */
static long alt_card_collect_111(void *request, void *out[4])
{
    void *ks = cf_str(0, "streams", CFENC_UTF8), *kt = cf_str(0, "type", CFENC_UTF8), *arr;
    long total = 0, n, i;
    if (ks && kt && (arr = cf_getval(request, ks)) != NULL &&
        (!cf_typeof || !cf_arrtype || cf_typeof(arr) == cf_arrtype())) {
        n = cf_arrcount(arr);
        for (i = 0; i < n; ++i) {
            void *sd = cf_arridx(arr, i);
            if (!sd || (cf_typeof && cf_dicttype && cf_typeof(sd) != cf_dicttype())) continue;
            if (cf_geti64(sd, kt, NULL) != ALT_STREAM_TYPE) continue;
            if (total < 4) out[total] = sd;
            ++total;
        }
    }
    if (ks && cf_rel) cf_rel(ks);
    if (kt && cf_rel) cf_rel(kt);
    return total;
}
static int64_t alt_card_desc_conn(void *desc)
{
    void *k = cf_str(0, "streamConnectionID", CFENC_UTF8);
    int64_t conn = k ? cf_geti64(desc, k, NULL) : 0;
    if (k && cf_rel) cf_rel(k);
    return conn;
}
/* Role named by a uuid-like key, -1 when none names one of our displays. */
static int alt_card_uuid_role(void *desc, const char **by)
{
    static const char *const keys[] = {"uuid", "displayUUID", "uid", NULL};
    static const char *const by_key[] = {"uuid:uuid", "uuid:displayUUID", "uuid:uid", NULL};
    int j;
    for (j = 0; keys[j]; ++j) {
        char s[64];
        void *k = cf_str(0, keys[j], CFENC_UTF8);
        void *v = k ? cf_getval(desc, k) : NULL;
        if (k && cf_rel) cf_rel(k);
        if (!v) continue;
        if (!card_cstr(v, s, sizeof(s))) {
            card_log(1, "111 desc %s is not a readable string; binding by order", keys[j]);
            continue;
        }
        if (!strcmp(s, CLUSTER_DISPLAY_UUID)) { *by = by_key[j]; return ALT_ROLE_MAP; }
        if (!strcmp(s, CARD_DISPLAY_UUID)) { *by = by_key[j]; return ALT_ROLE_CARD; }
        card_log(1, "111 desc %s=%s matches no display; binding by order", keys[j], s);
    }
    return -1;
}
/* Roles of this record for the session (caller does not hold alt_lock).  live:
 * still receiving, so a receiver that already left (the phone closed its
 * connection, no TEARDOWN) frees its role for a re-SETUP, as today's single
 * stream does.  Otherwise: every slot a TEARDOWN can still stop. */
static unsigned alt_card_roles(void *session, uint32_t cardSeq, int live)
{
    unsigned roles = 0;
    int i;
    pthread_mutex_lock(&alt_lock);
    for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
        const altslot_t *s = &alt_slots[i];
        if (!s->inUse || s->session != session || s->cardSeq != cardSeq || s->closing) continue;
        if (live && (s->stop || (s->threadCreated && s->threadExited))) continue;
        roles |= ALT_ROLE_BIT(s->role);
    }
    pthread_mutex_unlock(&alt_lock);
    return roles;
}
static unsigned alt_card_live_roles(void *session, uint32_t cardSeq)
{
    return alt_card_roles(session, cardSeq, 1);
}
/* By uuid if a descriptor names one of our displays, else by display order (map,
 * then card): the first role neither taken in this request nor live. */
static int alt_card_bind(void *desc, unsigned taken, unsigned live, const char **by)
{
    int role = alt_card_uuid_role(desc, by);
    if (role >= 0) return (taken & ALT_ROLE_BIT(role)) ? -1 : role;
    *by = "order";
    if (!(taken & ALT_ROLE_BIT(ALT_ROLE_MAP)) && !(live & ALT_ROLE_BIT(ALT_ROLE_MAP)))
        return ALT_ROLE_MAP;
    if (!(taken & ALT_ROLE_BIT(ALT_ROLE_CARD)) && !(live & ALT_ROLE_BIT(ALT_ROLE_CARD)))
        return ALT_ROLE_CARD;
    return -1;
}
/* Stop the live slots of one role in this record (transactional SETUP rollback),
 * the same steps as alt_setup_role's retire loop. */
static void alt_card_retire_role(void *session, uint32_t cardSeq, int role)
{
    int listeners[ALT_MAX_SESSIONS], count = 0, i;
    uint32_t revoked = 0;
    pthread_mutex_lock(&alt_lock);
    for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
        altslot_t *s = &alt_slots[i];
        if (!s->inUse || s->session != session || s->cardSeq != cardSeq || s->role != role ||
            s->closing)
            continue;
        s->closing = 1;
        s->stop = 1;
        if (s->altSock >= 0) {
            listeners[count++] = s->altSock;
            s->altSock = -1;
        }
        if (alt_slot_is_current_locked(s)) {
            revoked = g_alt_session_gen;
            g_alt_session = NULL;
            g_alt_session_gen = 0;
        }
        if (!s->threadCreated && !s->settingUp && !s->threadReaping)
            memset(s, 0, sizeof(*s));
    }
    pthread_cond_broadcast(&alt_cond);
    pthread_mutex_unlock(&alt_lock);
    if (revoked) alt_command_revoke(revoked);
    for (i = 0; i < count; ++i) close(listeners[i]);
}

/* Card-session replacement for the single-111 SETUP block. */
static int alt_card_setup_streams(void *session, void *request, void *resp, uint32_t cardSeq,
                                  int *had111)
{
    void *descs[4];
    int roles[2];
    const char *by[2] = {"?", "?"};
    unsigned taken = 0, live, done = 0;
    long total, i;
    total = alt_card_collect_111(request, descs);
    if (total == 0) {
        LOG_INFO(LOG_MODULE, "SETUP: no 111 stream in request (phone did not offer altScreen)");
        return 0;
    }
    *had111 = 1;
    if (total > 2) {
        char why[64];
        snprintf(why, sizeof(why), "SETUP offered %ld type-111 streams", total);
        alt_card_disarm(why);
        return kMalformedErr;
    }
    live = alt_card_live_roles(session, cardSeq);
    for (i = 0; i < total; ++i) {
        roles[i] = alt_card_bind(descs[i], taken, live, &by[i]);
        card_log(1, "SETUP seq=%u 111 desc[%ld] connID=%llu -> role=%s by=%s",
                 (unsigned)cardSeq, i, (unsigned long long)alt_card_desc_conn(descs[i]),
                 roles[i] < 0 ? "none" : alt_role_name(roles[i]), by[i]);
        if (roles[i] < 0) {
            alt_card_disarm((live & ALT_ROLE_ALL) == ALT_ROLE_ALL && strcmp(by[i], "order") == 0
                                ? "111 descriptor has no free role (map and card both receiving)"
                                : "111 descriptor has no free role");
            return kMalformedErr;
        }
        taken |= ALT_ROLE_BIT(roles[i]);
    }
    for (i = 0; i < total; ++i) {        /* response streams follow request order */
        int arc = alt_setup_role(session, descs[i], resp, roles[i], cardSeq);
        card_log(1, "SETUP: alt_setup role=%s returned %d", alt_role_name(roles[i]), arc);
        if (arc != 0) {
            char why[64];
            snprintf(why, sizeof(why), roles[i] == ALT_ROLE_CARD ? "card SETUP failed rc=%d"
                                                                 : "map SETUP failed in card session rc=%d",
                     arc);
            alt_card_disarm(why);
            /* keep the failed SETUP transactional: no receiver from this request survives */
            if (done) alt_card_retire_role(session, cardSeq, roles[0]);
            return arc;
        }
        alt_card_note_setup(cardSeq, roles[i], strcmp(by[i], "order") != 0);
        ++done;
    }
    return 0;
}

/* ── per-stream TEARDOWN naming ───────────────────────────────────────────── */
/* Roles named by the request's type-111 entries: 0 = none named (no 111 entry).
 * An entry naming no stream (no matching streamConnectionID or uuid) is resolved
 * by elimination among the record's remaining slots, the capture-only card
 * first, so a guess never cuts the map; *guessed is set when that was a choice.
 * ALT_ROLE_ALL when no slot of the record is left (today's behaviour: stop all). */
static unsigned alt_card_teardown_roles(void *session, void *request, uint32_t cardSeq, int *guessed)
{
    void *descs[4];
    unsigned mask = 0, cand;
    long total = alt_card_collect_111(request, descs), i, unnamed = 0;
    *guessed = 0;
    if (!total) return 0;
    if (total > 4) total = 4;
    for (i = 0; i < total; ++i) {
        int64_t conn = alt_card_desc_conn(descs[i]);
        int role = -1, j;
        const char *by = "connID";
        if (conn) {
            pthread_mutex_lock(&alt_lock);
            for (j = 0; j < ALT_MAX_SESSIONS; ++j) {
                const altslot_t *s = &alt_slots[j];
                if (s->inUse && s->session == session && s->cardSeq == cardSeq && s->connID == conn) {
                    role = s->role;
                    break;
                }
            }
            pthread_mutex_unlock(&alt_lock);
        }
        if (role < 0) role = alt_card_uuid_role(descs[i], &by);
        if (role < 0) {
            ++unnamed;
            card_log(1, "TEARDOWN 111 desc[%ld] connID=%llu names no stream", i, (unsigned long long)conn);
            continue;
        }
        card_log(1, "TEARDOWN 111 desc[%ld] connID=%llu -> role=%s by=%s", i,
                 (unsigned long long)conn, alt_role_name(role), by);
        mask |= ALT_ROLE_BIT(role);
    }
    if (!unnamed) return mask;
    cand = alt_card_roles(session, cardSeq, 0) & ~mask;
    if (!cand) {
        if (mask) return mask;
        card_log(1, "TEARDOWN 111 entry names no stream and no stream of this record is left; "
                    "stopping both (today's behaviour)");
        return ALT_ROLE_ALL;
    }
    if (cand == ALT_ROLE_ALL && unnamed < 2) {
        *guessed = 1;
        card_log(1, "TEARDOWN 111 entry names no stream; stopping the card only (guess: the "
                    "capture-only card first, the map keeps running)");
        return mask | ALT_ROLE_BIT(ALT_ROLE_CARD);
    }
    card_log(1, "TEARDOWN 111 entries name no stream; stopping the remaining role(s)%s%s",
             (cand & ALT_ROLE_BIT(ALT_ROLE_MAP)) ? " map" : "",
             (cand & ALT_ROLE_BIT(ALT_ROLE_CARD)) ? " card" : "");
    return mask | cand;
}
#endif
