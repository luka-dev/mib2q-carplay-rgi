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
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "../framework/common.h"
#include "../framework/logging.h"
#include "../framework/bus.h"
#include "altscreen_hook.h"
#include "altscreen_av.h"
#include "../../common/alt_timing_stats.h"
#include "avcc_config.h"
#include "command_policy.h"
#include "../../common/alt_monotonic_condition.h"
#ifdef MHI2Q_WIRELESS_ALT_QEMU
#include "../wireless/media.h"
#include "../wireless/paired_screen_crypto.h"
#endif
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
#include "pace_trace.h"
#endif

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
typedef int   (*cf_dictseti_t)(void *, const void *, int64_t);     /* CFDictionarySetInt64 -> OSStatus */
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
/* Seeded from stock /info's nightMode, then updated by the
 * dio->libairplay AirPlayReceiverSessionSetNightMode boundary call.  The /info
 * value is dio's startInNightMode byte, written only at startService and never by
 * requestNightMode (dio 0x167ccc): once dio has set the mode for a session, that
 * session's later /info must not overwrite it (issue #40: main day, cluster night). */
static volatile int g_alt_night_mode;
static void *volatile g_alt_night_dio_session;
static void advert_apply_night_locked(int night);

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
    int rc = k ? cf_dictseti(d, k, val) : -1;
    if (k && cf_rel) cf_rel(k);
    return rc;
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
 * has not been decompiled for this field.  See docs/connect-and-usb/IOS_VERSION_GATE.md. */
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
 *   fullscreen rectangular safeArea: cells D2 through M5, cut at y=380
 *              = x 270..1170, y 90..380 = (270,90) 900x290
 *   sportscreen rectangular safeArea (compact Sport layout): B2 through E5, cut at y=390
 *              = x 90..450, y 90..390 = (90,90) 360x300
 *   classicscreen uses the same compact dimensions, centered horizontally
 *              = x 540..900, y 90..390 = (540,90) 360x300
 *
 * Bottom edge: the VC draws its own translucent map bar ("10ft ... 100ft N" in
 * fullscreen, "offroad" in the small stage) from about y=390 down over row 5
 * and hides row 6 behind the clock/temperature bar.  Maps anchors its bottom
 * text to the safeArea edge, so with the old y=452 edge that text sat in the
 * lower half of H5 under the bar (calibration-grid photos, 2026-09-19).
 *
 * The background may render under a physical mask; route focus and critical UI
 * stay inside these nested safe rectangles. */
#define FULL_SAFE_X     270
#define FULL_SAFE_Y      90
#define FULL_SAFE_W     900
#define FULL_SAFE_H     290

/* Zone safe areas run y=90..390 (10 px lower than fullscreen's 380) to stay 300 px tall: CarPlaySupport's
 * CPSInstrumentClusterCardLayout treats a safe area narrower than ~498 px (660 px at its
 * 0.153 mm reference pitch, our pitch 292/1440 mm) as "narrow" and then needs
 * 0.3*height > ~87 px for the centred card layout; below that it logs "Undefined layout
 * (0), will not show cluster content" and hides ETA and turn card for third-party apps. */
#define SMALLSCREEN_SAFE_X       90
#define SMALLSCREEN_SAFE_Y       90
#define SMALLSCREEN_SAFE_W      360
#define SMALLSCREEN_SAFE_H      300
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
    /* Native MMI touchpad.  Stock dio registers the HID trackpad ("Eso_TrackPad") but builds
     * "features" from the knob and touchscreen flags only, never bit 0x10, and sends no
     * primaryInputDevice.  iOS then picks CarPlay knob focus mode, which has no swipe
     * recognizer: a pad touch only highlights the focused item.  Trackpad focus mode
     * (_UIFocusBehavior_CarPlayTouchpad) needs features & 0x10 and primaryInputDevice 2
     * (CarKit CARScreenInfo: 1 touch, 2 trackpad, 3/4 knob; 0 = pick touch > knob > trackpad).
     * See docs/reference/ARCHITECTURE.md, "MMI touchpad".
     * DISABLED 2026-10-13: in trackpad mode Apple Music on iOS 27.2 beta aborts on any pad
     * input (UIKit NSAssert "pan-gesture-based focus movement without a touchpad"; every
     * other app is fine).  The pad goes through the Java DPAD bridge (TouchpadController)
     * in knob focus mode again.  Flip to #if 1 once iOS is fixed. */
#if 0
    {
        int64_t features = 0;
        void *k = cf_str(0, "features", CFENC_UTF8);
        if (k && cf_geti64) features = cf_geti64(mainDisp, k, NULL);
        if (k && cf_rel) cf_rel(k);
        set_i64(mainDisp, "features", features | 0x10);
        LOG_INFO(LOG_MODULE, "main display: features 0x%llx -> 0x%llx",
                 (unsigned long long)features, (unsigned long long)(features | 0x10));
    }
    /* complete the main dict like a real HU (MHI3 set) — see build_cluster_dict note.
     * MHI2Q has no touchscreen hardware, so the pad is always the primary input. */
    set_i64(mainDisp, "primaryInputDevice", 2);
#else
    /* complete the main dict like a real HU (MHI3 set) — see build_cluster_dict note.
     * 0 = iOS picks (touch > knob > trackpad): knob focus mode, stock features. */
    set_i64(mainDisp, "primaryInputDevice", 0);
#endif
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
    /* a later session reusing this address seeds from its own /info again */
    if (!session || g_alt_night_dio_session == session) g_alt_night_dio_session = NULL;
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
 *   so CFDictionarySetValue on it is safe.  The framework owns the interposition
 *   itself (framework/airplay_seams.c) and calls alt_on_server_info below. */
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
/* Explicit QNX laboratory mode: capture the native stream without pretending
 * that the absent hardware renderer presented it. Production has no such mode. */
static int alt_qemu_capture_only(void)
{
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    const char *value = getenv("MHI2Q_CP_QEMU_CAPTURE_ONLY");
    return value && !strcmp(value,"1");
#else
    return 0;
#endif
}

static int g_forward_available = 1; /* module init clears this on worker failure */
static int alt_wireless_blocked(void)
{
    if (!cp_dio_owner_is_wireless()) return 0;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    return !cp_media_alt_available();
#else
    return 1;
#endif
}
#ifdef MHI2Q_WIRELESS_ALT_QEMU
static pthread_once_t cluster_capture_once = PTHREAD_ONCE_INIT;
static void (*cluster_capture)(const void *, uint32_t);
static void cluster_capture_resolve(void)
{ *(void **)(&cluster_capture) = dlsym(RTLD_DEFAULT,"mhi2q_probe_alt_video"); }
static void cluster_capture_native(const void *data, uint32_t length)
{
    pthread_once(&cluster_capture_once,cluster_capture_resolve);
    if (cluster_capture) cluster_capture(data,length);
}
#endif

