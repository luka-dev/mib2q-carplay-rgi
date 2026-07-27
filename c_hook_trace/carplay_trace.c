/* carplay_trace  --  on-UNIT capture hook for QEMU replay of from-source dio
 *
 * LD_PRELOAD this on the REAL unit against STOCK libs + a real iPhone. It records
 * the two boundaries dio_manager sees during a CarPlay session into ONE jsonl
 * trace. That trace then drives OUR from-source dio_manager on QEMU with no phone.
 *
 * WHY these two boundaries (and NOT a naive phone-socket replay):
 *   - The phone media is MFi-encrypted with EPHEMERAL, non-deterministic session
 *     keys (ECDH per connect; MFi out of scope). Recorded wire bytes are
 *     ciphertext -> they will NOT decrypt under from-source's fresh session keys,
 *     so a wire-replay dies at pair-verify. The wire log below is a REFERENCE /
 *     structural record only.
 *   - The REPLAYABLE boundary is libairplay -> dio, POST-decrypt: stock libairplay
 *     decrypts on the unit and invokes dio's delegate callbacks. Record that
 *     callback stream, and on QEMU replay it INTO from-source dio (call its
 *     registered callbacks in order) -> dio runs its full CarPlay path, crypto and
 *     phone entirely bypassed.
 *
 * TWO layers captured:
 *   L1  phone wire  (accept/connect/recv/send/read/write/recvfrom/sendto) -> the
 *       reference record: connection, RTSP control (plaintext), stream framing.
 *   L2  libairplay->dio delegate callbacks (server-side, slots reversed in the
 *       oracle probe / recon_airplay.h): copyProperty, sessionCreated,
 *       sessionCreationFailed, sessionScreenCreated, audioType, waitAudioAvailAck,
 *       + session teardown. This is the replay driver's input.
 *
 * L2-media  decoded FRAMES (post-decrypt): screen H.264 AVCC via the exported
 *       ScreenStreamProcessData, and PCM via exported ReadAudio/WriteAudio. RE
 *       showed the session screen/audio delegate carries NO frame slot (screen
 *       setter is a dead stub; session delegate is control-only) -> we interpose
 *       the exported process-data funcs directly. -> cb.screenFrame/cb.audioFrame.
 *
 * QNX 6.5 / ARMv7. TRAP: NO __thread (emutls). Single mutex guards seq + id map.
 * Lineage: the interposer bodies are lifted from from-source/tools/oracle/probe/
 * probe.c (same proven pattern); this is the standalone on-unit capture cut, no
 * twin/oracle semantic-trace baggage.
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
#include <time.h>
#include <sys/types.h>

/* ------------------------------------------------------------------ state */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long   g_seq  = 0;
static int             g_fd   = 2;     /* PROBE_LOG file, else stderr */
static long            g_io_cap = 0;   /* PROBE_IO_CAP payload cap; 0 = full */
/* Total-bytes budget for the BULK writers (media frames + wire) so a RAM-backed
 * /tmp capture can't fill memory and wedge the unit. Lifecycle emit() is tiny +
 * ungated, so the session skeleton keeps recording after the budget is spent.
 * PROBE_MAX overrides; 0 = unlimited (use only on real disk). */
static long            g_max     = 64L * 1024 * 1024;  /* default 64 MB (RAM-safe) */
static long            g_written = 0;
/* L1 wire (socket) capture is OFF by default: it's the highest-frequency capture,
 * on CarPlay's hot real-time network threads, and full-hex-under-lock there starves
 * the live session (black screen / early -6722). The wire is reference-only ciphertext
 * the replay driver never uses, so default off costs nothing. PROBE_WIRE=1 to study
 * the wire on a throwaway session where stability does not matter. */
static int             g_wire    = 0;

/* pointer -> stable id (deterministic first-seen). ponytail: linear scan, event
 * count is low; hash only if a full session ever makes it hot. */
#define MAP_MAX 8192
struct ent { void *p; int id; char ns; };
static struct ent g_map[MAP_MAX];
static int g_map_n = 0, g_next_obj = 0, g_next_tid = 0;

static int id_of(void *p, char ns) {
    int i, id;
    if (!p) return 0;
    pthread_mutex_lock(&g_lock);
    for (i = 0; i < g_map_n; i++)
        if (g_map[i].p == p && g_map[i].ns == ns) { id = g_map[i].id; goto out; }
    id = (ns == 't') ? ++g_next_tid : ++g_next_obj;
    if (g_map_n < MAP_MAX) { g_map[g_map_n].p = p; g_map[g_map_n].id = id;
                             g_map[g_map_n].ns = ns; g_map_n++; }
out:
    pthread_mutex_unlock(&g_lock);
    return id;
}
#define OID(p) id_of((void*)(p), 'o')

