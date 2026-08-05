/* libcarplay_replay.so  --  replay a REAL captured CarPlay callback trace INTO
 * from-source dio_manager on QEMU, with NO phone.
 *
 * LD_PRELOAD this into the from-source dio CarPlay bring-up (the reconstructed
 * dio::CDIOManager::CAirPlayMainThread::run(), exercised on QEMU via
 * carplay/trackf_fromsrc_probe -- same from-source libairplay). It:
 *
 *   1. PASSIVELY interposes AirPlayReceiverServerCreate to grab dio's REAL server
 *      pointer, and AirPlayReceiverServerSetDelegate to grab dio's REAL 15-word
 *      server delegate table + dio's context (delegate[0]). dio's real delegate is
 *      passed straight through to the real SetDelegate so dio boots normally.
 *   2. After SetDelegate fires, spawns a driver thread that reads the recorded
 *      jsonl (REPLAY_TRACE) and, in seq order, CALLS dio's captured delegate
 *      callbacks with reconstructed args -- object ids in the trace are mapped:
 *        o#<server-id>  -> dio's real server      (learned from the trace's own
 *        o#<context-id> -> dio's real context       SetDelegate record)
 *        every other o# -> a synthesized placeholder (one malloc'd dummy per id).
 *      Recorded ts deltas are honored (usleep, capped) so dio's timeout logic runs.
 *   3. Session funcs dio's callbacks may call BACK into libairplay with a SYNTH
 *      session (AirPlayReceiverSessionSetDelegate / ...ScreenSetDelegate) are
 *      interposed and STUBBED *only* for synth ptrs (log + benign return), so the
 *      lifecycle can't die dereferencing a fake session in real libairplay. A real
 *      session (there is none in this no-phone harness) passes straight through.
 *
 * FRAMES (Phase 2): no live libairplay stream ctx exists with no phone, so the priv is
 * RECONSTRUCTED via the REAL exported ScreenStreamCreate (@0x25344) and the recorded
 * AVCC is fed through the REAL exported ScreenStreamProcessData (@0x257d8) -- the genuine
 * frame-delivery boundary. On QEMU omx_stub answers OMX_GetHandle=ComponentNotFound so
 * ScreenStreamStart cannot reach EXECUTING -> ProcessData returns at its require(running)
 * guard: frames reach the real entry, the OMX decoder handoff is blocked exactly where
 * the absent Qualcomm OMX blocks it (honest QEMU limit). See screen_setup()/driver().
 *
 * Slot map (recon_airplay.h AirPlayReceiverServerDelegate, 15 words):
 *   [0]=context [3]=copyProperty [5]=sessionCreated [6]=sessionCreationFailed
 *   [7]=sessionScreenCreated [8]=audioType [9]=waitAudioAvailableAck
 *
 * QNX 6.5 / ARMv7. TRAP: NO __thread (emutls). Only libc.so.3.
 */
#define _QNX_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ state */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static void  *g_server = 0;       /* dio's real AirPlayReceiverServer   */
static void  *g_deleg[15];        /* dio's real server delegate (copied) */
static void  *g_ctx = 0;          /* dio's real context = delegate[0]    */
static int    g_spawned = 0;
static char  *g_buf = 0;          /* whole trace file                    */
static long   g_len = 0;
static long   g_maxgap_us = 500000;  /* per-event sleep cap; REPLAY_MAXGAP_MS */

/* trace object-id -> real/synth pointer. Learned server/ctx ids + dummies. */
static int    g_srv_id = -1, g_ctx_id = -1;
struct dent { int id; void *p; };
#define DMAX 4096
static struct dent g_dmap[DMAX];
static int g_dn = 0;

/* libairplay CoreFoundation-lite exports, resolved once (real key synthesis +
 * Phase 3 value reconstruction: decode the trace's captured bplist valData back
 * into a real CFTypeRef and deposit it so from-source dio returns the unit's
 * actual per-key value). All VA-cited from the staged libairplay.so exports. */
static void *(*g_CFStrNew)(void*, const char*, unsigned int) = 0;  /* @0x79b50 */
static void  (*g_CFRelease)(void*) = 0;                            /* @0x78048 */
static void *(*g_CFDataCreate)(void*, const unsigned char*, long) = 0;      /* @0x7895c */
static void *(*g_CFPLWithData)(void*, void*, unsigned long, void*, void*) = 0; /* @0x7ace0 CFPropertyListCreateWithData */
static void *(*g_CFPLData)(void*, void*, long, unsigned long, void*) = 0;   /* @0x7ab48 CFPropertyListCreateData */
static const unsigned char *(*g_CFDataPtr)(void*) = 0;             /* @0x78ba4 CFDataGetBytePtr */
static long  (*g_CFDataLen)(void*) = 0;                            /* @0x78b68 CFDataGetLength */
static int   (*g_CFEqual)(void*, void*) = 0;                       /* @0x78068 */

