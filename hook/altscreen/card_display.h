/* Internal implementation, included by altscreen_hook.c after ALT_MAX_BODY and
 * before alt_recv_loop (the same pattern as command_worker.h).
 *
 * Card display experiment: a third, marker-gated "card" display (CARD_DISPLAY_UUID,
 * initialURL /instructioncard) so iOS sets up a second type-111 stream for the
 * navigation app's turn card.  The card stream is capture-only: its receiver
 * decrypts with the same per-stream key derivation and appends Annex-B to
 * ALT_CARD_CAPTURE_PATH.  It never claims the renderer transport, is never the
 * command target and never retires or stops the map stream.
 *
 * Everything here is reached only while a card record is open for the session
 * (alt_card_seq_for_session() != 0), and a record opens only when the control
 * SETUP preflight saw ALT_CARD_DISPLAY_MARKER.  Without the marker the hook runs
 * exactly today's code: no lock, no clock read, no file access beyond the
 * marker check itself.
 *
 * Two-phase marker: an open record holds the marker (it is removed and
 * <marker>.active written), and only a proven-good close (advertised session of
 * 20 s or more whose map stream received a keyframe; a session superseded without
 * TEARDOWN is measured to its last 111 frame) puts it back.  A short or
 * keyframe-less session, a guessed per-stream TEARDOWN, a card receiver error or
 * a failed card dict self-disarms: the marker stays removed and the reason goes to
 * <marker>.disarmed for the logging card.  A crash or dio_manager restart leaves
 * the marker removed without a reason; the first connect that finds it re-armed
 * next to a stale <marker>.active writes one.  Either way the next phone connect
 * gets today's two displays.  The logging card revokes the loan by
 * removing <marker>.active (its trigger is gone): the close then keeps it removed.  g_card_lock is a leaf lock: the marker
 * files change only under it (so a close and the next connect's marker check are
 * ordered); logging happens after it is released. */
#ifndef ALTSCREEN_CARD_DISPLAY_H
#define ALTSCREEN_CARD_DISPLAY_H
#include <stdarg.h>
#include "capture_file.h"

/* ── logging: one prefix; tests may tap the formatted text ─────────────────── */
static void (*g_card_log_tap)(const char *line);   /* test seam; NULL in the product */
static void card_log(int warn, const char *fmt, ...)
{
    char line[900];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (warn) LOG_WARN(LOG_MODULE, "card display: %s", line);
    else LOG_INFO(LOG_MODULE, "card display: %s", line);
    if (g_card_log_tap) g_card_log_tap(line);
}

static const char *alt_role_name(int role) { return role == ALT_ROLE_CARD ? "card" : "map"; }

/* ── session record ───────────────────────────────────────────────────────── */
typedef struct {
    void    *session;        /* NULL = no open record */
    uint32_t seq;            /* != 0 while open; never reused */
    uint32_t advert_seq;     /* control-SETUP sequence that opened it */
    uint64_t start_us;       /* alt_timing_now_us() at alt_card_open (control SETUP) */
    int      advertised;     /* 3-display tree published in /info */
    unsigned streams111;     /* successful alt_setup_role, both roles */
    unsigned card_streams;   /* successful card-role setups */
    unsigned bind_uuid;      /* 111 streams bound by a uuid key ... */
    unsigned bind_order;     /* ... or by display order (unverified) */
    unsigned teardown_guess; /* per-stream TEARDOWNs resolved by a guess */
    int      consumed;       /* the marker was taken at open; a good close restores it */
    int      disarmed;       /* the marker must stay removed */
} alt_card_session_t;
static pthread_mutex_t g_card_lock = PTHREAD_MUTEX_INITIALIZER;
static alt_card_session_t g_card;
static uint32_t g_card_seq_counter;
static int g_card_active_mine;               /* this process wrote <marker>.active */
static volatile int      g_alt_card_guard;   /* map URL guard: no /instructioncard on the map */
static volatile uint32_t g_card_live_seq;    /* atomic copy of g_card.seq for receivers */
static volatile uint32_t g_card_start_ms;    /* (uint32)(start_us / 1000) for receivers */
static volatile uint32_t g_card_rx_ms;       /* last 111 frame, ms after start + 1 (0 = none) */
static volatile uint32_t g_card_bytes;       /* card bytes captured this record (<= 12 MB) */
static volatile uint32_t g_card_frames;      /* card AUs captured */
static volatile uint32_t g_card_map_frames;  /* map AUs received */
static volatile int      g_card_map_idr;     /* map saw an IDR */
/* test seams (no product behaviour): last closed record and last disarm reason */
static alt_card_session_t g_card_last;
static uint32_t g_card_last_bytes, g_card_last_frames, g_card_last_map_frames;
static uint64_t g_card_last_dur_us;
static int g_card_last_map_idr, g_card_last_map_healthy;
static char g_card_last_end[32];
static char g_card_last_marker[16];
static char g_card_last_disarm[160];