/* ---- async writer: hot threads only build a line + enqueue it; ONE background
 * thread does every write(). Keeps all write()+long-lock-holds off CarPlay's
 * real-time threads — synchronous capture delayed the audio-ack callback return
 * and AirPlay dropped the session (-6722). Bounded queue DROPS under overload
 * rather than stall a hot thread or grow RAM. */
struct qnode { char *line; long len; struct qnode *next; };
static pthread_mutex_t g_qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_qcond = PTHREAD_COND_INITIALIZER;
static struct qnode   *g_qhead = 0, *g_qtail = 0;
static int             g_qcount = 0, g_qstop = 0, g_writer_ok = 0;
static unsigned long   g_qdrop = 0;
static pthread_t       g_writer;
#define QMAX 2048

static unsigned long g_seq_next(void) {   /* ordered seq, brief lock */
    unsigned long s;
    pthread_mutex_lock(&g_lock); s = g_seq++; pthread_mutex_unlock(&g_lock);
    return s;
}
/* take ownership of a malloc'd line (len bytes); writer writes+frees it. If the
 * writer thread never started, fall back to a direct write so nothing is lost. */
static void enqueue(char *line, long len) {
    struct qnode *n;
    if (!line) return;
    if (!g_writer_ok) { (void)write(g_fd, line, (size_t)len); free(line); return; }
    pthread_mutex_lock(&g_qlock);
    if (g_qcount >= QMAX) { g_qdrop++; pthread_mutex_unlock(&g_qlock); free(line); return; }
    n = (struct qnode *)malloc(sizeof *n);
    if (!n) { pthread_mutex_unlock(&g_qlock); free(line); return; }
    n->line = line; n->len = len; n->next = 0;
    if (g_qtail) g_qtail->next = n; else g_qhead = n;
    g_qtail = n; g_qcount++; g_written += len;
    pthread_cond_signal(&g_qcond);
    pthread_mutex_unlock(&g_qlock);
}
static void *writer_thread(void *a) {
    (void)a;
    for (;;) {
        struct qnode *n;
        pthread_mutex_lock(&g_qlock);
        while (!g_qhead && !g_qstop) pthread_cond_wait(&g_qcond, &g_qlock);
        n = g_qhead;
        if (n) { g_qhead = n->next; if (!g_qhead) g_qtail = 0; g_qcount--; }
        pthread_mutex_unlock(&g_qlock);
        if (n) { (void)write(g_fd, n->line, (size_t)n->len); free(n->line); free(n); }
        else break;   /* stop requested + queue drained */
    }
    return 0;
}
static void writer_flush(void) {   /* atexit: drain the tail (teardown events) */
    if (!g_writer_ok) return;
    pthread_mutex_lock(&g_qlock); g_qstop = 1; pthread_cond_signal(&g_qcond); pthread_mutex_unlock(&g_qlock);
    pthread_join(g_writer, 0);
    g_writer_ok = 0;
}

/* emit one callback/lifecycle event (args/ret are pre-built JSON fragments). */
static void emit(const char *fn, int corr, int obj, const char *args, const char *ret) {
    struct timespec tp; unsigned long long tns; int tid, n; unsigned long seq;
    char *line = (char *)malloc(1024);
    if (!line) return;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    tns = (unsigned long long)tp.tv_sec * 1000000000ull + (unsigned long long)tp.tv_nsec;
    tid = id_of((void *)(uintptr_t)pthread_self(), 't');
    seq = g_seq_next();
    n = snprintf(line, 1024,
        "{\"seq\":%lu,\"corr\":\"o#%d\",\"tid\":\"t#%d\",\"fn\":\"%s\",\"args\":%s,\"ret\":%s,\"obj\":\"o#%d\",\"ts\":%llu}\n",
        seq, corr, tid, fn, args, ret, obj, tns);
    if (n <= 0) { free(line); return; }
    if (n > 1024) n = 1024;
    enqueue(line, n);
}

/* ================= L2: libairplay -> dio delegate callbacks ============== */
/* AirPlayReceiverServerDelegate is 15 words; replayable slot map (recon_airplay.h):
 *   [0]=context [3]=copyProperty [5]=sessionCreated [6]=sessionCreationFailed
 *   [7]=sessionScreenCreated [8]=audioType [9]=waitAudioAvailableAck */
static void *g_orig[15];
static void *g_wrap[15];

/* Resolve a CFStringRef key to UTF-8 (JSON-escaped) into out. libairplay embeds
 * CoreUtils/CFLite -> CFStringGetCStringPtr/GetCString are dynamic; dlsym'd lazily.
 * If unavailable (or key not a CFString) out="" -> graceful, no regression.
 * The replay driver rebuilds real CFStringRef keys from this string.
 * ponytail: property keys are short ASCII; 128 is plenty. */
