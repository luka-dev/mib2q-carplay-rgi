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

DEFINE_LOG_MODULE(altscreen);

#define CFENC_UTF8        0x08000100              /* kCFStringEncodingUTF8            */

/* ── libairplay CFLite builders (resolved lazily; all are dio imports) ─────────── */
typedef void *(*cf_str_t)(void *, const char *, unsigned);          /* CFStringCreateWithCString */
typedef void *(*cf_arrm_t)(void *, long, const void *);             /* CFArrayCreateMutable       */
typedef int   (*cf_arrappc_t)(void *, const char *, int);          /* CFArrayAppendCString(arr,str,len=-1) */
typedef void  (*cf_arrapp_t)(void *, const void *);                /* CFArrayAppendValue         */
typedef int   (*cf_arrappi64_t)(void *, int64_t);                   /* CFArrayAppendInt64         */
typedef void  (*cf_rel_t)(void *);                                 /* CFRelease                  */
typedef void  (*cf_dictset_t)(void *, const void *, const void *); /* CFDictionarySetValue       */
typedef void *(*cf_dictm_t)(void *, long, const void *, const void *); /* CFDictionaryCreateMutable */
typedef void  (*cf_dictseti_t)(void *, const void *, int64_t);     /* CFDictionarySetInt64       */
typedef int64_t (*fn_cfdict_geti64_t)(void *, const void *, void *); /* CFDictionaryGetInt64      */
typedef int64_t (*fn_cf_geti64_t)(void *, void *);                  /* CFGetInt64                 */
/* AirPlayReceiverSessionScreen_CopyDisplaysInfo(screenSession, outErr) — 280 ref
 * ignores the session arg ((void)inSession) and builds the MAIN display dict via
 * ScreenCopyMain(); we pass NULL. */
typedef void *(*fn_copydisp_t)(void *, void *);

static cf_str_t     cf_str;
static cf_arrm_t    cf_arrm;
static cf_arrappc_t cf_arrappc;
static cf_arrapp_t  cf_arrapp;
static cf_arrappi64_t cf_arrappi64;
static cf_rel_t     cf_rel;
static cf_dictset_t cf_dictset;
static cf_dictm_t   cf_dictm;
static cf_dictseti_t cf_dictseti;
static fn_cfdict_geti64_t cf_geti64;
static fn_cf_geti64_t cf_geti64_value;
static fn_copydisp_t fn_copydisp;
static const void  *arr_cb;          /* array callbacks constant (CFLite)            */
static const void  *dict_keycb;      /* dictionary key callbacks constant            */
static const void  *dict_valcb;      /* dictionary value callbacks constant          */
static int          cf_ok = -1;      /* extendedFeatures (#1a) builders ready         */
static int          disp_ok = -1;    /* displays (#1b) builders ready                 */
static pthread_mutex_t cf_lock = PTHREAD_MUTEX_INITIALIZER;
/* Seeded from stock /info's authoritative nightMode, then updated by the
 * dio->libairplay AirPlayReceiverSessionSetNightMode boundary call. */
static volatile int g_alt_night_mode;

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
    if (cf_ok < 0) {
        cf_str     = (cf_str_t)     dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
        cf_arrm    = (cf_arrm_t)    dlsym(RTLD_DEFAULT, "CFArrayCreateMutable");
        cf_arrappc = (cf_arrappc_t) dlsym(RTLD_DEFAULT, "CFArrayAppendCString");
        cf_arrapp  = (cf_arrapp_t)  dlsym(RTLD_DEFAULT, "CFArrayAppendValue");
        cf_arrappi64 = (cf_arrappi64_t)dlsym(RTLD_DEFAULT, "CFArrayAppendInt64");
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
                 cf_str && cf_dictset && cf_geti64 && cf_geti64_value) ? 1 : 0;
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
        cf_arrappc(arr, "altScreen", -1);   /* -1 = NUL-terminated (3rd arg is length) */
        cf_arrappc(arr, "viewAreas", -1);
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
static void set_str(void *d, const char *key, const char *val)
{
    void *k = cf_str(0, key, CFENC_UTF8);
    void *v = cf_str(0, val, CFENC_UTF8);
    if (k && v) cf_dictset(d, k, v);
    if (v && cf_rel) cf_rel(v);
    if (k && cf_rel) cf_rel(k);
}
static void set_i64(void *d, const char *key, int64_t val)
{
    void *k = cf_str(0, key, CFENC_UTF8);
    if (k) cf_dictseti(d, k, val);
    if (k && cf_rel) cf_rel(k);
}

/* Audi Kombi (instrument-cluster) panel — from firmware qcdisplaycfg_MMX2.xml
 * "Audi 1440x542 Kombi panel" (uVisWidth 1440 / uVisHeight 542); LVDS2 = Cluster
 * Display (displaymanager.json id 4).  CarPlay encodes the 111 stream at the size
 * we advertise here, so this MUST equal the panel. */
#define CLUSTER_W       1440
#define CLUSTER_H       542

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
 *   duo rectangular safeArea (the original compact layout): B2.5 through E5
 *              = x 90..450, y 135..452 = (90,135) 360x317
 *
 * The background may render under a physical mask; route focus and critical UI
 * stay inside these nested safe rectangles. */
#define FULL_SAFE_X     270
#define FULL_SAFE_Y      90
#define FULL_SAFE_W     900
#define FULL_SAFE_H     362

#define DUO_SAFE_X       90
#define DUO_SAFE_Y      135
#define DUO_SAFE_W      360
#define DUO_SAFE_H      317