static int alt_card_guard_get(void) { return __atomic_load_n(&g_alt_card_guard, __ATOMIC_ACQUIRE); }

/* ── marker files ─────────────────────────────────────────────────────────── */
#define ALT_CARD_ACTIVE_SUFFIX   ".active"
#define ALT_CARD_DISARMED_SUFFIX ".disarmed"
static int card_marker_file(char *buf, size_t n, const char *suffix)
{
    if (!alt_card_marker_path) return 0;
    return snprintf(buf, n, "%s%s", alt_card_marker_path, suffix) < (int)n;
}
/* A fresh file holding `text` (NULL = empty).  O_EXCL after the unlink: never
 * written through a symlink.  0 or an errno. */
static int card_file_write(const char *path, const char *text)
{
    int fd, err = 0;
    size_t n = text ? strlen(text) : 0;
    if (unlink(path) != 0 && errno != ENOENT) return errno ? errno : EIO;
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return errno ? errno : EIO;
    if (n && write(fd, text, n) != (ssize_t)n) err = errno ? errno : EIO;
    if (close(fd) != 0 && !err) err = errno ? errno : EIO;
    return err;
}
/* g_card_lock held: remove the marker and leave the reason for the logging card. */
static void card_disarm_files_locked(const char *why, int *rm_err, int *note_err)
{
    char path[600], line[200];
    *rm_err = *note_err = 0;
    snprintf(g_card_last_disarm, sizeof(g_card_last_disarm), "%s", why);
    if (!alt_card_marker_path) return;
    if (unlink(alt_card_marker_path) != 0 && errno != ENOENT) *rm_err = errno ? errno : EIO;
    if (card_marker_file(path, sizeof(path), ALT_CARD_DISARMED_SUFFIX)) {
        snprintf(line, sizeof(line), "%s\n", why);
        *note_err = card_file_write(path, line);
    }
}
/* g_card_lock held: the open record no longer holds the marker. */
static void card_active_drop_locked(void)
{
    char path[600];
    if (g_card_active_mine && card_marker_file(path, sizeof(path), ALT_CARD_ACTIVE_SUFFIX))
        unlink(path);
    g_card_active_mine = 0;
}
static void card_log_disarm(const char *why, int rm_err, int note_err)
{
    if (rm_err)
        card_log(1, "self-disarm (%s): removing %s FAILED errno=%d", why, alt_card_marker_path, rm_err);
    else
        card_log(1, "self-disarm (%s): %s removed; the next phone connect advertises today's 2 displays",
                 why, alt_card_marker_path ? alt_card_marker_path : "(no marker path)");
    if (note_err) card_log(1, "self-disarm reason not written to %s%s errno=%d",
                           alt_card_marker_path, ALT_CARD_DISARMED_SUFFIX, note_err);
}

/* Remove the marker so the next phone connect advertises today's two displays.
 * Acts once per record (always when no record is open), and also covers a record
 * opened since: a failure seen anywhere in card mode ends card mode until the
 * user re-arms.  It does not change the running session: its tree is published. */
static void alt_card_disarm(const char *why)
{
    int act = 1, rm_err = 0, note_err = 0;
    if (!why) why = "?";
    pthread_mutex_lock(&g_card_lock);
    if (g_card.seq) {
        act = !g_card.disarmed;
        g_card.disarmed = 1;
    }
    if (act) card_disarm_files_locked(why, &rm_err, &note_err);
    pthread_mutex_unlock(&g_card_lock);
    if (act) card_log_disarm(why, rm_err, note_err);
}