static void keystr(void *key, char *out, int cap) {
    static const char *(*getptr)(void*, unsigned) = 0;
    static int (*getcs)(void*, char*, long, unsigned) = 0;
    static int tried = 0;
    const char *s = 0; char raw[128]; int i, j = 0;
    out[0] = 0;
    if (!key) return;
    if (!tried) {
        tried = 1;
        getptr = (const char*(*)(void*,unsigned))dlsym(RTLD_NEXT, "CFStringGetCStringPtr");
        getcs  = (int(*)(void*,char*,long,unsigned))dlsym(RTLD_NEXT, "CFStringGetCString");
    }
    if (getptr) s = getptr(key, 0x08000100u);              /* kCFStringEncodingUTF8 */
    if (!s) { if (getcs && getcs(key, raw, sizeof raw, 0x08000100u)) s = raw; }
    if (!s) return;
    for (i = 0; s[i] && j < cap - 3; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '"' || ch == '\\') { out[j++] = '\\'; out[j++] = (char)ch; }
        else if (ch >= 0x20 && ch < 0x7f) out[j++] = (char)ch;   /* drop ctrl/non-ASCII */
    }
    out[j] = 0;
}

/* Serialize the CFTypeRef value dio returns from copyProperty into a portable,
 * replay-reconstructable JSON fragment (the "valType"/"valData"/... key:vals, no
 * leading comma; caller frees). Mirrors keystr's dlsym-from-libairplay pattern.
 *
 * Primary path: CFPropertyListCreateData(NULL, r, 200 = binary plist v1.0, 0,
 * NULL) @0x7ab48 -> a CFData; hex its bytes into valData, valType="bplist". One
 * serializer covers every property value dio hands back: CFString (model),
 * CFData (oemIcon), CFNumber/CFBoolean (flags), CFArray/CFDictionary (features).
 * RE (VA 0x7ab48): format arg is checked `cmp r2,#200` -> ONLY binary v1.0 (200)
 * is honored; allocator NULL = default; error* NULL is fine.
 *
 * Fallback (r is a non-plist CFType -> CFPropertyListCreateData returns NULL):
 * CFGetTypeID(r) @0x77fb0 compared against the exported per-type getters
 * (CFStringGetTypeID@0x79608, CFDataGetTypeID@0x78944, CFNumberGetTypeID@0x794c4,
 * CFBooleanGetTypeID@0x788e0, CFArrayGetTypeID@0x78100, CFDictionaryGetTypeID
 * @0x79104) -> valType = type name + numeric valTypeId. (This build EXPORTS no
 * CFCopyDescription/CFShow, so no human description string; the type id is the
 * honest, replay-useful record for the rare non-plist value.)
 *
 * r==NULL (err path, e.g. -6714) -> valType="null".
 *
 * PASSIVE + no leak: reads r only, does NOT consume its refcount (r is returned
 * unchanged to dio), and CFReleases the CFData it creates. Runs AFTER the real
 * callback, ~21x/session (not per-frame) -> cost is negligible. Blob capped:
 * PROBE_IO_CAP if set, else a sane 256 KB (oemIcon-sized values stay full-fidelity,
 * a pathological value can't bloat RAM); a truncated blob is marked valTrunc:1. */
static char *dupfrag(const char *s) { char *p = (char *)malloc(strlen(s) + 1); if (p) strcpy(p, s); return p; }

