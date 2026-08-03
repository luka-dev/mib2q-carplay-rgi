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
 *     info["extendedFeatures"] = ["altScreen"]
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

DEFINE_LOG_MODULE(altscreen);

#define CFENC_UTF8        0x08000100              /* kCFStringEncodingUTF8            */

/* ── libairplay CFLite builders (resolved lazily; all are dio imports) ─────────── */
typedef void *(*cf_str_t)(void *, const char *, unsigned);          /* CFStringCreateWithCString */
typedef void *(*cf_arrm_t)(void *, long, const void *);             /* CFArrayCreateMutable       */
typedef int   (*cf_arrappc_t)(void *, const char *, int);          /* CFArrayAppendCString(arr,str,len=-1) */
typedef void  (*cf_arrapp_t)(void *, const void *);                /* CFArrayAppendValue         */
typedef void  (*cf_rel_t)(void *);                                 /* CFRelease                  */
typedef void  (*cf_dictset_t)(void *, const void *, const void *); /* CFDictionarySetValue       */
typedef void *(*cf_dictm_t)(void *, long, const void *, const void *); /* CFDictionaryCreateMutable */
typedef void  (*cf_dictseti_t)(void *, const void *, int64_t);     /* CFDictionarySetInt64       */
/* AirPlayReceiverSessionScreen_CopyDisplaysInfo(screenSession, outErr) — 280 ref
 * ignores the session arg ((void)inSession) and builds the MAIN display dict via
 * ScreenCopyMain(); we pass NULL. */
typedef void *(*fn_copydisp_t)(void *, void *);

static cf_str_t     cf_str;
static cf_arrm_t    cf_arrm;
static cf_arrappc_t cf_arrappc;
static cf_arrapp_t  cf_arrapp;
static cf_rel_t     cf_rel;
static cf_dictset_t cf_dictset;
static cf_dictm_t   cf_dictm;
static cf_dictseti_t cf_dictseti;
static fn_copydisp_t fn_copydisp;
static const void  *arr_cb;          /* array callbacks constant (CFLite)            */
static const void  *dict_keycb;      /* dictionary key callbacks constant            */
static const void  *dict_valcb;      /* dictionary value callbacks constant          */
static int          cf_ok = -1;      /* extendedFeatures (#1a) builders ready         */
static int          disp_ok = -1;    /* displays (#1b) builders ready                 */
static pthread_mutex_t cf_lock = PTHREAD_MUTEX_INITIALIZER;

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
        cf_rel     = (cf_rel_t)     dlsym(RTLD_DEFAULT, "CFRelease");
        cf_dictset = (cf_dictset_t) dlsym(RTLD_DEFAULT, "CFDictionarySetValue");
        cf_dictm   = (cf_dictm_t)   dlsym(RTLD_DEFAULT, "CFDictionaryCreateMutable");
        cf_dictseti= (cf_dictseti_t)dlsym(RTLD_DEFAULT, "CFDictionarySetInt64");
        fn_copydisp= (fn_copydisp_t)dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_CopyDisplaysInfo");
        arr_cb     = resolve_arr_cb();
        dict_keycb = dlsym(RTLD_DEFAULT, "kCFLDictionaryKeyCallBacksCFLTypes");
        dict_valcb = dlsym(RTLD_DEFAULT, "kCFLDictionaryValueCallBacksCFLTypes");
        /* #1a needs: a mutable array + a way to add "altScreen" + CFStringCreate (the
         * /info key) + CFDictionarySetValue (add the key to the returned dict). */
        cf_ok = (arr_cb && cf_arrm && (cf_arrappc || (cf_str && cf_arrapp)) && cf_str && cf_dictset) ? 1 : 0;
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

