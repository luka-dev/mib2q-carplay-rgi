/*
 * CarPlay AltScreen — MHI2Q. dio-side `extendedFeatures` injector (LD_PRELOAD).
 *
 * VERIFIED iOS 26.6 activation path:
 *   - /info advertises a distinct cluster display in displays/displayPanels;
 *   - the RTSP control-SETUP response enables "altScreen";
 *   - iOS offers stream type 111;
 *   - this hook owns its TCP receive/decrypt/AVCC->Annex-B path and forwards to
 *     the external altscreen_render process.
 *
 * Mechanism (no dio binary patch): STOCK 210 libairplay has NO extendedFeatures
 * support — 0 occurrences of the string in the binary, so its /info builder never
 * reads config[extendedFeatures].  (Storing the key via SetProperty — the previous
 * approach — is therefore a no-op on stock; it only surfaces on the 280/from-source
 * lib.)  Instead we interpose the /info builder AirPlayCopyServerInfo itself and add
 *     info["extendedFeatures"] = ["altScreen", "viewAreas"]
 * to the CFMutableDictionary it returns, just before dio serializes it into the HTTP
 * /info response the phone reads.  dio imports AirPlayCopyServerInfo → interposable.
 *
 * Safety: CF objects are built with libairplay's own CFLite via dlsym (hand-built
 * CFStrings vs libairplay's interned CFLite constants was the "displays"-era crash);
 * the value is a fresh CFArray the dict retains.  No __thread (QNX emutls).  Deploy
 * only while dio_manager is stopped; never kill -9 it.
 *
 * The /info path also injects info["displays"] = [main, cluster] — main from the stock
 * builder AirPlayReceiverSessionScreen_CopyDisplaysInfo, cluster synthesized with
 * properties:["Instrument Cluster"] + type:111 → iOS-26.1 hasGaugeClusterScreen
 * (gate verified in CarKit CARSessionConfiguration.mm:956 + CARDisplayInfo.mm:409).
 */
#include "../framework/common.h"
#include "../framework/logging.h"
#include "../framework/bus.h"
#include "altscreen_hook.h"
#include "altscreen_av.h"
#include "../audio/audio_hook.h"   /* spa_augment_setup_response — this file owns SessionSetup */

#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>   /* realloc/free (self-contained receiver) */
#include <errno.h>    /* EINTR/EAGAIN in read_exact */
#include <string.h>   /* memcpy */
#include <poll.h>     /* poll() in read_exact — wait for data + honor teardown stop */
#include <stdio.h>    /* snprintf for avcC diagnostics */
#include <sys/time.h> /* rate-limit key-frame recovery without frame-rate assumptions */
#include <fcntl.h>    /* explicit O_NONBLOCK on accepted stream socket */
#include <time.h>     /* clock_gettime + timespec for the bounded alt_setup close-wait (R2) */

DEFINE_LOG_MODULE(altscreen);

#define CFENC_UTF8        0x08000100              /* kCFStringEncodingUTF8            */

/* ── libairplay CFLite builders (resolved lazily; all are dio imports) ─────────── */
typedef void *(*cf_str_t)(void *, const char *, unsigned);          /* CFStringCreateWithCString */
typedef void *(*cf_arrm_t)(void *, long, const void *);             /* CFArrayCreateMutable       */
typedef int   (*cf_arrappc_t)(void *, const char *, int);          /* CFArrayAppendCString(arr,str,len=-1) */
typedef void  (*cf_arrapp_t)(void *, const void *);                /* CFArrayAppendValue         */
typedef int   (*cf_arrappi64_t)(void *, int64_t);                   /* CFArrayAppendInt64         */
typedef void *(*cf_ret_t)(void *);                                 /* CFRetain                   */
typedef void  (*cf_rel_t)(void *);                                 /* CFRelease                  */
typedef void  (*cf_dictset_t)(void *, const void *, const void *); /* CFDictionarySetValue       */
typedef void *(*cf_dictm_t)(void *, long, const void *, const void *); /* CFDictionaryCreateMutable */
typedef void  (*cf_dictseti_t)(void *, const void *, int64_t);     /* CFDictionarySetInt64       */
typedef int64_t (*fn_cfdict_geti64_t)(void *, const void *, void *); /* CFDictionaryGetInt64      */
typedef int64_t (*fn_cf_geti64_t)(void *, void *);                  /* CFGetInt64                 */
typedef void *(*fn_cfdict_getval_t)(void *, const void *);          /* CFDictionaryGetValue       */
/* AirPlayReceiverSessionScreen_CopyDisplaysInfo(screenSession, outErr) — 280 ref
 * ignores the session arg ((void)inSession) and builds the MAIN display dict via
 * ScreenCopyMain(); we pass NULL. */
typedef void *(*fn_copydisp_t)(void *, void *);

static cf_str_t     cf_str;
static cf_arrm_t    cf_arrm;
static cf_arrappc_t cf_arrappc;
static cf_arrapp_t  cf_arrapp;
static cf_arrappi64_t cf_arrappi64;
static cf_ret_t     cf_ret;
static cf_rel_t     cf_rel;
static cf_dictset_t cf_dictset;
static cf_dictm_t   cf_dictm;
static cf_dictseti_t cf_dictseti;
static fn_cfdict_geti64_t cf_geti64;
static fn_cf_geti64_t cf_geti64_value;
static fn_copydisp_t fn_copydisp;
static fn_cfdict_getval_t cf_getval;   /* resolved in alt_resolve(); used by main_disp_modernize */
static const void  *arr_cb;          /* array callbacks constant (CFLite)            */
static const void  *dict_keycb;      /* dictionary key callbacks constant            */
static const void  *dict_valcb;      /* dictionary value callbacks constant          */
static int          cf_ok = -1;      /* extendedFeatures (#1a) builders ready         */
static int          disp_ok = -1;    /* displays (#1b) builders ready                 */
static pthread_mutex_t cf_lock = PTHREAD_MUTEX_INITIALIZER;
/* The control SETUP precedes /info on iOS 26.6. Retain the exact validated
 * display tree built before enabling altScreen and reuse it for /info, so the
 * feature flag and its two-display contract are one transaction. */
static pthread_mutex_t advert_lock = PTHREAD_MUTEX_INITIALIZER;
static void           *g_advert_session;
static void           *g_advert_displays;
static void           *g_advert_features;
static int             g_advert_ready;
static int             g_advert_published;
static uint32_t        g_advert_sequence;
static volatile uint32_t g_setup_sequence_counter;
/* Seeded from stock /info's authoritative nightMode, then updated by the
 * dio->libairplay AirPlayReceiverSessionSetNightMode boundary call. */
static volatile int g_alt_night_mode;
static volatile int g_control_setup_marked;
static void advert_apply_night_locked(int night);

/* PID-scoped evidence for the external SI-owned recovery policy.  The marker
 * distinguishes a real pre-RTSP USB/enumeration failure from every later
 * negotiation/video/renderer failure, for which a physical USB reset would be
 * both irrelevant and destructive. /tmp is RAM-backed on the unit. */
static void mark_control_setup_seen(void)
{
    char path[96];
    int fd;
    static const char one[] = "1\n";
    if (__sync_lock_test_and_set(&g_control_setup_marked, 1)) return;
    snprintf(path, sizeof(path), "/tmp/carplay_control_setup.%d", (int)getpid());
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        g_control_setup_marked = 0;
        LOG_WARN(LOG_MODULE, "control SETUP marker create failed path=%s err=%s",
                 path, strerror(errno));
        return;
    }
    (void)write(fd, one, sizeof(one) - 1);
    close(fd);
    LOG_INFO(LOG_MODULE, "control SETUP reached marker=%s", path);
}

/* the CFLite array-callbacks constant has moved names across builds; try both */
static const void *resolve_arr_cb(void)
{
    static const char *names[] = {
        "kCFTypeArrayCallBacks", "kCFLArrayCallBacksCFLTypes",
        "kCFLTypeArrayCallBacks", 0
    };
    int i;
    for (i = 0; names[i]; i++) {
        void *p = dlsym(RTLD_DEFAULT, names[i]);
        if (p) { LOG_INFO(LOG_MODULE, "array cb = %s (%p)", names[i], p); return p; }
    }
    return 0;   /* none resolved → injection is disabled (a NULL-callbacks array does
                 * NOT retain appended values, so our append+CFRelease would dangle) */
}

/* NB: no lock-free fast path — the ready flag gates MULTIPLE function-pointer
 * writes, and on weakly-ordered ARM a lock-free reader could observe the flag
 * set before the pointers.  cf_init/alt_resolve are cold (per /info, per SETUP,
 * per command — never per video frame), so always taking the mutex is free. */
static int cf_init(void)
{
    pthread_mutex_lock(&cf_lock);
    /* Retry until ready (!=1), not just once (<0): a first /info that lost a
     * symbol-resolution race must not latch cf_ok=0 for the whole boot and leave
     * altScreen silently dark.  Cold path (per /info/SETUP/command), so re-running
     * the dlsym batch until it resolves is free; it stops once cf_ok==1. */
    if (cf_ok != 1) {
        cf_str     = (cf_str_t)     dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
        cf_arrm    = (cf_arrm_t)    dlsym(RTLD_DEFAULT, "CFArrayCreateMutable");
        cf_arrappc = (cf_arrappc_t) dlsym(RTLD_DEFAULT, "CFArrayAppendCString");
        cf_arrapp  = (cf_arrapp_t)  dlsym(RTLD_DEFAULT, "CFArrayAppendValue");
        cf_arrappi64 = (cf_arrappi64_t)dlsym(RTLD_DEFAULT, "CFArrayAppendInt64");
        cf_ret     = (cf_ret_t)     dlsym(RTLD_DEFAULT, "CFRetain");
        cf_rel     = (cf_rel_t)     dlsym(RTLD_DEFAULT, "CFRelease");
        cf_dictset = (cf_dictset_t) dlsym(RTLD_DEFAULT, "CFDictionarySetValue");
        cf_dictm   = (cf_dictm_t)   dlsym(RTLD_DEFAULT, "CFDictionaryCreateMutable");
        cf_dictseti= (cf_dictseti_t)dlsym(RTLD_DEFAULT, "CFDictionarySetInt64");
        cf_geti64  = (fn_cfdict_geti64_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetInt64");
        cf_geti64_value = (fn_cf_geti64_t)dlsym(RTLD_DEFAULT, "CFGetInt64");
        fn_copydisp= (fn_copydisp_t)dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_CopyDisplaysInfo");
        arr_cb     = resolve_arr_cb();
        dict_keycb = dlsym(RTLD_DEFAULT, "kCFLDictionaryKeyCallBacksCFLTypes");
        dict_valcb = dlsym(RTLD_DEFAULT, "kCFLDictionaryValueCallBacksCFLTypes");
        /* #1a needs: a mutable array + a way to add the implemented feature keys + CFStringCreate (the
         * /info key) + CFDictionarySetValue (add the key to the returned dict). */
        cf_ok = (arr_cb && cf_arrm && (cf_arrappc || (cf_str && cf_arrapp)) &&
                 cf_str && cf_dictset && cf_geti64 && cf_geti64_value &&
                 cf_ret && cf_rel) ? 1 : 0;
        /* #1b (displays) additionally needs: mutable dict + SetInt64 + AppendValue (dicts
         * can't go via AppendCString) + both dict callbacks + the stock main-display
         * builder.  If any is missing we still advertise extendedFeatures (#1a). */
        disp_ok = (cf_ok && cf_dictm && cf_dictseti && cf_arrapp && cf_dictset && cf_str &&
                   dict_keycb && dict_valcb && fn_copydisp) ? 1 : 0;
        LOG_INFO(LOG_MODULE, "cf_init ok=%d disp_ok=%d dictm=%p seti=%p copydisp=%p keycb=%p valcb=%p",
                 cf_ok, disp_ok, (void*)cf_dictm, (void*)cf_dictseti, (void*)fn_copydisp,
                 dict_keycb, dict_valcb);
    }
    pthread_mutex_unlock(&cf_lock);
    return cf_ok;
}

/* Build the /info extended-feature advert with libairplay's own CFLite;
 * caller CFReleases.  The hook implements both contracts: altScreen owns stream
 * 111 and viewAreas supplies a non-empty display.viewAreas array plus the
 * updateViewArea command path. */
static void *build_extfeatures(void)
{
    void *arr = cf_arrm(0, 2, arr_cb);
    if (!arr) return 0;
    if (cf_arrappc) {
        if (cf_arrappc(arr, "altScreen", -1) != 0 ||
            cf_arrappc(arr, "viewAreas", -1) != 0) {
            if (cf_rel) cf_rel(arr);
            return 0;
        }
    } else {
        void *s = cf_str(0, "altScreen", CFENC_UTF8);
        if (!s) { if (cf_rel) cf_rel(arr); return 0; }
        cf_arrapp(arr, s);
        if (cf_rel) cf_rel(s);   /* AppendValue retains */
        s = cf_str(0, "viewAreas", CFENC_UTF8);
        if (!s) { if (cf_rel) cf_rel(arr); return 0; }
        cf_arrapp(arr, s);
        if (cf_rel) cf_rel(s);
    }
    return arr;
}

/* ── #1b: build the 2-display `displays` array ─────────────────────────────────
 * iOS-26.1 sets hasGaugeClusterScreen iff the /info `displays` array contains a
 * display whose `properties` array holds "Instrument Cluster" (verified in CarKit
 * CARSessionConfiguration.mm:956 + CARDisplayInfo.mm:409).  Stock 210 emits NO
 * displays array, so we build [main, cluster]: main from the stock display-info
 * builder (real ScreenCopyMain uuid/size), cluster synthesized. */
static int set_str(void *d, const char *key, const char *val)
{
    void *k = cf_str(0, key, CFENC_UTF8);
    void *v = cf_str(0, val, CFENC_UTF8);
    if (k && v) cf_dictset(d, k, v);
    if (v && cf_rel) cf_rel(v);
    if (k && cf_rel) cf_rel(k);
    return (k && v) ? 0 : -1;
}
static int set_i64(void *d, const char *key, int64_t val)
{
    void *k = cf_str(0, key, CFENC_UTF8);
    if (k) cf_dictseti(d, k, val);
    if (k && cf_rel) cf_rel(k);
    return k ? 0 : -1;
}

/* Audi Kombi (instrument-cluster) panel — from firmware qcdisplaycfg_MMX2.xml
 * "Audi 1440x542 Kombi panel" (uVisWidth 1440 / uVisHeight 542); LVDS2 = Cluster
 * Display (displaymanager.json id 4).  CarPlay encodes the 111 stream at the size
 * we advertise here, so this MUST equal the panel. */
#define CLUSTER_W       1440
#define CLUSTER_H       542

/* Physical panel size in millimetres, as widthPhysical/heightPhysical.
 *
 * The Audi Virtual Cockpit is a 12.3" panel: 1440x542 across a 312.4 mm diagonal is 4.93 px/mm,
 * i.e. 292 x 110 mm at ~125 dpi.  This used to advertise 180 x 68, which is 8.00 px/mm = 203 dpi
 * and implies a 7.6" cluster - wrong by a wide margin, and wrong in the direction that matters.
 *
 * Stock advertises 188 x 88 for the main panel (AbstractTerminalModeConfiguration:
 * sysConst(522) == 2 ? 188 : 152 / 88 : 91), i.e. 5.45 px/mm = 138 dpi, implying 8.17" - which
 * matches the real 8.25" screen.  292 x 110 implies 12.28" against a real 12.3", so the cluster is
 * legitimately slightly COARSER than the main screen (125/138 = 0.91x).  180 x 68 claimed the
 * opposite and by a lot (1.47x) for a panel it described as 7.6".
 *
 * NOTE, so nobody rebuilds a story on this: this is a correctness fix, not a known fix for the
 * old-iOS connect failures.  iOS <= 26 does carry a single car-wide zoomFactor
 * (-[CRCarKitServiceAgent fetchCarPlayControlAdvertisingForUSBWithReply:] ->
 * initWithDisplayScaleMode:zoomFactor:) where iOS 27 uses zoomFactorByDisplayIndex, but that zoom
 * is looked up from the phone's own CRCarPlayCapabilities store by vehicle UUID - it is NOT derived
 * from these fields.  Where our physical size is actually read is CarKit's display parsing, which
 * has not been decompiled for this field.  See docs/reference/IOS26_VS_IOS27_BONJOUR.md. */
#define CLUSTER_PHYS_W  292
#define CLUSTER_PHYS_H  110

/* Instrument-cluster calibration grid is 16 columns (A..P) by 6 rows on the
 * 1440x542 panel: one column is exactly 90 px; the row boundaries are rounded
 * to 0/90/181/271/361/452/542.
 *
 * CarKit 26.6 represents CARScreenViewArea.visibleFrame and safeFrame as two
 * separate CGRects.  The 1440x542 cluster stream is rejected during activation
 * when either declared visibleFrame is reduced to our physical safe region, so
 * both advertised view areas must retain the full encoded visibleFrame.  The
 * physical non-rectangular masks cannot be represented exactly; keep critical
 * UI in the nested safe frame:
 *
 *   fullscreen rectangular safeArea: complete cells D2 through M5
 *              = x 270..1170, y 90..452 = (270,90) 900x362
 *   sportscreen rectangular safeArea (compact Sport layout): B2.5 through E5
 *              = x 90..450, y 135..452 = (90,135) 360x317
 *   classicscreen uses the same compact dimensions, centered horizontally
 *              = x 540..900, y 135..452 = (540,135) 360x317
 *
 * The background may render under a physical mask; route focus and critical UI
 * stay inside these nested safe rectangles. */
#define FULL_SAFE_X     270
#define FULL_SAFE_Y      90
#define FULL_SAFE_W     900
#define FULL_SAFE_H     362

#define SMALLSCREEN_SAFE_X       90
#define SMALLSCREEN_SAFE_Y      135
#define SMALLSCREEN_SAFE_W      360
#define SMALLSCREEN_SAFE_H      317
#define CLASSICSCREEN_SAFE_X    540

/* one rect dict, iOS keys: {originXPixels, originYPixels, widthPixels, heightPixels}
 * (CarKit CROrigin/CRSizeFromAirPlayDictionary). */
static void *build_rect(int64_t x, int64_t y, int64_t w, int64_t h)
{
    void *d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!d) return 0;
    if (set_i64(d, "originXPixels", x) != 0 ||
        set_i64(d, "originYPixels", y) != 0 ||
        set_i64(d, "widthPixels",   w) != 0 ||
        set_i64(d, "heightPixels",  h) != 0) {
        if (cf_rel) cf_rel(d);
        return 0;
    }
    return d;
}