/* one rect dict, iOS keys: {originXPixels, originYPixels, widthPixels, heightPixels}
 * (CarKit CROrigin/CRSizeFromAirPlayDictionary). */
static void *build_rect(int64_t x, int64_t y, int64_t w, int64_t h)
{
    void *d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!d) return 0;
    set_i64(d, "originXPixels", x);
    set_i64(d, "originYPixels", y);
    set_i64(d, "widthPixels",   w);
    set_i64(d, "heightPixels",  h);
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
    if (safe) {
        void *k = cf_str(0, "safeArea", CFENC_UTF8);
        if (k) cf_dictset(va, k, safe);
        if (k && cf_rel) cf_rel(k);
        if (cf_rel) cf_rel(safe);
    }
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
    set_i64(d, "originXPixels", 0);
    set_i64(d, "originYPixels", 0);
    set_str(d, "uuid", uuid);
    set_i64(d, "zIndex", 0);
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
    if (s) { cf_arrapp(arr, s); if (cf_rel) cf_rel(s); }
    return arr;
}

static void *build_cluster_dict(int night)
{
    void *d = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (!d) return 0;
    set_str(d, "name",          "Instrument Cluster");
    set_i64(d, "type",          111);
    set_i64(d, "maxFPS",        60);
    set_i64(d, "widthPixels",   CLUSTER_W);
    set_i64(d, "heightPixels",  CLUSTER_H);
    set_i64(d, "widthPhysical", 180);
    set_i64(d, "heightPhysical",68);
    /* CarKit's CARDisplayInfo parser reads the display identifier as "uid" (verified in the
     * iOS 26.1 CarKit decompile — initWithPhysicalScreenDictionary reads objectForKey:@"uid";
     * a missing uid → CARDisplayInfo init returns nil → the whole displays array is rejected →
     * the phone teardown-loops the session).  We previously sent "uuid" → that was the reject. */
    set_str(d, "uid", "C1057E12-A0D1-4E20-B0C0-1057E12A0D10");  /* CarKit displayPanels reads "uid" */
    /* AirPlaySender's raw "displays" path reads "uuid" and picks stream 110 vs 111 by whether a
     * display's uuid DIFFERS from display-0's uuid (VERIFIED in iOS 26.6 AirPlaySender: 110/111 is a
     * UUID compare, NOT the type:111 field).  The dict is injected under BOTH "displays" (uuid consumer)
     * and "displayPanels" (uid consumer), so carry BOTH ids.  Cluster uuid MUST differ from main's. */
    set_str(d, "uuid", "C1057E12-A0D1-4E20-B0C0-1057E12A0D10");
    /* properties = ["Instrument Cluster"] → CarKit supportsInstrumentClusterContent (+0xB).  NOTE
     * (verified 26.6): this is NOT the stream-111 gate and NOT hasGaugeClusterScreen (that predicate =
     * supportsDDPContent).  The 111 gate is the negotiated enabledFeatures altScreen flag + raw displays
     * count>=2 + UUID≠display0.  Kept because CarKit's physical-panel model still parses it. */
    {
        void *k   = cf_str(0, "properties", CFENC_UTF8);
        void *arr = cf_arrm(0, 1, arr_cb);
        if (arr) {
            if (cf_arrappc) cf_arrappc(arr, "Instrument Cluster", -1);
            else { void *s = cf_str(0, "Instrument Cluster", CFENC_UTF8);
                   if (s) { cf_arrapp(arr, s); if (cf_rel) cf_rel(s); } }
            if (k) cf_dictset(d, k, arr);
            if (cf_rel) cf_rel(arr);
        }
        if (k && cf_rel) cf_rel(k);
    }
    /* [0] fullscreen, [1] duo. The encoded and visible frame stays 1440x542;
     * the two entries carry different nested safe frames.  Match MHI3
     * AirPlayScreenDictSetViewAreas by declaring the initial current/adjacent
     * state alongside the array; these are display-level keys, not fields of
     * the individual view-area dictionaries. */
    {
        void *k   = cf_str(0, "viewAreas", CFENC_UTF8);
        void *arr = cf_arrm(0, 2, arr_cb);
        if (arr) {
            void *full = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                        FULL_SAFE_X, FULL_SAFE_Y,
                                        FULL_SAFE_W, FULL_SAFE_H);
            void *split = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                         DUO_SAFE_X, DUO_SAFE_Y,
                                         DUO_SAFE_W, DUO_SAFE_H);
            if (full)  { cf_arrapp(arr, full);  if (cf_rel) cf_rel(full); }
            if (split) { cf_arrapp(arr, split); if (cf_rel) cf_rel(split); }
            if (k) cf_dictset(d, k, arr);
            LOG_INFO(LOG_MODULE,
                     "declared viewAreas: visible=(0,0 %dx%d); full safe=(%d,%d %dx%d); duo safe=(%d,%d %dx%d)",
                     CLUSTER_W, CLUSTER_H,
                     FULL_SAFE_X, FULL_SAFE_Y, FULL_SAFE_W, FULL_SAFE_H,
                     DUO_SAFE_X, DUO_SAFE_Y, DUO_SAFE_W, DUO_SAFE_H);
            if (cf_rel) cf_rel(arr);
        }
        if (k && cf_rel) cf_rel(k);
    }
    set_i64(d, "initialViewArea", 0);
    {
        void *k = cf_str(0, "adjacentViewAreas", CFENC_UTF8);
        void *a = cf_arrm(0, 0, arr_cb);
        if (k && a) cf_dictset(d, k, a);
        if (a && cf_rel) cf_rel(a);
        if (k && cf_rel) cf_rel(k);
    }
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
        if (k && arr) cf_dictset(d, k, arr);
        if (arr && cf_rel) cf_rel(arr);
        if (k && cf_rel) cf_rel(k);
    }
    return d;
}