/* build  ["altScreen"]  with libairplay's own CFLite; caller CFReleases. */
static void *build_extfeatures(void)
{
    void *arr = cf_arrm(0, 1, arr_cb);
    if (!arr) return 0;
    if (cf_arrappc) {
        cf_arrappc(arr, "altScreen", -1);   /* -1 = NUL-terminated (3rd arg is length) */
    } else {
        void *s = cf_str(0, "altScreen", CFENC_UTF8);
        if (!s) { if (cf_rel) cf_rel(arr); return 0; }
        cf_arrapp(arr, s);
        if (cf_rel) cf_rel(s);   /* AppendValue retains */
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

/* Cluster geometry.  Split-mode safe area is a PLACEHOLDER — replace SPLIT_SAFE_*
 * with the exact values.  The VISIBLE frame stays full (the video is always
 * fullscreen); only the SAFE AREA shrinks for split (left-pinned). */
/* Audi Kombi (instrument-cluster) panel — from firmware qcdisplaycfg_MMX2.xml
 * "Audi 1440x542 Kombi panel" (uVisWidth 1440 / uVisHeight 542); LVDS2 = Cluster
 * Display (displaymanager.json id 4).  CarPlay encodes the 111 stream at the size
 * we advertise here, so this MUST equal the panel. */
#define CLUSTER_W       1440
#define CLUSTER_H       542
#define SPLIT_SAFE_X    0        /* left-pinned                                  */
#define SPLIT_SAFE_Y    0
/* Split map zone = left side of the centered-gauge VC layout.  Measured from a
 * real VC nav photo: centered tacho ~415px, symmetric side panels → each side
 * ~(1440-415)/2 ≈ 512px.  ±~40px (photo angle); refine on-unit. */
#define SPLIT_SAFE_W    510
#define SPLIT_SAFE_H    542

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

static void *build_cluster_dict(void)
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
    /* viewAreas: predeclared modes for live index-switch (CarKit selects
     * viewAreas[viewAreaIndex] on a view-area-change payload, animated).
     *   [0] = fullscreen (default active),
     *   [1] = split — visible frame stays full, safe area left-pinned (TODO exact).
     * The video is always fullscreen; only the safe area changes. */
    {
        void *k   = cf_str(0, "viewAreas", CFENC_UTF8);
        void *arr = cf_arrm(0, 2, arr_cb);
        if (arr) {
            void *full  = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                         0, 0, CLUSTER_W, CLUSTER_H);
            void *split = build_viewarea(0, 0, CLUSTER_W, CLUSTER_H,
                                         SPLIT_SAFE_X, SPLIT_SAFE_Y, SPLIT_SAFE_W, SPLIT_SAFE_H);
            if (full)  { cf_arrapp(arr, full);  if (cf_rel) cf_rel(full);  }
            if (split) { cf_arrapp(arr, split); if (cf_rel) cf_rel(split); }
            if (k) cf_dictset(d, k, arr);
            if (cf_rel) cf_rel(arr);
        }
        if (k && cf_rel) cf_rel(k);
    }
    /* Extra fields a real HU (MHI3 libairplay 450) sends in each display dict — our subset was
     * incomplete, which the phone accepts to ADD the car but rejects at SESSION start (0s teardown). */
    set_i64(d, "primaryInputDevice", 0);
    set_i64(d, "cornerMasks",        0);
    set_i64(d, "nightMode",          0);
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
static void main_disp_modernize(void *mainDisp)
{
    if (!mainDisp) return;
    /* CarKit reads the display identifier as "uid" (stock emits only the legacy "uuid").  The value
     * only needs to be a non-nil, unique string — give the main a fixed uid distinct from the cluster. */
    set_str(mainDisp, "uid", "A0000000-0000-4000-8000-0000000000A1");
    /* complete the main dict like a real HU (MHI3 set) — see build_cluster_dict note */
    set_i64(mainDisp, "primaryInputDevice", 0);
    set_i64(mainDisp, "cornerMasks",        0);
    set_i64(mainDisp, "nightMode",          0);
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

static void *build_displays(void)
{
    int err = 0;
    void *mainDisp = fn_copydisp(0, &err);      /* arg ignored; builds via ScreenCopyMain */
    main_disp_modernize(mainDisp);              /* uuid→uid + initialVideoStreams for CarKit */
    void *cluster  = build_cluster_dict();
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
        /* #1a: extendedFeatures = ["altScreen"] */
        void *k = cf_str(0, "extendedFeatures", CFENC_UTF8);
        void *v = build_extfeatures();                        /* ["altScreen"] */
        if (k && v) {
            cf_dictset(info, k, v);                           /* add to the /info dict */
            LOG_INFO(LOG_MODULE, "injected extendedFeatures=[altScreen] into /info=%p", info);
        } else {
            LOG_ERROR(LOG_MODULE, "inject extendedFeatures build failed k=%p v=%p", k, v);
        }
        if (v && cf_rel) cf_rel(v);                           /* dict retained its own ref */
        if (k && cf_rel) cf_rel(k);

        /* displays/displayPanels provide the distinct second UUID that makes iOS
         * construct stream 111.  Keep the separate env gate for controlled rollback. */
        if (disp_ok == 1 && env_on("ALTSCREEN_ADVERT_DISPLAYS")) {
            void *dk = cf_str(0, "displays", CFENC_UTF8);
            void *dv = build_displays();
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
            void *pv = build_displays();
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

/* exported stock function pointers (resolved lazily via RTLD_DEFAULT) */
typedef int      (*fn_forcekey_t)(void *, void *, void *);  /* AirPlayReceiverSessionForceKeyFrame(session, cb, ctx) — control-channel IDR request */
/* stock AES-CTR exports (self-contained receiver decrypts opcode-0 bodies itself, no stock ScreenStream) */
typedef int      (*fn_aes_ctr_init_t)(void *ctx, const uint8_t *key, const uint8_t *iv);        /* AES_CTR_Init  */
typedef int      (*fn_aes_ctr_update_t)(void *ctx, const uint8_t *src, uint32_t len, uint8_t *dst); /* AES_CTR_Update */
typedef int      (*fn_aes_ctr_final_t)(void *ctx);                                              /* AES_CTR_Final */
typedef int      (*fn_sess_sendcmd_t)(void *, void *, void *, void *); /* AirPlayReceiverSessionSendCommand(session, cmdDict{type,params}, completion, ctx) */
typedef void     (*fn_derive_aes_t)(const uint8_t *, int, uint64_t, uint8_t *, uint8_t *);
typedef int      (*fn_srv_sockopen_t)(int, int, int, int, int *, int, int *);
typedef void     (*fn_sock_setqos_t)(int, int);
typedef int      (*fn_sock_accept_t)(int, int, int *, void *);
typedef int64_t  (*fn_cfdict_geti64_t)(void *, const void *, void *); /* CFDictionaryGetInt64(dict,key,OSStatus*outErr) */
typedef void *   (*fn_cfdict_getval_t)(void *, const void *);
typedef long     (*fn_cfarr_count_t)(void *);
typedef void *   (*fn_cfarr_getidx_t)(void *, long);

static fn_forcekey_t        scr_forcekey;
static fn_aes_ctr_init_t    aes_ctr_init;
static fn_aes_ctr_update_t  aes_ctr_update;
static fn_aes_ctr_final_t   aes_ctr_final;
static fn_sess_sendcmd_t    sess_sendcmd;   /* session-level {type,params} command shipper (zone switch) */
static void alt_kick_viewarea(void);        /* fwd: auto-select viewArea 0 once the 111 stream is live */
static void alt_kick_show_ui(void);         /* fwd: select Maps content for the cluster display */
static fn_derive_aes_t      derive_aes;

/* main AirPlay session with a live altScreen — set on alt_setup success, cleared
 * on teardown.  Read by the CMD_ALT_ZONE handler to target AirPlayReceiverSessionSendCommand.
 * ponytail: single altScreen session assumed; aligned pointer store = atomic on ARMv7. */
static void *volatile g_alt_session;
static fn_srv_sockopen_t    srv_sockopen;
static fn_sock_setqos_t     sock_setqos;
static fn_sock_accept_t     sock_accept;
static fn_cfdict_geti64_t   cf_geti64;
static fn_cfdict_getval_t   cf_getval;
static fn_cfarr_count_t     cf_arrcount;
static fn_cfarr_getidx_t    cf_arridx;

static int alt_enabled = -1;    /* env ALTSCREEN_HANDLE_ALT */
static int alt_syms_ok = -1;
static pthread_mutex_t alt_lock = PTHREAD_MUTEX_INITIALIZER;

static int alt_resolve(void)
{
    pthread_mutex_lock(&alt_lock);   /* no lock-free fast path — see cf_init note */
    if (alt_syms_ok < 0) {
        if (cf_init() != 1) { alt_syms_ok = 0; pthread_mutex_unlock(&alt_lock); return 0; }
        scr_forcekey    = (fn_forcekey_t)       dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionForceKeyFrame");
        aes_ctr_init    = (fn_aes_ctr_init_t)   dlsym(RTLD_DEFAULT, "AES_CTR_Init");
        aes_ctr_update  = (fn_aes_ctr_update_t) dlsym(RTLD_DEFAULT, "AES_CTR_Update");
        aes_ctr_final   = (fn_aes_ctr_final_t)  dlsym(RTLD_DEFAULT, "AES_CTR_Final");
        sess_sendcmd    = (fn_sess_sendcmd_t)   dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionSendCommand");
        derive_aes      = (fn_derive_aes_t)     dlsym(RTLD_DEFAULT, "AirPlay_DeriveAESKeySHA512ForScreen");
        srv_sockopen    = (fn_srv_sockopen_t)   dlsym(RTLD_DEFAULT, "ServerSocketOpen");
        sock_setqos     = (fn_sock_setqos_t)    dlsym(RTLD_DEFAULT, "SocketSetQoS");
        sock_accept     = (fn_sock_accept_t)    dlsym(RTLD_DEFAULT, "SocketAccept");
        cf_geti64       = (fn_cfdict_geti64_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetInt64");
        cf_getval       = (fn_cfdict_getval_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
        cf_arrcount     = (fn_cfarr_count_t)    dlsym(RTLD_DEFAULT, "CFArrayGetCount");
        cf_arridx       = (fn_cfarr_getidx_t)   dlsym(RTLD_DEFAULT, "CFArrayGetValueAtIndex");
        /* Only symbols used by the live self-contained 111 path are critical. */
        alt_syms_ok = (scr_forcekey && sess_sendcmd && derive_aes &&
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
    int         threadValid;
    int         inUse;
    uint8_t     key[16];     /* self-contained receiver: AES-CTR key ...  */
    uint8_t     iv[16];      /* ... + iv (derived in alt_setup)           */
    volatile int stop;       /* teardown sets =1 → recv poll-loop exits (unblocks join) */
} altslot_t;
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

#define ALT_HDR_SIZE  128
#define ALT_MAX_BODY  (4u * 1024 * 1024)

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
    int got_idr = 0;

    if (aes_ctr_init) { aes_ctr_init(aesctx, slot->key, slot->iv); aes_ready = 1; }

    /* We attach to the phone's screen stream mid-GOP, so it sends only P-slices; the decoder can't
     * start without an IDR.  Ask the phone to emit one over the session control channel
     * (AirPlayReceiverSessionForceKeyFrame → SendCommand) — and keep nudging until an IDR arrives. */
    if (scr_forcekey && slot->session) {
        scr_forcekey(slot->session, NULL, NULL);
        LOG_INFO(LOG_MODULE, "alt: requested keyframe (ForceKeyFrame) at stream start");
    }

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
            if (clen) altav_set_config(cfg, clen);   /* cache + send; replayed on every reconnect */
            LOG_INFO(LOG_MODULE,
                     "alt recv: avcC config encoded=%dx%d body=%u lengthSize=%u annexb=%u bytes=%s%s",
                     (int)(cfgWidth + 0.5f), (int)(cfgHeight + 0.5f), bodySize, ls, clen,
                     avccHex, (bodySize > hn) ? "..." : "");
        } else if (opcode == 0) {                         /* encrypted AVCC video access unit */
            if (aes_ready) aes_ctr_update(aesctx, body, bodySize, body);
            /* NAL type of the first unit (header byte at +ls): 5 = IDR keyframe.  Once one arrives the
             * decoder can start; stop nudging the phone.  0x21→type1 non-IDR, 0x65→type5 IDR. */
            if (!got_idr && bodySize > ls && (body[ls] & 0x1F) == 5) {
                got_idr = 1;
                LOG_INFO(LOG_MODULE, "alt recv: IDR keyframe arrived at frame#%lu (body=%u) — decoder can start", nframes, bodySize);
                /* A live video stream is only the canvas.  Select the Maps instrument-cluster
                 * UI explicitly, then tell the encoder which predeclared view area is visible. */
                alt_kick_show_ui();
                alt_kick_viewarea();
            }
            if (nframes < 6 && bodySize >= 8)
                LOG_INFO(LOG_MODULE, "alt recv frame#%lu body=%u dec[0..7]=%02x %02x %02x %02x %02x %02x %02x %02x nalhdr@ls=0x%02x",
                         nframes, bodySize, body[0],body[1],body[2],body[3],body[4],body[5],body[6],body[7],
                         (bodySize > ls) ? body[ls] : 0);
            altav_forward_avcc_frame(body, bodySize, ls);
            ++nframes;
            /* keep requesting an IDR (every ~32 frames) until we actually get one */
            if (!got_idr && scr_forcekey && slot->session && (nframes % 32) == 0) {
                scr_forcekey(slot->session, NULL, NULL);
                LOG_INFO(LOG_MODULE, "alt: re-requested keyframe (still no IDR after %lu frames)", nframes);
            }
            if ((nframes & 0x3F) == 0)
                LOG_INFO(LOG_MODULE, "alt recv: %lu video frames forwarded (last body=%u)", nframes, bodySize);
        }
        /* opcodes 2/4/5 (keepalive/misc): body already consumed → ignore */
    }

    if (aes_ready && aes_ctr_final) aes_ctr_final(aesctx);
    free(body);
}

/* receiver thread — self-contained (own socket read; NO stock ScreenStream) */
static void *alt_thread_main(void *arg)
{
    altslot_t *slot = (altslot_t *)arg;
    int        fd = -1;
    int        ls;

    if (sock_accept(slot->altSock, 10, &fd, NULL) != 0 || fd < 0) goto done;
    /* claim + close the listen socket (teardown may race to close it too — the
     * -1 sentinel under alt_lock ensures exactly one close). */
    pthread_mutex_lock(&alt_lock);
    ls = slot->altSock; slot->altSock = -1;
    pthread_mutex_unlock(&alt_lock);
    if (ls >= 0) close(ls);

    LOG_INFO(LOG_MODULE, "alt-screen receiver started (self-contained)");
    alt_recv_loop(slot, fd);

done:
    if (fd >= 0) close(fd);
    altav_shutdown();                                     /* disconnect renderer → it resets its decoder */
    LOG_INFO(LOG_MODULE, "alt-screen receiver thread exit");
    return NULL;
}

/* SETUP response and transport for stream type 111.  No stock Screen object is
 * created: the hook owns the socket and decrypts the AirPlay screen wire itself. */
static int alt_setup(void *session, void *streamDesc, void *responseParams)
{
    altslot_t *slot;
    void  *server;
    int    err, family, port = 0, sock = -1;
    int64_t connID;
    uint8_t key[16], iv[16];
    void  *rdesc;

    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_get(session);
    if (slot && (slot->threadValid || slot->altSock >= 0)) {
        pthread_mutex_unlock(&alt_lock);
        return -6719;   /* already set up */
    }
    pthread_mutex_unlock(&alt_lock);
    if (!slot) return -6728;
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
    if (alt_add_response_stream(responseParams, rdesc) != 0) {
        if (cf_rel) cf_rel(rdesc);
        err = -6728;
        goto fail;
    }
    if (cf_rel) cf_rel(rdesc);

    /* Publish the socket and thread atomically with respect to teardown. */
    pthread_mutex_lock(&alt_lock);
    slot->altSock = sock;
    if (pthread_create(&slot->thread, NULL, alt_thread_main, slot) == 0) {
        slot->threadValid = 1;
        g_alt_session = session;      /* enable CMD_ALT_* targeting */
    } else {
        slot->altSock = -1;
    }
    int started = slot->threadValid;
    pthread_mutex_unlock(&alt_lock);
    if (!started) { err = -6728; goto fail; }

    /* The acceptor is live before this function returns and the response is sent. */
    LOG_INFO(LOG_MODULE, "alt-screen set up on port %d connID=%llu",
             port, (unsigned long long)connID);
    return 0;

fail:
    if (sock >= 0) close(sock);
    pthread_mutex_lock(&alt_lock);
    if (g_alt_session == session) g_alt_session = NULL;   /* defensive (only set on success) */
    memset(slot, 0, sizeof(*slot));                       /* never reached with a live thread */
    pthread_mutex_unlock(&alt_lock);
    LOG_ERROR(LOG_MODULE, "alt-screen setup failed err=%d", err);
    return err;
}

/* find a type==111 stream desc in the request's "streams" array */
static void *alt_find_111(void *request)
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
                if (sd && cf_geti64(sd, ktype, NULL) == ALT_STREAM_TYPE) { found = sd; break; }
            }
        }
    }
    if (kstreams && cf_rel) cf_rel(kstreams);
    if (ktype && cf_rel) cf_rel(ktype);
    return found;
}

/* interpose the SETUP entry: real handles 100/101/110 (skips 111), then we
 * bring up 111 ourselves and append its desc to the response. */
typedef int (*fn_sess_setup_t)(void *, void *, void **, int);
int AirPlayReceiverSessionSetup(void *session, void *request, void **respOut, int a4)
{
    static fn_sess_setup_t real;
    if (!real) real = (fn_sess_setup_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionSetup");
    if (!real) { LOG_ERROR(LOG_MODULE, "SessionSetup real==NULL"); return -6740; }

    int ret = real(session, request, respOut, a4);

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
         *   - Seed from the receiver's EXISTING response enabledFeatures, add ONLY altScreen.  Each string in
         *     that array flips a sibling endpoint flag (viewAreas+0x3D, altScreen+0x3E, uiContext+0x3F,
         *     enhancedSiri+0x39), so NEVER echo the phone's proposal — that could falsely enable features
         *     stock doesn't implement.
         *   - Scope to the control SETUP: the request carries the "features" proposal key (media SETUPs don't). */
        void *kReq = cf_str(0, "features", CFENC_UTF8);
        int isControlSetup = (kReq && cf_getval(request, kReq) != 0);
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
                    if (cf_arrappc) cf_arrappc(neg, "altScreen", -1);
                    else { void *s = cf_str(0, "altScreen", CFENC_UTF8);
                           if (s) { cf_arrapp(neg, s); if (cf_rel) cf_rel(s); } }
                    cf_dictset(*respOut, kEnab, neg);              /* response.enabledFeatures = existing + altScreen */
                    if (cf_rel) cf_rel(neg);
                    LOG_INFO(LOG_MODULE, "control SETUP: response enabledFeatures merged (existing count=%ld) + altScreen", curN);
                }
                if (cf_rel) cf_rel(kEnab);
            }
        }

        void *altDesc = alt_find_111(request);
        if (altDesc) {
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
    return ret;
}

/* interpose teardown: quit + join our receiver thread before the real teardown
 * frees the session (whose fields the thread reads). */
typedef int (*fn_sess_teardown_t)(void *, void *, int, int);
int AirPlayReceiverSessionTearDown(void *session, void *request, int reason, int a4)
{
    static fn_sess_teardown_t real;
    if (!real) real = (fn_sess_teardown_t)dlsym(RTLD_NEXT, "AirPlayReceiverSessionTearDown");

    LOG_INFO(LOG_MODULE,
             "AirPlayReceiverSessionTearDown session=%p request=%p reason=%d a4=%d",
             session, request, reason, a4);

    altslot_t *slot;
    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_find(session);
    pthread_mutex_unlock(&alt_lock);
    if (slot) {
        if (slot->threadValid) {
            /* Stop the self-contained receiver: set the stop flag (its poll loop
             * exits within one 500ms tick), close the listen socket to break a
             * pre-connect SocketAccept, then join before stock frees session. */
            int ls;
            slot->stop = 1;
            pthread_mutex_lock(&alt_lock);
            ls = slot->altSock; slot->altSock = -1;
            pthread_mutex_unlock(&alt_lock);
            if (ls >= 0) close(ls);
            pthread_join(slot->thread, NULL);
            slot->threadValid = 0;
        }
        if (slot->altSock >= 0) { close(slot->altSock); slot->altSock = -1; }
        pthread_mutex_lock(&alt_lock);
        if (g_alt_session == session) g_alt_session = NULL;
        memset(slot, 0, sizeof(*slot));
        pthread_mutex_unlock(&alt_lock);
        LOG_INFO(LOG_MODULE, "altScreen torn down for session=%p", session);
    }

    if (!real) return reason;
    return real(session, request, reason, a4);
}

/* ════════════════════════════════════════════════════════════════════════════
 * knob (крутилка) input → altScreen — Java-driven, via the bidirectional bus.
 *
 * Java owns the wheel-knob + focus arbitration (cluster settings vs CarPlay).
 * On altScreen focus it sends CMD_ALT_HID {int8 wheel, u8 buttons}; we post it as a
 * HID report on a cluster-bound virtual HID device (bound to the SAME uuid we
 * advertised for the cluster display in /info, so iOS routes it to the cluster
 * scene).  All via stock libairplay exports.
 * ════════════════════════════════════════════════════════════════════════════ */

/* Knob HID report descriptor — verbatim from Apple CarPlay SDK Examples/HIDKnob.c
 * (MultiAxisController: Button + AC Home + AC Back + 5 pad, X, Y, Wheel).
 * 4-byte report: [buttons][X][Y][Wheel]; Wheel is relative (rotation delta). */
static const uint8_t kKnobDescriptor[] = {
    0x05,0x01, 0x09,0x08, 0xA1,0x01,
      0x05,0x09, 0x09,0x01, 0x15,0x00, 0x25,0x01, 0x75,0x01, 0x95,0x01, 0x81,0x02,
      0x05,0x0c, 0x0a,0x23,0x02, 0x0a,0x24,0x02, 0x95,0x02, 0x81,0x02,
      0x95,0x05, 0x81,0x01,
      0x05,0x01, 0x09,0x01, 0xA1,0x00,
        0x09,0x30, 0x09,0x31, 0x15,0x81, 0x25,0x7f, 0x75,0x08, 0x95,0x02, 0x81,0x02,
      0xC0,
      0x09,0x38, 0x15,0x81, 0x25,0x7f, 0x75,0x08, 0x95,0x01, 0x81,0x06,
    0xC0
};

/* MUST match the cluster display uuid advertised in build_cluster_dict() (#1b) —
 * iOS binds a HID device to a display by this uuid. */
#define CLUSTER_DISPLAY_UUID "C1057E12-A0D1-4E20-B0C0-1057E12A0D10"

typedef int   (*fn_hid_create_t)(void **, void *);           /* HIDDeviceCreateVirtual(&dev,props) */
typedef int   (*fn_hid_post_t)(void *, const void *, size_t);/* HIDDevicePostReport(dev,ptr,len)   */
typedef void *(*fn_cfdata_t)(void *, const uint8_t *, long); /* CFDataCreate(alloc,bytes,len)      */
static fn_hid_create_t hid_create;
static fn_hid_post_t   hid_post;
static fn_cfdata_t     cf_data;
static void           *g_knob_dev;
static int             g_knob_ready = -1;
static pthread_mutex_t knob_lock = PTHREAD_MUTEX_INITIALIZER;

/* Lazily create the cluster knob HID device on first CMD_ALT_HID (Java only sends
 * it once altScreen has focus → the cluster display exists). */
static int knob_ensure(void)
{
    if (g_knob_ready >= 0) return g_knob_ready;
    pthread_mutex_lock(&knob_lock);
    if (g_knob_ready < 0) {
        if (cf_init() != 1) { g_knob_ready = 0; pthread_mutex_unlock(&knob_lock); return 0; }
        if (!hid_create) hid_create = (fn_hid_create_t)dlsym(RTLD_DEFAULT, "HIDDeviceCreateVirtual");
        if (!hid_post)   hid_post   = (fn_hid_post_t)  dlsym(RTLD_DEFAULT, "HIDDevicePostReport");
        if (!cf_data)    cf_data    = (fn_cfdata_t)    dlsym(RTLD_DEFAULT, "CFDataCreate");
        if (hid_create && hid_post && cf_data && cf_dictm && cf_str && cf_dictset) {
            void *props = cf_dictm(0, 0, dict_keycb, dict_valcb);
            if (props) {
                void *kd = cf_str(0, "hidDescriptor", CFENC_UTF8);
                void *vd = cf_data(0, kKnobDescriptor, (long)sizeof(kKnobDescriptor));
                if (kd && vd) cf_dictset(props, kd, vd);
                if (vd && cf_rel) cf_rel(vd);
                if (kd && cf_rel) cf_rel(kd);
                set_str(props, "displayUUID", CLUSTER_DISPLAY_UUID);
                set_str(props, "name", "AltScreen Knob");
                int rc = hid_create(&g_knob_dev, props);
                if (cf_rel) cf_rel(props);
                if (rc != 0 || !g_knob_dev) {
                    LOG_ERROR(LOG_MODULE, "HIDDeviceCreateVirtual failed rc=%d", rc);
                    g_knob_dev = NULL;
                } else {
                    LOG_INFO(LOG_MODULE, "cluster knob HID device created (uuid=%s)", CLUSTER_DISPLAY_UUID);
                }
            }
        } else {
            LOG_ERROR(LOG_MODULE, "knob HID syms unresolved (create=%p post=%p data=%p)",
                      (void*)hid_create, (void*)hid_post, (void*)cf_data);
        }
        g_knob_ready = (g_knob_dev != NULL) ? 1 : 0;
    }
    pthread_mutex_unlock(&knob_lock);
    return g_knob_ready;
}

/* bus handler for CMD_ALT_HID — binary [int8 wheel][u8 buttons]. */
static void knob_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 2) return;
    if (knob_ensure() != 1) return;
    uint8_t buttons = payload[1] & 0x07;           /* bit0 press, bit1 home, bit2 back */
    uint8_t report[4] = { buttons, 0, 0, payload[0] /* wheel */ };
    hid_post(g_knob_dev, report, sizeof(report));
}