/* one viewArea: visible frame (top-level rect) + "safeArea" sub-rect
 * (CarKit CRViewArea initWithAirPlayDictionary:). */
static void *build_viewarea(int64_t x, int64_t y, int64_t w, int64_t h,
                            int64_t sx, int64_t sy, int64_t sw, int64_t sh)
{
    void *va = build_rect(x, y, w, h);
    if (!va) return 0;
    void *safe = build_rect(sx, sy, sw, sh);
    void *k = safe ? cf_str(0, "safeArea", CFENC_UTF8) : NULL;
    if (!safe || !k) {
        if (k && cf_rel) cf_rel(k);
        if (safe && cf_rel) cf_rel(safe);
        if (cf_rel) cf_rel(va);
        return 0;
    }
    cf_dictset(va, k, safe);
    if (cf_rel) cf_rel(k);
    if (cf_rel) cf_rel(safe);
    return va;
}

/* One initialVideoStreams entry.  CarKit CARStreamInfo::initWithDictionary reads origin
 * (CROriginFromAirPlayDictionary → originXPixels/originYPixels), uuid, zIndex.  CARDisplayInfo
 * REQUIRES initialVideoStreams to be a present, valid NSArray or it returns nil → the phone
 * rejects the display and teardown-loops the session (verified in the iOS 26.1 CarKit decompile). */
static void *build_stream(const char *uuid)
{
    void *d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!d) return 0;
    if (set_i64(d, "originXPixels", 0) != 0 ||
        set_i64(d, "originYPixels", 0) != 0 ||
        set_str(d, "uuid", uuid) != 0 ||
        set_i64(d, "zIndex", 0) != 0) {
        if (cf_rel) cf_rel(d);
        return 0;
    }
    return d;
}
static void *build_stream_array(const char *uuid)
{
    /* initialVideoStreams must be a NON-empty array: an empty one passes the parser but the session
     * has no video stream to run → the phone sets up streams then instantly tears down (Dur=0s).  A
     * present stream (origin + uuid + zIndex) keeps the session alive (observed: 23s + main decoder
     * start with a stream, vs 0s with empty). */
    void *arr = cf_arrm(0, 1, arr_cb);
    if (!arr) return 0;
    void *s = build_stream(uuid);
    if (!s) { if (cf_rel) cf_rel(arr); return 0; }
    cf_arrapp(arr, s);
    if (cf_rel) cf_rel(s);
    return arr;
}

/* Same as build_stream/_array but keys "uuid" to an existing CFString (borrowed,
 * CFDictionarySetValue retains it) instead of a C literal — used for the MAIN
 * display, whose stream uuid MUST equal the real ScreenCopyMain display uuid. */
static void *build_stream_array_cf(void *uuidCF)
{
    void *d;
    void *k;
    void *arr = cf_arrm(0, 1, arr_cb);
    if (!arr || !uuidCF) goto fail;
    d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!d) goto fail;
    if (set_i64(d, "originXPixels", 0) != 0 ||
        set_i64(d, "originYPixels", 0) != 0) {
        if (cf_rel) cf_rel(d);
        goto fail;
    }
    k = cf_str(0, "uuid", CFENC_UTF8);
    if (!k) { if (cf_rel) cf_rel(d); goto fail; }
    cf_dictset(d, k, uuidCF);
    if (cf_rel) cf_rel(k);
    if (set_i64(d, "zIndex", 0) != 0) {
        if (cf_rel) cf_rel(d);
        goto fail;
    }
    cf_arrapp(arr, d);
    if (cf_rel) cf_rel(d);
    return arr;

fail:
    if (arr && cf_rel) cf_rel(arr);
    return 0;
}

static void *build_cluster_dict(int night)
{
    int properties_ok = 0;
    int view_areas_ok = 0;
    int initial_stream_ok = 0;
    int adjacent_ok = 0;
    void *d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!d) return 0;
    if (set_str(d, "name", "Instrument Cluster") != 0 ||
        set_i64(d, "type", 111) != 0 ||
        /* 30, matching stock: dio_manager.json declares "screen": { "maxFPS": 30 } for the main
         * screen, and 30 is also what we actually deliver on the cluster - AltScreenModule's
         * CLUSTER_FPS is 30, maneuver_render's CR_TARGET_FPS is 30, and altscreen_render presents
         * at 30.  Advertising 60 promised twice the rate that exists. */
        set_i64(d, "maxFPS", 30) != 0 ||
        set_i64(d, "widthPixels", CLUSTER_W) != 0 ||
        set_i64(d, "heightPixels", CLUSTER_H) != 0 ||
        set_i64(d, "widthPhysical", CLUSTER_PHYS_W) != 0 ||
        set_i64(d, "heightPhysical", CLUSTER_PHYS_H) != 0)
        goto fail;
    /* CarKit's CARDisplayInfo parser reads the display identifier as "uid" (verified in the
     * iOS 26.1 CarKit decompile — initWithPhysicalScreenDictionary reads objectForKey:@"uid";
     * a missing uid → CARDisplayInfo init returns nil → the whole displays array is rejected →
     * the phone teardown-loops the session).  We previously sent "uuid" → that was the reject. */
    if (set_str(d, "uid", "C1057E12-A0D1-4E20-B0C0-1057E12A0D10") != 0)
        goto fail;  /* CarKit displayPanels reads "uid" */
    /* AirPlaySender's raw "displays" path reads "uuid" and picks stream 110 vs 111 by whether a
     * display's uuid DIFFERS from display-0's uuid (VERIFIED in iOS 26.6 AirPlaySender: 110/111 is a
     * UUID compare, NOT the type:111 field).  The dict is injected under BOTH "displays" (uuid consumer)
     * and "displayPanels" (uid consumer), so carry BOTH ids.  Cluster uuid MUST differ from main's. */
    if (set_str(d, "uuid", "C1057E12-A0D1-4E20-B0C0-1057E12A0D10") != 0)
        goto fail;
    /* properties = ["Instrument Cluster"] → CarKit supportsInstrumentClusterContent (+0xB).  NOTE
     * (verified 26.6): this is NOT the stream-111 gate and NOT hasGaugeClusterScreen (that predicate =
     * supportsDDPContent).  The 111 gate is the negotiated enabledFeatures altScreen flag + raw displays
     * count>=2 + UUID≠display0.  Kept because CarKit's physical-panel model still parses it. */
    {
        void *k   = cf_str(0, "properties", CFENC_UTF8);
        void *arr = cf_arrm(0, 1, arr_cb);
        if (arr) {
            if (cf_arrappc)
                properties_ok = (cf_arrappc(arr, "Instrument Cluster", -1) == 0);
            else {
                void *s = cf_str(0, "Instrument Cluster", CFENC_UTF8);
                if (s) {
                    cf_arrapp(arr, s);
                    properties_ok = 1;
                    if (cf_rel) cf_rel(s);
                }
            }
            if (k && properties_ok) cf_dictset(d, k, arr);
            if (cf_rel) cf_rel(arr);
        }
        if (k && cf_rel) cf_rel(k);
    }
    if (!properties_ok) goto fail;
    /* [0] fullscreen, [1] sportscreen (compact left), [2] classicscreen
     * (same compact size, centered). The encoded and visible frame stays 1440x542;
     * the entries carry different nested safe frames.  Match MHI3
     * AirPlayScreenDictSetViewAreas by declaring the initial current/adjacent
     * state alongside the array; these are display-level keys, not fields of
     * the individual view-area dictionaries. */
    {
        void *k   = cf_str(0, "viewAreas", CFENC_UTF8);
        void *arr = cf_arrm(0, 3, arr_cb);
        if (arr) {
            void *full = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                        FULL_SAFE_X, FULL_SAFE_Y,
                                        FULL_SAFE_W, FULL_SAFE_H);
            void *split = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                         SMALLSCREEN_SAFE_X, SMALLSCREEN_SAFE_Y,
                                         SMALLSCREEN_SAFE_W, SMALLSCREEN_SAFE_H);
            void *classic = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                           CLASSICSCREEN_SAFE_X, SMALLSCREEN_SAFE_Y,
                                           SMALLSCREEN_SAFE_W, SMALLSCREEN_SAFE_H);
            if (full && split && classic && k) {
                cf_arrapp(arr, full);
                cf_arrapp(arr, split);
                cf_arrapp(arr, classic);
                cf_dictset(d, k, arr);
                view_areas_ok = 1;
                LOG_INFO(LOG_MODULE,
                         "declared viewAreas: visible=(0,0 %dx%d); fullscreen=(%d,%d %dx%d); sportscreen=(%d,%d %dx%d); classicscreen=(%d,%d %dx%d)",
                         CLUSTER_W, CLUSTER_H,
                         FULL_SAFE_X, FULL_SAFE_Y, FULL_SAFE_W, FULL_SAFE_H,
                         SMALLSCREEN_SAFE_X, SMALLSCREEN_SAFE_Y, SMALLSCREEN_SAFE_W, SMALLSCREEN_SAFE_H,
                         CLASSICSCREEN_SAFE_X, SMALLSCREEN_SAFE_Y, SMALLSCREEN_SAFE_W, SMALLSCREEN_SAFE_H);
            }
            if (full && cf_rel) cf_rel(full);
            if (split && cf_rel) cf_rel(split);
            if (classic && cf_rel) cf_rel(classic);
            if (cf_rel) cf_rel(arr);
        }
        if (k && cf_rel) cf_rel(k);
    }
    if (!view_areas_ok || set_i64(d, "initialViewArea", 0) != 0) goto fail;
    {
        void *k = cf_str(0, "adjacentViewAreas", CFENC_UTF8);
        void *a = cf_arrm(0, 0, arr_cb);
        if (k && a) {
            cf_dictset(d, k, a);
            adjacent_ok = 1;
        }
        if (a && cf_rel) cf_rel(a);
        if (k && cf_rel) cf_rel(k);
    }
    if (!adjacent_ok) goto fail;
    /* Extra fields a real HU (MHI3 libairplay 450) sends in each display dict — our subset was
     * incomplete, which the phone accepts to ADD the car but rejects at SESSION start (0s teardown).
     *
     * Do NOT advertise a cluster primaryInputDevice here. The MHI3 alt-screen
     * dictionary helper omits both features and primaryInputDevice, and the live
     * virtual-knob experiment proved its HID reports control the main CarPlay UI,
     * not the instrument-cluster scene. Cluster map zoom is the UUID-addressed
     * changeMapZoomLevel session command below. */
    set_i64(d, "cornerMasks",        0);
    set_i64(d, "nightMode",          night ? 1 : 0);
    set_i64(d, "limitedUI",          0);
    {   /* limitedUIElements = [] */
        void *k = cf_str(0, "limitedUIElements", CFENC_UTF8);
        void *a = cf_arrm(0, 0, arr_cb);
        if (k && a) cf_dictset(d, k, a);
        if (a && cf_rel) cf_rel(a);
        if (k && cf_rel) cf_rel(k);
    }
    /* REQUIRED by CarKit (else display rejected → teardown loop) */
    {
        void *k   = cf_str(0, "initialVideoStreams", CFENC_UTF8);
        void *arr = build_stream_array("D1057E12-A0D1-4E20-B0C0-1057E12A0D11");
        if (k && arr) {
            cf_dictset(d, k, arr);
            initial_stream_ok = 1;
        }
        if (arr && cf_rel) cf_rel(arr);
        if (k && cf_rel) cf_rel(k);
    }
    if (!initial_stream_ok) goto fail;
    return d;

fail:
    LOG_ERROR(LOG_MODULE,
              "cluster display build incomplete — refusing altScreen advert");
    if (cf_rel) cf_rel(d);
    return 0;
}

/* stock CopyDisplaysInfo emits the OLD 210 schema (identifier key "uuid", NO initialVideoStreams);
 * iOS-26.1 CarKit needs "uid" + a present "initialVideoStreams" array or it rejects the display →
 * the phone teardown-loops the session.  Patch the stock main dict into the modern schema. */
static int main_disp_modernize(void *mainDisp, int night)
{
    if (!mainDisp) return -1;
    /* The stock dict already carries the REAL main display uuid (from
     * ScreenCopyMain via CopyDisplaysInfo — the SAME value
     * AirPlayReceiverSessionScreen_CopyDisplayUUID returns when the stock main
     * decoder matches the incoming 110 video stream).  iOS-26.1 CarKit routes
     * main video by the modern "uid"/"initialVideoStreams" uuid, so those MUST
     * equal the real main uuid; a fabricated uid (the old A0000000… constant)
     * routed main video to a stream the stock decoder never owned -> MAIN 110
     * BLACK.  Mirror how the cluster keeps uuid==uid==stream-uuid self-consistent
     * (RE-confirmed: iOS AirPlaySender fetch-once /info + MHI3 never fabricates). */
    void *realUuid = 0;
    {
        void *kuuid = cf_str(0, "uuid", CFENC_UTF8);
        if (kuuid && cf_getval) realUuid = cf_getval(mainDisp, kuuid);  /* borrowed */
        if (kuuid && cf_rel) cf_rel(kuuid);
    }
    if (realUuid) {
        void *kuid = cf_str(0, "uid", CFENC_UTF8);          /* uid = real main uuid */
        if (!kuid) {
            LOG_ERROR(LOG_MODULE,
                      "main display: uid key allocation failed — refusing partial advert");
            return -1;
        }
        cf_dictset(mainDisp, kuid, realUuid);
        if (cf_rel) cf_rel(kuid);
        LOG_INFO(LOG_MODULE, "main display: uid+stream = real ScreenCopyMain uuid (no fabrication)");
    } else {
        /* Never fabricate the main identity. Raw uuid, physical uid and
         * initialVideoStreams uuid must all name the stock type-110 screen. */
        LOG_ERROR(LOG_MODULE,
                  "main display: stock uuid missing — refusing altScreen advert");
        return -1;
    }
    /* What does stock actually claim for the main panel?  The whole density argument behind
     * CLUSTER_PHYS_W/H assumes stock reports a real physical size here; nothing in the firmware
     * configs states one (gal.json leaves physicalDisplayWidth at 0, "use the values provided by
     * the HMI"), so measure it instead of assuming.  If the physical pair comes back 0 the phone
     * has no density for display 0, and the cluster's numbers cannot be inconsistent with it -
     * which would make CLUSTER_PHYS_W/H merely truthful rather than the mechanism.
     * Needs `touch /mnt/app/carplay_verbose`: production runs at WARN. */
    if (cf_geti64) {
        static int logged;
        if (!logged) {
            logged = 1;
            int64_t v[4] = { 0, 0, 0, 0 };
            static const char *keys[4] =
                { "widthPixels", "heightPixels", "widthPhysical", "heightPhysical" };
            for (unsigned i = 0; i < 4; i++) {
                void *k = cf_str(0, keys[i], CFENC_UTF8);
                if (k) {
                    v[i] = cf_geti64(mainDisp, k, NULL);
                    if (cf_rel) cf_rel(k);
                }
            }
            LOG_INFO(LOG_MODULE,
                     "stock main panel: %lldx%lld px, physical %lldx%lld mm (0 = absent or zero)",
                     (long long)v[0], (long long)v[1], (long long)v[2], (long long)v[3]);
        }
    }
    /* complete the main dict like a real HU (MHI3 set) — see build_cluster_dict note */
    set_i64(mainDisp, "primaryInputDevice", 0);
    set_i64(mainDisp, "cornerMasks",        0);
    set_i64(mainDisp, "nightMode",          night ? 1 : 0);
    set_i64(mainDisp, "limitedUI",          0);
    {
        void *k = cf_str(0, "limitedUIElements", CFENC_UTF8);
        void *a = cf_arrm(0, 0, arr_cb);
        if (k && a) cf_dictset(mainDisp, k, a);
        if (a && cf_rel) cf_rel(a);
        if (k && cf_rel) cf_rel(k);
    }
    void *kivs = cf_str(0, "initialVideoStreams", CFENC_UTF8);
    void *ivs  = build_stream_array_cf(realUuid);
    if (kivs && ivs) cf_dictset(mainDisp, kivs, ivs);
    if (ivs && cf_rel) cf_rel(ivs);
    if (kivs && cf_rel) cf_rel(kivs);
    if (!kivs || !ivs) {
        LOG_ERROR(LOG_MODULE,
                  "main display: initialVideoStreams build failed — refusing altScreen advert");
        return -1;
    }
    return 0;
}