static char *valfrag(void *r) {
    static const char hx[] = "0123456789abcdef";
    static void *(*plcreate)(void*,void*,long,unsigned long,void**) = 0;
    static const unsigned char *(*datbytes)(void*) = 0;
    static long (*datlen)(void*) = 0;
    static void (*cfrel)(void*) = 0;
    static unsigned long (*gettype)(void*) = 0;
    static unsigned long (*t_str)(void) = 0, (*t_dat)(void) = 0, (*t_num)(void) = 0,
                         (*t_bool)(void) = 0, (*t_arr)(void) = 0, (*t_dict)(void) = 0;
    static int tried = 0;
    if (!r) return dupfrag("\"valType\":\"null\"");
    if (!tried) {
        tried = 1;
        plcreate = (void*(*)(void*,void*,long,unsigned long,void**))dlsym(RTLD_NEXT, "CFPropertyListCreateData");
        datbytes = (const unsigned char*(*)(void*))dlsym(RTLD_NEXT, "CFDataGetBytePtr");
        datlen   = (long(*)(void*))dlsym(RTLD_NEXT, "CFDataGetLength");
        cfrel    = (void(*)(void*))dlsym(RTLD_NEXT, "CFRelease");
        gettype  = (unsigned long(*)(void*))dlsym(RTLD_NEXT, "CFGetTypeID");
        t_str  = (unsigned long(*)(void))dlsym(RTLD_NEXT, "CFStringGetTypeID");
        t_dat  = (unsigned long(*)(void))dlsym(RTLD_NEXT, "CFDataGetTypeID");
        t_num  = (unsigned long(*)(void))dlsym(RTLD_NEXT, "CFNumberGetTypeID");
        t_bool = (unsigned long(*)(void))dlsym(RTLD_NEXT, "CFBooleanGetTypeID");
        t_arr  = (unsigned long(*)(void))dlsym(RTLD_NEXT, "CFArrayGetTypeID");
        t_dict = (unsigned long(*)(void))dlsym(RTLD_NEXT, "CFDictionaryGetTypeID");
    }
    /* primary: binary plist */
    if (plcreate && datbytes && datlen && cfrel) {
        void *d = plcreate(0, r, 200, 0, 0);   /* alloc=NULL, kCFPropertyListBinaryFormat_v1_0, opts=0, err=NULL */
        if (d) {
            long n = datlen(d), vcap = (g_io_cap > 0) ? g_io_cap : 262144;
            long cap = (n > vcap) ? vcap : n, i, p;
            const unsigned char *b = datbytes(d);
            char *out = (char *)malloc(96 + (size_t)cap * 2);
            if (out && b) {
                p = snprintf(out, 96, "\"valType\":\"bplist\",\"valLen\":%ld,\"valTrunc\":%d,\"valData\":\"",
                             n, (cap < n) ? 1 : 0);
                for (i = 0; i < cap; i++) { out[p++] = hx[b[i] >> 4]; out[p++] = hx[b[i] & 0xf]; }
                out[p++] = '"'; out[p] = 0;
            } else if (out) { out[0] = 0; }
            cfrel(d);
            if (out && out[0]) return out;
            free(out);
        }
    }
    /* fallback: identify the (non-plist) CFType by id */
    if (gettype) {
        unsigned long id = gettype(r); const char *nm = "other"; char buf[80];
        if      (t_str  && id == t_str())  nm = "string";
        else if (t_dat  && id == t_dat())  nm = "data";
        else if (t_num  && id == t_num())  nm = "number";
        else if (t_bool && id == t_bool()) nm = "boolean";
        else if (t_arr  && id == t_arr())  nm = "array";
        else if (t_dict && id == t_dict()) nm = "dictionary";
        snprintf(buf, sizeof buf, "\"valType\":\"%s\",\"valTypeId\":%lu", nm, id);
        return dupfrag(buf);
    }
    return dupfrag("\"valType\":\"unknown\"");
}

/* copyProperty gets its OWN dynamically-sized emit (not the fixed-1024 emit()):
 * a full oemIcon bplist blob can be tens of KB, so the line is malloc'd to fit
 * args + the value fragment. Same jsonl shape as emit(); valType/valData live
 * inside "ret". ponytail: only this one event needs a variable-size line. */
static void emit_cp(int corr, int obj, const char *args, int valobj, int err, const char *vf) {
    struct timespec tp; unsigned long long tns; int tid, n; unsigned long seq;
    size_t need = 256 + strlen(args) + (vf ? strlen(vf) : 0);
    char *line = (char *)malloc(need);
    if (!line) return;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    tns = (unsigned long long)tp.tv_sec * 1000000000ull + (unsigned long long)tp.tv_nsec;
    tid = id_of((void *)(uintptr_t)pthread_self(), 't');
    seq = g_seq_next();
    n = snprintf(line, need,
        "{\"seq\":%lu,\"corr\":\"o#%d\",\"tid\":\"t#%d\",\"fn\":\"cb.copyProperty\",\"args\":%s,"
        "\"ret\":{\"value\":\"o#%d\",\"err\":%d%s%s},\"obj\":\"o#%d\",\"ts\":%llu}\n",
        seq, corr, tid, args, valobj, err, vf ? "," : "", vf ? vf : "", obj, tns);
    if (n <= 0) { free(line); return; }
    if ((size_t)n >= need) n = (int)need - 1;   /* snprintf truncated -> emit what fit */
    enqueue(line, n);
}