/* ════════════════════════════════════════════════════════════════════════════
 * working zone (fullscreen ↔ split) live-switch — Java-driven via the bus.
 *
 * The two view areas are PREDECLARED in /info (build_cluster_dict: [0]=full,
 * [1]=split).  Switching between them at runtime is a plain session command —
 * NOT a stream reconfig: we ship {type:"updateViewArea", params:{uuid,
 * viewAreaIndex, animationDurationMillis}} via the stock generic command shipper
 * AirPlayReceiverSessionSendCommand (POST /command, binary-plist body).  The
 * phone (CarKit _sessionUpdatesQueue_handleViewAreaChangeWithPayload) reads those
 * three keys, bounds-checks the index against the declared viewAreas, and
 * animates the safe-area change.  Verified: stock 210 exports the shipper and it
 * does NOT whitelist command types.  adjacentViewAreas is NOT read on switch.
 *
 * CONFIRMATION: we pass a completion callback to SendCommand.  libairplay's
 * response handler (sub_28CF0) parses the phone's POST /command reply and calls
 * completion(int status, CFDictionaryRef resp|NULL, void *ctx) — status 0 = the
 * phone accepted/applied the switch.  We relay it to Java as CMD_ALT_ZONE_ACK so
 * the focus arbiter knows the zone actually changed (and can retry on failure).
 * The requested index rides in ctx (0/1 fits a pointer).  If SendCommand fails
 * synchronously the completion never fires, so we ack that error inline.
 * ════════════════════════════════════════════════════════════════════════════ */