/* CDIOManagerConfig copy-property result slots (recon_airplay.h; offsetof self-
 * checked): ctx==delegate.context==cfg; provider @0x158690 returns *(cfg+0x1f0)
 * with *outErr=*(cfg+0x1f4) after the worker deposits them. We ARE the worker. */
#define CFG_COPYVAL_OFF 0x1f0   /* void*  copyPropValue (@0x158714 LDR R4,[R4,#0x1F0]) */
#define CFG_COPYERR_OFF 0x1f4   /* int    copyPropErr   (@0x158710 LDRNE R3,[R4,#0x1F4]) */
#define kCFPropertyListBinaryFormat_v1_0 200

static int    g_err_diverge = 0, g_val_diverge = 0;   /* per-key comparison tally */
static int    g_match = 0, g_diverge = 0;             /* Phase 3: full {val,err} match tally */

/* --- Phase 6: pre-reconstructed value cache (concurrency race fix) -----------
 * The per-key CF reconstruct+deposit+FREE (Phase 3) raced libairplay's OWN
 * concurrent server-thread copyProperty. Both callers read the SINGLE shared cfg
 * slot cfg->copyPropValue@+0x1f0; the provider (CDIOManager::s_serverCopyProperty
 * Callback @0x158690, CAirPlayMainThread_impl.cpp:218 -- stock @0x158714 `LDR R4,
 * [R4,#0x1F0]`) returns that pointer WITHOUT CFRetain. So when the driver CFReleased
 * the deposited value, the concurrent reader (AirPlay/Bonjour startup info-gather)
 * was left using a freed CFTypeRef -> use-after-free -> libc _list_memalign SIGSEGV.
 * Only the values trace hits it (the stable trace deposits NULL and frees nothing).
 * CFL has NO pluggable allocator (CFLDataCreate rejects allocator!=NULL with -6705
 * and uses libc calloc directly), so a malloc-backed CFAllocator is impossible.
 * FIX: reconstruct EVERY value ONCE at SetDelegate time -- single-threaded, before
 * AirPlayReceiverServerControl(START) spawns the server threads -- cache the CFTypeRefs,
 * and during replay only DEPOSIT the cached (still-live) pointer. No CF alloc/free on
 * the driver thread in the concurrent window; the concurrent reader always sees a live
 * object. The cache is released ONCE at driver DONE (post info-burst, slot cleared). */
struct vcache { char key[64]; void *cfkey; void *cfval; void *cfdata;
                int isBplist; int byteexact; };
#define VCMAX 64
static struct vcache g_vc[VCMAX];
static int g_vcn = 0;
static struct vcache *vc_find(const char *key) {
    int i; if (!key) return 0;
    for (i = 0; i < g_vcn; i++) if (!strcmp(g_vc[i].key, key)) return &g_vc[i];
    return 0;
}

/* --- Phase 2: real from-source libairplay ScreenStream* video path (Option A) ---
 * The from-source libairplay (staged) exports the REAL screen-stream API; its
 * ScreenStreamProcessData is the genuine frame-delivery boundary the AirPlay engine
 * feeds decrypted AVCC into (screenstream.c:414). With no phone there is no live
 * stream ctx, so we reconstruct the `priv` with the REAL exported ScreenStreamCreate
 * (@0x25344) and feed the recorded AVCC through the REAL ScreenStreamProcessData
 * (@0x257d8). On QEMU omx_stub answers OMX_GetHandle with ComponentNotFound, so
 * ScreenStreamStart (@0x255ec) cannot reach EXECUTING -> me->running stays 0 ->
 * ProcessData returns at its require(running) guard (no OMX deref, no crash): the
 * frames reach the real entry, the decoder handoff is blocked exactly where the
 * absent Qualcomm OMX blocks it (the honest QEMU limit). */
static void *g_ss = 0;                 /* ScreenStreamImp* from real ScreenStreamCreate */
static int   g_ss_tried = 0, g_ss_running = 0;
static int  (*g_ScreenStreamCreate)(void**) = 0;                    /* @0x25344 */
static int  (*g_ScreenStreamStart)(void*) = 0;                      /* @0x255ec */
static int  (*g_ScreenStreamProcessData)(void*, const unsigned char*, size_t,
                                         unsigned long long, void*,
                                         void(*)(void*), void*) = 0; /* @0x257d8 */
static long long g_frame_bytes = 0;

static void LOGS(const char *s) { (void)write(2, s, strlen(s)); }
static void LOGF(const char *fmt, ...) {
    char b[512]; va_list ap; int n;
    __builtin_va_start(ap, fmt);
    n = vsnprintf(b, sizeof b, fmt, ap);
    __builtin_va_end(ap);
    if (n > 0) { if (n > (int)sizeof b) n = sizeof b; (void)write(2, b, (size_t)n); }
}