static void *build_displays(int night)
{
    int err = 0;
    void *mainDisp = fn_copydisp(0, &err);      /* arg ignored; builds via ScreenCopyMain */
    if (!mainDisp) {
        /* Without the stock main (display-0) the array degrades to just [cluster];
         * iOS's 110-vs-111 UUID compare and CarKit's display-0 assumptions then
         * reject the whole displays set and teardown-loop the session.  This can
         * happen on an early /info before graphics is up, so publish NEITHER key
         * this request (caller skips injection); a later /info retries. */
        LOG_ERROR(LOG_MODULE, "build_displays: stock main unavailable err=%d — skipping displays this /info", err);
        return 0;
    }
    if (main_disp_modernize(mainDisp, night) != 0) {
        if (cf_rel) cf_rel(mainDisp);
        return 0;
    }
    void *cluster  = build_cluster_dict(night);
    if (!cluster) {
        if (cf_rel) cf_rel(mainDisp);
        return 0;
    }
    void *arr = cf_arrm(0, 2, arr_cb);
    if (!arr) {
        if (cf_rel) cf_rel(mainDisp);
        if (cluster && cf_rel)  cf_rel(cluster);
        return 0;
    }
    cf_arrapp(arr, mainDisp); if (cf_rel) cf_rel(mainDisp);
    if (cluster)  { cf_arrapp(arr, cluster);  if (cf_rel) cf_rel(cluster);  }
    return arr;
}

static void advert_clear_locked(void)
{
    if (g_advert_displays && cf_rel) cf_rel(g_advert_displays);
    if (g_advert_features && cf_rel) cf_rel(g_advert_features);
    g_advert_displays = NULL;
    g_advert_features = NULL;
    g_advert_session = NULL;
    g_advert_ready = 0;
    g_advert_published = 0;
}

static void advert_clear_for_session(void *session)
{
    pthread_mutex_lock(&advert_lock);
    if (!session || g_advert_session == session) advert_clear_locked();
    pthread_mutex_unlock(&advert_lock);
}

/* ── the injector: interpose the stock /info builder ───────────────────────────
 *
 * WHY here and not AirPlayReceiverServerSetProperty(config): stock 210 libairplay
 * has NO `extendedFeatures` support at all (0 occurrences of the string in the
 * binary) — its AirPlayCopyServerInfo never reads config[extendedFeatures], so
 * storing the key in config is a no-op on stock (that path only works on the
 * 280/from-source lib).  Instead we interpose AirPlayCopyServerInfo itself and
 * add the key directly to the /info dict it returns, right before dio serializes
 * it into the HTTP /info response the phone reads.
 *
 * Signature (280 ref, Server.c): CFMutableDictionaryRef AirPlayCopyServerInfo(
 *   AirPlayReceiverSessionRef session, CFArrayRef qualifier, uint8_t *macAddr,
 *   OSStatus *outErr ).  The returned dict is mutable (CFDictionaryCreateMutable),
 *   so CFDictionarySetValue on it is safe. */
typedef void *(*fn_copyinfo)(void *, void *, void *, void *);
static int alt_resolve(void);

/* env gate by VALUE, not mere presence — `export FOO=0` must DISABLE, not enable
 * (a bare getenv()!=NULL is true even for "0"/"false", which silently kept altScreen
 * on when the launch wrapper set FOO=0 to turn it off). */
static int env_on(const char *name)
{
    const char *v = getenv(name);
    if (!v || !v[0]) return 0;
    if (v[0] == '0' && !v[1]) return 0;
    if (!strcmp(v, "false") || !strcmp(v, "no") || !strcmp(v, "off")) return 0;
    return 1;
}

/* Build and retain the exact display tree which the later /info will publish.
 * CopyDisplaysInfo ignores its session argument and asks stock ScreenCopyMain,
 * so this is the earliest safe point to prove that the negotiated contract can
 * actually be fulfilled. */
static int advert_preflight(void *session, uint32_t sequence)
{
    void *displays = NULL;
    void *features = NULL;
    if (!session || !env_on("ALTSCREEN_ADVERT_DISPLAYS") || disp_ok != 1)
        goto fail;
    displays = build_displays(g_alt_night_mode ? 1 : 0);
    if (!displays) goto fail;
    features = build_extfeatures();
    if (!features) goto fail;

    pthread_mutex_lock(&advert_lock);
    /* A slow control SETUP from the superseded RTSP session must never replace
     * the retained /info tree prepared by a newer SETUP. The sequence is
     * assigned at interpose entry, before stock work can reorder completions. */
    if ((int32_t)(sequence - g_advert_sequence) <= 0) {
        uint32_t ownerSequence = g_advert_sequence;
        pthread_mutex_unlock(&advert_lock);
        if (cf_rel) cf_rel(displays);
        if (cf_rel) cf_rel(features);
        LOG_INFO(LOG_MODULE,
                 "altScreen advert preflight ignored stale session=%p seq=%u owner=%u",
                 session, (unsigned)sequence, (unsigned)ownerSequence);
        return 0;
    }
    advert_clear_locked();
    g_advert_sequence = sequence;
    g_advert_session = session;
    g_advert_displays = displays;
    g_advert_features = features;
    g_advert_ready = 1;
    g_advert_published = 0;
    pthread_mutex_unlock(&advert_lock);
    LOG_INFO(LOG_MODULE,
             "altScreen advert preflight OK session=%p (real main uuid + 2 displays)",
             session);
    return 1;

fail:
    if (displays && cf_rel) cf_rel(displays);
    if (features && cf_rel) cf_rel(features);
    pthread_mutex_lock(&advert_lock);
    /* A newer failed preflight also retires an older cached contract. Otherwise
     * an even older SETUP which completes late could reclaim the advert owner. */
    if ((int32_t)(sequence - g_advert_sequence) > 0) {
        advert_clear_locked();
        g_advert_sequence = sequence;
    } else if (sequence == g_advert_sequence && g_advert_session == session) {
        advert_clear_locked();
    }
    pthread_mutex_unlock(&advert_lock);
    LOG_WARN(LOG_MODULE,
             "altScreen advert preflight FAILED session=%p disp_ok=%d gate=%d — feature not enabled",
             session, disp_ok, env_on("ALTSCREEN_ADVERT_DISPLAYS"));
    return 0;
}

static int advert_ready_for_session(void *session)
{
    int ready;
    pthread_mutex_lock(&advert_lock);
    ready = g_advert_ready && g_advert_session == session &&
            g_advert_displays != NULL && g_advert_features != NULL;
    pthread_mutex_unlock(&advert_lock);
    return ready;
}

void *AirPlayCopyServerInfo(void *session, void *qualifier, void *macAddr, void *outErr)
{
    static fn_copyinfo real;
    if (!real) real = (fn_copyinfo)dlsym(RTLD_NEXT, "AirPlayCopyServerInfo");
    if (!real) { LOG_ERROR(LOG_MODULE, "AirPlayCopyServerInfo real==NULL"); return 0; }

    void *info = real(session, qualifier, macAddr, outErr);   /* stock /info — untouched */

    /* Unconditional: this is the ONLY seam that proves the phone reached us over
     * the carplay0 IP link.  Everything below runs only after control SETUP, so a
     * generation that never starts CarPlay used to leave no trace at all here.
     * A failing generation WITH these lines means NCM/HTTP are up and the phone
     * read /info and then declined CarPlay (protocol/content — ours); a failing
     * generation WITHOUT them means the phone never reached us (cable/pairing). */
    {
        static int info_calls;
        LOG_INFO(LOG_MODULE, "/info #%d session=%p qualifier=%p out=%p",
                 ++info_calls, session, qualifier, info);
    }

    /* Advertise only when every primitive required by the self-contained 111
     * receiver resolves.  A partial advert would make iOS demand a contract the
     * stock receiver cannot satisfy and can restart the whole CarPlay session. */
    if (info && cf_init() == 1 && env_on("ALTSCREEN_HANDLE_ALT") &&
        alt_resolve() == 1 && advert_ready_for_session(session)) {
        /* Hold the retained contract stable until BOTH extendedFeatures and
         * display arrays have been inserted. Teardown may otherwise clear the
         * cache between the optimistic ready check and the second half of
         * /info, producing the exact partial advert this preflight prevents. */
        pthread_mutex_lock(&advert_lock);
        if (!g_advert_ready || g_advert_session != session ||
            !g_advert_displays || !g_advert_features) {
            pthread_mutex_unlock(&advert_lock);
            return info;
        }
        /* MU1316 dio's serverCopyPropertyCallback returns its startInNightMode
         * byte for the stock top-level /info key "nightMode" (dio 0x160484-
         * 0x1604C4).  This is the one hook-only seam that also covers a phone
         * connected while the HU is already in night mode: read it before
         * replacing displays, cache it for first-IDR replay, and advertise the
         * same value in both main and cluster panel dictionaries. */
        int nightErr = 0;
        int infoNight = 0;
        void *nightKey = cf_str(0, "nightMode", CFENC_UTF8);
        if (nightKey) {
            int64_t value = cf_geti64(info, nightKey, &nightErr);
            if (!nightErr) {
                infoNight = value ? 1 : 0;
                g_alt_night_mode = infoNight;
                LOG_INFO(LOG_MODULE,
                         "seeded cluster nightMode=%d from stock /info", infoNight);
            } else {
                infoNight = g_alt_night_mode ? 1 : 0;
                LOG_WARN(LOG_MODULE,
                         "stock /info nightMode unavailable err=%d; retaining cached=%d",
                         nightErr, infoNight);
            }
            if (cf_rel) cf_rel(nightKey);
        } else {
            infoNight = g_alt_night_mode ? 1 : 0;
        }

        /* Build all three keys before mutating the response, then commit the
         * complete feature+display shape together. */
        {
            /* iOS reads BOTH keys:
             *  - "displays"      → Activate.mm:822 = the display-COUNT gate (needs count vs altScreen-flag).
             *  - "displayPanels" → CARSessionConfiguration builds _displays (→ hasGaugeClusterScreen =
             *    _displays contains "Instrument Cluster") via initWithPhysicalScreenDictionary — which reads
             *    exactly our shape (uid/widthPixels/widthPhysical/properties/initialVideoStreams).
             * Providing only "displays" (LOGICAL-screen schema) leaves the cluster unrecognized → carkitd
             * never proposes "altScreen" → the 2-display count is rejected ("car won't add").
             *
             * Build the [main,cluster] array ONCE and publish it under both keys (cf_dictset retains, so a
             * single object under two keys is safe; the two arrays are byte-identical anyway).  This avoids
             * a second full CF-tree build AND a second stock ScreenCopyMain per /info (latency on the
             * negotiation path).  build_displays() returns NULL when the stock main (display-0) is
             * unavailable — publish NEITHER key then, rather than a display-0-less array that teardown-loops
             * the whole session. */
            void *dv;
            void *fv;
            void *fk;
            void *dk;
            void *pk;
            dv = (g_advert_ready && g_advert_session == session)
                ? g_advert_displays : NULL;
            fv = (g_advert_ready && g_advert_session == session)
                ? g_advert_features : NULL;
            fk = (dv && fv) ? cf_str(0, "extendedFeatures", CFENC_UTF8) : 0;
            dk = dv ? cf_str(0, "displays", CFENC_UTF8) : 0;
            pk = dv ? cf_str(0, "displayPanels", CFENC_UTF8) : 0;
            if (dv && fv && fk && dk && pk) {
                /* Mutate the retained array only before its first publication;
                 * subsequent /info dictionaries may still retain it. Runtime
                 * night changes are carried by the generation-scoped command. */
                if (!g_advert_published) advert_apply_night_locked(infoNight);
                cf_dictset(info, fk, fv);
                cf_dictset(info, dk, dv);
                cf_dictset(info, pk, dv);
                g_advert_published = 1;
                LOG_INFO(LOG_MODULE,
                         "injected atomic extendedFeatures+displays+displayPanels contract into /info=%p",
                         info);
            } else {
                LOG_ERROR(LOG_MODULE,
                          "atomic /info contract skipped fv=%p dv=%p fk=%p dk=%p pk=%p",
                          fv, dv, fk, dk, pk);
            }
            if (fk && cf_rel) cf_rel(fk);
            if (dk && cf_rel) cf_rel(dk);
            if (pk && cf_rel) cf_rel(pk);
        }
        pthread_mutex_unlock(&advert_lock);
    }
    return info;
}

/* ════════════════════════════════════════════════════════════════════════════
 * #2 handle_alt — accept + bring up the altScreen 111 stream.
 *
 * Self-contained receiver built from stock libairplay 210.81 public primitives:
 * the main session AES key + streamConnectionID derivation, ServerSocketOpen,
 * SocketAccept and AES-CTR.  It deliberately creates no stock ScreenStream or
 * AirPlayReceiverSessionScreen object, so stream 111 cannot collide with the
 * stock type-110 Screen/OMX pipeline.
 *
 * Gated by env ALTSCREEN_HANDLE_ALT (dark by default).  Fail-safe: if any stock
 * symbol is missing, handle_alt is skipped and /info advertise still works.
 * ════════════════════════════════════════════════════════════════════════════ */

/* stock session struct offsets (bytes, MU1316 210.81) */
#define OFF_SERVER          12    /* session+12  = server*                        */
#define OFF_FAMILY          85    /* session+85  = peerAddr sa_family (u8)         */
#define OFF_AESKEY          456   /* session+456 = aesSessionKey                  */
#define SVR_OFF_QOSDISABLED 452   /* server+452  = qosDisabled (u8)               */
#define ALT_STREAM_TYPE     111

/* Exported stock function pointers (resolved lazily via RTLD_DEFAULT).
 * The convenience AirPlayReceiverSessionForceKeyFrame export is intentionally
 * not used: stock 210 omits params.uuid, so iOS targets stream 110. */
/* stock AES-CTR exports (self-contained receiver decrypts opcode-0 bodies itself, no stock ScreenStream) */
typedef int      (*fn_aes_ctr_init_t)(void *ctx, const uint8_t *key, const uint8_t *iv);        /* AES_CTR_Init  */
typedef int      (*fn_aes_ctr_update_t)(void *ctx, const uint8_t *src, uint32_t len, uint8_t *dst); /* AES_CTR_Update */
typedef int      (*fn_aes_ctr_final_t)(void *ctx);                                              /* AES_CTR_Final */
typedef int      (*fn_sess_sendcmd_t)(void *, void *, void *, void *); /* AirPlayReceiverSessionSendCommand(session, cmdDict{type,params}, completion, ctx) */
typedef void     (*fn_derive_aes_t)(const uint8_t *, int, uint64_t, uint8_t *, uint8_t *);
typedef int      (*fn_srv_sockopen_t)(int, int, int, int, int *, int, int *);
typedef void     (*fn_sock_setqos_t)(int, int);
typedef int      (*fn_sock_accept_t)(int, int, int *, void *);
typedef long     (*fn_cfarr_count_t)(void *);
typedef void *   (*fn_cfarr_getidx_t)(void *, long);