static void zone_send_ack(int idx, int status)
{
    uint8_t ack[5];
    ack[0] = (uint8_t)idx;
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
    int idx = (int)(intptr_t)ctx;
    (void)resp;
    LOG_INFO(LOG_MODULE, "updateViewArea ack: idx=%d status=%d", idx, status);
    zone_send_ack(idx, status);
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

static void zone_on_cmd(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len, void *ctx)
{
    (void)type; (void)flags; (void)ctx;
    if (!payload || len < 1) return;
    if (cf_init() != 1 ||
        !cf_dictm || !dict_keycb || !dict_valcb || !cf_dictset || !cf_str || !cf_dictseti) {
        LOG_ERROR(LOG_MODULE, "CMD_ALT_ZONE: CF syms unresolved");
        return;
    }

    int idx = payload[0];
    int dur = (len >= 3) ? (payload[1] | (payload[2] << 8)) : 250;

    void *params = cf_dictm(0, 0, dict_keycb, dict_valcb);
    void *cmd    = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (params && cmd) {
        set_str(params, "uuid",                    CLUSTER_DISPLAY_UUID);
        set_i64(params, "viewAreaIndex",           idx);
        set_i64(params, "animationDurationMillis", dur);
        set_str(cmd, "type", "updateViewArea");
        void *pk = cf_str(0, "params", CFENC_UTF8);
        if (pk) cf_dictset(cmd, pk, params);
        if (pk && cf_rel) cf_rel(pk);
        int rc = alt_send_locked(cmd, (void *)zone_cmd_done, (void *)(intptr_t)idx);
        LOG_INFO(LOG_MODULE, "CMD_ALT_ZONE → updateViewArea idx=%d dur=%dms rc=%d", idx, dur, rc);
        /* completion fires only on a delivered request; ack sync failures here */
        if (rc != 0) zone_send_ack(idx, rc);
    }
    if (params && cf_rel) cf_rel(params);
    if (cmd && cf_rel) cf_rel(cmd);
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
    if (status == 0 && !strcmp(type, "showUI") && scr_forcekey) {
        int32_t keyErr = -6728;
        pthread_mutex_lock(&alt_lock);
        if (g_alt_session) keyErr = scr_forcekey((void *)g_alt_session, NULL, NULL);
        pthread_mutex_unlock(&alt_lock);
        LOG_INFO(LOG_MODULE, "alt: showUI accepted; post-activation ForceKeyFrame rc=%d", keyErr);
    }
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

/* Auto-select viewArea 0 (fullscreen) once the 111 stream is live.  This changes geometry only;
 * it does NOT activate an app scene (that is showUI below). */
static void alt_kick_viewarea(void)
{
    void *p = alt_params_new(1);                        /* uuid = CLUSTER_DISPLAY_UUID preset */
    if (!p) return;
    set_i64(p, "viewAreaIndex",           0);
    set_i64(p, "animationDurationMillis", 0);
    int rc = alt_ship("updateViewArea", p);
    LOG_INFO(LOG_MODULE, "alt: auto updateViewArea(0) at stream start rc=%d", rc);
}

/* iOS 26.6 Maps registers this exact URL for its map-based instrument-cluster UI.
 * The normal protocol is Maps -> suggestUI{urls:[...]} -> HU chooses one ->
 * showUI{uuid,url}.  Stock 210 has no altScreen policy layer to make that choice,
 * so choose the Maps map URL when the 111 stream first becomes decodable.
 * ALTSCREEN_CLUSTER_URL keeps the choice testable and permits another provider. */
static void alt_kick_show_ui(void)
{
    const char *url = getenv("ALTSCREEN_CLUSTER_URL");
    if (!url || !url[0]) url = "maps:/car/instrumentcluster/map";

    void *p = alt_params_new(1);                        /* uuid = cluster display */
    if (!p) return;
    set_str(p, "url", url);
    int rc = alt_ship("showUI", p);
    LOG_INFO(LOG_MODULE, "alt: auto showUI uuid=%s url=%s rc=%d",
             CLUSTER_DISPLAY_UUID, url, rc);
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
    bus_on(CMD_ALT_HID, knob_on_cmd, NULL);
    bus_on(CMD_ALT_ZONE, zone_on_cmd, NULL);
    bus_on(CMD_ALT_APPEARANCE, appearance_on_cmd, NULL);
    bus_on(CMD_ALT_UICTX, uictx_on_cmd, NULL);
    LOG_INFO(LOG_MODULE, "AltScreen injector loaded (control negotiation + self-contained stream 111; CMD_ALT_HID/ZONE/APPEARANCE/UICTX registered)");
}
void altscreen_shutdown(void) { altav_shutdown(); }

__attribute__((constructor)) static void altscreen_module_init(void) { altscreen_init(); }
__attribute__((destructor))  static void altscreen_module_fini(void) { altscreen_shutdown(); }