/* Decided once per advert tree, in advert_preflight.  The marker is read under
 * g_card_lock, so a concurrent close's marker change is either fully seen or not
 * at all.  A <marker>.active this process did not write is left by an earlier
 * process whose card session never closed: that disarms. */
static int alt_card_armed_for_advert(int legacy)
{
    static const char stale_why[] = "an earlier card session never closed (dio_manager restarted?)";
    char act[600];
    int armed, card, stale = 0, rm_err = 0, note_err = 0;
    if (!alt_card_marker_path) return 0;
    pthread_mutex_lock(&g_card_lock);
    armed = access(alt_card_marker_path, F_OK) == 0;
    if (armed && !g_card_active_mine && card_marker_file(act, sizeof(act), ALT_CARD_ACTIVE_SUFFIX) &&
        access(act, F_OK) == 0) {
        stale = 1;
        armed = 0;
        card_disarm_files_locked(stale_why, &rm_err, &note_err);
        unlink(act);
    }
    pthread_mutex_unlock(&g_card_lock);
    if (stale) card_log_disarm(stale_why, rm_err, note_err);
    card = armed && !legacy && !cp_dio_owner_is_wireless();
    if (armed && !card)
        card_log(1, "armed but ignored (%s)", legacy ? "legacy advert armed" : "wireless owner");
    if (card)
        card_log(1, "armed=1 (3 displays: main, cluster map, instruction card uuid=%s)",
                 CARD_DISPLAY_UUID);
    return card;
}

/* Close the open record selected under the lock: by session (NULL = any), or,
 * with use_seq, only when `sequence` is a newer control SETUP than its own.  The
 * marker decision is made under the lock; the summary is logged after it. */
static void alt_card_close_ex(void *session, int use_seq, uint32_t sequence, const char *end)
{
    alt_card_session_t snap;
    uint32_t bytes, frames, map_frames, rx;
    int map_idr, healthy, proven, disarm = 0, rm_err = 0, note_err = 0, restore_err = 0;
    uint64_t now, dur;
    const char *did = "kept";
    char why[128];
    if (!__atomic_load_n(&g_card_live_seq, __ATOMIC_ACQUIRE)) return;   /* no record */
    now = alt_timing_now_us();
    if (!end) end = "?";
    pthread_mutex_lock(&g_card_lock);
    if (!g_card.seq || (session && g_card.session != session) ||
        (use_seq && (int32_t)(sequence - g_card.advert_seq) <= 0)) {
        pthread_mutex_unlock(&g_card_lock);
        return;
    }
    snap = g_card;
    memset(&g_card, 0, sizeof(g_card));
    __atomic_store_n(&g_card_live_seq, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&g_alt_card_guard, 0, __ATOMIC_RELEASE);
    bytes = __atomic_load_n(&g_card_bytes, __ATOMIC_ACQUIRE);
    frames = __atomic_load_n(&g_card_frames, __ATOMIC_ACQUIRE);
    map_frames = __atomic_load_n(&g_card_map_frames, __ATOMIC_ACQUIRE);
    map_idr = __atomic_load_n(&g_card_map_idr, __ATOMIC_ACQUIRE);
    rx = __atomic_load_n(&g_card_rx_ms, __ATOMIC_ACQUIRE);
    /* Superseded: no TEARDOWN came, so the session ended at its last 111 frame,
     * not at the next connect. */
    if (use_seq) dur = rx ? (uint64_t)(rx - 1u) * 1000u : 0;
    else dur = (now && now >= snap.start_us) ? now - snap.start_us : 0;
    healthy = map_idr && map_frames >= ALT_CARD_MAP_HEALTHY_FRAMES;
    proven = dur >= ALT_CARD_MIN_SESSION_US && map_idr;
    if (snap.advertised && !snap.disarmed && !proven) {
        disarm = 1;
        if (dur < ALT_CARD_MIN_SESSION_US)
            snprintf(why, sizeof(why), "session ended after %llu ms (<20 s, end=%s)",
                     (unsigned long long)(dur / 1000u), end);
        else
            snprintf(why, sizeof(why), "session ended after %llu ms without a map keyframe (end=%s)",
                     (unsigned long long)(dur / 1000u), end);
        card_disarm_files_locked(why, &rm_err, &note_err);
        did = "removed";
    } else if (snap.disarmed) {
        did = "removed";
    } else if (snap.consumed && alt_card_marker_path) {
        char act[600];
        if (g_card_active_mine && card_marker_file(act, sizeof(act), ALT_CARD_ACTIVE_SUFFIX) &&
            access(act, F_OK) != 0) {
            did = "revoked";   /* the logging card ran without the trigger: stays removed */
        } else {
            if (access(alt_card_marker_path, F_OK) != 0)
                restore_err = card_file_write(alt_card_marker_path, NULL);
            did = restore_err ? "restore-failed" : "restored";
        }
    }
    card_active_drop_locked();
    g_card_last = snap;
    g_card_last_bytes = bytes;
    g_card_last_frames = frames;
    g_card_last_map_frames = map_frames;
    g_card_last_map_idr = map_idr;
    g_card_last_map_healthy = healthy;
    g_card_last_dur_us = dur;
    snprintf(g_card_last_end, sizeof(g_card_last_end), "%s", end);
    snprintf(g_card_last_marker, sizeof(g_card_last_marker), "%s", did);
    pthread_mutex_unlock(&g_card_lock);
    card_log(1, "session summary session=%p seq=%u advertised3=%s streams111=%u card_streams=%u "
             "bind_uuid=%u bind_order=%u teardown_guess=%u card_bytes=%u card_frames=%u "
             "map_first_idr=%d map_frames=%u map_healthy=%s duration_ms=%llu end=%s marker=%s",
             snap.session, (unsigned)snap.seq, snap.advertised ? "yes" : "no", snap.streams111,
             snap.card_streams, snap.bind_uuid, snap.bind_order, snap.teardown_guess,
             (unsigned)bytes, (unsigned)frames, map_idr, (unsigned)map_frames,
             healthy ? "yes" : "no", (unsigned long long)(dur / 1000u), end, did);
    if (disarm) card_log_disarm(why, rm_err, note_err);
    if (restore_err) card_log(1, "marker restore FAILED errno=%d", restore_err);
}
static void alt_card_close(void *session, const char *end)
{
    alt_card_close_ex(session, 0, 0, end);
}
/* A newer control SETUP ends a record whose session never sent TEARDOWN.  Runs
 * before that SETUP reads the marker, so a short session's self-disarm already
 * applies to the connect that superseded it. */