static fn_aes_ctr_init_t    aes_ctr_init;
static fn_aes_ctr_update_t  aes_ctr_update;
static fn_aes_ctr_final_t   aes_ctr_final;
static fn_sess_sendcmd_t    sess_sendcmd;   /* session-level {type,params} command shipper (zone switch) */
/* Receiver-owned session commands carry `gen`: the transport generation of the receiver
 * issuing them. gen>0 is dropped unless it still equals g_alt_session_gen, so neither an
 * immediate command nor a delayed async completion from a superseded stream-111 receiver
 * can command the NEW session. Java/BAP callers resolve gen=0 to the current generation
 * before any command whose completion can trigger follow-up work. */
static void alt_kick_viewarea(uint32_t gen);
static void alt_kick_show_ui(uint32_t gen);
static int  alt_show_ui_for_mode(uint32_t gen, int mode, const char *reason);
static int  alt_send_cluster_night_mode(uint32_t gen, int night, const char *reason);
static int  alt_request_cluster_idr(uint32_t gen, const char *reason);
static uint32_t alt_generation_for_session(void *session);
static int  alt_generation_is_current(uint32_t gen);
static fn_derive_aes_t      derive_aes;

/* Main AirPlay session belonging to the newest committed altScreen transport claim;
 * cleared on teardown. Read by CMD_ALT_ZONE to target AirPlayReceiverSessionSendCommand.
 * Single altScreen session assumed; all accesses that can race teardown use alt_lock. */
static void *volatile g_alt_session;
/* Transport generation that owns g_alt_session (set + cleared with it under alt_lock).
 * A receiver-owned command shipped with gen>0 is dropped unless gen==g_alt_session_gen. */
static uint32_t g_alt_session_gen;
static fn_srv_sockopen_t    srv_sockopen;
static fn_sock_setqos_t     sock_setqos;
static fn_sock_accept_t     sock_accept;
static fn_cfarr_count_t     cf_arrcount;
static fn_cfarr_getidx_t    cf_arridx;

/* advert_lock is held. Keep the retained display tree consistent with the
 * authoritative top-level nightMode from the same /info response. */
static void advert_apply_night_locked(int night)
{
    long i;
    if (!g_advert_displays || !cf_arrcount || !cf_arridx) return;
    for (i = 0; i < cf_arrcount(g_advert_displays); ++i) {
        void *display = cf_arridx(g_advert_displays, i);
        if (display) set_i64(display, "nightMode", night ? 1 : 0);
    }
}

static int alt_enabled = -1;    /* env ALTSCREEN_HANDLE_ALT */
static int alt_syms_ok = -1;
static pthread_mutex_t alt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t alt_cond = PTHREAD_COND_INITIALIZER;

/* All hook-originated AirPlay /command traffic is serialized off stock dio,
 * receiver and HTTP-completion threads.  SendCommand eventually enters
 * HTTPClientSendMessage and has no caller-visible timeout on stock 210; even
 * without alt_lock held, calling it from dio's SetNightMode boundary could
 * suppress the SI heartbeat.  Queue entries retain an immutable command and
 * the exact committed stream generation. */
#define ALT_CMD_QUEUE_CAPACITY 32
#define ALT_CMD_SHUTDOWN_WAIT_MS 750
typedef struct {
    uint32_t gen;
    void *cmd;
    void *completion;
    void *ctx;
} alt_cmd_item_t;
static pthread_mutex_t g_alt_cmd_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_alt_cmd_cond = PTHREAD_COND_INITIALIZER;
static pthread_t       g_alt_cmd_thread;
static alt_cmd_item_t  g_alt_cmd_queue[ALT_CMD_QUEUE_CAPACITY];
static unsigned int    g_alt_cmd_head;
static unsigned int    g_alt_cmd_tail;
static unsigned int    g_alt_cmd_count;
static int             g_alt_cmd_created;
static int             g_alt_cmd_shutdown;
static int             g_alt_cmd_exited;