/* map a trace object id to a pointer for the replay call */
static void *ptr_for(int id) {
    int i;
    if (id <= 0) return 0;                 /* o#0 == NULL */
    if (id == g_srv_id) return g_server;
    if (id == g_ctx_id) return g_ctx;
    for (i = 0; i < g_dn; i++) if (g_dmap[i].id == id) return g_dmap[i].p;
    if (g_dn < DMAX) {                      /* synth placeholder (small dummy) */
        void *p = calloc(1, 64);
        g_dmap[g_dn].id = id; g_dmap[g_dn].p = p; g_dn++;
        return p;
    }
    return 0;
}
static int is_synth(const void *p) {
    int i;
    if (!p) return 0;
    for (i = 0; i < g_dn; i++) if (g_dmap[i].p == p) return 1;
    return 0;
}

/* ---------------------------------------------------------- tiny json scan */
/* return integer after "key": (signed). def if not present on this line. */
static long jnum(const char *line, const char *key, long def) {
    const char *p = strstr(line, key);
    if (!p) return def;
    p += strlen(key);
    while (*p == ' ' || *p == ':' || *p == '"') p++;
    return strtol(p, 0, 10);
}
/* return the id in  "<field>":"o#N"  , or -1 if absent. */
static int joid(const char *line, const char *field) {
    char pat[48]; const char *p;
    snprintf(pat, sizeof pat, "\"%s\":\"o#", field);
    p = strstr(line, pat);
    if (!p) return -1;
    p += strlen(pat);
    return (int)strtol(p, 0, 10);
}
/* copy the fn value into out; return 1 if found. */
static int jfn(const char *line, char *out, int cap) {
    const char *p = strstr(line, "\"fn\":\"");
    const char *e; int n;
    if (!p) return 0;
    p += 6; e = strchr(p, '"');
    if (!e) return 0;
    n = (int)(e - p); if (n >= cap) n = cap - 1;
    memcpy(out, p, n); out[n] = 0;
    return 1;
}
/* copy a "<field>":"<str>" value into out; return 1 if found. */
static int jstr(const char *line, const char *field, char *out, int cap) {
    char pat[48]; const char *p; const char *e; int n;
    snprintf(pat, sizeof pat, "\"%s\":\"", field);
    p = strstr(line, pat); if (!p) return 0;
    p += strlen(pat); e = strchr(p, '"'); if (!e) return 0;
    n = (int)(e - p); if (n >= cap) n = cap - 1;
    memcpy(out, p, n); out[n] = 0;
    return 1;
}
/* 64-bit numeric fetch (QNX long is 32-bit -> pts/ts saturate under jnum; use this
 * for the 13-digit mach-tick pts so frame pacing is real). */
static long long jnum64(const char *line, const char *key, long long def) {
    const char *p = strstr(line, key);
    if (!p) return def;
    p += strlen(key);
    while (*p == ' ' || *p == ':' || *p == '"') p++;
    return strtoll(p, 0, 10);
}
static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* decode  "<field>":"<hex>"  into a malloc'd byte buffer; returns byte len (>0),
 * -1 on absence/parse error. Caller frees *out. */
static long jhex(const char *line, const char *field, unsigned char **out) {
    char pat[32]; const char *p; const char *e; long hn, n, i;
    unsigned char *b;
    snprintf(pat, sizeof pat, "\"%s\":\"", field);
    p = strstr(line, pat); if (!p) return -1;
    p += strlen(pat); e = strchr(p, '"'); if (!e) return -1;
    hn = (long)(e - p); if (hn < 2 || (hn & 1)) return -1;
    n = hn / 2; b = (unsigned char*)malloc((size_t)n); if (!b) return -1;
    for (i = 0; i < n; i++) {
        int hi = hexval(p[2*i]), lo = hexval(p[2*i+1]);
        if (hi < 0 || lo < 0) { free(b); return -1; }
        b[i] = (unsigned char)((hi << 4) | lo);
    }
    *out = b; return n;
}