static void *tr_copyProperty(void *server, void *key, const void *qual, int *outErr, void *ctx) {
    typedef void *(*fn)(void*,void*,const void*,int*,void*);
    fn o = (fn)g_orig[3];
    void *r = o ? o(server, key, qual, outErr, ctx) : 0;
    char args[256], ks[160]; char *vf;
    keystr(key, ks, sizeof ks);
    snprintf(args, sizeof args, "{\"server\":\"o#%d\",\"key\":\"o#%d\",\"keyStr\":\"%s\",\"context\":\"o#%d\"}",
             OID(server), OID(key), ks, OID(ctx));
    vf = valfrag(r);   /* serialize AFTER the real call; passive, CFRelease-clean */
    emit_cp(OID(ctx), OID(server), args, OID(r), outErr ? *outErr : 0, vf);
    free(vf);
    return r;
}
static void tr_sessionCreated(void *server, void *session, void *ctx) {
    typedef void (*fn)(void*,void*,void*);
    if (g_orig[5]) ((fn)g_orig[5])(server, session, ctx);
    char args[96];
    snprintf(args, sizeof args, "{\"server\":\"o#%d\",\"session\":\"o#%d\",\"context\":\"o#%d\"}",
             OID(server), OID(session), OID(ctx));
    emit("cb.sessionCreated", OID(ctx), OID(session), args, "{\"void\":1}");
}
static void tr_sessionCreationFailed(void *server, int status, void *ctx) {
    typedef void (*fn)(void*,int,void*);
    if (g_orig[6]) ((fn)g_orig[6])(server, status, ctx);
    char args[96];
    snprintf(args, sizeof args, "{\"server\":\"o#%d\",\"status\":%d,\"context\":\"o#%d\"}",
             OID(server), status, OID(ctx));
    emit("cb.sessionCreationFailed", OID(ctx), OID(server), args, "{\"void\":1}");
}
static void tr_sessionScreenCreated(void *ctx) {
    typedef void (*fn)(void*);
    if (g_orig[7]) ((fn)g_orig[7])(ctx);
    char args[48];
    snprintf(args, sizeof args, "{\"context\":\"o#%d\"}", OID(ctx));
    emit("cb.sessionScreenCreated", OID(ctx), OID(ctx), args, "{\"void\":1}");
}
/* AUDIO slots 8 (audioType) / 9 (waitAudioAvailableAck) are NOT wrapped: their
 * signatures were guessed (void(int,void*)); if the real ones return a value or
 * differ, the trampoline corrupts audio negotiation -> the session dies -6722
 * ~1s in (reproduced across sync/async/wire-off — deterministic, not latency).
 * Passing dio's originals through untouched keeps the audio path exact. We lose
 * only the audioType/waitAck log lines (trivial: audioType=1, arg=5000). */

void AirPlayReceiverServerSetDelegate(void *server, const void *delegate) {
    static void (*real)(void*, const void*) = 0;
    if (!real) real = (void(*)(void*,const void*))dlsym(RTLD_NEXT, "AirPlayReceiverServerSetDelegate");
    const void *const *in = (const void *const *)delegate;
    int i;
    pthread_mutex_lock(&g_lock);
    for (i = 0; i < 15; i++) { g_orig[i] = (void *)in[i]; g_wrap[i] = (void *)in[i]; }
    if (g_orig[3]) g_wrap[3] = (void *)tr_copyProperty;
    if (g_orig[5]) g_wrap[5] = (void *)tr_sessionCreated;
    if (g_orig[6]) g_wrap[6] = (void *)tr_sessionCreationFailed;
    if (g_orig[7]) g_wrap[7] = (void *)tr_sessionScreenCreated;
    /* slots 8/9 (audio) left as dio's originals — see note above */
    pthread_mutex_unlock(&g_lock);
    char args[96];
    snprintf(args, sizeof args, "{\"server\":\"o#%d\",\"context\":\"o#%d\",\"wrapped\":4}",
             OID(server), OID((void*)in[0]));
    emit("AirPlayReceiverServerSetDelegate", OID(server), OID(server), args, "{\"void\":1}");
    real(server, g_wrap);
}

/* server lifecycle bookends (dio imports these) — frame the session in the trace */
int AirPlayReceiverServerControl(void *server, int flags, void *cmd, void *a, void *b, void *c) {
    static int (*real)(void*,int,void*,void*,void*,void*) = 0;
    if (!real) real = (int(*)(void*,int,void*,void*,void*,void*))dlsym(RTLD_NEXT, "AirPlayReceiverServerControl");
    int r = real(server, flags, cmd, a, b, c);
    char args[96], ret[32];
    snprintf(args, sizeof args, "{\"server\":\"o#%d\",\"flags\":%d,\"command\":\"o#%d\"}", OID(server), flags, OID(cmd));
    snprintf(ret, sizeof ret, "{\"err\":%d}", r);
    emit("AirPlayReceiverServerControl", OID(server), OID(server), args, ret);
    return r;
}
int AirPlayReceiverSessionTearDown(void *session, void *reason, int err, void *flag) {
    static int (*real)(void*,void*,int,void*) = 0;
    if (!real) real = (int(*)(void*,void*,int,void*))dlsym(RTLD_NEXT, "AirPlayReceiverSessionTearDown");
    int r = real(session, reason, err, flag);
    char args[96], ret[32];
    snprintf(args, sizeof args, "{\"session\":\"o#%d\",\"reason\":\"o#%d\",\"err\":%d}", OID(session), OID(reason), err);
    snprintf(ret, sizeof ret, "{\"ret\":%d}", r);
    emit("AirPlayReceiverSessionTearDown", OID(session), OID(session), args, ret);
    return r;
}