static int alt_resolve(void)
{
    pthread_mutex_lock(&alt_lock);   /* no lock-free fast path — see cf_init note */
    /* Retry until ready (!=1), not just once (<0): same startup-race reasoning as
     * cf_init — never latch a transient miss as "altScreen off for this boot". */
    if (alt_syms_ok != 1) {
        if (cf_init() != 1) { alt_syms_ok = 0; pthread_mutex_unlock(&alt_lock); return 0; }
        aes_ctr_init    = (fn_aes_ctr_init_t)   dlsym(RTLD_DEFAULT, "AES_CTR_Init");
        aes_ctr_update  = (fn_aes_ctr_update_t) dlsym(RTLD_DEFAULT, "AES_CTR_Update");
        aes_ctr_final   = (fn_aes_ctr_final_t)  dlsym(RTLD_DEFAULT, "AES_CTR_Final");
        sess_sendcmd    = (fn_sess_sendcmd_t)   dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionSendCommand");
        derive_aes      = (fn_derive_aes_t)     dlsym(RTLD_DEFAULT, "AirPlay_DeriveAESKeySHA512ForScreen");
        srv_sockopen    = (fn_srv_sockopen_t)   dlsym(RTLD_DEFAULT, "ServerSocketOpen");
        sock_setqos     = (fn_sock_setqos_t)    dlsym(RTLD_DEFAULT, "SocketSetQoS");
        sock_accept     = (fn_sock_accept_t)    dlsym(RTLD_DEFAULT, "SocketAccept");
        cf_getval       = (fn_cfdict_getval_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
        cf_arrcount     = (fn_cfarr_count_t)    dlsym(RTLD_DEFAULT, "CFArrayGetCount");
        cf_arridx       = (fn_cfarr_getidx_t)   dlsym(RTLD_DEFAULT, "CFArrayGetValueAtIndex");
        /* Only symbols used by the live self-contained 111 path are critical. */
        alt_syms_ok = (sess_sendcmd && derive_aes &&
                       srv_sockopen && sock_accept &&
                       aes_ctr_init && aes_ctr_update && aes_ctr_final &&
                       cf_geti64 && cf_getval && cf_arrcount && cf_arridx &&
                       cf_dictm && cf_dictset && cf_dictseti && cf_arrapp) ? 1 : 0;
        LOG_INFO(LOG_MODULE, "alt_resolve syms_ok=%d", alt_syms_ok);
    }
    pthread_mutex_unlock(&alt_lock);
    return alt_syms_ok;
}

/* hook-side alt-screen state (stock has no altScreen slots) */
typedef struct {
    void       *session;     /* key */
    int         altSock;     /* listen socket */
    pthread_t   thread;
    int         threadCreated; /* pthread is joinable until one owner reaps it */
    int         threadExited;  /* receiver returned; safe for SETUP to reap */
    int         threadReaping; /* exactly one SETUP/TEARDOWN owns pthread_join */
    int         settingUp;
    int         closing;
    uint32_t    generation;
    int         inUse;
    uint8_t     key[16];     /* self-contained receiver: AES-CTR key ...  */
    uint8_t     iv[16];      /* ... + iv (derived in alt_setup)           */
    volatile int stop;       /* teardown sets =1 → recv poll-loop exits (unblocks join) */
} altslot_t;
typedef struct {
    altslot_t *slot;
    uint32_t generation;   /* per-slot generation (slot-state reuse guard) */
    uint32_t av_gen;       /* global transport-owner generation (altav ownership) */
    int listenSock;
} altthread_arg_t;
#define ALT_MAX_SESSIONS 4
/* R2: bounded wait for a concurrent teardown to finish clearing this session's slot
 * before a new SETUP gives up. Fast-replug pointer reuse can otherwise hit a slot
 * mid-close and fail the whole control SETUP. */
#define ALT_SETUP_CLOSE_WAIT_MS 500
/* A SETUP/TEARDOWN must never block forever in pthread_join.  The receiver
 * checks stop every 60 ms and renderer forwarding has a 1000 ms stall cap;
 * 1800 ms covers both while staying far below AirPlay's control-SETUP timeout. */
#define ALT_REAP_WAIT_MS 1800
/* The setup transaction contains one bounded old-receiver reap.  TearDown must
 * not wait forever if a stock CF/socket primitive nevertheless wedges. */
#define ALT_TEARDOWN_SETUP_WAIT_MS 2500
static altslot_t alt_slots[ALT_MAX_SESSIONS];
/* Monotonic transport-owner generation, assigned per successfully-created receiver
 * (guarded by alt_lock).  Distinct from the per-slot generation: it is GLOBAL so a
 * later receiver on any slot supersedes every earlier one as the single altav owner. */
static uint32_t g_alt_gen_counter = 0;

static altslot_t *alt_slot_get(void *session)   /* find or alloc (caller holds alt_lock) */
{
    int i, free_i = -1;
    for (i = 0; i < ALT_MAX_SESSIONS; i++) {
        if (alt_slots[i].inUse && alt_slots[i].session == session) return &alt_slots[i];
        if (!alt_slots[i].inUse && free_i < 0) free_i = i;
    }
    if (free_i < 0) return NULL;
    memset(&alt_slots[free_i], 0, sizeof(altslot_t));
    alt_slots[free_i].session = session;
    alt_slots[free_i].altSock = -1;
    alt_slots[free_i].inUse = 1;
    return &alt_slots[free_i];
}
static altslot_t *alt_slot_find(void *session)
{
    int i;
    for (i = 0; i < ALT_MAX_SESSIONS; i++)
        if (alt_slots[i].inUse && alt_slots[i].session == session) return &alt_slots[i];
    return NULL;
}

static void alt_deadline_after_ms(struct timespec *deadline, long ms);

/* Claim and reap exactly one receiver generation.  A live generation is asked
 * to stop and its listener is closed; collection is allowed only after the
 * bounded terminal-flag wait below, then join runs outside alt_lock. */
static void alt_deadline_after_ms(struct timespec *deadline, long ms)
{
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_sec += ms / 1000L;
    deadline->tv_nsec += (ms % 1000L) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

static int alt_reap_thread(altslot_t *slot, int requestStop)
{
    pthread_t thread;
    uint32_t generation;
    int listener = -1;
    int wait_rc = 0;
    struct timespec deadline;

    if (!slot) return 0;
    alt_deadline_after_ms(&deadline, ALT_REAP_WAIT_MS);
    pthread_mutex_lock(&alt_lock);
    while (slot->threadReaping && wait_rc != ETIMEDOUT)
        wait_rc = pthread_cond_timedwait(&alt_cond, &alt_lock, &deadline);
    if (slot->threadReaping) {
        pthread_mutex_unlock(&alt_lock);
        LOG_WARN(LOG_MODULE, "alt: reap owner wait timed out session=%p", slot->session);
        return -1;
    }
    if (!slot->threadCreated) {
        if (requestStop) {
            slot->stop = 1;
            listener = slot->altSock;
            slot->altSock = -1;
        }
        pthread_mutex_unlock(&alt_lock);
        if (listener >= 0) close(listener);
        return 0;
    }
    if (!requestStop && !slot->threadExited) {
        pthread_mutex_unlock(&alt_lock);
        return -1;
    }
    if (requestStop) slot->stop = 1;
    listener = slot->altSock;
    slot->altSock = -1;
    slot->threadReaping = 1;
    thread = slot->thread;
    generation = slot->generation;
    pthread_mutex_unlock(&alt_lock);

    if (listener >= 0) close(listener);

    /* pthread_join itself has no portable timeout on QNX 6.5.  Wait for the
     * receiver's terminal flag first; once set, join can only collect a thread
     * that is already at its final unlock/return. */
    pthread_mutex_lock(&alt_lock);
    wait_rc = 0;
    while (slot->generation == generation && slot->threadCreated &&
           !slot->threadExited && wait_rc != ETIMEDOUT)
        wait_rc = pthread_cond_timedwait(&alt_cond, &alt_lock, &deadline);
    if (slot->generation == generation && slot->threadCreated && !slot->threadExited) {
        slot->threadReaping = 0;
        pthread_cond_broadcast(&alt_cond);
        pthread_mutex_unlock(&alt_lock);
        LOG_WARN(LOG_MODULE,
                 "alt: receiver stop timed out after %d ms session=%p gen=%u; cleanup deferred",
                 ALT_REAP_WAIT_MS, slot->session, (unsigned)generation);
        return -1;
    }
    pthread_mutex_unlock(&alt_lock);

    pthread_join(thread, NULL);

    pthread_mutex_lock(&alt_lock);
    if (slot->generation == generation) {
        slot->threadCreated = 0;
        slot->threadExited = 0;
    }
    slot->threadReaping = 0;
    pthread_cond_broadcast(&alt_cond);
    pthread_mutex_unlock(&alt_lock);
    return 0;
}

/* Reclaim slots whose receiver has already EXITED (a superseded different-slot stream on
 * fast reconnect) but whose stock TearDown never arrived.  Without this a few missed
 * teardowns fill all ALT_MAX_SESSIONS and every later SETUP fails with -6728.  Caller must
 * NOT hold alt_lock.  Only a fully-idle exited slot that is neither the current session nor
 * mid-lifecycle is reclaimed. */
static void alt_gc_exited_slots(void)
{
    int i;
    for (i = 0; i < ALT_MAX_SESSIONS; i++) {
        altslot_t *slot = &alt_slots[i];
        int eligible = 0;
        pthread_mutex_lock(&alt_lock);
        if (slot->inUse && slot->threadCreated && slot->threadExited &&
            !slot->threadReaping && !slot->settingUp &&
            slot->session != g_alt_session)
            eligible = 1;
        pthread_mutex_unlock(&alt_lock);
        if (!eligible) continue;
        alt_reap_thread(slot, 1);   /* join the already-exited thread (fast) */
        pthread_mutex_lock(&alt_lock);
        if (slot->inUse && !slot->threadCreated && !slot->settingUp &&
            slot->session != g_alt_session) {
            LOG_INFO(LOG_MODULE, "alt: reclaimed orphaned exited slot session=%p", slot->session);
            memset(slot, 0, sizeof(*slot));
        }
        pthread_mutex_unlock(&alt_lock);
    }
}

/* _AddResponseStream replica (stock sub_26EFC): append desc to resp["streams"] */
static int alt_add_response_stream(void *responseParams, void *streamDesc)
{
    void *kstreams = cf_str(0, "streams", CFENC_UTF8);
    int added = 0;
    if (!kstreams) return -1;
    void *arr = cf_getval(responseParams, kstreams);
    if (arr) {
        cf_arrapp(arr, streamDesc);
        added = 1;
    } else {
        arr = cf_arrm(0, 0, arr_cb);
        if (arr) {
            cf_arrapp(arr, streamDesc);
            cf_dictset(responseParams, kstreams, arr);
            if (cf_rel) cf_rel(arr);
            added = 1;
        }
    }
    if (cf_rel) cf_rel(kstreams);
    return added ? 0 : -1;
}

/* ── self-contained 111 receiver ─────────────────────────────────────────────
 * Read the phone's encrypted TCP stream (our own socket from alt_setup), AES-CTR
 * decrypt the video bodies, convert AVCC→Annex-B, forward to altscreen_render.
 * We do NOT hand the socket to a stock ScreenStream: that path binds the MAIN
 * display and creates a second in-process OMX decoder -> black main +
 * OMX FreeBufferUnexpected crash (verified on-unit).  Wire = 128-byte screen header
 * + body, matching stock ProcessFrames (sub_2b154 reads 128 @0x2b450, body @0x2b510):
 *   hdr[0..3]=bodySize(LE), hdr[4]=opcode.  opcode 1 = unencrypted avcC config;
 *   opcode 0 = AES-CTR-encrypted AVCC access unit; 2/4/5 = ignore. */
/* poll-then-read: the accepted socket is non-blocking, and teardown cannot close this
 * thread-local fd, so we poll with a 500ms tick and bail when *stop is set (teardown) —
 * that unblocks the pthread_join in the teardown interpose. */
static int read_exact(int fd, uint8_t *buf, uint32_t n, volatile int *stop)
{
    uint32_t got = 0;
    while (got < n) {
        if (stop && *stop) return -1;
        struct pollfd pfd; pfd.fd = fd; pfd.events = POLLIN; pfd.revents = 0;
        int pr = poll(&pfd, 1, 60);   /* short tick so teardown stop is noticed in <=60ms
                                       * (fast reconnect); during streaming poll returns on
                                       * data anyway, so the idle-only tick cost is trivial */
        if (pr == 0) continue;                            /* tick → re-check stop */
        if (pr < 0) { if (errno == EINTR) continue; return -1; }
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) return -1;
        ssize_t r = read(fd, buf + got, n - got);
        if (r > 0) { got += (uint32_t)r; continue; }
        if (r == 0) return -1;                            /* peer closed */
        if (r < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        return -1;
    }
    return 0;
}

/* avcC config box → Annex-B SPS/PPS (into out); returns the AVCC NAL length-size (1..4). */
static uint8_t avcc_config_to_annexb(const uint8_t *a, uint32_t len,
                                     uint8_t *out, uint32_t outcap, uint32_t *outlen)
{
    uint32_t o = 0, i;
    int n, count;
    *outlen = 0;
    if (len < 7) return 4;
    uint8_t ls = (uint8_t)((a[4] & 3) + 1);
    i = 5;
    count = a[i++] & 0x1F;                                 /* numSPS */
    for (n = 0; n < count && i + 2 <= len; n++) {
        uint32_t l = ((uint32_t)a[i] << 8) | a[i + 1]; i += 2;
        if (l == 0 || i + l > len) break;
        if (o + 4 + l <= outcap) {
            out[o]=0; out[o+1]=0; out[o+2]=0; out[o+3]=1;
            memcpy(out + o + 4, a + i, l); o += 4 + l;
        }
        i += l;
    }
    if (i < len) {
        count = a[i++];                                   /* numPPS */
        for (n = 0; n < count && i + 2 <= len; n++) {
            uint32_t l = ((uint32_t)a[i] << 8) | a[i + 1]; i += 2;
            if (l == 0 || i + l > len) break;
            if (o + 4 + l <= outcap) {
                out[o]=0; out[o+1]=0; out[o+2]=0; out[o+3]=1;
                memcpy(out + o + 4, a + i, l); o += 4 + l;
            }
            i += l;
        }
    }
    *outlen = o;
    return ls;
}

/* Return non-zero when any NAL in this AVCC access unit has the requested type.
 * Do not inspect only the first NAL: an IDR access unit may begin with AUD/SEI.
 * Length parsing mirrors avcc_to_annexb and never reads beyond the decrypted body. */
static int avcc_has_nal_type(const uint8_t *a, uint32_t len, uint8_t ls,
                             uint8_t wanted)
{
    uint32_t i = 0;
    if (!a || ls < 1 || ls > 4) return 0;
    while (len - i >= ls) {
        uint32_t nal = 0, k;
        for (k = 0; k < ls; ++k) nal = (nal << 8) | a[i + k];
        i += ls;
        if (nal == 0 || nal > len - i) return 0;
        if ((a[i] & 0x1f) == wanted) return 1;
        i += nal;
    }
    return 0;
}

#define ALT_HDR_SIZE  128
#define ALT_MAX_BODY  (4u * 1024 * 1024)
#define ALT_IDR_RETRY_MS       2000ULL
#define ALT_IDR_MAX_REQUESTS   6U

static uint64_t alt_now_ms(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return 0;
    return ((uint64_t)tv.tv_sec * 1000ULL) + ((uint64_t)tv.tv_usec / 1000ULL);
}

static void alt_recv_loop(altslot_t *slot, int fd, uint32_t av_gen)
{
    uint8_t  aesctx[512] __attribute__((aligned(16)));
    int      aes_ready = 0;
    uint8_t  ls = 4;
    uint8_t  hdr[ALT_HDR_SIZE];
    uint8_t  cfg[4096];
    uint8_t *body = NULL;
    uint32_t bodycap = 0;
    unsigned long nframes = 0;
    int stream_has_idr = 0;
    int renderer_wait_idr = 1;
    unsigned int idr_requests = 0;
    int idr_limit_logged = 0;
    uint64_t last_idr_request_ms = 0;

    if (aes_ctr_init) { aes_ctr_init(aesctx, slot->key, slot->iv); aes_ready = 1; }

    /* A decoder cannot start mid-GOP. Ask iOS for a cluster-targeted IDR over
     * the generic command channel. Retries below are deliberately slow and
     * bounded: stock 210 owns the asynchronous control queue, so an unbounded
     * frame-count retry can exhaust dio_manager when iOS never answers. */
    alt_request_cluster_idr(av_gen, "stream-start");
    idr_requests = 1;
    last_idr_request_ms = alt_now_ms();

    for (;;) {
        /* A later receiver (fast reconnect) may have claimed the single renderer
         * transport.  Once superseded, exit BEFORE reading/forwarding/commanding so
         * this stream can neither touch the new owner's connection nor send control
         * commands (ForceKeyFrame/showUI) into the new session. */
        if (!altav_is_owner(av_gen)) {
            LOG_INFO(LOG_MODULE, "alt recv: superseded (gen=%u no longer owner) — exiting", (unsigned)av_gen);
            break;
        }
        if (read_exact(fd, hdr, ALT_HDR_SIZE, &slot->stop) != 0) {
            LOG_INFO(LOG_MODULE,
                     "alt recv: header read ended stop=%d errno=%d after %lu frames",
                     slot->stop, errno, nframes);
            break;
        }
        uint32_t bodySize = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                            ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
        uint8_t opcode = hdr[4];
        if (bodySize > ALT_MAX_BODY) {
            LOG_ERROR(LOG_MODULE, "alt recv: invalid bodySize=%u opcode=%u", bodySize, opcode);
            break;                                         /* fail-closed on garbage length */
        }
        if (bodySize > bodycap) {
            uint8_t *nb = (uint8_t *)realloc(body, bodySize);
            if (!nb) break;
            body = nb; bodycap = bodySize;
        }
        if (bodySize && read_exact(fd, body, bodySize, &slot->stop) != 0) {
            LOG_INFO(LOG_MODULE,
                     "alt recv: body read ended opcode=%u body=%u stop=%d errno=%d after %lu frames",
                     opcode, bodySize, slot->stop, errno, nframes);
            break;
        }

        if (opcode == 1) {                                /* avcC config (NOT encrypted) */
            uint32_t clen = 0;
            float cfgWidth = 0.0f, cfgHeight = 0.0f;
            char avccHex[129];
            uint32_t hi, hn = (bodySize < 64U) ? bodySize : 64U;

            /* AirPlayScreenHeader.params[1].f32[0..1] are the sender's actual
             * encoded width/height (header offsets 16 and 20). */
            memcpy(&cfgWidth,  hdr + 16, sizeof(cfgWidth));
            memcpy(&cfgHeight, hdr + 20, sizeof(cfgHeight));
            for (hi = 0; hi < hn; ++hi)
                snprintf(avccHex + (hi * 2), sizeof(avccHex) - (hi * 2), "%02x", body[hi]);
            avccHex[hn * 2] = 0;

            ls = avcc_config_to_annexb(body, bodySize, cfg, sizeof(cfg), &clen);
            if (clen) altav_set_config(av_gen, cfg, clen);   /* cache; replayed before first safe IDR */
            LOG_INFO(LOG_MODULE,
                     "alt recv: avcC config encoded=%dx%d body=%u lengthSize=%u annexb=%u bytes=%s%s",
                     (int)(cfgWidth + 0.5f), (int)(cfgHeight + 0.5f), bodySize, ls, clen,
                     avccHex, (bodySize > hn) ? "..." : "");
        } else if (opcode == 0) {                         /* encrypted AVCC video access unit */
            int is_idr;
            if (aes_ready) aes_ctr_update(aesctx, body, bodySize, body);
            is_idr = avcc_has_nal_type(body, bodySize, ls, 5);
            /* Re-check ownership here: the header/body read above can block in poll for
             * up to 500 ms, during which a reconnect may have superseded us.  The control
             * commands below target g_alt_session and must never run for a stale stream. */
            if (!altav_is_owner(av_gen)) break;
            if (is_idr && !stream_has_idr) {
                stream_has_idr = 1;
                LOG_INFO(LOG_MODULE,
                         "alt recv: first IDR arrived at frame#%lu (body=%u)",
                         nframes, bodySize);
                /* A live video stream is only the canvas.  Select the Maps instrument-cluster
                 * UI explicitly, then tell the encoder which predeclared view area is visible. */
                alt_kick_show_ui(av_gen);
                /* Never call back through the live stock session from this
                 * receiver thread: teardown may free it after its bounded reap.
                 * /info and dio's SetNightMode boundary keep this cache current. */
                alt_send_cluster_night_mode(av_gen, g_alt_night_mode, "first-IDR");
                alt_kick_viewarea(av_gen);
            }

            if (is_idr && renderer_wait_idr) {
                renderer_wait_idr = 0;
                LOG_INFO(LOG_MODULE,
                         "alt recv: WAIT_IDR gate OPEN at frame#%lu body=%u — decoder may resume",
                         nframes, bodySize);
            }
            if (nframes < 6 && bodySize >= 8)
                LOG_INFO(LOG_MODULE, "alt recv frame#%lu body=%u dec[0..7]=%02x %02x %02x %02x %02x %02x %02x %02x nalhdr@ls=0x%02x",
                         nframes, bodySize, body[0],body[1],body[2],body[3],body[4],body[5],body[6],body[7],
                         (bodySize > ls) ? body[ls] : 0);

            /* A fresh OMX decoder must never see a P-frame before an IDR. If the
             * localhost link failed, leave it disconnected and drop decrypted
             * P-frames until iOS answers the cluster-targeted keyframe request.
             * The first IDR then reconnects, replays cached SPS/PPS, and opens
             * decoding with a valid random-access picture. */
            if (!renderer_wait_idr) {
                int forward_rc = altav_forward_avcc_frame(av_gen, body, bodySize, ls);
                if (forward_rc < 0) {
                    renderer_wait_idr = 1;
                    alt_request_cluster_idr(av_gen, "renderer-link-lost");
                    idr_requests = 1;
                    idr_limit_logged = 0;
                    last_idr_request_ms = alt_now_ms();
                    LOG_INFO(LOG_MODULE,
                             "alt recv: renderer link lost — WAIT_IDR gate CLOSED");
                } else if (forward_rc > 0) {
                    LOG_INFO(LOG_MODULE,
                             "alt recv: renderer connected on IDR-safe access unit");
                }
            } else if ((nframes & 0x7f) == 0) {
                LOG_INFO(LOG_MODULE,
                         "alt recv: dropping frame pending fresh IDR (frame#%lu body=%u)",
                         nframes, bodySize);
            }
            ++nframes;
            if (renderer_wait_idr) {
                uint64_t now_ms = alt_now_ms();
                if (idr_requests < ALT_IDR_MAX_REQUESTS && now_ms != 0 &&
                    (last_idr_request_ms == 0 ||
                     now_ms - last_idr_request_ms >= ALT_IDR_RETRY_MS)) {
                    alt_request_cluster_idr(av_gen, "WAIT_IDR-retry");
                    ++idr_requests;
                    last_idr_request_ms = now_ms;
                } else if (idr_requests >= ALT_IDR_MAX_REQUESTS &&
                           !idr_limit_logged) {
                    idr_limit_logged = 1;
                    LOG_ERROR(LOG_MODULE,
                              "alt recv: WAIT_IDR request limit reached (%u); dropping P-frames without flooding control queue",
                              idr_requests);
                }
            }
            if ((nframes & 0xFF) == 0)
                LOG_INFO(LOG_MODULE,
                         "alt recv: %lu video frames received (last body=%u waitIDR=%d)",
                         nframes, bodySize, renderer_wait_idr);
        }
        /* opcodes 2/4/5 (keepalive/misc): body already consumed → ignore */
    }

    if (aes_ready && aes_ctr_final) aes_ctr_final(aesctx);
    free(body);
}

/* receiver thread — self-contained (own socket read; NO stock ScreenStream) */
static void *alt_thread_main(void *arg)
{
    altthread_arg_t *threadArg = (altthread_arg_t *)arg;
    altslot_t *slot = threadArg->slot;
    uint32_t generation = threadArg->generation;
    uint32_t av_gen = threadArg->av_gen;
    int        listenSock = threadArg->listenSock;
    int        fd = -1;
    int        ls;
    free(threadArg);

    /* Use a finite accept tick.  Cross-thread close normally wakes QNX
     * SocketAccept, but this loop does not depend on that undocumented detail:
     * teardown's stop flag is observed within one second even if close alone
     * leaves the wrapper asleep.  Exited slots are collected by SETUP GC. */
    while (!slot->stop) {
        fd = -1;
        if (sock_accept(listenSock, 1, &fd, NULL) == 0 && fd >= 0) break;
    }
    if (fd < 0 || slot->stop) goto done;
    /* Do not rely on undocumented SocketAccept flags.  read_exact() polls first,
     * but an explicit non-blocking fd also closes the tiny poll->read race and
     * guarantees teardown responsiveness on QNX io-pkt. */
    {
        int fdFlags = fcntl(fd, F_GETFL, 0);
        if (fdFlags >= 0 && fcntl(fd, F_SETFL, fdFlags | O_NONBLOCK) != 0)
            LOG_WARN(LOG_MODULE, "alt: failed to set accepted fd non-blocking errno=%d", errno);
    }
    /* claim + close the listen socket (teardown may race to close it too — the
     * -1 sentinel under alt_lock ensures exactly one close). */
    pthread_mutex_lock(&alt_lock);
    if (slot->generation == generation && slot->altSock == listenSock) {
        ls = slot->altSock;
        slot->altSock = -1;
        pthread_cond_broadcast(&alt_cond);
    } else {
        ls = -1;
    }
    pthread_mutex_unlock(&alt_lock);
    if (ls >= 0) close(ls);

    LOG_INFO(LOG_MODULE, "alt-screen receiver started (self-contained)");
    alt_recv_loop(slot, fd, av_gen);

done:
    if (fd >= 0) close(fd);
    /* Only close the renderer transport if WE still own it — a superseded receiver's
     * exit must not tear down the new owner's connection (that caused black 111 on
     * fast reconnect). */
    altav_shutdown_if_owner(av_gen);
    /* This log is intentionally before threadExited.  Once the terminal flag
     * is published, the reaper may call pthread_join and must not wait behind
     * a potentially blocking logger write. */
    LOG_INFO(LOG_MODULE, "alt-screen receiver thread exit generation=%u",
             (unsigned)generation);
    pthread_mutex_lock(&alt_lock);
    if (slot->generation == generation && slot->threadCreated) {
        slot->threadExited = 1;
        pthread_cond_broadcast(&alt_cond);
    }
    pthread_mutex_unlock(&alt_lock);
    return NULL;
}

/* SETUP response and transport for stream type 111.  No stock Screen object is
 * created: the hook owns the socket and decrypts the AirPlay screen wire itself. */
static int alt_setup(void *session, void *streamDesc, void *responseParams)
{
    altslot_t *slot;
    void  *server;
    int    err = -6719, family, port = 0, sock = -1, started = 0;
    int    applyQos = 0;
    int64_t connID;
    uint8_t key[16], iv[16];
    void  *rdesc = NULL;
    altthread_arg_t *threadArg = NULL;
    uint32_t av_gen = 0;   /* transport-owner generation for THIS receiver (claimed on success) */

    alt_gc_exited_slots();   /* reclaim orphaned exited slots so a fresh slot is available */
    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_get(session);
    /* R2: a concurrent teardown of THIS session's slot (possible on a fast replug when
     * dio frees the old session and the new one reuses its address) must NOT fail the
     * whole control SETUP — the interpose propagates our error as ret=arc.  Wait,
     * bounded, for teardown to finish clearing the slot, then re-acquire it fresh. */
    if (slot && slot->closing) {
        int waited_ms = 0;
        while (slot->closing && waited_ms < ALT_SETUP_CLOSE_WAIT_MS) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 50 * 1000000L;
            if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
            pthread_cond_timedwait(&alt_cond, &alt_lock, &ts);
            waited_ms += 50;
        }
        slot = alt_slot_get(session);   /* teardown may have freed+reallocated the slot */
    }
    if (slot && (slot->closing || slot->settingUp)) {
        pthread_mutex_unlock(&alt_lock);
        LOG_WARN(LOG_MODULE, "alt_setup: slot busy (closing/settingUp) after wait — deferring");
        return -6719;
    }
    if (slot) slot->settingUp = 1; /* reserve lifecycle across old-thread reap */
    pthread_mutex_unlock(&alt_lock);
    if (!slot) return -6728;
    /* A live receiver from a prior session/SETUP must NOT fail this SETUP.  The
     * interpose propagates alt_setup's error as the WHOLE control SETUP result
     * (ret=arc in AirPlayReceiverSessionSetup), so returning -6719 for a lingering
     * thread after a fast replug killed the entire CarPlay session — it only came
     * up on the retry once the old thread finally exited.  Force-stop and join the
     * stale generation, then set up a fresh stream.  alt_reap_thread(,1) also
     * handles the not-created and already-exited cases and never returns the -1
     * "still live" refusal, so this replaces the old reap(,0) + hard bail. */
    if (alt_reap_thread(slot, 1) != 0) goto fail;

    pthread_mutex_lock(&alt_lock);
    if (slot->closing || !slot->inUse || slot->session != session) {
        pthread_mutex_unlock(&alt_lock);
        goto fail;
    }
    slot->stop = 0;
    pthread_mutex_unlock(&alt_lock);
    LOG_INFO(LOG_MODULE, "alt_setup: begin session=%p streamDesc=%p (OFF aes=%d fam=%d srv=%d)",
             session, streamDesc, OFF_AESKEY, OFF_FAMILY, OFF_SERVER);

    {
        void *kconn = cf_str(0, "streamConnectionID", CFENC_UTF8);
        connID = kconn ? cf_geti64(streamDesc, kconn, NULL) : 0;
        if (kconn && cf_rel) cf_rel(kconn);
    }
    if (!connID) { LOG_ERROR(LOG_MODULE, "alt_setup: connID missing (streamConnectionID absent)"); err = -6715; goto fail; }
    LOG_INFO(LOG_MODULE, "alt_setup: connID=%llu", (unsigned long long)connID);

    derive_aes((const uint8_t *)((char *)session + OFF_AESKEY), 16, (uint64_t)connID, key, iv);
    /* self-contained receiver decrypts the opcode-0 bodies itself (no stock ScreenStream — see
     * alt_recv_loop); stash the derived key/iv in the slot for the receiver thread's AES-CTR. */
    memcpy(slot->key, key, 16);
    memcpy(slot->iv,  iv,  16);
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));

    family = *(uint8_t *)((char *)session + OFF_FAMILY);
    /* Snapshot every remaining stock-session field before ServerSocketOpen.
     * If TearDown's bounded setup wait expires while that stock primitive is
     * blocked, the resumed setup must not dereference a freed session. */
    server = *(void **)((char *)session + OFF_SERVER);
    applyQos = (server && sock_setqos &&
                !*(uint8_t *)((char *)server + SVR_OFF_QOSDISABLED));
    /* Native altScreen uses the adjacent preferred port: MHI3
     * AirPlayReceiverSessionSetup@0x63eb0-0x63ed8 selects -6030 for type 110 and
     * -6031 for type 111. Sharing -6030 races stock 210's main listener. */
    err = srv_sockopen(family, 1 /*SOCK_STREAM*/, 6 /*IPPROTO_TCP*/, -6031, &port, -180224, &sock);
    LOG_INFO(LOG_MODULE, "alt_setup: srv_sockopen err=%d port=%d sock=%d family=%d", err, port, sock, family);
    if (err) goto fail;

    pthread_mutex_lock(&alt_lock);
    if (slot->closing || !slot->settingUp || !slot->inUse ||
        slot->session != session) {
        pthread_mutex_unlock(&alt_lock);
        err = -6719;
        goto fail;
    }
    pthread_mutex_unlock(&alt_lock);
    if (applyQos) sock_setqos(sock, 12);

    /* Build the response descriptor before starting the acceptor.  The response
     * cannot leave this function before the thread is live; if thread creation
     * fails, the interpose propagates the error and the response is discarded. */
    rdesc = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!rdesc) { err = -6728; goto fail; }
    set_i64(rdesc, "type", ALT_STREAM_TYPE);
    set_i64(rdesc, "dataPort", port);
    /* Start the acceptor before publishing its descriptor into the response.
     * This keeps failure transactional: a pthread failure cannot leave a dead
     * type-111/dataPort entry in an otherwise-successful stock SETUP response. */
    threadArg = (altthread_arg_t *)calloc(1, sizeof(*threadArg));
    if (!threadArg) { err = -6728; goto fail; }
    pthread_mutex_lock(&alt_lock);
    if (slot->closing || !slot->settingUp) {
        pthread_mutex_unlock(&alt_lock);
        err = -6719;
        goto fail;
    }
    slot->generation++;
    if (slot->generation == 0) slot->generation = 1;
    av_gen = ++g_alt_gen_counter;
    if (av_gen == 0) av_gen = ++g_alt_gen_counter;   /* 0 = unclaimed sentinel; never assign it */
    slot->altSock = sock;
    slot->threadExited = 0;
    threadArg->slot = slot;
    threadArg->generation = slot->generation;
    threadArg->av_gen = av_gen;
    threadArg->listenSock = sock;
    if (pthread_create(&slot->thread, NULL, alt_thread_main, threadArg) == 0) {
        slot->threadCreated = 1;
        started = 1;
        threadArg = NULL;              /* receiver owns it */
        sock = -1;                    /* slot/thread owns the listener now */
    } else {
        slot->altSock = -1;
    }
    pthread_mutex_unlock(&alt_lock);
    if (!started) { err = -6728; goto fail; }
    if (alt_add_response_stream(responseParams, rdesc) != 0) {
        err = -6728;
        goto fail;
    }
    if (cf_rel) { cf_rel(rdesc); rdesc = NULL; }
    /* Commit the SETUP atomically with respect to Java/BAP/dio session commands.  Until
     * the response descriptor exists and the monotonic transport claim succeeds,
     * g_alt_session must continue to name the previous committed session: publishing it
     * before this point allowed a gen=0 command to run while the new control SETUP was
     * still being built.  Hold alt_lock across claim+publication.  No altav path holds
     * g_lock while acquiring alt_lock, so the alt_lock -> g_lock order cannot deadlock.
     * A slower older SETUP whose claim is rejected still returns its already-built
     * response, but it never becomes the command target; its receiver observes that it
     * is not transport owner and exits after the connection arrives. */
    pthread_mutex_lock(&alt_lock);
    if (slot->closing || !slot->settingUp || !slot->inUse ||
        slot->session != session) {
        pthread_mutex_unlock(&alt_lock);
        err = -6719;
        goto fail;
    }
    if (altav_claim(av_gen)) {
        g_alt_session = session;
        g_alt_session_gen = av_gen;
    } else {
        LOG_INFO(LOG_MODULE,
                 "alt_setup: gen=%u committed response but not session target (newer owner won)",
                 (unsigned)av_gen);
    }
    slot->settingUp = 0;
    pthread_cond_broadcast(&alt_cond);
    pthread_mutex_unlock(&alt_lock);

    /* The acceptor is live before this function returns and the response is sent. */
    LOG_INFO(LOG_MODULE, "alt-screen set up on port %d connID=%llu",
             port, (unsigned long long)connID);
    return 0;