/* --------------------------------------------------- delegate call thunks */
static void resolve_cf(void) {
    if (!g_CFStrNew) g_CFStrNew = (void*(*)(void*, const char*, unsigned int))
        dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
    if (!g_CFRelease) g_CFRelease = (void(*)(void*))dlsym(RTLD_DEFAULT, "CFRelease");
    if (!g_CFDataCreate) g_CFDataCreate = (void*(*)(void*, const unsigned char*, long))
        dlsym(RTLD_DEFAULT, "CFDataCreate");
    if (!g_CFPLWithData) g_CFPLWithData = (void*(*)(void*, void*, unsigned long, void*, void*))
        dlsym(RTLD_DEFAULT, "CFPropertyListCreateWithData");
    if (!g_CFPLData) g_CFPLData = (void*(*)(void*, void*, long, unsigned long, void*))
        dlsym(RTLD_DEFAULT, "CFPropertyListCreateData");
    if (!g_CFDataPtr) g_CFDataPtr = (const unsigned char*(*)(void*))dlsym(RTLD_DEFAULT, "CFDataGetBytePtr");
    if (!g_CFDataLen) g_CFDataLen = (long(*)(void*))dlsym(RTLD_DEFAULT, "CFDataGetLength");
    if (!g_CFEqual) g_CFEqual = (int(*)(void*, void*))dlsym(RTLD_DEFAULT, "CFEqual");
}

/* Reconstruct the REAL CFTypeRef the unit returned from the trace's captured
 * bplist bytes: CFDataCreate(valData) -> CFPropertyListCreateWithData. Returns
 * the CFTypeRef (caller CFReleases) and hands back the intermediate CFData in
 * *outData (also CFReleased by caller). NULL on decode failure / non-bplist. */
static void *recon_value(unsigned char *bytes, long n, void **outData) {
    void *d, *v;
    *outData = 0;
    if (!bytes || n <= 0 || !g_CFDataCreate || !g_CFPLWithData) return 0;
    d = g_CFDataCreate(0, bytes, n);
    if (!d) return 0;
    v = g_CFPLWithData(0, d, 0 /*immutable*/, 0, 0);
    *outData = d;
    return v;   /* may be NULL if the bytes are not a valid plist */
}
/* Drive the REAL server copyProperty provider with a FAITHFUL CFStringRef key
 * AND deposit the unit's REAL captured value into the cfg result slots so the
 * from-source provider (@0x158690) returns it -- exactly as the on-unit worker
 * (CAirPlayServiceEvent_SessionCopyProperty::execute) deposits copyPropValue/
 * copyPropErr at cfg+0x1f0/+0x1f4. Then compare dio's returned {value,err} to
 * the unit-recorded trace ret, per key.
 *   bplist key: cfg+0x1f0 = CFPropertyListCreateWithData(valData), +0x1f4 = err(0)
 *   null   key: cfg+0x1f0 = NULL,                                  +0x1f4 = err(-6714)
 * Value fidelity is verified by re-serializing dio's returned CFTypeRef back to a
 * binary plist and byte-diffing it against the unit's captured valData (with a
 * CFEqual semantic fallback). */
static void call_copyProperty(const char *keyStr, int keyid, int ctxid,
                              int recValId, long recErr,
                              const char *valType, const char *line) {
    typedef void *(*fn)(void*, void*, const void*, int*, void*);
    fn f = (fn)g_deleg[3]; int err = 0; void *r; void *cfkey = 0;
    void *ctxp, *cfval = 0;
    int isBplist = (valType && !strcmp(valType, "bplist"));
    int deposited = 0, vmatch, ematch, byteexact = 0;
    struct vcache *vc;
    (void)recValId; (void)line;
    if (!f) return;

    /* Look up the value RECONSTRUCTED ONCE at SetDelegate time (single-threaded,
     * pre-START). NO CF alloc/free here -- the cached CFTypeRef stays live for the
     * whole run, so libairplay's concurrent server-thread copyProperty (which reads
     * the same shared cfg slot without retaining) can never use a freed value. */
    vc = vc_find(keyStr);
    if (vc) { cfkey = vc->cfkey; cfval = vc->cfval; byteexact = vc->byteexact; }
    if (!cfkey) cfkey = ptr_for(keyid);                /* fallback placeholder key */
    ctxp = ptr_for(ctxid);

    /* DEPOSIT into the cfg result slots -- ONLY into the real cfg (g_ctx), never a
     * synth 64-byte placeholder (writing +0x1f0 there would overflow the heap). The
     * deposited pointer is cached (live); it is NOT freed here (see free_value_cache). */
    if (ctxp && ctxp == g_ctx) {
        *(void**)((char*)ctxp + CFG_COPYVAL_OFF) = cfval;    /* NULL for null keys */
        *(int*)  ((char*)ctxp + CFG_COPYERR_OFF) = (int)recErr;
        deposited = 1;
    }

    r = f(ptr_for(g_srv_id), cfkey, 0, &err, ctxp);

    /* per-key fidelity: dio-replayed {value,err} vs unit-recorded {value,err}.
     * VALUE check for a bplist key = pointer identity: dio's provider returns
     * cfg->copyPropValue (@0x158714), i.e. the exact CFTypeRef we reconstructed from
     * the unit's captured valData and deposited (r==cfval, non-NULL). `byteexact` was
     * precomputed single-threaded at SetDelegate (re-serialize + byte-diff vs the raw
     * captured valData) so it is now race-free for EVERY bplist key incl. the first. */
    ematch = (err == (int)recErr);
    vmatch = isBplist ? (r != 0 && r == cfval) : (r == 0);
    if (vmatch && ematch) g_match++; else g_diverge++;
    if (!vmatch) g_val_diverge++;
    if (!ematch) g_err_diverge++;

    LOGF("[REPLAY] copyProperty %-18s dio{val=%p err=%d} unit{%s err=%ld} depd=%p %s val=%s err=%s %s%s%s\n",
         (keyStr && keyStr[0]) ? keyStr : "?", r, err,
         isBplist ? "bplist" : "null", recErr, cfval,
         deposited ? "[deposited]" : "[no-cfg]",
         vmatch ? "MATCH" : "DIVERGE",
         ematch ? "MATCH" : "DIVERGE",
         (vmatch && ematch) ? "=> KEY-MATCH" : "=> KEY-DIVERGE",
         (isBplist && byteexact) ? " [byte-exact roundtrip]" : "",
         (isBplist && !cfval) ? " (valData decode FAILED)" : "");
}