/* ============= L2-media: DECODED screen + audio frames (post-decrypt) =====
 * RE of stock MHI2Q libairplay.so (unstripped VM copy, VAs cited below) showed
 * the "session-screen/audio delegate frame slot" model does NOT hold in this
 * build:
 *   - AirPlayReceiverSessionScreenSetDelegate (0x248dc) is a DEAD STUB: it only
 *     spills its two args to stack locals and returns; the delegate is never
 *     stored, so no screen-frame callback slot exists to swap.
 *   - AirPlayReceiverSessionSetDelegate (0x241358) DOES store a 12-word (0x30)
 *     delegate at session+0x18, but that is the CONTROL delegate (modes/UI/siri/
 *     nightmode) -- no audio/video FRAME slot. Audio frames flow through
 *     libairplay's own POSIX bridge (_AudioInput/OutputCallBack) into the
 *     exported ReadAudio/WriteAudio, never a dio delegate (PCM passthrough).
 *
 * The real post-decrypt DECODED-frame boundaries are three EXPORTED functions,
 * so we interpose them DIRECTLY (fully passive: call real, then log) -- no
 * struct-layout dependency, cleaner than a slot swap:
 *
 *   ScreenStreamProcessData(priv, uint8_t* data, size_t len, uint64_t pts,
 *                           CFLDictionary* opts, void(*done)(void*), void* ctx)
 *       @0x257d8  -> Forward(priv, 70, data, len).  data = decrypted H.264 AVCC
 *       (length-prefixed NALs). Signature = embedded __PRETTY_FUNCTION__ string.
 *   AirPlayReceiverSessionReadAudio (s, type, a3, a4, a5, a6, uint8_t* buf, size_t len)
 *       @0x2442e8  phone->HU decoded PCM; buf FILLED by the call (log AFTER).
 *   AirPlayReceiverSessionWriteAudio(s, type, a3, a4, a5, a6, uint8_t* src, size_t len)
 *       @0x244cc8  HU mic->phone PCM; src is input.
 *   type = audio stream type char 'd'/'e'/'f' (0x64/0x65/0x66 = main/alt/etc).
 *
 * Frames are big -> malloc the line like emit_io. PROBE_IO_CAP caps payload. */

/* generalized frame emit: extra = pre-built JSON fragment (no braces), buf/len
 * hex-dumped full (PROBE_IO_CAP-capped). Mirrors emit_io's malloc+hex path. */
static void emit_frame(const char *fn, const char *extra, const void *buf, long len) {
    static const char hx[] = "0123456789abcdef";
    const unsigned char *b = (const unsigned char *)buf;
    struct timespec tp; unsigned long long tns; int tid; unsigned long seq;
    long i, cap, pos, hdrlen; char head[256]; char *line;
    if (len < 0) len = 0;
    if (!b) len = 0;
    if (g_max > 0 && g_written >= g_max) return;   /* budget spent -> stop frame bloat */
    cap = (g_io_cap > 0 && len > g_io_cap) ? g_io_cap : len;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    tns = (unsigned long long)tp.tv_sec * 1000000000ull + (unsigned long long)tp.tv_nsec;
    tid = id_of((void *)(uintptr_t)pthread_self(), 't');
    line = (char *)malloc(320 + (size_t)cap * 2 + 8);
    if (!line) return;
    seq = g_seq_next();
    hdrlen = snprintf(head, sizeof head,
        "{\"seq\":%lu,\"tid\":\"t#%d\",\"fn\":\"%s\",%s,\"len\":%ld,\"cap\":%ld,\"ts\":%llu,\"data\":\"",
        seq, tid, fn, extra, len, cap, tns);
    memcpy(line, head, (size_t)hdrlen); pos = hdrlen;
    for (i = 0; i < cap; i++) { line[pos++] = hx[b[i] >> 4]; line[pos++] = hx[b[i] & 0xf]; }
    line[pos++] = '"'; line[pos++] = '}'; line[pos++] = '\n';
    enqueue(line, pos);
}

/* SCREEN: decoded H.264 AVCC frame IN. "nal" = first NAL unit type (data[4]&0x1f:
 * 5=IDR 1=non-IDR 7=SPS 8=PPS), the replay driver's frame-type hint; "pts" = the
 * 64-bit stream timestamp. Passive: tail-call real, then log. */
