/*
 * CarPlay AltScreen — MHI2Q. dio-side `extendedFeatures` injector (LD_PRELOAD).
 *
 * VERIFIED activation gate (iOS 26.1 sender decompile, AirPlaySender/Activate.mm):
 * altScreen (instrument-cluster CarPlay, stream 111) turns on when the car's /info
 * advertises  extendedFeatures = ["altScreen"]  — a CFArray of feature-key strings.
 * The phone then sets carEndpoint DerivedStorage+63, calls CRFetchInstrumentClusterURLs
 * (needs a cluster-capable app installed, e.g. Apple Maps; empty -> -16721), and SETUPs
 * stream 111. Our from-source libairplay (../../../libairplay-mhi2q) IMPLEMENTS the 111
 * receiver (altScreenSession + security + _AltScreenThread + SetAltScreen + the
 * screenstream SS_FWD_PORT_ALT forward to maneuver_render). This hook supplies only the
 * DECLARATION, which is dio's job (feature *policy*, not libairplay's).
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
 * #1b (DONE): also inject info["displays"] = [main, cluster] — main from the stock
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
    set_str(d, "uuid", "C1057E12-A0D1-4E20-B0C0-1057E12A0D10");  /* distinct from main */
    /* properties = ["Instrument Cluster"]  ← the actual hasGaugeClusterScreen gate */
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
    return d;
}
static void *build_displays(void)
{
    int err = 0;
    void *mainDisp = fn_copydisp(0, &err);      /* arg ignored; builds via ScreenCopyMain */
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

void *AirPlayCopyServerInfo(void *session, void *qualifier, void *macAddr, void *outErr)
{
    static fn_copyinfo real;
    if (!real) real = (fn_copyinfo)dlsym(RTLD_NEXT, "AirPlayCopyServerInfo");
    if (!real) { LOG_ERROR(LOG_MODULE, "AirPlayCopyServerInfo real==NULL"); return 0; }

    void *info = real(session, qualifier, macAddr, outErr);   /* stock /info — untouched */

    if (info && cf_init() == 1) {
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

        /* #1b: displays = [main, cluster] → iOS hasGaugeClusterScreen */
        if (disp_ok == 1) {
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
        }
    }
    return info;
}

/* ════════════════════════════════════════════════════════════════════════════
 * #2 handle_alt — accept + bring up the altScreen 111 stream.
 *
 * Pure orchestration of stock libairplay 210.81 EXPORTS (no reimplementation):
 * cloned from stock _ScreenSetup (sub_26F9C) + _ScreenThread (sub_24220).  The
 * screen session stays opaque (obtained from Screen_Create, only handed to
 * exports); only a few TOP-LEVEL session + server struct offsets are read,
 * all RE-verified against MU1316 stock.  Alt-screen state lives in a hook-side
 * table (stock has no altScreen slots).  The NTP time-synchronizer is supplied
 * as our OWN trivial callback (ProcessFrames falls back to UpTicks anyway), so
 * NO fragile internal lib addresses are used.
 *
 * Gated by env ALTSCREEN_HANDLE_ALT (dark by default).  Fail-safe: if any stock
 * symbol is missing, handle_alt is skipped and /info advertise still works.
 * ════════════════════════════════════════════════════════════════════════════ */

/* stock session struct offsets (bytes, MU1316 210.81) */
#define OFF_SERVER          12    /* session+12  = server*                        */
#define OFF_TRANSPORTTYPE   80    /* session+80  = transportType (int)            */
#define OFF_FAMILY          85    /* session+85  = peerAddr sa_family (u8)         */
#define OFF_UUID            112   /* session+112 = sessionUUID[16]                */
#define OFF_DEVICEID        136   /* session+136 = clientDeviceID (u64)           */
#define OFF_SESSIONID       152   /* session+152 = sessionID (u32 low)            */
#define OFF_AESKEY          456   /* session+456 = aesSessionKey                  */
#define SVR_OFF_QOSDISABLED 452   /* server+452  = qosDisabled (u8)               */
#define SVR_OFF_TIMEOUT     456   /* server+456  = timeoutDataSecs (int)          */
#define SVR_OFF_STREAMOPTS  464   /* server+464  = screenStreamOptions*           */
#define ALT_STREAM_TYPE     111

/* exported stock function pointers (resolved lazily via RTLD_DEFAULT) */
typedef int      (*fn_scr_create_t)(void **);
typedef void     (*fn_scr_delete_t)(void *);
typedef int      (*fn_scr_setup_t)(void *, void *, uint32_t);
typedef int      (*fn_scr_setsec_t)(void *, const uint8_t *, const uint8_t *);
typedef int      (*fn_scr_start_t)(void *, void *);
typedef int      (*fn_scr_process_t)(void *, void *, int);
typedef void     (*fn_scr_updatestate_t)(void *, int);
typedef void     (*fn_scr_settimesync_t)(void *, const void *);
typedef void     (*fn_scr_setuuid_t)(void *, uint8_t *);
typedef void     (*fn_scr_setdevid_t)(void *, uint64_t);
typedef void     (*fn_scr_setendtime_t)(void *, double);
typedef int      (*fn_scr_sendcmd_t)(void *, int, void *, int);
typedef int      (*fn_sess_sendcmd_t)(void *, void *, void *, void *); /* AirPlayReceiverSessionSendCommand(session, cmdDict{type,params}, completion, ctx) */
typedef void     (*fn_derive_aes_t)(const uint8_t *, int, uint64_t, uint8_t *, uint8_t *);
typedef int      (*fn_srv_sockopen_t)(int, int, int, int, int *, int, int *);
typedef void     (*fn_sock_setqos_t)(int, int);
typedef int      (*fn_sock_accept_t)(int, int, int *, void *);
typedef int      (*fn_netsock_new_t)(void **, int);
typedef void     (*fn_netsock_del_t)(void **);
typedef double   (*fn_cfabs_now_t)(void);
typedef uint64_t (*fn_upticks_t)(void);
typedef int64_t  (*fn_cfdict_geti64_t)(void *, const void *, void *); /* CFDictionaryGetInt64(dict,key,OSStatus*outErr) */
typedef void *   (*fn_cfdict_getval_t)(void *, const void *);
typedef long     (*fn_cfarr_count_t)(void *);
typedef void *   (*fn_cfarr_getidx_t)(void *, long);

static fn_scr_create_t      scr_create;
static fn_scr_delete_t      scr_delete;
static fn_scr_setup_t       scr_setup;
static fn_scr_setsec_t      scr_setsec;
static fn_scr_start_t       scr_start;
static fn_scr_process_t     scr_process;
static fn_scr_updatestate_t scr_updatestate;
static fn_scr_settimesync_t scr_settimesync;
static fn_scr_setuuid_t     scr_setuuid;
static fn_scr_setdevid_t    scr_setdevid;
static fn_scr_setendtime_t  scr_setrecvend, scr_setauthend, scr_setntpend;
static fn_scr_sendcmd_t     scr_sendcmd;
static fn_sess_sendcmd_t    sess_sendcmd;   /* session-level {type,params} command shipper (zone switch) */
static fn_derive_aes_t      derive_aes;

/* main AirPlay session with a live altScreen — set on alt_setup success, cleared
 * on teardown.  Read by the CMD_ALT_ZONE handler to target AirPlayReceiverSessionSendCommand.
 * ponytail: single altScreen session assumed; aligned pointer store = atomic on ARMv7. */
static void *volatile g_alt_session;
static fn_srv_sockopen_t    srv_sockopen;
static fn_sock_setqos_t     sock_setqos;
static fn_sock_accept_t     sock_accept;
static fn_netsock_new_t     netsock_new;
static fn_netsock_del_t     netsock_del;
static fn_cfabs_now_t       cfabs_now;
static fn_upticks_t         fn_upticks;
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
        scr_create      = (fn_scr_create_t)     dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_Create");
        scr_delete      = (fn_scr_delete_t)     dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_Delete");
        scr_setup       = (fn_scr_setup_t)      dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_Setup");
        scr_setsec      = (fn_scr_setsec_t)     dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetSecurityInfo");
        scr_start       = (fn_scr_start_t)      dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_StartSession");
        scr_process     = (fn_scr_process_t)    dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_ProcessFrames");
        scr_updatestate = (fn_scr_updatestate_t)dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_UpdateState");
        scr_settimesync = (fn_scr_settimesync_t)dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetTimeSynchronizer");
        scr_setuuid     = (fn_scr_setuuid_t)    dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetSessionUUID");
        scr_setdevid    = (fn_scr_setdevid_t)   dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetClientDeviceID");
        scr_setrecvend  = (fn_scr_setendtime_t) dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetReceiveEndTime");
        scr_setauthend  = (fn_scr_setendtime_t) dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetAuthEndTime");
        scr_setntpend   = (fn_scr_setendtime_t) dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SetNTPEndTime");
        scr_sendcmd     = (fn_scr_sendcmd_t)    dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionScreen_SendCommand");
        sess_sendcmd    = (fn_sess_sendcmd_t)   dlsym(RTLD_DEFAULT, "AirPlayReceiverSessionSendCommand"); /* zone switch (non-critical) */
        derive_aes      = (fn_derive_aes_t)     dlsym(RTLD_DEFAULT, "AirPlay_DeriveAESKeySHA512ForScreen");
        srv_sockopen    = (fn_srv_sockopen_t)   dlsym(RTLD_DEFAULT, "ServerSocketOpen");
        sock_setqos     = (fn_sock_setqos_t)    dlsym(RTLD_DEFAULT, "SocketSetQoS");
        sock_accept     = (fn_sock_accept_t)    dlsym(RTLD_DEFAULT, "SocketAccept");
        netsock_new     = (fn_netsock_new_t)    dlsym(RTLD_DEFAULT, "NetSocket_CreateWithNative");
        netsock_del     = (fn_netsock_del_t)    dlsym(RTLD_DEFAULT, "NetSocket_Delete");
        cfabs_now       = (fn_cfabs_now_t)      dlsym(RTLD_DEFAULT, "CFAbsoluteTimeGetCurrent");
        fn_upticks      = (fn_upticks_t)        dlsym(RTLD_DEFAULT, "UpTicks");
        cf_geti64       = (fn_cfdict_geti64_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetInt64");
        cf_getval       = (fn_cfdict_getval_t)  dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
        cf_arrcount     = (fn_cfarr_count_t)    dlsym(RTLD_DEFAULT, "CFArrayGetCount");
        cf_arridx       = (fn_cfarr_getidx_t)   dlsym(RTLD_DEFAULT, "CFArrayGetValueAtIndex");
        /* all critical must resolve; else skip handle_alt (advertise still works) */
        alt_syms_ok = (scr_create && scr_setup && scr_setsec && scr_start && scr_process &&
                       scr_updatestate && scr_settimesync && scr_setuuid && scr_setdevid &&
                       scr_setrecvend && scr_setauthend && scr_setntpend && derive_aes &&
                       srv_sockopen && sock_accept && netsock_new && cfabs_now && fn_upticks &&
                       cf_geti64 && cf_getval && cf_arrcount && cf_arridx && cf_dictm && cf_dictseti) ? 1 : 0;
        LOG_INFO(LOG_MODULE, "alt_resolve syms_ok=%d", alt_syms_ok);
    }
    pthread_mutex_unlock(&alt_lock);
    return alt_syms_ok;
}