static int advert_preflight(void *session, uint32_t sequence)
{
    if (!g_forward_available) return 0;
    void *displays = NULL;
    void *features = NULL;
    if (alt_wireless_blocked() || !session ||
        !env_on("ALTSCREEN_ADVERT_DISPLAYS") || disp_ok != 1)
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

/* /info seam (module def's on_server_info).  The framework owns
 * AirPlayCopyServerInfo and hands us the mutable dictionary stock just built.
 *
 * Advertise only when every primitive required by the self-contained 111
 * receiver resolves.  A partial advert would make iOS demand a contract the
 * stock receiver cannot satisfy and can restart the whole CarPlay session. */
static void alt_on_server_info(void *session, void *info)
{
    /* Production stream111 uses legacy AES and stays blocked for paired
     * media. Only the explicit QEMU build adds an authenticated alternative. */
    if (alt_wireless_blocked()) return;
    if (cf_init() == 1 && env_on("ALTSCREEN_HANDLE_ALT") &&
        alt_resolve() == 1 && advert_ready_for_session(session)) {
        /* Hold the retained contract stable until BOTH extendedFeatures and
         * display arrays have been inserted. Teardown may otherwise clear the
         * cache between the optimistic ready check and the second half of
         * /info, producing the exact partial advert this preflight prevents. */
        pthread_mutex_lock(&advert_lock);
        if (!g_advert_ready || g_advert_session != session ||
            !g_advert_displays || !g_advert_features) {
            pthread_mutex_unlock(&advert_lock);
            return;
        }
        /* MU1316 dio's serverCopyPropertyCallback returns its startInNightMode
         * byte for the stock top-level /info key "nightMode" (dio 0x160484-
         * 0x1604C4).  This is the one hook-only seam that also covers a phone
         * connected while the HU is already in night mode: read it before
         * replacing displays, cache it for first-IDR replay, and advertise the
         * same value in both main and cluster panel dictionaries. */
        int nightErr = 0;
        int infoNight = 0;
        void *nightKey = NULL;
        if (g_alt_night_dio_session == session) {
            infoNight = g_alt_night_mode ? 1 : 0;
            set_i64(info, "nightMode", infoNight);      /* the top-level key too */
            LOG_INFO(LOG_MODULE,
                     "cluster nightMode=%d kept from dio (stock /info value is stale)", infoNight);
        } else if ((nightKey = cf_str(0, "nightMode", CFENC_UTF8)) != NULL) {
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
static int alt_zone_mode_get(void);
static int  alt_show_ui_for_mode(uint32_t gen, int mode, const char *reason);
static int  alt_send_cluster_night_mode(uint32_t gen, int night, const char *reason);
static int  alt_request_cluster_idr(uint32_t gen, const char *reason);
static uint32_t alt_generation_for_session(void *session);
static void alt_command_revoke(uint32_t gen);
static void *alt_params_new(int wantUuid);
static void alt_cmd_done(int status, void *resp, void *ctx);
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
#define ALT_CMD_SHUTDOWN_WAIT_MS 750
#define ALT_CMD_SUPERSEDED (-ECANCELED)
typedef alt_command_item_t alt_cmd_item_t;
static pthread_mutex_t g_alt_cmd_lock = PTHREAD_MUTEX_INITIALIZER;
static alt_monotonic_condition_t g_alt_cmd_clock = {.fallback = PTHREAD_COND_INITIALIZER,
                                                    .active = &g_alt_cmd_clock.fallback};
static pthread_t g_alt_cmd_thread;
static alt_command_policy_t g_alt_commands = {.zoom_interval_us = ALT_ZOOM_INTERVAL_US,
                                              .zoom_tail_us = ALT_ZOOM_TAIL_US};
static int g_alt_cmd_created, g_alt_cmd_shutdown, g_alt_cmd_exited;

static int alt_resolve(void) {
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
    uint32_t    avGeneration; /* global transport-owner generation for exact lookup */
    int         inUse;
    uint8_t     key[16];     /* self-contained receiver: AES-CTR key ...  */
    uint8_t     iv[16];      /* ... + iv (derived in alt_setup)           */
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    int paired;
    uint8_t paired_key[32];
#endif
    volatile int stop;       /* teardown sets =1 → recv poll-loop exits (unblocks join) */
} altslot_t;
typedef struct {
    altslot_t *slot;
    uint32_t generation;   /* per-slot generation (slot-state reuse guard) */
    uint32_t av_gen;       /* global transport-owner generation (altav ownership) */
    int listenSock;
} altthread_arg_t;
#define ALT_MAX_SESSIONS 4
/* TearDown may wait for an in-progress SETUP to stop touching stock session
 * fields. Receiver collection itself is always deferred until threadExited. */
#define ALT_TEARDOWN_SETUP_WAIT_MS 2500
static altslot_t alt_slots[ALT_MAX_SESSIONS];
/* Monotonic transport-owner generation, assigned per successfully-created receiver
 * (guarded by alt_lock).  Distinct from the per-slot generation: it is GLOBAL so a
 * later receiver on any slot supersedes every earlier one as the single altav owner. */
static uint32_t g_alt_gen_counter = 0;

static int alt_slot_is_current_locked(const altslot_t *slot)
{
    return slot && g_alt_session && slot->session == g_alt_session &&
           slot->avGeneration != 0 &&
           slot->avGeneration == g_alt_session_gen;
}

/* SETUP never reuses a live receiver slot.  It retires the old generation and
 * allocates another slot, so the RTSP thread is not held for up to 1.8 seconds
 * while the old receiver leaves a stock/socket call. */
static altslot_t *alt_slot_alloc(void *session) /* caller holds alt_lock */
{
    int i;
    for (i = 0; i < ALT_MAX_SESSIONS; i++) {
        if (!alt_slots[i].inUse) {
            memset(&alt_slots[i], 0, sizeof(alt_slots[i]));
            alt_slots[i].session = session;
            alt_slots[i].altSock = -1;
            alt_slots[i].inUse = 1;
            return &alt_slots[i];
        }
    }
    return NULL;
}

static altslot_t *alt_slot_find_generation(void *session, uint32_t avGeneration)
{
    int i;
    for (i = 0; i < ALT_MAX_SESSIONS; i++) {
        altslot_t *slot = &alt_slots[i];
        if (slot->inUse && slot->session == session &&
            slot->avGeneration == avGeneration)
            return slot;
    }
    return NULL;
}

static void alt_deadline_after_ms(struct timespec *deadline, long ms);

/* pthread_cond_timedwait uses an absolute CLOCK_REALTIME deadline on QNX 6.5. */
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

/* Collect only a receiver which has already published threadExited.  There is
 * no wait here: SETUP/TEARDOWN merely set stop and close the listener; a later
 * GC pass performs this guaranteed-fast join. */
static int alt_reap_exited_thread(altslot_t *slot)
{
    pthread_t thread;
    uint32_t generation;

    if (!slot) return 0;
    pthread_mutex_lock(&alt_lock);
    if (!slot->threadCreated) {
        pthread_mutex_unlock(&alt_lock);
        return 0;
    }
    if (!slot->threadExited || slot->threadReaping) {
        pthread_mutex_unlock(&alt_lock);
        return -1;
    }
    slot->threadReaping = 1;
    thread = slot->thread;
    generation = slot->generation;
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

/* Reclaim slots whose receiver has already EXITED (a superseded stream on
 * fast reconnect) but whose stock TearDown never arrived.  Without this a few missed
 * teardowns fill all ALT_MAX_SESSIONS and every later SETUP fails with -6728.  Caller must
 * NOT hold alt_lock.  Exact transport generation, not the possibly-reused
 * session pointer, identifies the current slot. */
static void alt_gc_exited_slots(void)
{
    int i;
    for (i = 0; i < ALT_MAX_SESSIONS; i++) {
        altslot_t *slot = &alt_slots[i];
        int eligible = 0;
        pthread_mutex_lock(&alt_lock);
        if (slot->inUse && slot->threadCreated && slot->threadExited &&
            !slot->threadReaping && !slot->settingUp &&
            !alt_slot_is_current_locked(slot))
            eligible = 1;
        pthread_mutex_unlock(&alt_lock);
        if (!eligible) continue;
        alt_reap_exited_thread(slot);
        pthread_mutex_lock(&alt_lock);
        if (slot->inUse && !slot->threadCreated && !slot->settingUp &&
            !alt_slot_is_current_locked(slot)) {
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

#define ALT_HDR_SIZE  128
#define ALT_MAX_BODY  (4u * 1024 * 1024)
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
static uint64_t alt_pace_now_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000) + (uint64_t)ts.tv_nsec / 1000;
}

static void alt_pace_report(alt_pace_trace_t *s, uint32_t generation)
{
#if !ENABLE_ALTSCREEN_PACE_TRACE
    if (!log_verbose()) { alt_pace_reset_window(s); return; }   /* only with carplay_verbose */
#endif
    /* Bounded asynchronous writer, one aggregate per 300 video access units.
     * read includes network waits; work includes decrypt/command/queue submission.
     * Arrival alone cannot distinguish a slow sender from local backpressure. */
    log_write(LOG_LEVEL_WARN, "altscreen",
        "111 pace gen=%u n=%u fwd=%u arrival_us=%llu/%llu samples=%u zero=%u "
        "read_us=%llu/%llu work_us=%llu/%llu pts_us=%llu/%llu pts_n=%u bad=%u",
        (unsigned)generation, s->frames, s->forwarded,
        (unsigned long long)alt_pace_mean(&s->arrival), (unsigned long long)s->arrival.max,
        s->arrival.count, s->arrival.zero,
        (unsigned long long)alt_pace_mean(&s->read), (unsigned long long)s->read.max,
        (unsigned long long)alt_pace_mean(&s->work), (unsigned long long)s->work.max,
        (unsigned long long)alt_pace_mean(&s->pts), (unsigned long long)s->pts.max,
        s->pts.count, s->pts_invalid);
    alt_pace_reset_window(s);
}
#endif

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

#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
    alt_pace_trace_t pace;
    memset(&pace, 0, sizeof(pace));
#endif

    int use_aes = 1;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    cp_screen_crypto paired_crypto = {0,0};
    use_aes = !slot->paired;
#endif
    if (use_aes && (!aes_ctr_init || !aes_ctr_update ||
        aes_ctr_init(aesctx, slot->key, slot->iv) != 0)) {
        LOG_ERROR(LOG_MODULE, "alt recv: AES-CTR initialization failed");
        return;
    }
    aes_ready = use_aes;
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
    log_write(LOG_LEVEL_WARN, "altscreen", "111 pace begin gen=%u maxFPS=30; times are mean/max microseconds",
              (unsigned)av_gen);
#endif

    /* A decoder cannot start mid-GOP. Ask for a cluster-targeted IDR. The
     * forwarding worker owns generation-scoped retry backoff, including while
     * this receiver waits for another phone AU. */
    alt_request_cluster_idr(av_gen, "stream-start");


    for (;;) {
        /* A later receiver (fast reconnect) may have claimed the single renderer
         * transport.  Once superseded, exit BEFORE reading/forwarding/commanding so
         * this stream can neither touch the new owner's connection nor send control
         * commands (ForceKeyFrame/showUI) into the new session. */
        if (!altav_is_owner(av_gen)) {
            LOG_INFO(LOG_MODULE, "alt recv: superseded (gen=%u no longer owner) — exiting", (unsigned)av_gen);
            break;
        }
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
        uint64_t read_started_us = alt_pace_now_us();
#endif
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

            int config_ls = alt_video_config_to_annexb(body, bodySize,
                                                       cfg, sizeof(cfg), &clen);
            if (config_ls == 0) {
                LOG_INFO(LOG_MODULE,
                         "alt recv: screen-state config flags=0x%02x body=%u; keeping avcC",
                         hdr[6], bodySize);
                continue;
            }
            if (config_ls < 0) {
                LOG_ERROR(LOG_MODULE, "alt recv: invalid or oversized avcC config (%u bytes)", bodySize);
                break;
            }
            ls = (uint8_t)config_ls;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
            if (slot->paired) cluster_capture_native(cfg,clen);
#endif
            if (altav_enqueue_config(av_gen, cfg, clen) != 0) break;
            LOG_INFO(LOG_MODULE,
                     "alt recv: avcC config encoded=%dx%d body=%u lengthSize=%u annexb=%u bytes=%s%s",
                     (int)(cfgWidth + 0.5f), (int)(cfgHeight + 0.5f), bodySize, ls, clen,
                     avccHex, (bodySize > hn) ? "..." : "");
        } else if (opcode == 0) {                         /* encrypted AVCC video access unit */
            int is_idr,frame_valid;
            alt_avcc_frame_t prepared;
            alt_frame_meta_t frame_meta;
            unsigned meta_i;
            memset(&frame_meta, 0, sizeof(frame_meta));
            frame_meta.sequence = nframes + 1;
            frame_meta.received_us = alt_timing_now_us();
            for (meta_i = 0; meta_i < 8; ++meta_i)
                frame_meta.sender_pts |= (uint64_t)hdr[8 + meta_i] << (8 * meta_i);
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
            uint64_t arrived_us = alt_pace_now_us();
            uint64_t sender_pts = 0;
            int pace_forwarded = 0;
            unsigned pace_i;
            for (pace_i = 0; pace_i < 8; pace_i++)
                sender_pts |= (uint64_t)hdr[8 + pace_i] << (8 * pace_i);
#endif
            int crypto_error;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
            if (slot->paired)
                crypto_error=cp_screen_open(&paired_crypto,slot->paired_key,hdr,body,&bodySize,cp_pair_open);
            else
#endif
                crypto_error=aes_ctr_update(aesctx, body, bodySize, body);
            if (crypto_error != 0) {
                LOG_ERROR(LOG_MODULE, "alt recv: screen authentication/decryption failed rc=%d", crypto_error);
                break;
            }
            frame_valid=alt_avcc_prepare(body,bodySize,ls,ls==4,&prepared)==0;
            is_idr=frame_valid && prepared.idr;
            if(!frame_valid){if(!alt_qemu_capture_only())altav_note_input_loss(av_gen);renderer_wait_idr=1;alt_request_cluster_idr(av_gen,"invalid-AU");}
            /* Re-check ownership here: the header/body read above can block in poll for
             * up to 500 ms, during which a reconnect may have superseded us.  The control
             * commands below target g_alt_session and must never run for a stale stream. */
            if (!altav_is_owner(av_gen)) break;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
            if (slot->paired && frame_valid && prepared.annexb)
                cluster_capture_native(body,prepared.annexb_len);
#endif
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

            if (!alt_qemu_capture_only() && altav_take_failure(av_gen)) {
                renderer_wait_idr = 1;

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
            if (frame_valid && !renderer_wait_idr && !alt_qemu_capture_only()) {
                int forward_rc = altav_enqueue_owned(av_gen, &body, &bodycap, &prepared, &frame_meta);
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
                pace_forwarded = forward_rc >= 0;
#endif
                if (forward_rc < 0) {
                    renderer_wait_idr = 1;
                    alt_request_cluster_idr(av_gen, "renderer-link-lost");

                    LOG_INFO(LOG_MODULE,
                             "alt recv: renderer link lost — WAIT_IDR gate CLOSED");
                } else if (forward_rc > 0) {
                    LOG_INFO(LOG_MODULE,
                             "alt recv: renderer connected on IDR-safe access unit");
                }
            } else if (!alt_qemu_capture_only() && (nframes & 0x7f) == 0) {
                LOG_INFO(LOG_MODULE,
                         "alt recv: dropping frame pending fresh IDR (frame#%lu body=%u)",
                         nframes, bodySize);
            }
            ++nframes;
            /* Recovery retries are generation-scoped and timer-driven by the
             * forwarder, including when this phone scene produces no frames. */
#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
            {
                uint64_t done_us = alt_pace_now_us();
                alt_pace_record(&pace, arrived_us,
                    arrived_us >= read_started_us ? arrived_us - read_started_us : 0,
                    done_us >= arrived_us ? done_us - arrived_us : 0,
                    sender_pts, pace_forwarded);
                if (pace.frames >= 300) alt_pace_report(&pace, av_gen);
            }
#endif
            if ((nframes & 0xFF) == 0)
                LOG_INFO(LOG_MODULE,
                         "alt recv: %lu video frames received (last body=%u waitIDR=%d)",
                         nframes, bodySize, renderer_wait_idr);
        }
        /* opcodes 2/4/5 (keepalive/misc): body already consumed → ignore */
    }

#if ENABLE_LOGGING || ENABLE_ALTSCREEN_PACE_TRACE
    if (pace.frames) alt_pace_report(&pace, av_gen);
#endif
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
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    /* The instrument scene may be empty until selected. Wait for the actual
     * stream socket, after committed SETUP, before selecting that native UI. */
    if (slot->paired) alt_show_ui_for_mode(av_gen,alt_zone_mode_get(),"qemu-stream-connected");
#endif
    alt_recv_loop(slot, fd, av_gen);

done:
    if (fd >= 0) close(fd);
    /* Only close the renderer transport if WE still own it — a superseded receiver's
     * exit must not tear down the new owner's connection (that caused black 111 on
     * fast reconnect). */
    altav_shutdown_if_owner(av_gen);
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    /* No reaper may reuse this slot until threadExited is published below. */
    {
        volatile uint8_t *key_bytes = slot->paired_key;
        unsigned key_index;
        for (key_index=0; key_index<sizeof(slot->paired_key); ++key_index) key_bytes[key_index]=0;
    }
#endif
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
    int    retiredListeners[ALT_MAX_SESSIONS];
    int    retiredCount = 0;
    int    applyQos = 0;
    int64_t connID;
    uint8_t key[16], iv[16];
    void  *rdesc = NULL;
    altthread_arg_t *threadArg = NULL;
    uint32_t av_gen = 0;   /* transport-owner generation for THIS receiver (claimed on success) */
    uint32_t revokedCommandGen = 0;

    int i;

    alt_gc_exited_slots();   /* reclaim orphaned exited slots so a fresh slot is available */

    /* Pointer reuse is normal on a fast reconnect.  Retire every older slot
     * carrying this address, but never wait for its receiver.  Closing the
     * listening socket plus stop makes it leave asynchronously; the exact
     * avGeneration guard prevents it from targeting the new session. */
    pthread_mutex_lock(&alt_lock);
    for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
        altslot_t *old = &alt_slots[i];
        if (!old->inUse || old->session != session) continue;
        old->closing = 1;
        old->stop = 1;
        if (old->altSock >= 0) {
            retiredListeners[retiredCount++] = old->altSock;
            old->altSock = -1;
        }
        if (alt_slot_is_current_locked(old)) {
            revokedCommandGen = g_alt_session_gen;
            g_alt_session = NULL;
            g_alt_session_gen = 0;
        }
        if (!old->threadCreated && !old->settingUp &&
            !old->threadReaping)
            memset(old, 0, sizeof(*old));
    }
    pthread_mutex_unlock(&alt_lock);
    if (revokedCommandGen) alt_command_revoke(revokedCommandGen);
    for (i = 0; i < retiredCount; ++i) close(retiredListeners[i]);

    /* A receiver which was already terminal can now be joined without any
     * wait.  A still-running one keeps its old slot until a future GC pass. */
    alt_gc_exited_slots();

    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_alloc(session);
    if (slot) slot->settingUp = 1;
    pthread_mutex_unlock(&alt_lock);
    if (!slot) {
        LOG_ERROR(LOG_MODULE,
                  "alt_setup: all %d receiver slots are still retiring; refusing without blocking",
                  ALT_MAX_SESSIONS);
        return -6728;
    }
    LOG_INFO(LOG_MODULE, "alt_setup: begin session=%p streamDesc=%p (OFF aes=%d fam=%d srv=%d)",
             session, streamDesc, OFF_AESKEY, OFF_FAMILY, OFF_SERVER);

    {
        void *kconn = cf_str(0, "streamConnectionID", CFENC_UTF8);
        connID = kconn ? cf_geti64(streamDesc, kconn, NULL) : 0;
        if (kconn && cf_rel) cf_rel(kconn);
    }
    if (!connID) { LOG_ERROR(LOG_MODULE, "alt_setup: connID missing (streamConnectionID absent)"); err = -6715; goto fail; }
    LOG_INFO(LOG_MODULE, "alt_setup: connID=%llu", (unsigned long long)connID);

    int use_legacy = 1;
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    if (cp_dio_owner_is_wireless()) {
        err=cp_media_alt_claim(session,(uint64_t)connID,slot->paired_key);
        if(err)goto fail;
        slot->paired=1;
        use_legacy=0;
        LOG_INFO(LOG_MODULE,"paired stream 111 QEMU: fresh authenticated key claimed");
    }
#endif
    if (use_legacy) {
        derive_aes((const uint8_t *)((char *)session + OFF_AESKEY), 16, (uint64_t)connID, key, iv);
        /* Self-contained receiver; no stock ScreenStream. */
        memcpy(slot->key, key, 16);
        memcpy(slot->iv,  iv,  16);
        memset(key, 0, sizeof(key));
        memset(iv, 0, sizeof(iv));
    }

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
#ifdef MHI2Q_WIRELESS_ALT_QEMU
    if (slot->paired) set_i64(rdesc,"streamConnectionID",connID);
#endif
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
    slot->avGeneration = av_gen;
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
    /* Commit the SETUP atomically with respect to Java/BAP/dio session commands. Until
     * the response descriptor exists and the monotonic transport claim succeeds,
     * no command may target this new session. Publishing it before this point allowed a
     * gen=0 command to run while the new control SETUP was still being built. Hold
     * alt_lock across claim+publication. No altav path holds
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
    if (sock >= 0) { close(sock); sock = -1; }
    pthread_mutex_lock(&alt_lock);
    if (g_alt_session == session && g_alt_session_gen == slot->avGeneration) {
        revokedCommandGen = g_alt_session_gen;
        g_alt_session = NULL;
        g_alt_session_gen = 0;
    }
    /* A started receiver owns the slot until it publishes threadExited.  Stop
     * it and defer collection; failure must not add a synchronous join to the
     * RTSP SETUP path. */
    if (!slot->threadCreated) {
        memset(slot, 0, sizeof(*slot));
    } else {
        slot->stop = 1;
        slot->closing = 1;
        sock = slot->altSock;
        slot->altSock = -1;
    }
    if (slot->inUse) slot->settingUp = 0;
    pthread_cond_broadcast(&alt_cond);
    pthread_mutex_unlock(&alt_lock);
    if (revokedCommandGen) alt_command_revoke(revokedCommandGen);
    if (sock >= 0) close(sock);
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

/* Enhanced Siri probe, step 1 of docs/altscreen-and-video/ENHANCED_SIRI_CONTRACT.md: log
 * the phone's control-SETUP feature proposal.  Behaviour is unchanged; the line only tells
 * whether this phone would ever propose "enhancedSiri" to this car (WARN, so it reaches the
 * default log). */
static void alt_log_phone_features(void *request)
{
    typedef unsigned long (*fn_typeof_t)(const void *);
    typedef unsigned long (*fn_typeid_t)(void);
    typedef int (*fn_getcstr_t)(const void *, char *, long, uint32_t);
    fn_typeof_t typeOf = (fn_typeof_t)dlsym(RTLD_DEFAULT, "CFGetTypeID");
    fn_typeid_t arrayType = (fn_typeid_t)dlsym(RTLD_DEFAULT, "CFArrayGetTypeID");
    fn_typeid_t stringType = (fn_typeid_t)dlsym(RTLD_DEFAULT, "CFStringGetTypeID");
    fn_getcstr_t getCStr = (fn_getcstr_t)dlsym(RTLD_DEFAULT, "CFStringGetCString");
    char list[320];
    size_t w = 0;
    int siri = 0;
    long n, i;
    void *k, *arr;
    if (!typeOf || !arrayType || !stringType || !getCStr) return;
    k = cf_str(0, "features", CFENC_UTF8);
    arr = k ? cf_getval(request, k) : NULL;
    if (k && cf_rel) cf_rel(k);
    if (!arr || typeOf(arr) != arrayType()) return;
    list[0] = 0;
    n = cf_arrcount(arr);
    for (i = 0; i < n; i++) {
        void *e = cf_arridx(arr, i);
        char name[48];
        int r;
        if (!e || typeOf(e) != stringType()) continue;
        if (!getCStr(e, name, sizeof(name), CFENC_UTF8)) continue;
        if (!strcmp(name, "enhancedSiri")) siri = 1;
        r = snprintf(list + w, sizeof(list) - w, "%s%s", w ? "," : "", name);
        if (r < 0 || (size_t)r >= sizeof(list) - w) break;
        w += (size_t)r;
    }
    LOG_WARN(LOG_MODULE, "control SETUP: phone proposes %ld features [%s]; enhancedSiri %s",
             n, list, siri ? "PROPOSED" : "not proposed");
}

/* SETUP seam (module def's on_setup_response).  The framework owns
 * AirPlayReceiverSessionSetup; stock handles 100/101/110 and skips 111, then we
 * negotiate altScreen and bring 111 up ourselves, appending its descriptor to
 * the response. */
static void alt_on_setup_response(hook_setup_ctx_t *setup)
{
    if (alt_wireless_blocked()) return;
    void *session = setup->session;
    void *request = setup->request;
    void *respOut = setup->response;
    uint32_t setupSequence = setup->sequence;
    int ret = setup->result;
    int setupIsControl = 0;
    int setupHas111 = 0;

    if (alt_enabled < 0) alt_enabled = env_on("ALTSCREEN_HANDLE_ALT") ? 1 : 0;

    if (alt_enabled && ret == 0 && respOut && session && request && alt_resolve() == 1) {
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
        if (isControlSetup) alt_log_phone_features(request);
        if (isControlSetup && advert_preflight(session, setupSequence)) {
            int featureEnabled = 0;
            void *kEnab = cf_str(0, "enabledFeatures", CFENC_UTF8);
            if (kEnab) {
                void *cur  = cf_getval(respOut, kEnab);            /* receiver's existing accepted set (usually NULL on 210) */
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
                        cf_dictset(respOut, kEnab, neg); /* existing + contracts implemented by this hook */
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
            int arc = alt_setup(session, altDesc, respOut);
            LOG_INFO(LOG_MODULE, "SETUP: alt_setup returned %d", arc);
            if (arc != 0) ret = arc;  /* never return HTTP success without a usable 111 dataPort */
        } else {
            LOG_INFO(LOG_MODULE, "SETUP: no 111 stream in request (phone did not offer altScreen)");
        }
    } else if (alt_enabled && ret == 0) {
        LOG_INFO(LOG_MODULE, "SETUP: alt gated off in interpose (resp=%p session=%p resolve=%d)",
                 respOut, session, alt_resolve());
    }
    LOG_INFO(LOG_MODULE, "SETUP seq=%u control=%d alt111=%d ret=%d",
             (unsigned)setupSequence, setupIsControl, setupHas111, ret);
    setup->result = ret;
}

/* TearDown seam (module def's on_teardown).  A non-NULL request is a per-stream
 * teardown; only stop our receiver when that request actually contains type
 * 111 — stock also calls this entry for unrelated streams while altScreen
 * remains live. A receiver never dereferences the stock session pointer; only
 * an in-progress SETUP does, so TearDown waits for that bounded transaction but
 * leaves receiver joins to exited-slot GC. */
static void alt_on_teardown(void *session, void *request, int reason)
{
    uint32_t revokedCommandGen = 0;
    int requestHas111 = 0;
    int tearDownAlt = (request == NULL);
    int setupTimedOut = 0;
    int listeners[ALT_MAX_SESSIONS];
    int listenerCount = 0;
    int haveSlot = 0;
    int i;

    if (request && alt_resolve() == 1) {
        requestHas111 = (alt_find_111(request) != NULL);
        if (requestHas111) tearDownAlt = 1;
    }

    LOG_INFO(LOG_MODULE,
             "altScreen teardown session=%p request=%p reason=%d alt111=%d action=%s",
             session, request, reason, requestHas111,
             tearDownAlt ? "stop-alt" : "preserve-alt");

    pthread_mutex_lock(&alt_lock);
    if (tearDownAlt) {
        struct timespec setupDeadline;
        int waitRc = 0;

        for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
            altslot_t *slot = &alt_slots[i];
            if (!slot->inUse || slot->session != session) continue;
            haveSlot = 1;
            slot->closing = 1;
            slot->stop = 1;
        }
        alt_deadline_after_ms(&setupDeadline, ALT_TEARDOWN_SETUP_WAIT_MS);
        for (;;) {
            int setupActive = 0;
            for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
                if (alt_slots[i].inUse && alt_slots[i].session == session &&
                    alt_slots[i].settingUp) {
                    setupActive = 1;
                    break;
                }
            }
            if (!setupActive || waitRc == ETIMEDOUT) {
                setupTimedOut = setupActive;
                break;
            }
            waitRc = pthread_cond_timedwait(&alt_cond, &alt_lock,
                                            &setupDeadline);
        }
        if (setupTimedOut) {
            LOG_WARN(LOG_MODULE,
                     "altScreen TearDown: setup still active after %d ms; revoking generation and deferring slot GC",
                     ALT_TEARDOWN_SETUP_WAIT_MS);
        }

        /* Revoke command ownership and wake every generation sharing this
         * possibly-reused session address. */
        if (g_alt_session == session) {
            revokedCommandGen = g_alt_session_gen;
            g_alt_session = NULL;
            g_alt_session_gen = 0;
        }
        for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
            altslot_t *slot = &alt_slots[i];
            if (!slot->inUse || slot->session != session) continue;
            if (slot->altSock >= 0) {
                listeners[listenerCount++] = slot->altSock;
                slot->altSock = -1;
            }
            if (!slot->threadCreated && !slot->settingUp &&
                !slot->threadReaping)
                memset(slot, 0, sizeof(*slot));
        }
        pthread_cond_broadcast(&alt_cond);
    }
    pthread_mutex_unlock(&alt_lock);
    if (revokedCommandGen) alt_command_revoke(revokedCommandGen);
    for (i = 0; i < listenerCount; ++i) close(listeners[i]);
    if (tearDownAlt) advert_clear_for_session(session);
    if (haveSlot && tearDownAlt) {
        alt_gc_exited_slots();
        if (setupTimedOut)
            LOG_WARN(LOG_MODULE,
                     "altScreen teardown proceeding with setup cleanup deferred session=%p",
                     session);
        else
            LOG_INFO(LOG_MODULE,
                     "altScreen torn down; receiver cleanup is asynchronous session=%p",
                     session);
    } else if (tearDownAlt) {
        LOG_INFO(LOG_MODULE, "altScreen teardown had no receiver session=%p", session);
    } else if (haveSlot) {
        LOG_INFO(LOG_MODULE,
                 "per-stream TEARDOWN has no 111 - preserving altScreen session=%p",
                 session);
    }
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
/* Java's confirmed RGI presentation (CMD_ALT_RGI); selects the Maps cluster URL. */
static volatile int g_alt_rgi_active = 0;

static int alt_zone_mode_get(void) { return __atomic_load_n(&g_alt_zone_mode, __ATOMIC_ACQUIRE); }
static void alt_zone_mode_set(int mode) {
    __atomic_store_n(&g_alt_zone_mode, mode, __ATOMIC_RELEASE);
}
static int alt_rgi_active_get(void) { return __atomic_load_n(&g_alt_rgi_active, __ATOMIC_ACQUIRE); }
static void alt_rgi_active_set(int active) {
    __atomic_store_n(&g_alt_rgi_active, active, __ATOMIC_RELEASE);
}

static const char *alt_zone_name(int mode) {
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

/* SDK completion runs on libairplay's HTTP-client thread; pending cancellation
 * or synchronous send failure completes on the producer/command worker.
 * bus_send is mutex-backed/thread-safe for all these callers. */
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
    if (status == 0 && mode == alt_zone_mode_get())
        alt_show_ui_for_mode(gen, mode, "viewArea-accepted");
}

static uint32_t alt_generation_for_session(void *session)
{
    uint32_t gen = 0;
    pthread_mutex_lock(&alt_lock);
    if (g_alt_session && (!session || g_alt_session == session)) {
        altslot_t *s = alt_slot_find_generation(g_alt_session,
                                                g_alt_session_gen);
        if (s && s->threadCreated && !s->threadExited &&
            !s->closing && !s->stop)
            gen = g_alt_session_gen;
    }
    pthread_mutex_unlock(&alt_lock);
    return gen;
}

static int alt_generation_is_current(uint32_t gen)
{
    return gen != 0 && alt_generation_for_session(NULL) == gen;
}

/* Acquire a retained live altScreen session. Stock 210
 * AirPlayReceiverSessionSendCommand@0x28460 dereferences session+704 at 0x28468,
 * before its own conditional CFRetain at 0x28548. Retain while holding alt_lock,
 * then DROP the lock for the stock call: a wedged HTTP client can no longer pin
 * TEARDOWN behind alt_lock, while the external retain prevents session UAF.
 * Returns the retained target, or NULL if no active session/shipper. */
static void *alt_retain_session_for_gen(uint32_t gen)
{
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
        altslot_t *s = alt_slot_find_generation(g_alt_session,
                                                g_alt_session_gen);
        if (s && s->threadCreated && !s->threadExited &&
            !s->closing && !s->stop) {
            sess = cf_ret(g_alt_session);
        }
    }
    pthread_mutex_unlock(&alt_lock);
    return sess;
}

static int alt_send_locked_for_gen(uint32_t gen, void *cmd, void *completion, void *ctx)
{
    int rc = -6735;
    void *sess = alt_retain_session_for_gen(gen);
    if (sess) {
        rc = sess_sendcmd(sess, cmd, completion, ctx);
        cf_rel(sess);
    }
    return rc;
}

#include "command_worker.h"

/* Build and ship one of the three predeclared view-area indexes. */
static int zone_send_mode(uint32_t gen, int mode, int dur, void *completion)
{
    int index, rc = -6728;
    void *params = NULL, *cmd = NULL, *adj = NULL, *pk = NULL, *ak = NULL;

    if (!gen) gen = alt_generation_for_session(NULL);
    if (!gen) return -6735; /* callback context and queued command use one snapshot */

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
        if (set_str(params, "uuid", CLUSTER_DISPLAY_UUID) != 0 ||
            set_i64(params, "viewAreaIndex", index) != 0 ||
            set_i64(params, "animationDurationMillis", dur) != 0 ||
            (mode != ALT_ZONE_FULLSCREEN && cf_arrappi64(adj, 0) != 0)) goto done;
        ak = cf_str(0, "adjacentViewAreas", CFENC_UTF8);
        if (!ak) goto done;
        cf_dictset(params, ak, adj);
        if (set_str(cmd, "type", "updateViewArea") != 0) goto done;
        pk = cf_str(0, "params", CFENC_UTF8);
        if (!pk) goto done;
        cf_dictset(cmd, pk, params);
        uintptr_t packed = (((uintptr_t)(gen & 0x3FFFFFFFU)) << 2) |
                           (uintptr_t)(mode & 3);
        rc = alt_send_async_for_gen_key(gen, cmd, completion,
                                    completion ? (void *)packed : NULL, ALT_COMMAND_VIEW_AREA);
        LOG_INFO(LOG_MODULE,
                 "updateViewArea ship mode=%s index=%d adjacent=%s dur=%dms rc=%d",
                 alt_zone_name(mode), index,
                 mode == ALT_ZONE_FULLSCREEN ? "[]" : "[0]", dur, rc);
    }
done:
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
    alt_zone_mode_set(mode);
    gen = alt_generation_for_session(NULL);
    rc = zone_send_mode(gen, mode, dur, (void *)zone_cmd_done);
    if (rc != 0 && (!gen || alt_generation_is_current(gen))) zone_send_ack(mode, rc); /* no async completion on sync failure */
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
    if (p && wantUuid && set_str(p, "uuid", CLUSTER_DISPLAY_UUID) != 0) {
        if (cf_rel) cf_rel(p);
        return NULL; /* never send a cluster command with no display address */
    }
    return p;
}

/* ship {type, params} with the logging completion; consumes (releases) params. */
static int alt_ship(uint32_t gen, const char *type, void *params) {
    if (!params)
        return -6728;
    int rc = -6728;
    void *cmd = cf_dictm(0, 0, dict_keycb, dict_valcb), *pk = NULL;
    if (cmd) {
        if (set_str(cmd, "type", type) != 0) goto done;
        pk = cf_str(0, "params", CFENC_UTF8);
        if (!pk) goto done;
        cf_dictset(cmd, pk, params);
        unsigned key = !strcmp(type, "showUI")                ? ALT_COMMAND_SHOW_UI
                       : !strcmp(type, "setNightMode")        ? ALT_COMMAND_NIGHT_MODE
                       : !strcmp(type, "mapAppearanceUpdate") ? ALT_COMMAND_MAP_APPEARANCE
                       : !strcmp(type, "uiAppearanceUpdate")  ? ALT_COMMAND_UI_APPEARANCE
                                                              : ALT_COMMAND_GENERIC;
        if (!strcmp(type, "showUI"))
            rc = alt_send_async_for_gen_key(gen, cmd, (void *)alt_showui_done,
                                            (void *)(uintptr_t)gen, key);
        else
            rc = alt_send_async_for_gen_key(gen, cmd, (void *)alt_cmd_done, (void *)type, key);
        LOG_INFO(LOG_MODULE, "session cmd '%s' shipped gen=%u rc=%d", type, (unsigned)gen, rc);
    }
done:
    if (pk && cf_rel) cf_rel(pk);
    if (cmd && cf_rel) cf_rel(cmd);
    if (cf_rel)
        cf_rel(params);
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
    if (set_i64(p, "nightMode", night ? 1 : 0) != 0) {
        cf_rel(p);
        return -6728;
    }
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
HOOK_EXPORT int AirPlayReceiverSessionSetNightMode(void *session, int night,
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
    g_alt_night_dio_session = session;   /* this session's /info no longer seeds it */
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
 * accumulated as bounded pending intent. The shared worker paces semantic
 * actions, drops an old unsent direction on reversal, and never replays stale
 * zoom into a new generation. iOS still owns the actual map animation. */
static void zoom_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len,
                        void *ctx) {
    int step, rc;
    (void)type;
    (void)flags;
    (void)ctx;
    if (!payload || len < 1)
        return;
    step = (int)(int8_t)payload[0];
    if (!step)
        return;
    if (env_on("ALTSCREEN_ZOOM_INVERT"))
        step = -step;
    rc = alt_command_zoom(0, step);
    LOG_INFO(LOG_MODULE, "cluster zoom step=%d scheduled rc=%d uuid=%s", step, rc,
             CLUSTER_DISPLAY_UUID);
}

/* Stock 210 AirPlayReceiverSessionForceKeyFrame sends only
 * {type:"forceKeyFrame"}. iOS 26.6 then falls back to the primary screen stream.
 * Its command handler selects the requested screen through params["uuid"], so
 * build the generic command ourselves and target the
 * cluster UUID explicitly. This request intentionally has no completion callback:
 * recovery is proven by observing an actual type-5 NAL, and retaining a callback
 * for every retry would grow stock 210's asynchronous response queue. */
static int alt_send_cluster_idr_raw(uint32_t gen, const char *reason)
{
    void *p = alt_params_new(1);
    void *cmd = NULL;
    void *pk = NULL;
    int rc = -6728;
    if (!p) return -6728;
    cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (cmd) {
        if (set_str(cmd, "type", "forceKeyFrame") != 0) goto done;
        pk = cf_str(0, "params", CFENC_UTF8);
        if (!pk) goto done;
        cf_dictset(cmd, pk, p);
        rc = alt_send_async_for_gen_key(gen, cmd, NULL, NULL, ALT_COMMAND_IDR);
    }
done:
    if (pk && cf_rel) cf_rel(pk);
    if (cmd && cf_rel) cf_rel(cmd);
    if (cf_rel) cf_rel(p);
    LOG_INFO(LOG_MODULE,
             "alt: cluster forceKeyFrame uuid=%s reason=%s rc=%d",
             CLUSTER_DISPLAY_UUID, reason ? reason : "?", rc);
    return rc;
}

static int alt_request_cluster_idr(uint32_t gen,const char *reason)
{
    if (alt_qemu_capture_only()) return alt_send_cluster_idr_raw(gen, reason);
    unsigned kind=ALT_KEYFRAME_RECOVERY;
    if(reason && !strcmp(reason,"stream-start"))kind=ALT_KEYFRAME_START;
    else if(reason && !strcmp(reason,"showUI-accepted"))kind=ALT_KEYFRAME_UI;
    if(!altav_keyframe_permit(gen,kind))return 0; /* coalesced, not a transport failure */
    return alt_send_cluster_idr_raw(gen,reason);
}

/* Apply the latest Audi fullscreen/smallscreen state once the 111 stream is live. The
 * Java command may have arrived before g_alt_session existed; alt_zone_mode_get()
 * intentionally preserves it across that window. */
static void alt_kick_viewarea(uint32_t gen)
{
    int mode = alt_zone_mode_get();
    int rc = zone_send_mode(gen, mode, 0, NULL);
    LOG_INFO(LOG_MODULE, "alt: initial updateViewArea mode=%s rc=%d",
             alt_zone_name(mode), rc);
}

/* Cluster scene URL (DashBoard -[DBInstrumentClusterURLHandler applySettingsForClusterURL:
 * toSceneSettings:], iOS 26.6 23G71; same keys in iOS 27 24A5418b):
 *   path  maps:/car/instrumentcluster                 -> itemType 3 (map + system cards)
 *         maps:/car/instrumentcluster/map             -> itemType 1 (map only)
 *         maps:/car/instrumentcluster/instructioncard -> itemType 2 (card only)
 *   query showETA / showSpeedLimit / showCompass (Maps branch, iOS 26.6 and 27;
 *           reverified 2026-09-30, docs/cluster-and-rgi/CARPLAY_SPEED_LIMIT.md):
 *           yes|1 -> enabled(1), no|0 -> disabled(2), user -> userPreference(3),
 *           anything else -> disabled; key names are case-sensitive, values are not;
 *           every one defaults to disabled(2) when the key is absent
 *         maneuverLayout=topAligned|leftAligned|rightAligned -> layoutJustification 1|2|3
 *         (any other value, e.g. "no", leaves the justification unset = 0)
 * The app-side CarPlaySupport card controller (CPSInstrumentClusterCardViewController)
 * draws ETA + turn card for third-party apps in ONE platter: layout 1 = centred column
 * (turn card top, ETA bottom), 2/3 = 213 px column at the left/right edge of the safe
 * area (turn card top, ETA bottom).  Apple Maps reads the same scene settings.
 * The normal protocol is app -> suggestUI{urls:[...]} -> HU chooses -> showUI{uuid,url};
 * stock 210 has no policy layer, so we choose: when the 111 stream first becomes
 * decodable, after every accepted view-area switch, and on every RGI edge from Java
 * (CMD_ALT_RGI is the runtime switch).  Same URL for every zone.
 *   RGI off -> map + cards (turn card right-aligned, ETA on), so the cluster shows
 *              guidance while our BAP/KDK path is idle;
 *   RGI on  -> /map = itemType 1 (map only): the root URL is "anyContent" and Maps keeps
 *              drawing its turn card + ETA there even with showETA=no (seen 2026-09-19);
 *              maneuvers and ETA come from BAP/KDK. */
#define ALT_CLUSTER_URL_RGI_OFF "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=rightAligned&showSpeedLimit=no"
#define ALT_CLUSTER_URL_RGI_ON  "maps:/car/instrumentcluster/map?showETA=no&maneuverLayout=no&showSpeedLimit=no"

static const char *alt_cluster_url(void)
{
    return alt_rgi_active_get() ? ALT_CLUSTER_URL_RGI_ON : ALT_CLUSTER_URL_RGI_OFF;
}

static int alt_show_ui_for_mode(uint32_t gen, int mode, const char *reason)
{
    const char *url = alt_cluster_url();
    if (!gen) gen = alt_generation_for_session(NULL);
    if (!gen) {
        LOG_INFO(LOG_MODULE,
                 "alt: showUI skipped (no committed altScreen generation) reason=%s",
                 reason ? reason : "?");
        return -6735;
    }
    void *p = alt_params_new(1);                        /* uuid = cluster display */
    if (!p) return -6728;
    if (set_str(p, "url", url) != 0) {
        cf_rel(p);
        return -6728;
    }
    int rc = alt_ship(gen, "showUI", p);
    LOG_INFO(LOG_MODULE, "alt: showUI mode=%s rgi=%d reason=%s uuid=%s url=%s rc=%d",
             alt_zone_name(mode), (int)alt_rgi_active_get(),
             reason ? reason : "?", CLUSTER_DISPLAY_UUID, url, rc);
    return rc;
}

static void alt_kick_show_ui(uint32_t gen)
{
    alt_show_ui_for_mode(gen, alt_zone_mode_get(), "first-IDR");
}

static void appearance_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 3) return;                    /* [target][mode][setting] */
    void *p = alt_params_new(1);
    if (!p) return;
    if (set_i64(p, "appearanceMode", (uint8_t)payload[1]) != 0 ||
        set_i64(p, "appearanceSetting", (uint8_t)payload[2]) != 0) {
        cf_rel(p);
        return;
    }
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
    if (set_str(p, "url", url) != 0) {
        cf_rel(p);
        return;
    }
    alt_ship(0, "changeUIContext", p);   /* Java → current session */
}

/* Java's confirmed RGI presentation edge.  Cached like the zone mode so a
 * pre-session edge is applied on the first IDR; a live session re-targets the
 * Maps scene at once (alt_show_ui_for_mode resolves the current generation). */
static void rgi_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 1) return;
    int active = payload[0] ? 1 : 0;
    if (active == alt_rgi_active_get()) return;
    alt_rgi_active_set(active);
    int rc = alt_show_ui_for_mode(0, alt_zone_mode_get(), "rgi-edge");
    LOG_WARN(LOG_MODULE, "alt: RGI presentation=%d -> cluster url %s rc=%d",
             active, alt_cluster_url(), rc);
}