int ScreenStreamProcessData(void *priv, unsigned char *data, size_t len, uint64_t pts,
                            void *opts, void (*done)(void*), void *ctx) {
    static int (*real)(void*, unsigned char*, size_t, uint64_t, void*, void(*)(void*), void*) = 0;
    if (!real) real = (int(*)(void*,unsigned char*,size_t,uint64_t,void*,void(*)(void*),void*))
                        dlsym(RTLD_NEXT, "ScreenStreamProcessData");
    int r = real(priv, data, len, pts, opts, done, ctx);
    int nal = (data && len >= 5) ? (data[4] & 0x1f) : -1;
    char extra[112];
    snprintf(extra, sizeof extra,
        "\"stream\":\"o#%d\",\"nal\":%d,\"pts\":%llu,\"rc\":%d",
        OID(priv), nal, (unsigned long long)pts, r);
    emit_frame("cb.screenFrame", extra, data, (long)len);
    return r;
}

/* AUDIO interpose REMOVED. The Read/WriteAudio interposers used a GUESSED 8-arg
 * signature (a3..a6 unknown) on the exact audio path that fails the live session
 * (-6722 right after audio negotiation) -> overriding them with a wrong ABI mangled
 * real()'s args and broke audio setup. They also captured 0 frames in every run
 * (dio never consumes audio -> PCM passthrough to ALSA), so they were pure risk,
 * zero value. Deleted. If audio-frame capture is ever wanted, recover the REAL
 * signature first (idalib on libairplay) before re-interposing. */

/* ======================= L1: phone wire (reference) ===================== */
/* Capture ONLY fds tagged as sockets (accept/connect) and never the log fd ->
 * no file/log noise, no self-recursion. ponytail: QNX mesh is MsgSend not
 * sockets, so tagged sockets == phone/mDNS; filter by port if mDNS shows up. */
#define FD_MAX 1024
static char g_sock[FD_MAX];

static void tag_fd(int fd) {
    if (fd >= 0 && fd < FD_MAX) { pthread_mutex_lock(&g_lock); g_sock[fd] = 1; pthread_mutex_unlock(&g_lock); }
}
static int is_tagged(int fd) {
    return (fd >= 0 && fd < FD_MAX && fd != g_fd) ? g_sock[fd] : 0;
}
static void emit_io(const char *fn, int fd, const char *dir, const void *buf, long len) {
    static const char hx[] = "0123456789abcdef";
    const unsigned char *b = (const unsigned char *)buf;
    struct timespec tp; unsigned long long tns; int tid; unsigned long seq;
    long i, cap, pos, hdrlen; char head[192]; char *line;
    if (!g_wire) return;                           /* wire capture off (default) */
    if (len < 0) len = 0;
    if (g_max > 0 && g_written >= g_max) return;   /* budget spent -> stop wire bloat */
    cap = (g_io_cap > 0 && len > g_io_cap) ? g_io_cap : len;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    tns = (unsigned long long)tp.tv_sec * 1000000000ull + (unsigned long long)tp.tv_nsec;
    tid = id_of((void *)(uintptr_t)pthread_self(), 't');
    line = (char *)malloc(256 + (size_t)cap * 2 + 8);
    if (!line) return;
    seq = g_seq_next();
    hdrlen = snprintf(head, sizeof head,
        "{\"seq\":%lu,\"tid\":\"t#%d\",\"fn\":\"%s\",\"fd\":%d,\"dir\":\"%s\",\"len\":%ld,\"cap\":%ld,\"data\":\"",
        seq, tid, fn, fd, dir, len, cap);
    memcpy(line, head, (size_t)hdrlen); pos = hdrlen;
    for (i = 0; i < cap; i++) { line[pos++] = hx[b[i] >> 4]; line[pos++] = hx[b[i] & 0xf]; }
    line[pos++] = '"'; line[pos++] = '}'; line[pos++] = '\n';
    enqueue(line, pos);
}