/* Reconstruct EVERY unique copyProperty value ONCE, single-threaded, BEFORE the
 * driver thread and BEFORE Control(START) spawns libairplay's server threads --
 * this is the whole race fix: all CF allocation + byte-exact verification happens
 * in the quiet boot window; the busy phase only deposits cached live pointers.
 * Non-destructive scan of g_buf (temporarily NUL-terminates each line, restores). */
static void precompute_values(void) {
    char *p = g_buf;
    resolve_cf();
    while (p && *p) {
        char *le = strchr(p, '\n');
        char fn[64], ks[64], vt[16];
        if (le) *le = 0;
        if (jfn(p, fn, sizeof fn) && !strcmp(fn, "cb.copyProperty") &&
            jstr(p, "keyStr", ks, sizeof ks) && !vc_find(ks) && g_vcn < VCMAX) {
            struct vcache *v = &g_vc[g_vcn];
            memset(v, 0, sizeof *v);
            strncpy(v->key, ks, sizeof v->key - 1);
            if (!jstr(p, "valType", vt, sizeof vt)) vt[0] = 0;
            v->isBplist = !strcmp(vt, "bplist");
            if (g_CFStrNew && ks[0]) v->cfkey = g_CFStrNew(0, ks, 0x08000100u); /* UTF8 key */
            if (v->isBplist) {
                unsigned char *vb = 0; long vl = jhex(p, "valData", &vb);
                if (vl > 0) {
                    v->cfval = recon_value(vb, vl, &v->cfdata);
                    if (v->cfval && g_CFPLData && g_CFDataPtr && g_CFDataLen) {
                        void *gd = g_CFPLData(0, v->cfval, kCFPropertyListBinaryFormat_v1_0, 0, 0);
                        if (gd) {
                            const unsigned char *gb = g_CFDataPtr(gd);
                            long gl = g_CFDataLen(gd);
                            if (gb && gl == vl && memcmp(gb, vb, (size_t)vl) == 0) v->byteexact = 1;
                            g_CFRelease(gd);
                        }
                    }
                }
                if (vb) free(vb);              /* boot-thread free: single-threaded, safe */
            }
            g_vcn++;
        }
        if (le) { *le = '\n'; p = le + 1; } else break;
    }
    LOGF("[REPLAY] precomputed %d unique copyProperty values (single-threaded, pre-START) "
         "-- deposit is now CF-alloc-free; no shared-allocator race\n", g_vcn);
}

/* Release the value cache ONCE, after the replay burst. Clear the shared cfg slot
 * first so any late concurrent copyProperty reads NULL, not a to-be-freed pointer. */