static void alt_card_close_superseded(uint32_t sequence)
{
    alt_card_close_ex(NULL, 1, sequence, "superseded");
}

/* Called inside advert_preflight's advert_lock commit, so records open in control
 * SETUP order; a stale (older) open never replaces a newer record. */
static uint32_t alt_card_open(void *session, uint32_t advert_seq)
{
    char act[600], line[96];
    uint32_t seq, newer = 0;
    int consumed = 0, gone = 0, rm_err = 0, active_err = 0;
    pthread_mutex_lock(&g_card_lock);
    if (g_card.seq && (int32_t)(advert_seq - g_card.advert_seq) < 0) newer = g_card.advert_seq;
    pthread_mutex_unlock(&g_card_lock);
    if (newer) {
        card_log(1, "stale open ignored advert_seq=%u (the record of seq=%u stays)",
                 (unsigned)advert_seq, (unsigned)newer);
        return 0;
    }
    alt_card_close(NULL, "replaced");          /* overlapping control SETUPs */
    pthread_mutex_lock(&g_card_lock);
    memset(&g_card, 0, sizeof(g_card));
    seq = ++g_card_seq_counter;
    if (!seq) seq = ++g_card_seq_counter;
    g_card.session = session;
    g_card.seq = seq;
    g_card.advert_seq = advert_seq;
    g_card.start_us = alt_timing_now_us();
    if (alt_card_marker_path) {               /* hold the marker while the record is open */
        if (unlink(alt_card_marker_path) == 0) consumed = 1;
        else if (errno == ENOENT) gone = 1;   /* removed since the check: it stays removed */
        else rm_err = errno ? errno : EIO;
        if (card_marker_file(act, sizeof(act), ALT_CARD_ACTIVE_SUFFIX)) {
            snprintf(line, sizeof(line), "session=%p seq=%u\n", session, (unsigned)seq);
            active_err = card_file_write(act, line);
            g_card_active_mine = !active_err;
        }
    }
    g_card.consumed = consumed;
    g_card.disarmed = gone;
    __atomic_store_n(&g_card_start_ms, (uint32_t)(g_card.start_us / 1000u), __ATOMIC_RELAXED);
    __atomic_store_n(&g_card_rx_ms, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_card_bytes, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_card_frames, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_card_map_frames, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_card_map_idr, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&g_card_live_seq, seq, __ATOMIC_RELEASE);
    __atomic_store_n(&g_alt_card_guard, 1, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&g_card_lock);
    card_log(1, "session open session=%p seq=%u marker=%s", session, (unsigned)seq,
             consumed ? "held until the session ends" : gone ? "gone (stays removed)" : "not held");
    if (rm_err) card_log(1, "marker not held: unlink errno=%d", rm_err);
    if (active_err) card_log(1, "%s%s not written errno=%d", alt_card_marker_path,
                             ALT_CARD_ACTIVE_SUFFIX, active_err);
    return seq;
}
/* advert_lock is held (leaf lock below it). */
static void alt_card_note_advertised(void *session)
{
    pthread_mutex_lock(&g_card_lock);
    if (g_card.seq && g_card.session == session) g_card.advertised = 1;
    pthread_mutex_unlock(&g_card_lock);
}
/* Every card branch is gated on a non-zero value; no record: no lock. */
static uint32_t alt_card_seq_for_session(void *session)
{
    uint32_t seq = 0;
    if (!session || !__atomic_load_n(&g_card_live_seq, __ATOMIC_ACQUIRE)) return 0;
    pthread_mutex_lock(&g_card_lock);
    if (g_card.seq && g_card.session == session) seq = g_card.seq;
    pthread_mutex_unlock(&g_card_lock);
    return seq;
}
static void alt_card_note_setup(uint32_t seq, int role, int by_uuid)
{
    pthread_mutex_lock(&g_card_lock);
    if (seq && g_card.seq == seq) {
        ++g_card.streams111;
        if (role == ALT_ROLE_CARD) ++g_card.card_streams;
        if (by_uuid) ++g_card.bind_uuid;
        else ++g_card.bind_order;
    }
    pthread_mutex_unlock(&g_card_lock);
}
static void alt_card_note_guess(uint32_t seq)
{
    pthread_mutex_lock(&g_card_lock);
    if (seq && g_card.seq == seq) ++g_card.teardown_guess;
    pthread_mutex_unlock(&g_card_lock);
}
static void card_note_rx(uint64_t now_us)
{
    uint32_t start = __atomic_load_n(&g_card_start_ms, __ATOMIC_RELAXED);
    if (!now_us) return;                       /* no clock: keep the last good value */
    __atomic_store_n(&g_card_rx_ms, (uint32_t)(now_us / 1000u) - start + 1u, __ATOMIC_RELAXED);
}
static void alt_card_note_map_frame(uint32_t seq, int idr, uint64_t now_us)
{
    if (!seq || seq != __atomic_load_n(&g_card_live_seq, __ATOMIC_ACQUIRE)) return;
    __atomic_add_fetch(&g_card_map_frames, 1, __ATOMIC_RELAXED);
    if (idr) __atomic_store_n(&g_card_map_idr, 1, __ATOMIC_RELAXED);
    card_note_rx(now_us);
}
static void alt_card_note_card(uint32_t seq, uint32_t bytes, int frame, uint64_t now_us)
{
    if (!seq || seq != __atomic_load_n(&g_card_live_seq, __ATOMIC_ACQUIRE)) return;
    __atomic_add_fetch(&g_card_bytes, bytes, __ATOMIC_RELAXED);
    if (frame) {
        __atomic_add_fetch(&g_card_frames, 1, __ATOMIC_RELAXED);
        card_note_rx(now_us);
    }
}