static void alt_forward_failed(uint32_t gen, void *context)
{
    (void)context;
    if (!alt_qemu_capture_only()) alt_send_cluster_idr_raw(gen, "coordinated-recovery");
}

static void altscreen_init(void)
{
    g_forward_available = altav_async_start(alt_forward_failed, NULL) == 0;
    if (!g_forward_available)
        LOG_ERROR(LOG_MODULE, "AltScreen forwarding worker create failed; advert suppressed");
    bus_on(CMD_ALT_ZOOM, zoom_on_cmd, NULL);
    bus_on(CMD_ALT_ZONE, zone_on_cmd, NULL);
    bus_on(CMD_ALT_APPEARANCE, appearance_on_cmd, NULL);
    bus_on(CMD_ALT_UICTX, uictx_on_cmd, NULL);
    bus_on(CMD_ALT_RGI, rgi_on_cmd, NULL);
    LOG_INFO(LOG_MODULE, "AltScreen injector loaded (control negotiation + self-contained stream 111; cluster ZOOM/ZONE/APPEARANCE/UICTX/RGI registered; NIGHT mirrored from stock /info + SetNightMode)");
}

static void altscreen_shutdown(void)
{
    alt_command_worker_shutdown();
    altav_shutdown();
}

/* altScreen lives entirely on the AirPlay side: it negotiates the feature in
 * the SETUP response, publishes the two-display contract in /info, and owns the
 * type-111 receiver that TearDown must stop.  It reads no iAP2 message.
 * AirPlayReceiverSessionSetNightMode stays interposed here — it has exactly one
 * consumer, so it needs no framework seam. */
const hook_module_def_t altscreen_module_def = {
    .name = "altscreen",
    .priority = HOOK_PRIORITY_NORMAL,
    .on_init = altscreen_init,
    .on_shutdown = altscreen_shutdown,
    .airplay = {
        .on_setup_response = alt_on_setup_response,
        .on_server_info = alt_on_server_info,
        .on_teardown = alt_on_teardown
    }
};