static void free_value_cache(void) {
    int i;
    if (g_ctx) {
        *(void**)((char*)g_ctx + CFG_COPYVAL_OFF) = 0;
        *(int*)  ((char*)g_ctx + CFG_COPYERR_OFF) = 0;
    }
    for (i = 0; i < g_vcn; i++) {
        if (g_vc[i].cfval  && g_CFRelease) g_CFRelease(g_vc[i].cfval);
        if (g_vc[i].cfdata && g_CFRelease) g_CFRelease(g_vc[i].cfdata);
        if (g_vc[i].cfkey  && g_CFRelease) g_CFRelease(g_vc[i].cfkey);
        g_vc[i].cfval = g_vc[i].cfdata = g_vc[i].cfkey = 0;
    }
}
static void call_sessionCreated(int sessid, int ctxid) {
    typedef void (*fn)(void*, void*, void*);
    fn f = (fn)g_deleg[5];
    LOGF("[REPLAY] -> sessionCreated(session=o#%d)\n", sessid);
    if (f) f(ptr_for(g_srv_id), ptr_for(sessid), ptr_for(ctxid));
}
static void call_sessionCreationFailed(int status, int ctxid) {
    typedef void (*fn)(void*, int, void*);
    fn f = (fn)g_deleg[6];
    LOGF("[REPLAY] -> sessionCreationFailed(status=%d)\n", status);
    if (f) f(ptr_for(g_srv_id), status, ptr_for(ctxid));
}
static void call_sessionScreenCreated(int ctxid) {
    typedef void (*fn)(void*);
    fn f = (fn)g_deleg[7];
    LOGF("[REPLAY] -> sessionScreenCreated(ctx=o#%d)\n", ctxid);
    if (f) f(ptr_for(ctxid));
}
static void call_audioType(int type, int ctxid) {
    typedef void (*fn)(int, void*);
    fn f = (fn)g_deleg[8];
    LOGF("[REPLAY] -> audioType(%d)\n", type);
    if (f) f(type, ptr_for(ctxid));
}
static void call_waitAudioAck(int arg, int ctxid) {
    typedef void (*fn)(int, void*);
    fn f = (fn)g_deleg[9];
    LOGF("[REPLAY] -> waitAudioAvailableAck(%d)\n", arg);
    if (f) f(arg, ptr_for(ctxid));
}

/* Reconstruct the libairplay screen-stream priv with the REAL exported
 * ScreenStreamCreate, then attempt the REAL ScreenStreamStart. Lazy one-shot:
 * called on the first frame so it runs only if the trace actually has video. */
static void screen_setup(void) {
    int rc;
    if (g_ss_tried) return;
    g_ss_tried = 1;
    g_ScreenStreamCreate      = (int(*)(void**))dlsym(RTLD_DEFAULT, "ScreenStreamCreate");
    g_ScreenStreamStart       = (int(*)(void*))dlsym(RTLD_DEFAULT, "ScreenStreamStart");
    g_ScreenStreamProcessData = (int(*)(void*, const unsigned char*, size_t, unsigned long long,
                                        void*, void(*)(void*), void*))
                                dlsym(RTLD_DEFAULT, "ScreenStreamProcessData");
    if (!g_ScreenStreamCreate || !g_ScreenStreamProcessData) {
        LOGF("[REPLAY] screen: exports missing (Create=%p ProcessData=%p) -- frame inject disabled\n",
             (void*)g_ScreenStreamCreate, (void*)g_ScreenStreamProcessData);
        return;
    }
    rc = g_ScreenStreamCreate(&g_ss);
    LOGF("[REPLAY] screen: ScreenStreamCreate rc=%d stream=%p (real libairplay priv reconstructed)\n",
         rc, g_ss);
    if (rc != 0 || !g_ss) { g_ss = 0; return; }
    if (g_ScreenStreamStart) {
        int sr = g_ScreenStreamStart(g_ss);
        g_ss_running = (sr == 0);
        LOGF("[REPLAY] screen: ScreenStreamStart rc=%d (%s)\n", sr,
             g_ss_running ? "EXECUTING -- OMX decoder up"
                          : "OMX_GetHandle=ComponentNotFound on QEMU -> running=0, ProcessData gated at require(running)");
    }
}

/* ------------------------------------------------------------ driver body */
static void bind_ids(void) {
    /* learn which o# ids the capture assigned to server/context from the trace's
     * own SetDelegate record (robust vs assuming o#1/o#2). */
    char *p = strstr(g_buf, "AirPlayReceiverServerSetDelegate");
    if (p) {
        char *ls = p; while (ls > g_buf && ls[-1] != '\n') ls--;
        char *le = strchr(p, '\n'); if (le) *le = 0;
        g_srv_id = joid(ls, "server");
        g_ctx_id = joid(ls, "context");
        if (le) *le = '\n';
    }
    if (g_srv_id < 0) g_srv_id = 1;   /* documented capture default o#1 */
    if (g_ctx_id < 0) g_ctx_id = 2;   /* documented capture default o#2 */
    LOGF("[REPLAY] bound server=o#%d->%p  context=o#%d->%p\n",
         g_srv_id, g_server, g_ctx_id, g_ctx);
}