/* trivial NTP synchronizer — value only affects presentation-late detection,
 * which is moot for the tapped cluster stream.  Matches the UpTicks fallback. */
static uint64_t alt_get_ntp(void *ctx)                 { (void)ctx; return fn_upticks(); }
static uint64_t alt_get_upticks_near(void *ctx, uint64_t t) { (void)ctx; (void)t; return fn_upticks(); }

/* hook-side alt-screen state (stock has no altScreen slots) */
typedef struct {
    void       *session;     /* key */
    void       *altSess;     /* AirPlayReceiverSessionScreenRef */
    int         altSock;     /* listen socket */
    pthread_t   thread;
    int         threadValid;
    int         inUse;
    void       *cfgLast;     /* last-forwarded Annex-B SPS/PPS: ptr + ... */
    uint32_t    cfgLastLen;  /* ... length + ...                          */
    uint32_t    cfgLastSum;  /* ... checksum (addr can be reused)         */
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

/* the slot whose receiver thread == the caller (ScreenStreamProcessData runs
 * inline on our alt thread — verified in ProcessFrames), else NULL for the main
 * stream.  This is how we tell OUR alt 111 stream from the stock 110 stream. */
static altslot_t *alt_slot_for_thread(void)
{
    pthread_t me = pthread_self();
    altslot_t *hit = NULL;
    int i;
    pthread_mutex_lock(&alt_lock);
    for (i = 0; i < ALT_MAX_SESSIONS; i++)
        if (alt_slots[i].inUse && alt_slots[i].threadValid &&
            pthread_equal(alt_slots[i].thread, me)) { hit = &alt_slots[i]; break; }
    pthread_mutex_unlock(&alt_lock);
    return hit;
}

/* ScreenStreamPrivate field offsets (MU1316 210.81) — verified in stock
 * _ScreenStreamSetProperty (writes) + ScreenStreamProcessData (reads):
 *   +40 = Annex-B SPS/PPS header ptr (H264ConvertAVCCtoAnnexBHeader of avcC)
 *   +44 = its length,  +52 = AVCC NAL length-prefix size. */
#define SS_OFF_CFG      40
#define SS_OFF_CFGLEN   44
#define SS_OFF_LENSIZE  52

/* ── #3: tap the alt 111 stream's decoded NALs → forward to altscreen_render ───
 * Global interpose of the exported ScreenStreamProcessData.  libairplay calls it
 * (via PLT) for BOTH the stock main 110 stream and our alt 111 stream; we
 * distinguish by thread.  For the alt stream we forward the decrypted AVCC frame
 * (converted to Annex-B) + the Annex-B SPS/PPS header, then swallow the call
 * (return 0 + fire the completion) — we must NOT let stock decode it (its decoder
 * targets the main display / isn't configured for the cluster).  The main stream
 * passes straight through to the real function.
 *
 * Frame data arrives already AES-decrypted (ProcessFrames runs AES_CTR_Update
 * before calling us).  Prototype from the binary:
 *   OSStatus ScreenStreamProcessData(ScreenStreamPrivate*, uint8_t* data,
 *     size_t len, uint64_t ts, CFLDictionary*, void(*done)(void*), void* ctx). */
typedef int (*fn_sspd_t)(void *, const uint8_t *, size_t, uint64_t, void *, void (*)(void *), void *);
/* This interpose runs concurrently on the stock main-screen thread AND our alt
 * thread, so resolve `real` via pthread_once (a lock-free `if(!real)` would be a
 * data race on the pointer across those threads). */
static fn_sspd_t       sspd_real;
static pthread_once_t  sspd_once = PTHREAD_ONCE_INIT;
static void sspd_resolve(void) { sspd_real = (fn_sspd_t)dlsym(RTLD_NEXT, "ScreenStreamProcessData"); }

int ScreenStreamProcessData(void *self, const uint8_t *data, size_t len, uint64_t ts,
                            void *dict, void (*done)(void *), void *ctx)
{
    pthread_once(&sspd_once, sspd_resolve);

    altslot_t *slot = alt_slot_for_thread();
    if (slot) {
        if (self && data && len) {
            void    *cfg    = *(void **)   ((char *)self + SS_OFF_CFG);
            uint32_t cfglen = *(uint32_t *)((char *)self + SS_OFF_CFGLEN);
            uint8_t  ls     = (uint8_t)*(uint32_t *)((char *)self + SS_OFF_LENSIZE);
            if (cfg && cfglen && cfglen <= 4096) {       /* Annex-B SPS/PPS header */
                /* dedup on (ptr,len,checksum): an allocator can reuse the same
                 * address for a new config, so address alone is insufficient. */
                const uint8_t *cb = (const uint8_t *)cfg;
                uint32_t sum = 0, k;
                for (k = 0; k < cfglen && k < 256; k++) sum = sum * 131u + cb[k];
                if (cfg != slot->cfgLast || cfglen != slot->cfgLastLen || sum != slot->cfgLastSum) {
                    altav_forward_frame(cb, cfglen);
                    slot->cfgLast = cfg; slot->cfgLastLen = cfglen; slot->cfgLastSum = sum;
                }
            }
            altav_forward_avcc_frame(data, (uint32_t)len, ls);
        }
        if (done) done(ctx);                             /* caller expects the release cb */
        return 0;
    }

    if (!sspd_real) return -6740;
    return sspd_real(self, data, len, ts, dict, done, ctx);   /* main 110 stream — untouched */
}

/* _AddResponseStream replica (stock sub_26EFC): append desc to resp["streams"] */
static void alt_add_response_stream(void *responseParams, void *streamDesc)
{
    void *kstreams = cf_str(0, "streams", CFENC_UTF8);
    if (!kstreams) return;
    void *arr = cf_getval(responseParams, kstreams);
    if (arr) {
        cf_arrapp(arr, streamDesc);
    } else {
        arr = cf_arrm(0, 0, arr_cb);
        if (arr) {
            cf_arrapp(arr, streamDesc);
            cf_dictset(responseParams, kstreams, arr);
            if (cf_rel) cf_rel(arr);
        }
    }
    if (cf_rel) cf_rel(kstreams);
}

/* receiver thread — clone of stock _ScreenThread (sub_24220), alt slots */
static void *alt_thread_main(void *arg)
{
    altslot_t *slot   = (altslot_t *)arg;
    void      *session = slot->session;
    void      *altSess = slot->altSess;
    int        fd = -1;
    void      *netSock = NULL;
    struct { void *ctx; void *f1; void *f2; } ts;
    void      *server;
    void      *opts;
    int        timeout;
    int        ls;

    if (sock_accept(slot->altSock, 10, &fd, NULL) != 0 || fd < 0) goto done;
    /* claim + close the listen socket (teardown may race to close it too — the
     * -1 sentinel under alt_lock ensures exactly one close). */
    pthread_mutex_lock(&alt_lock);
    ls = slot->altSock; slot->altSock = -1;
    pthread_mutex_unlock(&alt_lock);
    if (ls >= 0) close(ls);

    scr_setuuid(altSess, (uint8_t *)((char *)session + OFF_UUID));
    scr_setdevid(altSess, *(uint64_t *)((char *)session + OFF_DEVICEID));

    if (netsock_new(&netSock, fd) != 0 || !netSock) goto done;
    fd = -1;

    ts.ctx = session; ts.f1 = (void *)alt_get_ntp; ts.f2 = (void *)alt_get_upticks_near;
    scr_settimesync(altSess, &ts);
    scr_setrecvend(altSess, cfabs_now());
    scr_setauthend(altSess, cfabs_now());
    scr_setntpend(altSess, cfabs_now());

    server  = *(void **)((char *)session + OFF_SERVER);
    opts    = server ? *(void **)((char *)server + SVR_OFF_STREAMOPTS) : NULL;
    timeout = server ? *(int *)((char *)server + SVR_OFF_TIMEOUT) : 30;

    if (scr_start(altSess, opts) != 0) goto done;
    LOG_INFO(LOG_MODULE, "alt-screen receiver started");
    scr_process(altSess, netSock, timeout);

done:
    if (netSock && netsock_del) netsock_del(&netSock);
    if (fd >= 0) close(fd);
    LOG_INFO(LOG_MODULE, "alt-screen receiver thread exit");
    return NULL;
}

/* setup — clone of stock _ScreenSetup (sub_26F9C) for stream type 111 */
static int alt_setup(void *session, void *streamDesc, void *responseParams)
{
    altslot_t *slot;
    void  *altSess = NULL;
    void  *server;
    int    err, family, port = 0, sock = -1;
    int64_t connID;
    uint8_t key[16], iv[16];
    void  *rdesc;

    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_get(session);
    pthread_mutex_unlock(&alt_lock);
    if (!slot) return -6728;
    if (slot->altSess) return -6719;   /* already set up */

    err = scr_create(&altSess);
    if (err || !altSess) return err ? err : -6728;
    scr_updatestate(altSess, 1);

    err = scr_setup(altSess, streamDesc, *(uint32_t *)((char *)session + OFF_SESSIONID));
    if (err) goto fail;

    {
        void *kconn = cf_str(0, "streamConnectionID", CFENC_UTF8);
        connID = kconn ? cf_geti64(streamDesc, kconn, NULL) : 0;
        if (kconn && cf_rel) cf_rel(kconn);
    }
    if (!connID) { err = -6715; goto fail; }

    derive_aes((const uint8_t *)((char *)session + OFF_AESKEY), 16, (uint64_t)connID, key, iv);
    err = scr_setsec(altSess, key, iv);
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));
    if (err) goto fail;

    family = *(uint8_t *)((char *)session + OFF_FAMILY);
    err = srv_sockopen(family, 1 /*SOCK_STREAM*/, 6 /*IPPROTO_TCP*/, -6030, &port, -180224, &sock);
    if (err) goto fail;

    server = *(void **)((char *)session + OFF_SERVER);
    if (server && sock_setqos && !*(uint8_t *)((char *)server + SVR_OFF_QOSDISABLED))
        sock_setqos(sock, 12);

    /* Publish to the slot + start the receiver thread under alt_lock, BEFORE
     * advertising the port.  This (a) publishes thread/threadValid atomically vs
     * alt_slot_for_thread readers, and (b) leaves nothing half-live / no stale
     * g_alt_session if pthread_create fails — the fail path frees the locals. */
    pthread_mutex_lock(&alt_lock);
    slot->altSess = altSess;
    slot->altSock = sock;
    if (pthread_create(&slot->thread, NULL, alt_thread_main, slot) == 0) {
        slot->threadValid = 1;
        g_alt_session = session;      /* enable CMD_ALT_* targeting */
    } else {
        slot->altSess = NULL;         /* hand ownership back to the fail-path locals */
        slot->altSock = -1;
    }
    int started = slot->threadValid;
    pthread_mutex_unlock(&alt_lock);
    if (!started) { err = -6728; goto fail; }

    /* acceptor is live → NOW advertise the data port in the 111 response */
    rdesc = cf_dictm(0, 0, dict_keycb, dict_valcb);
    if (rdesc) {
        set_i64(rdesc, "type", ALT_STREAM_TYPE);
        set_i64(rdesc, "dataPort", port);
        alt_add_response_stream(responseParams, rdesc);
        if (cf_rel) cf_rel(rdesc);
    }
    LOG_INFO(LOG_MODULE, "alt-screen set up on port %d connID=%llu",
             port, (unsigned long long)connID);
    return 0;