/* stock CopyDisplaysInfo emits the OLD 210 schema (identifier key "uuid", NO initialVideoStreams);
 * iOS-26.1 CarKit needs "uid" + a present "initialVideoStreams" array or it rejects the display →
 * the phone teardown-loops the session.  Patch the stock main dict into the modern schema. */
static void main_disp_modernize(void *mainDisp, int night)
{
    if (!mainDisp) return;
    /* CarKit reads the display identifier as "uid" (stock emits only the legacy "uuid").  The value
     * only needs to be a non-nil, unique string — give the main a fixed uid distinct from the cluster. */
    set_str(mainDisp, "uid", "A0000000-0000-4000-8000-0000000000A1");
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
    void *ivs  = build_stream_array("A0000000-0000-4000-8000-000000000001");
    if (kivs && ivs) cf_dictset(mainDisp, kivs, ivs);
    if (ivs && cf_rel) cf_rel(ivs);
    if (kivs && cf_rel) cf_rel(kivs);
}

static void *build_displays(int night)
{
    int err = 0;
    void *mainDisp = fn_copydisp(0, &err);      /* arg ignored; builds via ScreenCopyMain */
    main_disp_modernize(mainDisp, night);       /* uuid→uid + initialVideoStreams for CarKit */
    void *cluster  = build_cluster_dict(night);
    void *arr = cf_arrm(0, 2, arr_cb);
    if (!arr) {
        if (mainDisp && cf_rel) cf_rel(mainDisp);
        if (cluster && cf_rel)  cf_rel(cluster);
        return 0;
    }
    if (mainDisp) { cf_arrapp(arr, mainDisp); if (cf_rel) cf_rel(mainDisp); }
    if (cluster)  { cf_arrapp(arr, cluster);  if (cf_rel) cf_rel(cluster);  }
    return arr;
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

void *AirPlayCopyServerInfo(void *session, void *qualifier, void *macAddr, void *outErr)
{
    static fn_copyinfo real;
    if (!real) real = (fn_copyinfo)dlsym(RTLD_NEXT, "AirPlayCopyServerInfo");
    if (!real) { LOG_ERROR(LOG_MODULE, "AirPlayCopyServerInfo real==NULL"); return 0; }

    void *info = real(session, qualifier, macAddr, outErr);   /* stock /info — untouched */

    /* Advertise only when every primitive required by the self-contained 111
     * receiver resolves.  A partial advert would make iOS demand a contract the
     * stock receiver cannot satisfy and can restart the whole CarPlay session. */
    if (info && cf_init() == 1 && env_on("ALTSCREEN_HANDLE_ALT") &&
        alt_resolve() == 1) {
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

        /* #1a: extendedFeatures = ["altScreen", "viewAreas"] */
        void *k = cf_str(0, "extendedFeatures", CFENC_UTF8);
        void *v = build_extfeatures();
        if (k && v) {
            cf_dictset(info, k, v);                           /* add to the /info dict */
            LOG_INFO(LOG_MODULE, "injected extendedFeatures=[altScreen,viewAreas] into /info=%p", info);
        } else {
            LOG_ERROR(LOG_MODULE, "inject extendedFeatures build failed k=%p v=%p", k, v);
        }
        if (v && cf_rel) cf_rel(v);                           /* dict retained its own ref */
        if (k && cf_rel) cf_rel(k);

        /* displays/displayPanels provide the distinct second UUID that makes iOS
         * construct stream 111.  Keep the separate env gate for controlled rollback. */
        if (disp_ok == 1 && env_on("ALTSCREEN_ADVERT_DISPLAYS")) {
            void *dk = cf_str(0, "displays", CFENC_UTF8);
            void *dv = build_displays(infoNight);
            if (dk && dv) {
                cf_dictset(info, dk, dv);
                LOG_INFO(LOG_MODULE, "injected displays=[main,cluster] into /info=%p", info);
            } else {
                LOG_ERROR(LOG_MODULE, "inject displays build failed dk=%p dv=%p", dk, dv);
            }
            if (dv && cf_rel) cf_rel(dv);
            if (dk && cf_rel) cf_rel(dk);

            /* ALSO inject displayPanels = [main, cluster].  iOS reads BOTH keys:
             *  - "displays"      → Activate.mm:822 = the display-COUNT gate (needs count vs altScreen-flag).
             *  - "displayPanels" → CARSessionConfiguration builds _displays (→ hasGaugeClusterScreen =
             *    _displays contains "Instrument Cluster") via initWithPhysicalScreenDictionary — which reads
             *    exactly our shape (uid/widthPixels/widthPhysical/properties/initialVideoStreams).
             * Providing only "displays" (parsed as LOGICAL screens, different schema) likely leaves the
             * cluster unrecognized → carkitd never proposes "altScreen" → the negotiated altScreen flag
             * stays off → the 2-display count is rejected ("car won't add").  Give the phone displayPanels
             * in our physical schema so hasGaugeClusterScreen is set. */
            void *pk = cf_str(0, "displayPanels", CFENC_UTF8);
            void *pv = build_displays(infoNight);
            if (pk && pv) {
                cf_dictset(info, pk, pv);
                LOG_INFO(LOG_MODULE, "injected displayPanels=[main,cluster] into /info=%p", info);
            } else {
                LOG_ERROR(LOG_MODULE, "inject displayPanels build failed pk=%p pv=%p", pk, pv);
            }
            if (pv && cf_rel) cf_rel(pv);
            if (pk && cf_rel) cf_rel(pk);
        }
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
typedef void *   (*fn_srv_copyprop_t)(void *, int, void *, void *, int *); /* AirPlayReceiverServerPlatformCopyProperty */
typedef void *   (*fn_cfdict_getval_t)(void *, const void *);
typedef long     (*fn_cfarr_count_t)(void *);
typedef void *   (*fn_cfarr_getidx_t)(void *, long);

static fn_aes_ctr_init_t    aes_ctr_init;
static fn_aes_ctr_update_t  aes_ctr_update;
static fn_aes_ctr_final_t   aes_ctr_final;
static fn_sess_sendcmd_t    sess_sendcmd;   /* session-level {type,params} command shipper (zone switch) */
static void alt_kick_viewarea(void);        /* fwd: apply cached fullscreen/duo geometry when 111 is live */
static void alt_kick_show_ui(void);         /* fwd: select Maps content for the cluster display */
static int  alt_show_ui_for_mode(int mode, const char *reason); /* fwd: full cards vs duo map-only */
static int  alt_send_cluster_night_mode(int night, const char *reason); /* fwd: per-display day/night */
static int  alt_request_cluster_idr(const char *reason); /* fwd: forceKeyFrame targeted by params.uuid */
static fn_derive_aes_t      derive_aes;

/* main AirPlay session with a live altScreen — set on alt_setup success, cleared
 * on teardown.  Read by the CMD_ALT_ZONE handler to target AirPlayReceiverSessionSendCommand.
 * ponytail: single altScreen session assumed; aligned pointer store = atomic on ARMv7. */
static void *volatile g_alt_session;
static fn_srv_sockopen_t    srv_sockopen;
static fn_sock_setqos_t     sock_setqos;
static fn_sock_accept_t     sock_accept;
static fn_srv_copyprop_t    srv_copyprop;
static fn_cfdict_getval_t   cf_getval;
static fn_cfarr_count_t     cf_arrcount;
static fn_cfarr_getidx_t    cf_arridx;

static int alt_enabled = -1;    /* env ALTSCREEN_HANDLE_ALT */
static int alt_syms_ok = -1;
static pthread_mutex_t alt_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t alt_cond = PTHREAD_COND_INITIALIZER;

static int alt_resolve(void)
{
    pthread_mutex_lock(&alt_lock);   /* no lock-free fast path — see cf_init note */
    if (alt_syms_ok < 0) {
        if (cf_init() != 1) { alt_syms_ok = 0; pthread_mutex_unlock(&alt_lock); return 0; }
        aes_ctr_init    = (fn_aes_ctr_init_t)   dlsym(RTLD_DEFAULT, "AES_CTR_Init");
        aes_ctr_update  = (fn_aes_ctr_update_t) dlsym(RTLD_DEFAULT, "AES_CTR_Update");
        aes_ctr_final   = (fn_aes_ctr_final_t)  dlsym(RTLD_DEFAULT, "AES_CTR_Final");
        sess_sendcmd    = (fn_sess_sendcmd_t)   dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionSendCommand");
        derive_aes      = (fn_derive_aes_t)     dlsym(RTLD_DEFAULT, "AirPlay_DeriveAESKeySHA512ForScreen");
        srv_sockopen    = (fn_srv_sockopen_t)   dlsym(RTLD_DEFAULT, "ServerSocketOpen");
        sock_setqos     = (fn_sock_setqos_t)    dlsym(RTLD_DEFAULT, "SocketSetQoS");
        sock_accept     = (fn_sock_accept_t)    dlsym(RTLD_DEFAULT, "SocketAccept");
        srv_copyprop    = (fn_srv_copyprop_t)   dlsym(RTLD_DEFAULT, "AirPlayReceiverServerPlatformCopyProperty");
        cf_getval       = (fn_cfdict_getval_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
        cf_arrcount     = (fn_cfarr_count_t)    dlsym(RTLD_DEFAULT, "CFArrayGetCount");
        cf_arridx       = (fn_cfarr_getidx_t)   dlsym(RTLD_DEFAULT, "CFArrayGetValueAtIndex");
        /* Only symbols used by the live self-contained 111 path are critical. */
        alt_syms_ok = (sess_sendcmd && derive_aes &&
                       srv_sockopen && sock_accept && srv_copyprop &&
                       aes_ctr_init && aes_ctr_update && aes_ctr_final &&
                       cf_geti64 && cf_getval && cf_arrcount && cf_arridx &&
                       cf_dictm && cf_dictset && cf_dictseti && cf_arrapp) ? 1 : 0;
        LOG_INFO(LOG_MODULE, "alt_resolve syms_ok=%d", alt_syms_ok);
    }
    pthread_mutex_unlock(&alt_lock);
    return alt_syms_ok;
}

/* `/info` may be generated before dio's CarPlay startService has copied
 * ServiceConfiguration.startInNightMode into its service-state byte.  Query the
 * same authoritative dio callback again once stream 111 is actually decoding:
 * stock AirPlayReceiverServerPlatformCopyProperty dispatches key `nightMode`
 * through server->copyPropertyCallback (stock 210 0x20048-0x2006C), and the
 * MU1316 callback reads the now-live byte at dio 0x160484-0x1604C4.
 *
 * This is a direct hook->stock call, not an attempted interpose of an internal
 * libairplay call, so it is unaffected by the LD_PRELOAD boundary limitation. */
static int alt_refresh_night_mode(void *session, const char *reason)
{
    void *server;
    void *key;
    void *value;
    int copyErr = 0;
    int valueErr = 0;
    int night;

    if (!session || !srv_copyprop || !cf_geti64_value || !cf_str)
        return -6740;
    server = *(void **)((uint8_t *)session + OFF_SERVER);
    if (!server) return -6735;

    key = cf_str(0, "nightMode", CFENC_UTF8);
    if (!key) return -6728;
    value = srv_copyprop(server, 1, key, NULL, &copyErr);
    if (cf_rel) cf_rel(key);
    if (!value) {
        LOG_WARN(LOG_MODULE,
                 "alt: live nightMode refresh failed reason=%s copyErr=%d",
                 reason ? reason : "?", copyErr);
        return copyErr ? copyErr : -6714;
    }

    night = cf_geti64_value(value, &valueErr) ? 1 : 0;
    if (cf_rel) cf_rel(value);
    if (valueErr) {
        LOG_WARN(LOG_MODULE,
                 "alt: live nightMode conversion failed reason=%s valueErr=%d",
                 reason ? reason : "?", valueErr);
        return valueErr;
    }
    g_alt_night_mode = night;
    LOG_INFO(LOG_MODULE,
             "alt: refreshed live HU nightMode=%d reason=%s",
             night, reason ? reason : "?");
    return 0;
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
    uint32_t generation;
    int listenSock;
} altthread_arg_t;
#define ALT_MAX_SESSIONS 4
static altslot_t alt_slots[ALT_MAX_SESSIONS];

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

/* Claim and join exactly one receiver generation.  For a replacement SETUP,
 * only an already-exited thread is eligible.  TEARDOWN first requests stop and
 * closes the listener, then joins outside alt_lock. */
static int alt_reap_thread(altslot_t *slot, int requestStop)
{
    pthread_t thread;
    uint32_t generation;
    int listener = -1;

    if (!slot) return 0;
    pthread_mutex_lock(&alt_lock);
    while (slot->threadReaping)
        pthread_cond_wait(&alt_cond, &alt_lock);
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
        int pr = poll(&pfd, 1, 500);
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

static void alt_recv_loop(altslot_t *slot, int fd)
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
    alt_request_cluster_idr("stream-start");
    idr_requests = 1;
    last_idr_request_ms = alt_now_ms();

    for (;;) {
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
            if (clen) altav_set_config(cfg, clen);   /* cache; replayed before first safe IDR */
            LOG_INFO(LOG_MODULE,
                     "alt recv: avcC config encoded=%dx%d body=%u lengthSize=%u annexb=%u bytes=%s%s",
                     (int)(cfgWidth + 0.5f), (int)(cfgHeight + 0.5f), bodySize, ls, clen,
                     avccHex, (bodySize > hn) ? "..." : "");
        } else if (opcode == 0) {                         /* encrypted AVCC video access unit */
            int is_idr;
            if (aes_ready) aes_ctr_update(aesctx, body, bodySize, body);
            is_idr = avcc_has_nal_type(body, bodySize, ls, 5);
            if (is_idr && !stream_has_idr) {
                stream_has_idr = 1;
                LOG_INFO(LOG_MODULE,
                         "alt recv: first IDR arrived at frame#%lu (body=%u)",
                         nframes, bodySize);
                /* A live video stream is only the canvas.  Select the Maps instrument-cluster
                 * UI explicitly, then tell the encoder which predeclared view area is visible. */
                alt_kick_show_ui();
                /* `/info` can precede dio startService and contain the old
                 * day/night bit. Refresh from the live platform callback now,
                 * before applying the per-display cluster command. */
                alt_refresh_night_mode(slot->session, "first-IDR");
                alt_send_cluster_night_mode(g_alt_night_mode, "first-IDR");
                alt_kick_viewarea();
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
                int forward_rc = altav_forward_avcc_frame(body, bodySize, ls);
                if (forward_rc < 0) {
                    renderer_wait_idr = 1;
                    alt_request_cluster_idr("renderer-link-lost");
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
                    alt_request_cluster_idr("WAIT_IDR-retry");
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
    int        listenSock = threadArg->listenSock;
    int        fd = -1;
    int        ls;
    free(threadArg);

    /* No arbitrary accept deadline: teardown closes altSock and reliably wakes
     * SocketAccept.  A 10 s timeout left a joinable dead thread unreaped, so a
     * delayed phone connect permanently poisoned every later 111 re-SETUP. */
    if (sock_accept(listenSock, -1, &fd, NULL) != 0 || fd < 0) goto done;
    /* claim + close the listen socket (teardown may race to close it too — the
     * -1 sentinel under alt_lock ensures exactly one close). */
    pthread_mutex_lock(&alt_lock);
    if (slot->generation == generation && slot->altSock == listenSock) {
        ls = slot->altSock;
        slot->altSock = -1;
    } else {
        ls = -1;
    }
    pthread_mutex_unlock(&alt_lock);
    if (ls >= 0) close(ls);

    LOG_INFO(LOG_MODULE, "alt-screen receiver started (self-contained)");
    alt_recv_loop(slot, fd);

done:
    if (fd >= 0) close(fd);
    altav_shutdown();                                     /* disconnect renderer → it resets its decoder */
    pthread_mutex_lock(&alt_lock);
    if (slot->generation == generation && slot->threadCreated) {
        slot->threadExited = 1;
        pthread_cond_broadcast(&alt_cond);
    }
    pthread_mutex_unlock(&alt_lock);
    LOG_INFO(LOG_MODULE, "alt-screen receiver thread exit generation=%u",
             (unsigned)generation);
    return NULL;
}

/* SETUP response and transport for stream type 111.  No stock Screen object is
 * created: the hook owns the socket and decrypts the AirPlay screen wire itself. */
static int alt_setup(void *session, void *streamDesc, void *responseParams)
{
    altslot_t *slot;
    void  *server;
    int    err = -6719, family, port = 0, sock = -1, started = 0;
    int64_t connID;
    uint8_t key[16], iv[16];
    void  *rdesc = NULL;
    altthread_arg_t *threadArg = NULL;

    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_get(session);
    if (slot && (slot->closing || slot->settingUp)) {
        pthread_mutex_unlock(&alt_lock);
        return -6719;   /* already set up */
    }
    if (slot && slot->threadCreated && !slot->threadExited) {
        pthread_mutex_unlock(&alt_lock);
        return -6719;   /* live stream already set up */
    }
    if (slot) slot->settingUp = 1; /* reserve lifecycle across old-thread reap */
    pthread_mutex_unlock(&alt_lock);
    if (!slot) return -6728;
    if (alt_reap_thread(slot, 0) != 0) goto fail;

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
    err = srv_sockopen(family, 1 /*SOCK_STREAM*/, 6 /*IPPROTO_TCP*/, -6030, &port, -180224, &sock);
    LOG_INFO(LOG_MODULE, "alt_setup: srv_sockopen err=%d port=%d sock=%d family=%d", err, port, sock, family);
    if (err) goto fail;

    server = *(void **)((char *)session + OFF_SERVER);
    if (server && sock_setqos && !*(uint8_t *)((char *)server + SVR_OFF_QOSDISABLED))
        sock_setqos(sock, 12);

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
    slot->altSock = sock;
    slot->threadExited = 0;
    threadArg->slot = slot;
    threadArg->generation = slot->generation;
    threadArg->listenSock = sock;
    g_alt_session = session;      /* thread blocks on alt_lock before its first command */
    if (pthread_create(&slot->thread, NULL, alt_thread_main, threadArg) == 0) {
        slot->threadCreated = 1;
        started = 1;
        threadArg = NULL;              /* receiver owns it */
        sock = -1;                    /* slot/thread owns the listener now */
    } else {
        slot->altSock = -1;
        g_alt_session = NULL;
    }
    pthread_mutex_unlock(&alt_lock);
    if (!started) { err = -6728; goto fail; }
    if (alt_add_response_stream(responseParams, rdesc) != 0) {
        err = -6728;
        goto fail;
    }
    if (cf_rel) { cf_rel(rdesc); rdesc = NULL; }

    pthread_mutex_lock(&alt_lock);
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
    if (started) {
        alt_reap_thread(slot, 1);
    }
    if (sock >= 0) close(sock);
    pthread_mutex_lock(&alt_lock);
    if (g_alt_session == session) g_alt_session = NULL;   /* defensive (only set on success) */
    slot->altSock = -1;
    slot->stop = 0;
    memset(slot->key, 0, sizeof(slot->key));
    memset(slot->iv, 0, sizeof(slot->iv));
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
    uint64_t setupBeginMs = alt_now_ms();
    uint64_t realDoneMs;
    int setupIsControl = 0;
    int setupHas111 = 0;
    if (!real) real = (fn_sess_setup_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionSetup");
    if (!real) { LOG_ERROR(LOG_MODULE, "SessionSetup real==NULL"); return -6740; }

    int ret = real(session, request, respOut, a4);
    realDoneMs = alt_now_ms();

    if (alt_enabled < 0) alt_enabled = env_on("ALTSCREEN_HANDLE_ALT") ? 1 : 0;
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
        if (isControlSetup) {
            void *kEnab = cf_str(0, "enabledFeatures", CFENC_UTF8);
            if (kEnab) {
                void *cur  = cf_getval(*respOut, kEnab);            /* receiver's existing accepted set (usually NULL on 210) */
                long  curN = (cur && cf_arrcount) ? cf_arrcount(cur) : -1;
                void *neg  = cf_arrm(0, 0, arr_cb);
                if (neg) {
                    long i;
                    for (i = 0; i < curN; i++) {                    /* preserve stock-accepted features */
                        void *e = cf_arridx(cur, i);
                        if (e) cf_arrapp(neg, e);
                    }
                    if (cf_arrappc) {
                        cf_arrappc(neg, "altScreen", -1);
                        cf_arrappc(neg, "viewAreas", -1);
                    } else {
                        void *s = cf_str(0, "altScreen", CFENC_UTF8);
                        if (s) { cf_arrapp(neg, s); if (cf_rel) cf_rel(s); }
                        s = cf_str(0, "viewAreas", CFENC_UTF8);
                        if (s) { cf_arrapp(neg, s); if (cf_rel) cf_rel(s); }
                    }
                    cf_dictset(*respOut, kEnab, neg); /* existing + contracts implemented by this hook */
                    if (cf_rel) cf_rel(neg);
                    LOG_INFO(LOG_MODULE,
                             "control SETUP: response enabledFeatures merged (existing count=%ld) + altScreen + viewAreas",
                             curN);
                }
                if (cf_rel) cf_rel(kEnab);
            }
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
        slot->closing = 1;
        while (slot->settingUp)
            pthread_cond_wait(&alt_cond, &alt_lock);
    }
    pthread_mutex_unlock(&alt_lock);
    if (slot && tearDownAlt) {
        /* Stop the self-contained receiver: the reaper sets stop, closes the
         * listener to wake pre-connect accept, and joins outside alt_lock.  An
         * already-exited receiver follows the identical single-owner path. */
        alt_reap_thread(slot, 1);
        pthread_mutex_lock(&alt_lock);
        if (slot->altSock >= 0) {
            int ls = slot->altSock;
            slot->altSock = -1;
            pthread_mutex_unlock(&alt_lock);
            close(ls);
            pthread_mutex_lock(&alt_lock);
        }
        if (g_alt_session == session) g_alt_session = NULL;
        memset(slot, 0, sizeof(*slot));
        pthread_cond_broadcast(&alt_cond);
        pthread_mutex_unlock(&alt_lock);
        LOG_INFO(LOG_MODULE, "altScreen torn down for session=%p", session);
    } else if (slot) {
        LOG_INFO(LOG_MODULE,
                 "per-stream TEARDOWN has no 111 - preserving altScreen session=%p",
                 session);
    }

    if (!real) return reason;
    return real(session, request, reason, a4);
}

/* MUST match the cluster display uuid advertised in build_cluster_dict() (#1b). */
#define CLUSTER_DISPLAY_UUID "C1057E12-A0D1-4E20-B0C0-1057E12A0D10"

/* ════════════════════════════════════════════════════════════════════════════
 * working zone (fullscreen ↔ duo) live-switch — Java-driven via the bus.
 *
 * Two full-frame view areas are PREDECLARED in /info, with different nested
 * safeArea geometry: fullscreen=[0], duo=[1]. Switching is a plain
 * session command — NOT a stream reconfig: we ship {type:"updateViewArea",
 * params:{uuid,viewAreaIndex,animationDurationMillis,adjacentViewAreas}} via the
 * stock generic command shipper
 * AirPlayReceiverSessionSendCommand (POST /command, binary-plist body).  The
 * phone (CarKit _sessionUpdatesQueue_handleViewAreaChangeWithPayload) reads those
 * keys, bounds-checks the index against the declared viewAreas, and applies the
 * change.  Match MHI3 AirPlayReceiverSessionViewAreaUpdate exactly by always
 * carrying `adjacentViewAreas`: [] clears it in fullscreen, [0] tells CarKit
 * that view-area 0 is adjacent while duo area 1 is current.  iOS stores this as
 * CARScreenInfo.adjacentViewArea separately from currentViewArea, so omitting it
 * is observably different from the native helper. Verified: stock 210 exports
 * the generic shipper and it does NOT whitelist command types.
 *
 * CONFIRMATION: we pass a completion callback to SendCommand.  libairplay's
 * response handler (sub_28CF0) parses the phone's POST /command reply and calls
 * completion(int status, CFDictionaryRef resp|NULL, void *ctx) — status 0 = the
 * phone accepted/applied the switch.  We relay it to Java as CMD_ALT_ZONE_ACK so
 * the focus arbiter knows the zone actually changed (and can retry on failure).
 * The requested mode rides in ctx (0/1 fits a pointer). If SendCommand fails
 * synchronously the completion never fires, so we ack that error inline.
 * ════════════════════════════════════════════════════════════════════════════ */
enum { ALT_ZONE_FULLSCREEN = 0, ALT_ZONE_DUO = 1 };
static volatile int g_alt_zone_mode = ALT_ZONE_FULLSCREEN;

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
    int mode = (int)(intptr_t)ctx;
    (void)resp;
    LOG_INFO(LOG_MODULE, "updateViewArea ack: mode=%d status=%d", mode, status);
    zone_send_ack(mode, status);
    /* Maps scene options are independent of safeArea. Apply the matching URL
     * only after this view area succeeded, and ignore a stale ACK after a rapid
     * second View-button press. */
    if (status == 0 && mode == g_alt_zone_mode)
        alt_show_ui_for_mode(mode, "viewArea-accepted");
}

/* Ship a command against the live altScreen session, serialized with teardown:
 * read g_alt_session and call SendCommand under alt_lock, so teardown (which
 * clears g_alt_session + frees the session under the same lock) cannot free it
 * mid-call.  Returns the SendCommand rc, or -6735 if no active session/shipper.
 * (SendCommand only posts to the HTTP control channel — it never re-enters
 * alt_lock — so holding the lock across it is safe.) */
static int alt_send_locked(void *cmd, void *completion, void *ctx)
{
    int rc = -6735;
    pthread_mutex_lock(&alt_lock);
    if (sess_sendcmd && g_alt_session)
        rc = sess_sendcmd(g_alt_session, cmd, completion, ctx);
    pthread_mutex_unlock(&alt_lock);
    return rc;
}

/* Build and ship one of the two known-good view-area indexes. */
static int zone_send_mode(int mode, int dur, void *completion)
{
    int index, rc = -6728;
    void *params = NULL, *cmd = NULL, *adj = NULL, *pk = NULL, *ak = NULL;

    if (mode == ALT_ZONE_DUO) index = 1;
    else { mode = ALT_ZONE_FULLSCREEN; index = 0; }
    if (cf_init() != 1 || !cf_dictm || !dict_keycb || !dict_valcb ||
        !cf_dictset || !cf_str || !cf_dictseti || !cf_arrm || !arr_cb ||
        !cf_arrappi64) {
        LOG_ERROR(LOG_MODULE, "updateViewArea: CF symbols unresolved");
        return -6735;
    }

    params = cf_dictm(0, 0, dict_keycb, dict_valcb);
    cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    adj = cf_arrm(0, mode == ALT_ZONE_DUO ? 1 : 0, arr_cb);
    if (params && cmd && adj) {
        set_str(params, "uuid", CLUSTER_DISPLAY_UUID);
        set_i64(params, "viewAreaIndex", index);
        set_i64(params, "animationDurationMillis", dur);
        if (mode == ALT_ZONE_DUO) cf_arrappi64(adj, 0);
        ak = cf_str(0, "adjacentViewAreas", CFENC_UTF8);
        if (ak) cf_dictset(params, ak, adj);
        set_str(cmd, "type", "updateViewArea");
        pk = cf_str(0, "params", CFENC_UTF8);
        if (pk) cf_dictset(cmd, pk, params);
        rc = alt_send_locked(cmd, completion, (void *)(intptr_t)mode);
        LOG_INFO(LOG_MODULE,
                 "updateViewArea ship mode=%s index=%d adjacent=%s dur=%dms rc=%d",
                 mode == ALT_ZONE_DUO ? "duo" : "fullscreen",
                 index, mode == ALT_ZONE_DUO ? "[0]" : "[]", dur, rc);
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
    int mode = payload[0] == ALT_ZONE_DUO ? ALT_ZONE_DUO : ALT_ZONE_FULLSCREEN;
    int dur = (len >= 3) ? (payload[1] | (payload[2] << 8)) : 250;
    int rc;

    /* Cache even before the RTSP session exists. The first IDR will apply the
     * latest Audi mode, making fast reconnect and pre-session View presses safe. */
    g_alt_zone_mode = mode;
    rc = zone_send_mode(mode, dur, (void *)zone_cmd_done);
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

    /* Wait for the successful showUI reply before asking for the next IDR.  Sending
     * ForceKeyFrame immediately after the asynchronous command can race ahead of
     * CarKit attaching/foregrounding the cluster app scene and merely encode a
     * second blank random-access picture.  Serialize with teardown so session stays
     * live for the entire boundary call. */
    if (status == 0 && !strcmp(type, "showUI"))
        alt_request_cluster_idr("showUI-accepted");
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
static int alt_ship(const char *type, void *params)
{
    if (!params) return -6728;
    int rc = -6728;
    void *cmd = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (cmd) {
        set_str(cmd, "type", type);
        void *pk = cf_str(0, "params", CFENC_UTF8);
        if (pk) cf_dictset(cmd, pk, params);
        if (pk && cf_rel) cf_rel(pk);
        rc = alt_send_locked(cmd, (void *)alt_cmd_done, (void *)type);  /* serialized vs teardown */
        if (cf_rel) cf_rel(cmd);
        LOG_INFO(LOG_MODULE, "session cmd '%s' shipped rc=%d", type, rc);
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
static int alt_send_cluster_night_mode(int night, const char *reason)
{
    void *p = alt_params_new(1);
    int rc;
    if (!p) return -6728;
    set_i64(p, "nightMode", night ? 1 : 0);
    rc = alt_ship("setNightMode", p);
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
    if (env_on("ALTSCREEN_HANDLE_ALT"))
        clusterRc = alt_send_cluster_night_mode(g_alt_night_mode, "dio-update");

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
        rc = alt_ship("changeMapZoomLevel", p);
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
static int alt_request_cluster_idr(const char *reason)
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
        rc = alt_send_locked(cmd, NULL, NULL);
    }
    if (pk && cf_rel) cf_rel(pk);
    if (cmd && cf_rel) cf_rel(cmd);
    if (cf_rel) cf_rel(p);
    LOG_INFO(LOG_MODULE,
             "alt: cluster forceKeyFrame uuid=%s reason=%s rc=%d",
             CLUSTER_DISPLAY_UUID, reason ? reason : "?", rc);
    return rc;
}

/* Apply the latest Audi fullscreen/duo state once the 111 stream is live. The
 * Java command may have arrived before g_alt_session existed; g_alt_zone_mode
 * intentionally preserves it across that window. */
static void alt_kick_viewarea(void)
{
    int mode = g_alt_zone_mode;
    int rc = zone_send_mode(mode, 0, NULL);
    LOG_INFO(LOG_MODULE, "alt: initial updateViewArea mode=%s rc=%d",
             mode == ALT_ZONE_DUO ? "duo" : "fullscreen", rc);
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
    if (mode == ALT_ZONE_DUO) {
        url = getenv("ALTSCREEN_CLUSTER_URL_DUO");
        if (!url || !url[0])
            url = "maps:/car/instrumentcluster/map";
    } else {
        url = getenv("ALTSCREEN_CLUSTER_URL");
        if (!url || !url[0])
            url = "maps:/car/instrumentcluster/map";
    }
    return url;
}

static int alt_show_ui_for_mode(int mode, const char *reason)
{
    const char *url = alt_cluster_url_for_mode(mode);
    void *p = alt_params_new(1);                        /* uuid = cluster display */
    if (!p) return -6728;
    set_str(p, "url", url);
    int rc = alt_ship("showUI", p);
    LOG_INFO(LOG_MODULE, "alt: showUI mode=%s reason=%s uuid=%s url=%s rc=%d",
             mode == ALT_ZONE_DUO ? "duo-map-only" : "fullscreen-cards",
             reason ? reason : "?", CLUSTER_DISPLAY_UUID, url, rc);
    return rc;
}

static void alt_kick_show_ui(void)
{
    alt_show_ui_for_mode(g_alt_zone_mode, "first-IDR");
}

static void appearance_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 3) return;                    /* [target][mode][setting] */
    void *p = alt_params_new(1);
    if (!p) return;
    set_i64(p, "appearanceMode",    (uint8_t)payload[1]);
    set_i64(p, "appearanceSetting", (uint8_t)payload[2]);
    alt_ship(payload[0] ? "mapAppearanceUpdate" : "uiAppearanceUpdate", p);
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
    alt_ship("changeUIContext", p);
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
    altav_shutdown();
}

__attribute__((constructor)) static void altscreen_module_init(void) { altscreen_init(); }
__attribute__((destructor))  static void altscreen_module_fini(void) { altscreen_shutdown(); }