static void *driver(void *arg) {
    char *line, *save = 0; long prev_ts = 0; long long prev_pts = 0;
    int nframe = 0, nprop = 0, nlc = 0, ndeliv = 0;
    (void)arg;
    bind_ids();
    LOGS("[REPLAY] === replay driver START ===\n");

    for (line = strtok_r(g_buf, "\n", &save); line; line = strtok_r(0, "\n", &save)) {
        char fn[64];
        if (!jfn(line, fn, sizeof fn)) continue;
        if (strncmp(fn, "cb.", 3) != 0) continue;    /* only delegate callbacks */

        /* pace: honor recorded ts delta (capped) between acted events */
        {
            long ts = jnum(line, "\"ts\":", 0);
            if (prev_ts && ts > prev_ts) {
                long us = (ts - prev_ts) / 1000;
                if (us > g_maxgap_us) us = g_maxgap_us;
                if (us > 0) usleep((useconds_t)us);
            }
            if (ts) prev_ts = ts;
        }

        if (!strcmp(fn, "cb.copyProperty")) {
            char ks[64], vt[16];
            if (!jstr(line, "keyStr", ks, sizeof ks)) ks[0] = 0;
            if (!jstr(line, "valType", vt, sizeof vt)) vt[0] = 0;
            call_copyProperty(ks, joid(line, "key"), joid(line, "context"),
                              joid(line, "value"), jnum(line, "\"err\":", 0), vt, line);
            nprop++;
        } else if (!strcmp(fn, "cb.sessionScreenCreated")) {
            call_sessionScreenCreated(joid(line, "context")); nlc++;
        } else if (!strcmp(fn, "cb.sessionCreated")) {
            call_sessionCreated(joid(line, "session"), joid(line, "context")); nlc++;
        } else if (!strcmp(fn, "cb.audioType")) {
            call_audioType((int)jnum(line, "\"audioType\":", 0), joid(line, "context")); nlc++;
        } else if (!strcmp(fn, "cb.waitAudioAvailableAck")) {
            call_waitAudioAck((int)jnum(line, "\"arg\":", 0), joid(line, "context")); nlc++;
        } else if (!strcmp(fn, "cb.sessionCreationFailed")) {
            call_sessionCreationFailed((int)jnum(line, "\"status\":", 0), joid(line, "context")); nlc++;
        } else if (!strcmp(fn, "cb.screenFrame")) {
            /* pace by the recorded pts (real ~26fps cadence; ts is capture-time and
             * saturates the 32-bit generic pacer above, so pace here with 64-bit pts). */
            long long pts = jnum64(line, "\"pts\":", 0);
            if (prev_pts && pts > prev_pts) {
                long long us = (pts - prev_pts) / 1000;
                if (us > g_maxgap_us) us = g_maxgap_us;
                if (us > 0) usleep((useconds_t)us);
            }
            if (pts) prev_pts = pts;

            screen_setup();     /* one-shot real ScreenStream bring-up on first frame */
            {
                unsigned char *avcc = 0;
                long n = jhex(line, "data", &avcc);
                long nal = jnum(line, "\"nal\":", -1);
                if (n <= 0) {
                    LOGS("[REPLAY] screenFrame: hex decode failed\n");
                } else {
                    int rc = -12345;
                    if (g_ss && g_ScreenStreamProcessData) {
                        rc = g_ScreenStreamProcessData(g_ss, avcc, (size_t)n,
                                                       (unsigned long long)pts, 0, 0, 0);
                        ndeliv++;
                    }
                    g_frame_bytes += n;
                    if (nframe < 4 || (nframe % 200) == 0)
                        LOGF("[REPLAY] screenFrame #%d nal=%ld len=%ld -> ScreenStreamProcessData rc=%d %s\n",
                             nframe, nal, n, rc,
                             g_ss_running ? "[running: fed to OMX EmptyThisBuffer]"
                                          : "[running=0: gated at require(running), OMX absent on QEMU]");
                    free(avcc);
                }
            }
            nframe++;
        }
        /* cb.audioFrame is PCM-passthrough reference; dio consumes no such cb */
    }
    if (g_ss)
        LOGF("[REPLAY] screen: frames=%d delivered-to-ScreenStreamProcessData=%d bytes=%lld running=%d "
             "(running=0 => real entry reached, OMX decoder handoff gated: QEMU has no Qualcomm OMX)\n",
             nframe, ndeliv, g_frame_bytes, g_ss_running);
    else if (nframe)
        LOGF("[REPLAY] screen: %d frames present but ScreenStream priv not reconstructed (see screen: log)\n", nframe);
    LOGF("[REPLAY] === replay driver DONE: copyProperty=%d lifecycle=%d screenFrames=%d(injected=%d) ===\n",
         nprop, nlc, nframe, ndeliv);
    LOGF("[REPLAY] copyProperty per-key compare: KEY-MATCH=%d KEY-DIVERGE=%d of %d "
         "(val-diverge=%d err-diverge=%d) [CF %s]\n",
         g_match, g_diverge, nprop, g_val_diverge, g_err_diverge,
         (g_CFPLWithData && g_CFPLData) ? "value-recon resolved" : "value-recon UNRESOLVED");
    free_value_cache();   /* release cached CFTypeRefs once, post-burst (slot cleared) */
    return 0;
}