fail:
    if (rdesc && cf_rel) cf_rel(rdesc);
    if (threadArg) free(threadArg);
    if (started) alt_reap_thread(slot, 1);
    if (sock >= 0) close(sock);
    pthread_mutex_lock(&alt_lock);
    if (g_alt_session == session) { g_alt_session = NULL; g_alt_session_gen = 0; }   /* defensive (only set on success) */
    /* If bounded reap timed out, the receiver still owns this slot/key/iv.
     * Keep it stopped+closing and let alt_gc_exited_slots collect it after its
     * terminal flag; clearing it here would be a slot UAF on fast reconnect. */
    if (!slot->threadCreated) {
        slot->altSock = -1;
        slot->stop = 0;
        memset(slot->key, 0, sizeof(slot->key));
        memset(slot->iv, 0, sizeof(slot->iv));
    } else {
        slot->stop = 1;
        slot->closing = 1;
    }
    slot->settingUp = 0;
    pthread_cond_broadcast(&alt_cond);
    pthread_mutex_unlock(&alt_lock);
    LOG_ERROR(LOG_MODULE, "alt-screen setup failed err=%d", err);
    return err;
}

/* find a type==111 stream desc in the request's "streams" array */
static void *alt_find_stream_type(void *request, int streamType)
{
    void *kstreams = cf_str(0, "streams", CFENC_UTF8);
    void *ktype    = cf_str(0, "type", CFENC_UTF8);
    void *found = NULL;
    if (kstreams && ktype) {
        void *arr = cf_getval(request, kstreams);
        if (arr) {
            long n = cf_arrcount(arr), i;
            for (i = 0; i < n; i++) {
                void *sd = cf_arridx(arr, i);
                if (sd && cf_geti64(sd, ktype, NULL) == streamType) { found = sd; break; }
            }
        }
    }
    if (kstreams && cf_rel) cf_rel(kstreams);
    if (ktype && cf_rel) cf_rel(ktype);
    return found;
}

static void *alt_find_111(void *request)
{
    return alt_find_stream_type(request, ALT_STREAM_TYPE);
}

/* interpose the SETUP entry: real handles 100/101/110 (skips 111), then we
 * bring up 111 ourselves and append its desc to the response. */