int accept(int fd, void *addr, void *addrlen) {
    static int (*real)(int, void *, void *) = 0;
    if (!real) real = (int(*)(int, void *, void *))dlsym(RTLD_NEXT, "accept");
    int r = real(fd, addr, addrlen);
    if (r >= 0) { tag_fd(r); emit_io("accept", r, "conn", "", 0); }
    return r;
}
int connect(int fd, const void *addr, unsigned addrlen) {
    static int (*real)(int, const void *, unsigned) = 0;
    if (!real) real = (int(*)(int, const void *, unsigned))dlsym(RTLD_NEXT, "connect");
    int r = real(fd, addr, addrlen);
    if (r == 0) { tag_fd(fd); emit_io("connect", fd, "conn", "", 0); }
    return r;
}
ssize_t recv(int fd, void *buf, size_t n, int flags) {
    static ssize_t (*real)(int, void *, size_t, int) = 0;
    if (!real) real = (ssize_t(*)(int, void *, size_t, int))dlsym(RTLD_NEXT, "recv");
    ssize_t r = real(fd, buf, n, flags);
    if (r > 0 && is_tagged(fd)) emit_io("recv", fd, "rx", buf, (long)r);
    return r;
}
ssize_t send(int fd, const void *buf, size_t n, int flags) {
    static ssize_t (*real)(int, const void *, size_t, int) = 0;
    if (!real) real = (ssize_t(*)(int, const void *, size_t, int))dlsym(RTLD_NEXT, "send");
    ssize_t r = real(fd, buf, n, flags);
    if (r > 0 && is_tagged(fd)) emit_io("send", fd, "tx", buf, (long)r);
    return r;
}
ssize_t recvfrom(int fd, void *buf, size_t n, int flags, void *addr, void *addrlen) {
    static ssize_t (*real)(int, void *, size_t, int, void *, void *) = 0;
    if (!real) real = (ssize_t(*)(int, void *, size_t, int, void *, void *))dlsym(RTLD_NEXT, "recvfrom");
    ssize_t r = real(fd, buf, n, flags, addr, addrlen);
    if (r > 0 && is_tagged(fd)) emit_io("recvfrom", fd, "rx", buf, (long)r);
    return r;
}
ssize_t sendto(int fd, const void *buf, size_t n, int flags, const void *addr, unsigned addrlen) {
    static ssize_t (*real)(int, const void *, size_t, int, const void *, unsigned) = 0;
    if (!real) real = (ssize_t(*)(int, const void *, size_t, int, const void *, unsigned))dlsym(RTLD_NEXT, "sendto");
    ssize_t r = real(fd, buf, n, flags, addr, addrlen);
    if (r > 0 && is_tagged(fd)) emit_io("sendto", fd, "tx", buf, (long)r);
    return r;
}
ssize_t read(int fd, void *buf, size_t n) {
    static ssize_t (*real)(int, void *, size_t) = 0;
    if (!real) real = (ssize_t(*)(int, void *, size_t))dlsym(RTLD_NEXT, "read");
    ssize_t r = real(fd, buf, n);
    if (r > 0 && is_tagged(fd)) emit_io("read", fd, "rx", buf, (long)r);
    return r;
}
ssize_t write(int fd, const void *buf, size_t n) {
    static ssize_t (*real)(int, const void *, size_t) = 0;
    if (!real) real = (ssize_t(*)(int, const void *, size_t))dlsym(RTLD_NEXT, "write");
    ssize_t r = real(fd, buf, n);
    if (r > 0 && is_tagged(fd)) emit_io("write", fd, "tx", buf, (long)r);
    return r;
}

/* ============================== init ==================================== */
__attribute__((constructor))
static void trace_init(void) {
    const char *p = getenv("PROBE_LOG");
    const char *c = getenv("PROBE_IO_CAP");
    char hdr[256], autopath[64]; int n, fd;
    /* PROBE_LOG unset -> fresh per-run file /tmp/carplay_<pid>.jsonl (two runs
     * never collide). Explicit PROBE_LOG uses that path verbatim (append). */
    if (!(p && *p)) { snprintf(autopath, sizeof autopath, "/tmp/carplay_%d.jsonl", (int)getpid()); p = autopath; }
    fd = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) g_fd = fd;
    { char note[96]; int nn = snprintf(note, sizeof note, "carplay_trace -> %s\n", p); (void)write(2, note, (size_t)nn); }
    if (c && *c) g_io_cap = strtol(c, 0, 10);
    { const char *m = getenv("PROBE_MAX"); if (m && *m) g_max = strtol(m, 0, 10); }
    { const char *w = getenv("PROBE_WIRE"); if (w && *w) g_wire = atoi(w); }
    n = snprintf(hdr, sizeof hdr,
        "{\"_trace\":\"carplay_trace\",\"fmt\":\"jsonl-v1\",\"boundaries\":[\"L2:cb.*\",\"L1:wire\"],"
        "\"note\":\"o#0=NULL; ids first-seen; cb.* = replay driver input; wire = reference (ciphertext)\"}\n");
    (void)write(g_fd, hdr, (size_t)n);
    /* async writer: keeps every write() off CarPlay's real-time threads. */
    if (pthread_create(&g_writer, 0, writer_thread, 0) == 0) g_writer_ok = 1;
    atexit(writer_flush);   /* drain the teardown tail on normal exit */
}