/* ---------------------------------------------------- passive interposers */
int AirPlayReceiverServerCreate(void **outServer) {
    static int (*real)(void**) = 0;
    int r;
    if (!real) real = (int(*)(void**))dlsym(RTLD_NEXT, "AirPlayReceiverServerCreate");
    r = real(outServer);
    if (r == 0 && outServer && *outServer) {
        pthread_mutex_lock(&g_lock);
        g_server = *outServer;
        pthread_mutex_unlock(&g_lock);
        LOGF("[REPLAY] captured server=%p (ServerCreate)\n", g_server);
    }
    return r;
}

void AirPlayReceiverServerSetDelegate(void *server, const void *delegate) {
    static void (*real)(void*, const void*) = 0;
    const void *const *in = (const void *const *)delegate;
    int i, spawn = 0;
    if (!real) real = (void(*)(void*,const void*))dlsym(RTLD_NEXT, "AirPlayReceiverServerSetDelegate");

    pthread_mutex_lock(&g_lock);
    if (server) g_server = server;
    for (i = 0; i < 15; i++) g_deleg[i] = in ? (void*)in[i] : 0;
    g_ctx = g_deleg[0];
    if (!g_spawned && g_buf) { g_spawned = 1; spawn = 1; }
    pthread_mutex_unlock(&g_lock);

    LOGF("[REPLAY] captured delegate: server=%p ctx=%p copyProperty=%p sessionCreated=%p "
         "screenCreated=%p audioType=%p waitAck=%p\n",
         g_server, g_ctx, g_deleg[3], g_deleg[5], g_deleg[7], g_deleg[8], g_deleg[9]);

    real(server, delegate);     /* pass dio's REAL delegate through unchanged */

    if (spawn) {
        pthread_t th;
        /* RACE FIX: reconstruct + cache all copyProperty values here -- single-
         * threaded, before Control(START) brings up libairplay's server threads --
         * so the driver's replay never CF-allocates/frees in the concurrent window. */
        precompute_values();
        if (pthread_create(&th, 0, driver, 0) != 0)
            LOGS("[REPLAY] pthread_create(driver) FAILED\n");
        else
            pthread_detach(th);
    }
}

/* Session funcs dio's callbacks call back into libairplay with a SYNTH session.
 * Stub ONLY when the session ptr is one of our synth placeholders (a real session
 * -- none here, no phone -- passes straight through to real libairplay). */
void AirPlayReceiverSessionSetDelegate(void *session, const void *delegate) {
    static void (*real)(void*, const void*) = 0;
    if (!real) real = (void(*)(void*,const void*))dlsym(RTLD_NEXT, "AirPlayReceiverSessionSetDelegate");
    if (is_synth(session)) {
        LOGF("[REPLAY]    (stub) SessionSetDelegate(synth session=%p) -> benign\n", session);
        return;
    }
    real(session, delegate);
}
void AirPlayReceiverSessionScreenSetDelegate(void *session, const void *delegate) {
    static void (*real)(void*, const void*) = 0;
    if (!real) real = (void(*)(void*,const void*))dlsym(RTLD_NEXT, "AirPlayReceiverSessionScreenSetDelegate");
    if (is_synth(session)) {
        LOGF("[REPLAY]    (stub) SessionScreenSetDelegate(synth session=%p) -> benign\n", session);
        return;
    }
    real(session, delegate);
}

/* ------------------------------------------------------------------- init */
__attribute__((constructor))
static void replay_init(void) {
    const char *path = getenv("REPLAY_TRACE");
    const char *mg = getenv("REPLAY_MAXGAP_MS");
    struct stat st; int fd; long off = 0; ssize_t r;
    if (mg && *mg) g_maxgap_us = strtol(mg, 0, 10) * 1000L;
    if (!(path && *path)) { LOGS("[REPLAY] REPLAY_TRACE unset -- driver disabled\n"); return; }
    fd = open(path, O_RDONLY);
    if (fd < 0) { LOGF("[REPLAY] open(%s) failed -- driver disabled\n", path); return; }
    if (fstat(fd, &st) != 0 || st.st_size <= 0) { close(fd); LOGS("[REPLAY] stat failed\n"); return; }
    g_buf = (char*)malloc((size_t)st.st_size + 1);
    if (!g_buf) { close(fd); LOGS("[REPLAY] malloc failed\n"); return; }
    while (off < st.st_size) {
        r = read(fd, g_buf + off, (size_t)(st.st_size - off));
        if (r <= 0) break;
        off += r;
    }
    close(fd);
    g_buf[off] = 0; g_len = off;
    LOGF("[REPLAY] loaded trace %s (%ld bytes), maxgap=%ldus\n", path, g_len, g_maxgap_us);
}