typedef int (*fn_sess_setup_t)(void *, void *, void **, int);
int AirPlayReceiverSessionSetup(void *session, void *request, void **respOut, int a4)
{
    static fn_sess_setup_t real;
    uint32_t setupSequence = __sync_add_and_fetch(&g_setup_sequence_counter, 1);
    uint64_t setupBeginMs = alt_now_ms();
    uint64_t realDoneMs;
    int setupIsControl = 0;
    int setupHas111 = 0;
    if (!real) real = (fn_sess_setup_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionSetup");
    if (!real) { LOG_ERROR(LOG_MODULE, "SessionSetup real==NULL"); return -6740; }

    if (alt_enabled < 0) alt_enabled = env_on("ALTSCREEN_HANDLE_ALT") ? 1 : 0;
    int ret = real(session, request, respOut, a4);
    realDoneMs = alt_now_ms();

    /* Any successful RTSP SETUP proves USB+iAP2 reached the AirPlay control
     * plane. The first one is control SETUP in the real protocol, but marking
     * here also remains fail-safe if optional altScreen CF symbol resolution is
     * unavailable: that must never be misclassified as a pre-RTSP USB wedge. */
    if (ret == 0 && session && request && respOut && *respOut)
        mark_control_setup_seen();

    /* type-103 buffered-media negotiation probe (audio module). Deliberately OUTSIDE the alt_enabled
     * gate: it has nothing to do with altScreen and must work with the altScreen advert off. It gates
     * itself on its own marker file and is a no-op without it. This wrapper is simply the single
     * owner of the AirPlayReceiverSessionSetup interposition. */
    if (ret == 0 && session && request && respOut && *respOut)
        spa_augment_setup_response(session, request, *respOut);

    if (alt_enabled && ret == 0 && respOut && *respOut && session && request && alt_resolve() == 1) {
        /* NEGOTIATION — the REAL altScreen gate (VERIFIED against iOS 26.6 AirPlaySender/CarKit disasm):
         * The feature exchange is ASYMMETRIC — the phone proposes in the control-SETUP *request* under
         * "features", but reads the NEGOTIATED set from the SETUP *response* under **"enabledFeatures"**
         * (apsession_ensureControlStreamSetup@0x22786DB80 typed-gets response["enabledFeatures"];
         * "enabledFeatures"@0x2278FE1DC vs request "features"@0x2278FE4F4 — verified).  carEndpoint_Activate
         * then sets endpoint[+0x3E] = CFArrayContainsValue(enabled, CFSTR("altScreen")); carEndpoint_setupStreams
         * makes a stream per display iff (raw displays count>=2 && endpoint[+0x3E]); 110 vs 111 by UUID≠display0.
         * /info extendedFeatures is a SEPARATE message and does NOT set +0x3E.  Stock 210 writes no response
         * enabledFeatures → altScreen never negotiates → no 111.
         *   - Seed from the receiver's EXISTING response enabledFeatures and add only the two contracts this
         *     hook implements: altScreen and viewAreas. Each string in
         *     that array flips a sibling endpoint flag (viewAreas+0x3D, altScreen+0x3E, uiContext+0x3F,
         *     enhancedSiri+0x39), so NEVER echo the phone's proposal — that could falsely enable features
         *     stock doesn't implement.
         *   - Scope to the control SETUP: the request carries the "features" proposal key (media SETUPs don't). */
        void *kReq = cf_str(0, "features", CFENC_UTF8);
        int isControlSetup = (kReq && cf_getval(request, kReq) != 0);
        setupIsControl = isControlSetup;
        if (kReq && cf_rel) cf_rel(kReq);
        if (isControlSetup && advert_preflight(session, setupSequence)) {
            int featureEnabled = 0;
            void *kEnab = cf_str(0, "enabledFeatures", CFENC_UTF8);
            if (kEnab) {
                void *cur  = cf_getval(*respOut, kEnab);            /* receiver's existing accepted set (usually NULL on 210) */
                long  curN = (cur && cf_arrcount) ? cf_arrcount(cur) : -1;
                void *neg  = cf_arrm(0, 0, arr_cb);
                if (neg) {
                    long i;
                    int negOk = 1;
                    for (i = 0; i < curN; i++) {                    /* preserve stock-accepted features */
                        void *e = cf_arridx(cur, i);
                        if (e) cf_arrapp(neg, e);
                    }
                    if (cf_arrappc) {
                        negOk = (cf_arrappc(neg, "altScreen", -1) == 0 &&
                                 cf_arrappc(neg, "viewAreas", -1) == 0);
                    } else {
                        void *s = cf_str(0, "altScreen", CFENC_UTF8);
                        if (s) { cf_arrapp(neg, s); if (cf_rel) cf_rel(s); }
                        else negOk = 0;
                        s = cf_str(0, "viewAreas", CFENC_UTF8);
                        if (s) { cf_arrapp(neg, s); if (cf_rel) cf_rel(s); }
                        else negOk = 0;
                    }
                    if (negOk) {
                        cf_dictset(*respOut, kEnab, neg); /* existing + contracts implemented by this hook */
                        featureEnabled = 1;
                        LOG_INFO(LOG_MODULE,
                                 "control SETUP: response enabledFeatures merged (existing count=%ld) + altScreen + viewAreas",
                                 curN);
                    }
                    if (cf_rel) cf_rel(neg);
                }
                if (cf_rel) cf_rel(kEnab);
            }
            if (!featureEnabled) {
                advert_clear_for_session(session);
                LOG_ERROR(LOG_MODULE,
                          "control SETUP: enabledFeatures build failed — display advert rolled back");
            }
        } else if (isControlSetup) {
            LOG_WARN(LOG_MODULE,
                     "control SETUP: altScreen/viewAreas not enabled because display preflight failed");
        }

        void *altDesc = alt_find_111(request);
        if (altDesc) {
            setupHas111 = 1;
            LOG_INFO(LOG_MODULE, "SETUP: 111 stream offered — bringing up altScreen");
            int arc = alt_setup(session, altDesc, *respOut);
            LOG_INFO(LOG_MODULE, "SETUP: alt_setup returned %d", arc);
            if (arc != 0) ret = arc;  /* never return HTTP success without a usable 111 dataPort */
        } else {
            LOG_INFO(LOG_MODULE, "SETUP: no 111 stream in request (phone did not offer altScreen)");
        }
    } else if (alt_enabled && ret == 0) {
        LOG_INFO(LOG_MODULE, "SETUP: alt gated off in interpose (resp=%p session=%p resolve=%d)",
                 (respOut ? *respOut : (void*)0), session, alt_resolve());
    }
    {
        uint64_t setupDoneMs = alt_now_ms();
        unsigned long realMs = (setupBeginMs && realDoneMs && realDoneMs >= setupBeginMs) ?
                               (unsigned long)(realDoneMs - setupBeginMs) : 0;
        unsigned long hookMs = (realDoneMs && setupDoneMs && setupDoneMs >= realDoneMs) ?
                               (unsigned long)(setupDoneMs - realDoneMs) : 0;
        LOG_INFO(LOG_MODULE,
                 "STARTUP SETUP timing control=%d alt111=%d real=%lums hook=%lums total=%lums ret=%d",
                 setupIsControl, setupHas111, realMs, hookMs,
                 (unsigned long)(realMs + hookMs), ret);
    }
    return ret;
}

/* Interpose teardown. A non-NULL request is a per-stream teardown; only stop
 * our receiver when that request actually contains type 111. Stock also calls
 * this entry for unrelated streams while altScreen remains live. A NULL request
 * tears down the full session, so the receiver must be joined before stock frees
 * the session fields used by the thread. */
typedef int (*fn_sess_teardown_t)(void *, void *, int, int);
int AirPlayReceiverSessionTearDown(void *session, void *request, int reason, int a4)
{
    static fn_sess_teardown_t real;
    int requestHas111 = 0;
    int tearDownAlt = (request == NULL);
    int reapRc = 0;
    int setupTimedOut = 0;
    if (!real) real = (fn_sess_teardown_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionTearDown");

    if (request && alt_resolve() == 1) {
        requestHas111 = (alt_find_111(request) != NULL);
        if (requestHas111) tearDownAlt = 1;
    }

    LOG_INFO(LOG_MODULE,
             "AirPlayReceiverSessionTearDown session=%p request=%p reason=%d a4=%d alt111=%d action=%s",
             session, request, reason, a4, requestHas111,
             tearDownAlt ? "stop-alt" : "preserve-alt");

    altslot_t *slot;
    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_find(session);
    if (slot && tearDownAlt) {
        struct timespec setupDeadline;
        int waitRc = 0;
        slot->closing = 1;
        alt_deadline_after_ms(&setupDeadline, ALT_TEARDOWN_SETUP_WAIT_MS);
        while (slot->settingUp && waitRc != ETIMEDOUT)
            waitRc = pthread_cond_timedwait(&alt_cond, &alt_lock,
                                            &setupDeadline);
        if (slot->settingUp) {
            setupTimedOut = 1;
            slot->stop = 1;
            LOG_WARN(LOG_MODULE,
                     "altScreen TearDown: setup still active after %d ms; revoking generation and deferring slot GC",
                     ALT_TEARDOWN_SETUP_WAIT_MS);
        }
        /* Revoke command-channel ownership before asking the receiver to stop.
         * Even if bounded reap is deferred, it cannot send a command through a
         * stock session that TearDown is about to free. */
        if (g_alt_session == session) {
            g_alt_session = NULL;
            g_alt_session_gen = 0;
        }
    }
    pthread_mutex_unlock(&alt_lock);
    if (tearDownAlt) advert_clear_for_session(session);
    if (slot && tearDownAlt && !setupTimedOut) {
        /* Stop the self-contained receiver: the reaper sets stop, closes the
         * listener to wake pre-connect accept, and joins outside alt_lock.  An
         * already-exited receiver follows the identical single-owner path. */
        reapRc = alt_reap_thread(slot, 1);
        pthread_mutex_lock(&alt_lock);
        if (slot->altSock >= 0) {
            int ls = slot->altSock;
            slot->altSock = -1;
            pthread_mutex_unlock(&alt_lock);
            close(ls);
            pthread_mutex_lock(&alt_lock);
        }
        if (reapRc == 0 && !slot->threadCreated) {
            memset(slot, 0, sizeof(*slot));
        } else {
            /* The thread owns the slot until it publishes threadExited.  GC at
             * the next SETUP will join and clear it without blocking teardown. */
            slot->closing = 1;
            slot->stop = 1;
        }
        pthread_cond_broadcast(&alt_cond);
        pthread_mutex_unlock(&alt_lock);
        if (reapRc == 0)
            LOG_INFO(LOG_MODULE, "altScreen torn down for session=%p", session);
        else
            LOG_WARN(LOG_MODULE,
                     "altScreen teardown deferred receiver cleanup session=%p", session);
    } else if (slot && tearDownAlt) {
        /* The setup owner will observe closing, unwind its transaction and
         * publish settingUp=0.  Do not race it by touching thread/listener
         * fields here; the next SETUP's GC collects the terminal slot. */
        LOG_WARN(LOG_MODULE,
                 "altScreen teardown proceeding with setup cleanup deferred session=%p",
                 session);
    } else if (slot) {
        LOG_INFO(LOG_MODULE,
                 "per-stream TEARDOWN has no 111 - preserving altScreen session=%p",
                 session);
    }

    /* NOTE: deliberately NO hook_notify_disconnect() here.  A full AirPlay teardown is
     * an AirPlay-layer event; the iAP2 link (and thus RGD component_valid) can survive it.
     * Firing HOOK_EVENT_DISCONNECT would rgd_clear_state()->component_valid=false, and only
     * a new iAP2 Identify restores it — so an AirPlay restart without a re-Identify would
     * strand RGI dead until a physical reconnect.  The authoritative per-session RGD reset
     * stays on IAP2_MSG_IDENTIFY_START (hook_framework.c handle_state_messages). */
    if (!real) return reason;
    return real(session, request, reason, a4);
}

/* MUST match the cluster display uuid advertised in build_cluster_dict() (#1b). */
#define CLUSTER_DISPLAY_UUID "C1057E12-A0D1-4E20-B0C0-1057E12A0D10"

/* ════════════════════════════════════════════════════════════════════════════
 * working zone (fullscreen / sportscreen / classicscreen) live-switch — Java-driven via the bus.
 *
 * Three full-frame view areas are PREDECLARED in /info, with different nested
 * safeArea geometry: fullscreen=[0], sportscreen=[1], classicscreen=[2].
 * Switching is a plain
 * session command — NOT a stream reconfig: we ship {type:"updateViewArea",
 * params:{uuid,viewAreaIndex,animationDurationMillis,adjacentViewAreas}} via the
 * stock generic command shipper
 * AirPlayReceiverSessionSendCommand (POST /command, binary-plist body).  The
 * phone (CarKit _sessionUpdatesQueue_handleViewAreaChangeWithPayload) reads those
 * keys, bounds-checks the index against the declared viewAreas, and applies the
 * change.  Match MHI3 AirPlayReceiverSessionViewAreaUpdate exactly by always
 * carrying `adjacentViewAreas`: [] clears it in fullscreen, [0] tells CarKit
 * that view-area 0 is adjacent while smallscreen area 1 is current.  iOS stores this as
 * CARScreenInfo.adjacentViewArea separately from currentViewArea, so omitting it
 * is observably different from the native helper. Verified: stock 210 exports
 * the generic shipper and it does NOT whitelist command types.
 *
 * CONFIRMATION: we pass a completion callback to SendCommand.  libairplay's
 * response handler (sub_28CF0) parses the phone's POST /command reply and calls
 * completion(int status, CFDictionaryRef resp|NULL, void *ctx) — status 0 = the
 * phone accepted/applied the switch.  We relay it to Java as CMD_ALT_ZONE_ACK so
 * the focus arbiter knows the zone actually changed (and can retry on failure).
 * The requested mode rides in ctx (two low bits). If SendCommand fails
 * synchronously the completion never fires, so we ack that error inline.
 * ════════════════════════════════════════════════════════════════════════════ */
enum {
    ALT_ZONE_FULLSCREEN = 0,
    ALT_ZONE_SPORTSCREEN = 1,
    ALT_ZONE_CLASSICSCREEN = 2
};
static volatile int g_alt_zone_mode = ALT_ZONE_FULLSCREEN;

static const char *alt_zone_name(int mode)
{
    if (mode == ALT_ZONE_SPORTSCREEN) return "sportscreen";
    if (mode == ALT_ZONE_CLASSICSCREEN) return "classicscreen";
    return "fullscreen";
}

static void zone_send_ack(int mode, int status)
{
    uint8_t ack[5];
    ack[0] = (uint8_t)mode;
    ack[1] = (uint8_t)(status);
    ack[2] = (uint8_t)(status >> 8);
    ack[3] = (uint8_t)(status >> 16);
    ack[4] = (uint8_t)(status >> 24);
    bus_send(CMD_ALT_ZONE_ACK, 0, ack, sizeof(ack));
}

/* completion for updateViewArea — runs on libairplay's HTTP-client thread.
 * bus_send is mutex-backed/thread-safe, so acking from here is fine. */
static void zone_cmd_done(int status, void *resp, void *ctx)
{
    uintptr_t packed = (uintptr_t)ctx;
    int mode = (int)(packed & 3U);
    uint32_t gen = (uint32_t)(packed >> 2);
    (void)resp;
    if (!gen || !alt_generation_is_current(gen)) {
        LOG_INFO(LOG_MODULE,
                 "updateViewArea stale ack dropped: mode=%d gen=%u status=%d",
                 mode, (unsigned)gen, status);
        return;
    }
    LOG_INFO(LOG_MODULE, "updateViewArea ack: mode=%d gen=%u status=%d",
             mode, (unsigned)gen, status);
    zone_send_ack(mode, status);
    /* Maps scene options are independent of safeArea. Apply the matching URL
     * only after this view area succeeded, and ignore a stale ACK after a rapid
     * second View-button press. */
    if (status == 0 && mode == g_alt_zone_mode)
        alt_show_ui_for_mode(gen, mode, "viewArea-accepted");
}

static uint32_t alt_generation_for_session(void *session)
{
    uint32_t gen = 0;
    pthread_mutex_lock(&alt_lock);
    if (g_alt_session && (!session || g_alt_session == session)) {
        altslot_t *s = alt_slot_find(g_alt_session);
        if (s && s->threadCreated && !s->threadExited)
            gen = g_alt_session_gen;
    }
    pthread_mutex_unlock(&alt_lock);
    return gen;
}

static int alt_generation_is_current(uint32_t gen)
{
    return gen != 0 && alt_generation_for_session(NULL) == gen;
}

/* Ship a command against a retained live altScreen session. Stock 210
 * AirPlayReceiverSessionSendCommand@0x28460 dereferences session+704 at 0x28468,
 * before its own conditional CFRetain at 0x28548. Retain while holding alt_lock,
 * then DROP the lock for the stock call: a wedged HTTP client can no longer pin
 * TEARDOWN behind alt_lock, while the external retain prevents session UAF.
 * Returns the SendCommand rc, or -6735 if no active session/shipper. */
static int alt_send_locked_for_gen(uint32_t gen, void *cmd, void *completion, void *ctx)
{
    int rc = -6735;
    void *sess = NULL;
    pthread_mutex_lock(&alt_lock);
    /* gen==0: Java/BAP/dio command — deliberately targets the current session.  gen>0:
     * receiver-owned command — additionally require g_alt_session_gen to still equal it,
     * checked HERE under alt_lock right before the call, so a supersede between a caller's
     * owner check and this send cannot leak the command into the NEW session.
     *
     * R3: also require an in-use slot with a created, not-exited receiver — g_alt_session
     * can momentarily dangle if a stock session is freed without a TearDown that clears it;
     * this avoids sess_sendcmd on a stale pointer (use-after-free). */
    if (sess_sendcmd && cf_ret && cf_rel && g_alt_session &&
        (gen == 0 || g_alt_session_gen == gen)) {
        altslot_t *s = alt_slot_find(g_alt_session);
        if (s && s->threadCreated && !s->threadExited) {
            sess = g_alt_session;
            cf_ret(sess);
        }
    }
    pthread_mutex_unlock(&alt_lock);
    if (sess) {
        rc = sess_sendcmd(sess, cmd, completion, ctx);
        cf_rel(sess);
    }
    return rc;
}

static void *alt_command_worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        alt_cmd_item_t item;
        int rc;
        pthread_mutex_lock(&g_alt_cmd_lock);
        while (!g_alt_cmd_shutdown && g_alt_cmd_count == 0)
            pthread_cond_wait(&g_alt_cmd_cond, &g_alt_cmd_lock);
        if (g_alt_cmd_shutdown) {
            g_alt_cmd_exited = 1;
            pthread_cond_broadcast(&g_alt_cmd_cond);
            pthread_mutex_unlock(&g_alt_cmd_lock);
            return NULL;
        }
        item = g_alt_cmd_queue[g_alt_cmd_head];
        memset(&g_alt_cmd_queue[g_alt_cmd_head], 0, sizeof(item));
        g_alt_cmd_head = (g_alt_cmd_head + 1U) % ALT_CMD_QUEUE_CAPACITY;
        --g_alt_cmd_count;
        pthread_mutex_unlock(&g_alt_cmd_lock);

        rc = alt_send_locked_for_gen(item.gen, item.cmd,
                                     item.completion, item.ctx);
        /* Stock does not invoke the completion when SendCommand fails before
         * queueing the HTTP message. Preserve zone ACK/error and logging
         * semantics without making the original caller wait for the result. */
        if (rc != 0 && item.completion) {
            void (*done)(int, void *, void *) =
                (void (*)(int, void *, void *))item.completion;
            done(rc, NULL, item.ctx);
        }
        if (cf_rel) cf_rel(item.cmd);
    }
}

/* Queue against a snapshot generation. gen==0 means "the session current at
 * enqueue time", never "whatever session exists when the worker wakes": this
 * prevents a delayed Java/BAP command from leaking across fast reconnect. */
static int alt_send_async_for_gen(uint32_t gen, void *cmd,
                                  void *completion, void *ctx)
{
    int startRc = 0;
    if (!cmd || !cf_ret || !cf_rel) return -6735;
    if (!gen) gen = alt_generation_for_session(NULL);
    if (!gen) return -6735;

    pthread_mutex_lock(&g_alt_cmd_lock);
    if (g_alt_cmd_shutdown) {
        pthread_mutex_unlock(&g_alt_cmd_lock);
        return -6735;
    }
    if (!g_alt_cmd_created) {
        g_alt_cmd_exited = 0;
        startRc = pthread_create(&g_alt_cmd_thread, NULL,
                                 alt_command_worker_main, NULL);
        if (startRc == 0) g_alt_cmd_created = 1;
    }
    if (!g_alt_cmd_created || g_alt_cmd_count == ALT_CMD_QUEUE_CAPACITY) {
        pthread_mutex_unlock(&g_alt_cmd_lock);
        return g_alt_cmd_created ? -6729 : -6728;
    }
    g_alt_cmd_queue[g_alt_cmd_tail].gen = gen;
    g_alt_cmd_queue[g_alt_cmd_tail].cmd = cf_ret(cmd);
    g_alt_cmd_queue[g_alt_cmd_tail].completion = completion;
    g_alt_cmd_queue[g_alt_cmd_tail].ctx = ctx;
    g_alt_cmd_tail = (g_alt_cmd_tail + 1U) % ALT_CMD_QUEUE_CAPACITY;
    ++g_alt_cmd_count;
    pthread_cond_signal(&g_alt_cmd_cond);
    pthread_mutex_unlock(&g_alt_cmd_lock);
    return 0;
}

static void alt_command_worker_shutdown(void)
{
    pthread_t thread;
    int collect = 0;
    int waitRc = 0;
    struct timespec deadline;
    unsigned int i;

    pthread_mutex_lock(&g_alt_cmd_lock);
    g_alt_cmd_shutdown = 1;
    for (i = 0; i < g_alt_cmd_count; ++i) {
        unsigned int index = (g_alt_cmd_head + i) % ALT_CMD_QUEUE_CAPACITY;
        if (g_alt_cmd_queue[index].cmd && cf_rel)
            cf_rel(g_alt_cmd_queue[index].cmd);
        memset(&g_alt_cmd_queue[index], 0, sizeof(g_alt_cmd_queue[index]));
    }
    g_alt_cmd_head = g_alt_cmd_tail = g_alt_cmd_count = 0;
    pthread_cond_broadcast(&g_alt_cmd_cond);
    if (g_alt_cmd_created) {
        thread = g_alt_cmd_thread;
        alt_deadline_after_ms(&deadline, ALT_CMD_SHUTDOWN_WAIT_MS);
        while (!g_alt_cmd_exited && waitRc != ETIMEDOUT)
            waitRc = pthread_cond_timedwait(&g_alt_cmd_cond,
                                            &g_alt_cmd_lock, &deadline);
        if (g_alt_cmd_exited) {
            g_alt_cmd_created = 0;
            collect = 1;
        }
    }
    pthread_mutex_unlock(&g_alt_cmd_lock);

    if (collect) pthread_join(thread, NULL); /* terminal-only collection */
    else if (g_alt_cmd_created)
        LOG_WARN(LOG_MODULE,
                 "alt command worker did not exit within %d ms; process-exit cleanup will reclaim it",
                 ALT_CMD_SHUTDOWN_WAIT_MS);
}