fail:
    if (sock >= 0) close(sock);
    if (altSess && scr_delete) scr_delete(altSess);
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

    if (alt_enabled < 0) alt_enabled = getenv("ALTSCREEN_HANDLE_ALT") ? 1 : 0;
    if (alt_enabled && ret == 0 && respOut && *respOut && session && request && alt_resolve() == 1) {
        void *altDesc = alt_find_111(request);
        if (altDesc) {
            LOG_INFO(LOG_MODULE, "111 stream offered — bringing up altScreen");
            (void)alt_setup(session, altDesc, *respOut);
        }
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

    altslot_t *slot;
    pthread_mutex_lock(&alt_lock);
    slot = alt_slot_find(session);
    pthread_mutex_unlock(&alt_lock);
    if (slot) {
        if (slot->threadValid) {
            /* break a blocking SocketAccept: close the listen socket first (sentinel
             * vs the thread's own post-accept close), THEN 'q' the ProcessFrames
             * select loop, THEN join.  Otherwise a teardown before the phone connects
             * would stall on join until the 10s accept timeout. */
            int ls;
            pthread_mutex_lock(&alt_lock);
            ls = slot->altSock; slot->altSock = -1;
            pthread_mutex_unlock(&alt_lock);
            if (ls >= 0) close(ls);
            if (scr_sendcmd && slot->altSess) scr_sendcmd(slot->altSess, 'q', NULL, 0);
            pthread_join(slot->thread, NULL);
            slot->threadValid = 0;
        }
        if (slot->altSock >= 0) { close(slot->altSock); slot->altSock = -1; }
        if (slot->altSess && scr_delete) scr_delete(slot->altSess);
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
    LOG_INFO(LOG_MODULE, "session command '%s' ack: status=%d", ctx ? (const char *)ctx : "?", status);
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
    LOG_INFO(LOG_MODULE, "AltScreen injector loaded (advertise always-on; handle_alt gated by ALTSCREEN_HANDLE_ALT; CMD_ALT_HID/ZONE/APPEARANCE/UICTX registered)");
}
void altscreen_shutdown(void) { altav_shutdown(); }

__attribute__((constructor)) static void altscreen_module_init(void) { altscreen_init(); }
__attribute__((destructor))  static void altscreen_module_fini(void) { altscreen_shutdown(); }