/* ── /info: the card display dict (simulator DisplayPanel.Alt2 + VideoStream.Alt2,
 * geometry = today's cluster fullscreen layout so card frames match the measured ones) */
static void *build_card_dict(void)
{
    void *d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    void *k = NULL, *arr = NULL, *va = NULL;
    if (!d) goto fail;
    if (set_str(d, "name", "Instruction Card") != 0 ||
        set_i64(d, "type", 111) != 0 ||
        set_i64(d, "maxFPS", 30) != 0 ||
        set_i64(d, "widthPixels", CLUSTER_W) != 0 ||
        set_i64(d, "heightPixels", CLUSTER_H) != 0 ||
        set_i64(d, "widthPhysical", CLUSTER_PHYS_W) != 0 ||
        set_i64(d, "heightPhysical", CLUSTER_PHYS_H) != 0 ||
        set_str(d, "uid", CARD_DISPLAY_UUID) != 0 ||      /* displayPanels id (CarKit) */
        set_str(d, "uuid", CARD_DISPLAY_UUID) != 0)       /* raw displays id: != display 0 -> 111 */
        goto fail;
    /* properties = ["showsInstruments"] (simulator Alt2 displayProperties).  "Instrument
     * Cluster" is left off: the map stays the only supportsInstrumentClusterContent display. */
    k = cf_str(0, "properties", CFENC_UTF8);
    arr = cf_arrm(0, 1, arr_cb);
    if (!k || !arr) goto fail;
    if (cf_arrappc) {
        if (cf_arrappc(arr, "showsInstruments", -1) != 0) goto fail;
    } else {
        void *s = cf_str(0, "showsInstruments", CFENC_UTF8);
        if (!s) goto fail;
        cf_arrapp(arr, s);
        if (cf_rel) cf_rel(s);
    }
    cf_dictset(d, k, arr);
    if (cf_rel) { cf_rel(arr); cf_rel(k); }
    arr = k = NULL;
    /* one fullscreen view area: visible 1440x542, safe x330..1109 y88..381 */
    k = cf_str(0, "viewAreas", CFENC_UTF8);
    arr = cf_arrm(0, 1, arr_cb);
    va = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H, FULL_SAFE_X, FULL_SAFE_Y, FULL_SAFE_W, FULL_SAFE_H);
    if (!k || !arr || !va) goto fail;
    cf_arrapp(arr, va);
    cf_dictset(d, k, arr);
    if (cf_rel) { cf_rel(va); cf_rel(arr); cf_rel(k); }
    va = arr = k = NULL;
    if (set_i64(d, "initialViewArea", 0) != 0) goto fail;
    k = cf_str(0, "adjacentViewAreas", CFENC_UTF8);
    arr = cf_arrm(0, 0, arr_cb);
    if (!k || !arr) goto fail;
    cf_dictset(d, k, arr);
    if (cf_rel) { cf_rel(arr); cf_rel(k); }
    arr = k = NULL;
    if (set_str(d, "initialURL", ALT_CARD_DISPLAY_URL) != 0 ||
        set_i64(d, "cornerMasks", 0) != 0 ||
        set_i64(d, "nightMode", ALT_CARD_NIGHT_MODE) != 0 ||   /* fixed; never re-seeded */
        set_i64(d, "limitedUI", 0) != 0)
        goto fail;
    k = cf_str(0, "limitedUIElements", CFENC_UTF8);
    arr = cf_arrm(0, 0, arr_cb);
    if (!k || !arr) goto fail;
    cf_dictset(d, k, arr);
    if (cf_rel) { cf_rel(arr); cf_rel(k); }
    arr = k = NULL;
    /* its own video stream (simulator VideoStream.Alt2); required non-empty */
    k = cf_str(0, "initialVideoStreams", CFENC_UTF8);
    arr = build_stream_array(CARD_DISPLAY_UUID);
    if (!k || !arr) goto fail;
    cf_dictset(d, k, arr);
    if (cf_rel) { cf_rel(arr); cf_rel(k); }
    card_log(0, "declared uuid=%s %dx%d safe=(%d,%d %dx%d) url=%s nightMode=%d",
             CARD_DISPLAY_UUID, CLUSTER_W, CLUSTER_H, FULL_SAFE_X, FULL_SAFE_Y, FULL_SAFE_W,
             FULL_SAFE_H, ALT_CARD_DISPLAY_URL, ALT_CARD_NIGHT_MODE);
    return d;

fail:
    if (cf_rel) {
        if (va) cf_rel(va);
        if (arr) cf_rel(arr);
        if (k) cf_rel(k);
        if (d) cf_rel(d);
    }
    LOG_ERROR(LOG_MODULE, "card display dict build incomplete; advertising today's 2 displays");
    return NULL;
}

#include "card_display_streams.h"   /* descriptor dump, role binding, TEARDOWN naming */
#include "card_display_recv.h"      /* capture owner, the card's showUI, its receiver */
#endif