/* Build and ship one of the three predeclared view-area indexes. */
static int zone_send_mode(uint32_t gen, int mode, int dur, void *completion)
{
    int index, rc = -6728;
    void *params = NULL, *cmd = NULL, *adj = NULL, *pk = NULL, *ak = NULL;

    if (mode == ALT_ZONE_SPORTSCREEN) index = 1;
    else if (mode == ALT_ZONE_CLASSICSCREEN) index = 2;
    else { mode = ALT_ZONE_FULLSCREEN; index = 0; }
    if (cf_init() != 1 || !cf_dictm || !dict_keycb || !dict_valcb ||
        !cf_dictset || !cf_str || !cf_dictseti || !cf_arrm || !arr_cb ||
        !cf_arrappi64) {
        LOG_ERROR(LOG_MODULE, "updateViewArea: CF symbols unresolved");
        return -6735;
    }

    params = cf_dictm(0, 0, dict_keycb, dict_valcb);
    cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    adj = cf_arrm(0, mode == ALT_ZONE_FULLSCREEN ? 0 : 1, arr_cb);
    if (params && cmd && adj) {
        set_str(params, "uuid", CLUSTER_DISPLAY_UUID);
        set_i64(params, "viewAreaIndex", index);
        set_i64(params, "animationDurationMillis", dur);
        if (mode != ALT_ZONE_FULLSCREEN) cf_arrappi64(adj, 0);
        ak = cf_str(0, "adjacentViewAreas", CFENC_UTF8);
        if (ak) cf_dictset(params, ak, adj);
        set_str(cmd, "type", "updateViewArea");
        pk = cf_str(0, "params", CFENC_UTF8);
        if (pk) cf_dictset(cmd, pk, params);
        uintptr_t packed = (((uintptr_t)(gen & 0x3FFFFFFFU)) << 2) |
                           (uintptr_t)(mode & 3);
        rc = alt_send_async_for_gen(gen, cmd, completion,
                                    completion ? (void *)packed : NULL);
        LOG_INFO(LOG_MODULE,
                 "updateViewArea ship mode=%s index=%d adjacent=%s dur=%dms rc=%d",
                 alt_zone_name(mode), index,
                 mode == ALT_ZONE_FULLSCREEN ? "[]" : "[0]", dur, rc);
    }
    if (ak && cf_rel) cf_rel(ak);
    if (pk && cf_rel) cf_rel(pk);
    if (adj && cf_rel) cf_rel(adj);
    if (params && cf_rel) cf_rel(params);
    if (cmd && cf_rel) cf_rel(cmd);
    return rc;
}

static void zone_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 1) return;
    int mode = payload[0] == ALT_ZONE_SPORTSCREEN ? ALT_ZONE_SPORTSCREEN
        : (payload[0] == ALT_ZONE_CLASSICSCREEN
            ? ALT_ZONE_CLASSICSCREEN : ALT_ZONE_FULLSCREEN);
    int dur = (len >= 3) ? (payload[1] | (payload[2] << 8)) : 250;
    int rc;
    uint32_t gen;

    /* Cache even before the RTSP session exists. The first IDR will apply the
     * latest Audi mode, making fast reconnect and pre-session View presses safe. */
    g_alt_zone_mode = mode;
    gen = alt_generation_for_session(NULL);
    rc = zone_send_mode(gen, mode, dur, (void *)zone_cmd_done);
    if (rc != 0) zone_send_ack(mode, rc); /* no async completion on sync failure */
}

/* ════════════════════════════════════════════════════════════════════════════
 * More MHI3-ported session commands — same {type,params}+SendCommand pattern as
 * the zone switch, so factored into one shipper.  Each targets the cluster
 * (CLUSTER_DISPLAY_UUID); changeUIContext is session-global (no uuid).
 *
 * NOTE: changeMapZoomLevel is a SEMANTIC command (zoomDirection int), NOT a HID
 * report — it zooms the map deterministically regardless of input focus, unlike
 * the knob (whose rotation the CarPlay UI interprets contextually).
 * RE recipes (MHI3 450, keys verified): changeMapZoomLevel{uuid,zoomDirection};
 * uiAppearanceUpdate/mapAppearanceUpdate{uuid,appearanceMode,appearanceSetting};
 * changeUIContext{url}.
 * ════════════════════════════════════════════════════════════════════════════ */

/* logging completion — confirmation for the fire-and-relay commands.  ctx = the
 * (static-literal) command type, so it stays valid until the async reply. */
static void alt_cmd_done(int status, void *resp, void *ctx)
{
    (void)resp;
    const char *type = ctx ? (const char *)ctx : "?";
    LOG_INFO(LOG_MODULE, "session command '%s' ack: status=%d", type, status);

}

/* showUI has a stateful follow-up, so unlike logging-only completions it carries
 * the immutable transport generation in ctx. A delayed response from an old
 * HTTP client must never force a keyframe on a replacement session. */
static void alt_showui_done(int status, void *resp, void *ctx)
{
    uint32_t gen = (uint32_t)(uintptr_t)ctx;
    (void)resp;
    LOG_INFO(LOG_MODULE, "session command 'showUI' ack: gen=%u status=%d",
             (unsigned)gen, status);
    if (status == 0 && alt_generation_is_current(gen))
        alt_request_cluster_idr(gen, "showUI-accepted");
    else if (status == 0)
        LOG_INFO(LOG_MODULE, "showUI stale ack dropped gen=%u", (unsigned)gen);
}

/* new params dict (uuid preset when wantUuid); NULL if CF syms not ready. */
static void *alt_params_new(int wantUuid)
{
    if (cf_init() != 1 ||
        !cf_dictm || !dict_keycb || !dict_valcb || !cf_dictset || !cf_str || !cf_dictseti)
        return NULL;
    void *p = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (p && wantUuid) set_str(p, "uuid", CLUSTER_DISPLAY_UUID);
    return p;
}

/* ship {type, params} with the logging completion; consumes (releases) params. */
static int alt_ship(uint32_t gen, const char *type, void *params)
{
    if (!params) return -6728;
    int rc = -6728;
    void *cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (cmd) {
        set_str(cmd, "type", type);
        void *pk = cf_str(0, "params", CFENC_UTF8);
        if (pk) cf_dictset(cmd, pk, params);
        if (pk && cf_rel) cf_rel(pk);
        if (!strcmp(type, "showUI"))
            rc = alt_send_async_for_gen(gen, cmd, (void *)alt_showui_done,
                                        (void *)(uintptr_t)gen);
        else
            rc = alt_send_async_for_gen(gen, cmd, (void *)alt_cmd_done,
                                        (void *)type);
        if (cf_rel) cf_rel(cmd);
        LOG_INFO(LOG_MODULE, "session cmd '%s' shipped gen=%u rc=%d", type, (unsigned)gen, rc);
    }
    if (cf_rel) cf_rel(params);
    return rc;
}

/* Stock 210's SetNightMode command has no display UUID, so CarKit applies it
 * to the primary screen and to secondary screens which do NOT advertise a
 * per-display nightMode.  Our cluster does advertise that key, therefore the
 * same command must be repeated with its UUID.  CarKit accepts NSNumber here
 * (it reads boolValue), so set_i64 produces the same boolean semantics without
 * depending on CFLite's private boolean singleton symbols. */
static int alt_send_cluster_night_mode(uint32_t gen, int night, const char *reason)
{
    void *p = alt_params_new(1);
    int rc;
    if (!p) return -6728;
    set_i64(p, "nightMode", night ? 1 : 0);
    rc = alt_ship(gen, "setNightMode", p);
    LOG_INFO(LOG_MODULE,
             "alt: cluster nightMode=%d uuid=%s reason=%s rc=%d",
             night ? 1 : 0, CLUSTER_DISPLAY_UUID,
             reason ? reason : "?", rc);
    return rc;
}

/* This is a proven LD_PRELOAD boundary seam: MU1316 dio_manager imports and
 * calls AirPlayReceiverSessionSetNightMode at 0x155898 with
 * (session, requestedNightMode, NULL, NULL).  Preserve stock/main-screen
 * behaviour and mirror the same state to the UUID-addressed cluster display.
 * The initial value is already seeded from stock /info; any pre-session call is
 * also retained and replayed at the first stream-111 IDR, after
 * g_alt_session becomes valid. */
typedef int (*fn_sess_set_night_t)(void *, int, void *, void *);
int AirPlayReceiverSessionSetNightMode(void *session, int night,
                                       void *completion, void *context)
{
    static fn_sess_set_night_t real;
    int rc;
    int clusterRc = -6735;

    if (!real)
        real = (fn_sess_set_night_t)dlsym(RTLD_NEXT,
                                          "AirPlayReceiverSessionSetNightMode");
    if (!real) {
        LOG_ERROR(LOG_MODULE, "SetNightMode real==NULL");
        return -6740;
    }

    g_alt_night_mode = night ? 1 : 0;
    rc = real(session, night, completion, context);
    if (env_on("ALTSCREEN_HANDLE_ALT")) {
        uint32_t gen = alt_generation_for_session(session);
        if (gen)
            clusterRc = alt_send_cluster_night_mode(gen, g_alt_night_mode,
                                                    "dio-update");
    }

    LOG_INFO(LOG_MODULE,
             "AirPlayReceiverSessionSetNightMode night=%d stock_rc=%d cluster_rc=%d",
             g_alt_night_mode, rc, clusterRc);
    return rc; /* a cluster-only failure must never break stock main CarPlay */
}

/* Steering-wheel MapScale -> cluster-only map zoom.
 *
 * Live testing proved that a virtual HID knob, even with displayUUID set to the
 * cluster UUID, moves only the main CarPlay UI. iOS 26.6 has a separate semantic
 * path for the instrument cluster: DashBoard matches params.uuid against the
 * cluster display hardwareIdentifier, reads params.zoomDirection, and emits a
 * CRSUIClusterZoomAction. Its wire values are 0=zoom in and nonzero=zoom out.
 *
 * Keep command id 0x0110 so the Java code already running on the HU continues to
 * work without a Java restart. Old Java sends two bytes [step,0]; the second byte
 * is deliberately ignored. A rare accumulated BAP delta (for example -2) is
 * expanded into individual semantic actions, capped to avoid an abusive burst. */
static void zoom_on_cmd(uint16_t type, uint8_t flags,
                        const uint8_t *payload, uint32_t len, void *ctx)
{
    int step, count, direction, i, rc = 0;
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 1) return;

    step = (int)(int8_t)payload[0];
    if (step == 0) return;
    count = step < 0 ? -step : step;
    if (count > 8) count = 8;

    /* Audi MapScale increases when zooming out. Keep an on-unit inversion knob
     * in case a vehicle's steering-wheel BAP polarity is reversed. */
    direction = step > 0 ? 1 : 0; /* iOS DashBoard: 0=in, 1=out */
    if (env_on("ALTSCREEN_ZOOM_INVERT")) direction ^= 1;

    for (i = 0; i < count; ++i) {
        void *p = alt_params_new(1); /* params.uuid = cluster display UUID */
        if (!p) { rc = -6728; break; }
        set_i64(p, "zoomDirection", direction);
        rc = alt_ship(0, "changeMapZoomLevel", p);   /* Java MFL roller → current session */
        if (rc != 0) break;
    }
    LOG_INFO(LOG_MODULE,
             "cluster zoom step=%d direction=%s sent=%d/%d rc=%d uuid=%s",
             step, direction ? "out" : "in", i, count, rc,
             CLUSTER_DISPLAY_UUID);
}

/* Stock 210 AirPlayReceiverSessionForceKeyFrame sends only
 * {type:"forceKeyFrame"}. iOS 26.6 then falls back to the primary screen stream.
 * Its command handler selects the requested screen through params["uuid"], so
 * build the generic command ourselves and target the
 * cluster UUID explicitly. This request intentionally has no completion callback:
 * recovery is proven by observing an actual type-5 NAL, and retaining a callback
 * for every retry would grow stock 210's asynchronous response queue. */
static int alt_request_cluster_idr(uint32_t gen, const char *reason)
{
    void *p = alt_params_new(1);
    void *cmd = NULL;
    void *pk = NULL;
    int rc = -6728;
    if (!p) return -6728;
    cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (cmd) {
        set_str(cmd, "type", "forceKeyFrame");
        pk = cf_str(0, "params", CFENC_UTF8);
        if (pk) cf_dictset(cmd, pk, p);
        rc = alt_send_async_for_gen(gen, cmd, NULL, NULL);
    }
    if (pk && cf_rel) cf_rel(pk);
    if (cmd && cf_rel) cf_rel(cmd);
    if (cf_rel) cf_rel(p);
    LOG_INFO(LOG_MODULE,
             "alt: cluster forceKeyFrame uuid=%s reason=%s rc=%d",
             CLUSTER_DISPLAY_UUID, reason ? reason : "?", rc);
    return rc;
}

/* Apply the latest Audi fullscreen/smallscreen state once the 111 stream is live. The
 * Java command may have arrived before g_alt_session existed; g_alt_zone_mode
 * intentionally preserves it across that window. */
static void alt_kick_viewarea(uint32_t gen)
{
    int mode = g_alt_zone_mode;
    int rc = zone_send_mode(gen, mode, 0, NULL);
    LOG_INFO(LOG_MODULE, "alt: initial updateViewArea mode=%s rc=%d",
             alt_zone_name(mode), rc);
}

/* iOS 26.6 Maps registers this exact URL for its map-based instrument-cluster UI.
 * The normal protocol is Maps -> suggestUI{urls:[...]} -> HU chooses one ->
 * showUI{uuid,url}.  Stock 210 has no altScreen policy layer to make that choice,
 * so choose the Maps map URL when the 111 stream first becomes decodable.
 *
 * Use Maps' plain map item in both layouts. ETA and maneuver guidance are
 * supplied by Audi BAP/KDK, so do not request Maps-owned overlays here. */
static const char *alt_cluster_url_for_mode(int mode)
{
    const char *url;
    if (mode != ALT_ZONE_FULLSCREEN) {
        url = getenv("ALTSCREEN_CLUSTER_URL_SMALLSCREEN");
        /* Compatibility with already deployed startup scripts. */
        if (!url || !*url) url = getenv("ALTSCREEN_CLUSTER_URL_DUO");
        if (!url || !url[0])
            url = "maps:/car/instrumentcluster/map";
    } else {
        url = getenv("ALTSCREEN_CLUSTER_URL");
        if (!url || !url[0])
            url = "maps:/car/instrumentcluster/map";
    }
    return url;
}

static int alt_show_ui_for_mode(uint32_t gen, int mode, const char *reason)
{
    const char *url = alt_cluster_url_for_mode(mode);
    if (!gen) gen = alt_generation_for_session(NULL);
    if (!gen) {
        LOG_INFO(LOG_MODULE,
                 "alt: showUI skipped (no committed altScreen generation) reason=%s",
                 reason ? reason : "?");
        return -6735;
    }
    void *p = alt_params_new(1);                        /* uuid = cluster display */
    if (!p) return -6728;
    set_str(p, "url", url);
    int rc = alt_ship(gen, "showUI", p);
    LOG_INFO(LOG_MODULE, "alt: showUI mode=%s reason=%s uuid=%s url=%s rc=%d",
             mode == ALT_ZONE_FULLSCREEN ? "fullscreen-cards" : alt_zone_name(mode),
             reason ? reason : "?", CLUSTER_DISPLAY_UUID, url, rc);
    return rc;
}

static void alt_kick_show_ui(uint32_t gen)
{
    alt_show_ui_for_mode(gen, g_alt_zone_mode, "first-IDR");
}

static void appearance_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 3) return;                    /* [target][mode][setting] */
    void *p = alt_params_new(1);
    if (!p) return;
    set_i64(p, "appearanceMode",    (uint8_t)payload[1]);
    set_i64(p, "appearanceSetting", (uint8_t)payload[2]);
    alt_ship(0, payload[0] ? "mapAppearanceUpdate" : "uiAppearanceUpdate", p);   /* Java → current session */
}

static void uictx_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len == 0) return;
    void *p = alt_params_new(0);                        /* changeUIContext has no uuid */
    if (!p) return;
    char url[512];
    size_t n = (len < sizeof(url) - 1) ? len : sizeof(url) - 1;
    memcpy(url, payload, n);
    url[n] = 0;
    set_str(p, "url", url);
    alt_ship(0, "changeUIContext", p);   /* Java → current session */
}

void altscreen_init(void)
{
    bus_on(CMD_ALT_ZOOM, zoom_on_cmd, NULL);
    bus_on(CMD_ALT_ZONE, zone_on_cmd, NULL);
    bus_on(CMD_ALT_APPEARANCE, appearance_on_cmd, NULL);
    bus_on(CMD_ALT_UICTX, uictx_on_cmd, NULL);
    LOG_INFO(LOG_MODULE, "AltScreen injector loaded (control negotiation + self-contained stream 111; cluster ZOOM/ZONE/APPEARANCE/UICTX registered; NIGHT mirrored from stock /info + SetNightMode)");
}
void altscreen_shutdown(void)
{
    alt_command_worker_shutdown();
    altav_shutdown();
}

__attribute__((constructor)) static void altscreen_module_init(void) { altscreen_init(); }
__attribute__((destructor))  static void altscreen_module_fini(void) { altscreen_shutdown(); }
