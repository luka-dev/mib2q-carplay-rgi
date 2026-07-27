/*
 * libaltscreen_hook.c — injected into dio_manager on MHI3
 *
 * ── What it does ──────────────────────────────────────────────────────────
 *
 * 1. AirPlayReceiverSessionHasFeatureAltScreen  [libairplay.so, inline patch]
 *    Force-returns 1 so onEvent_sessionCreated stores altScreen=true.
 *    This propagates into createMainDisplaysDictionary → advertises AltScreen
 *    capability to iPhone → iPhone negotiates AltScreen stream.
 *
 * 2. AirPlayReceiverSessionSetDelegate  [libairplay.so, inline patch + trampoline]
 *    Captures every AirPlay session pointer for the command socket.
 *
 * 3. gfx::Decoder::feed  [RTLD_NEXT interposition]
 *    Intercepts H.264 NAL units for AltScreen decoder, forwards over TCP :19820.
 *
 * 4. dint_create_displayable_media / dint_destroy_displayable  [RTLD_NEXT]
 *    Injects a fake 96-byte stub when the real call fails (no AltScreen layer
 *    in mcp::CLayerConfig), preventing "smartphone interface deactivated".
 *
 * 5. pxInit / pxFeed  [RTLD_NEXT]
 *    No-ops when called with our fake displayable stub.
 *
 * 6. TCP command socket  :19821
 *    Runtime inspection: status / decoders / sessions / altscreen on|off
 *
 * ── AltScreen decoder identification ─────────────────────────────────────
 * Track unique 'this' pointers seen in feed():
 *   index 0  → main screen decoder (always first)
 *   index 1+ → AltScreen decoder(s)
 * Forward only for index >= ALTSCREEN_DECODER_INDEX.
 *
 * Build (aarch64-linux-gnu):
 *   aarch64-linux-gnu-gcc -shared -fPIC -O2 -std=c11 -Wall \
 *       -o libaltscreen_hook.so libaltscreen_hook.c \
 *       -ldl -lpthread -Wl,-soname,libaltscreen_hook.so
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>

/* ── tunables ──────────────────────────────────────────────────────────── */
#define FORWARD_HOST            "127.0.0.1"
#define FORWARD_PORT            19820
#define CMD_PORT                19821
#define LOG_HOST                "172.16.250.123"   /* Mac IP on asix/USB     */
#define LOG_PORT                8766               /* Mac log receiver port  */
#define ALTSCREEN_STREAM_INDEX  1          /* 0=main, 1+=AltScreen        */
#define MAX_STREAMS             4
#define MAX_FAKE_DISP           4
#define MAX_SESSIONS            4
#define FAKE_DISP_SIZE          96
#define ALTSCREEN_DISPLAY_NAME  "Audi AltScreen"   /* advertised to iPhone */
#define ALTSCREEN_DISPLAY_ID    "AltScreen"        /* extra identifier     */
#define ALTSCREEN_PIX_W         960        /* AltScreen pixel width        */
#define ALTSCREEN_PIX_H         440        /* AltScreen pixel height       */
/* ──────────────────────────────────────────────────────────────────────── */


/* ══════════════════════════════════════════════════════════════════════════
 * § 1  Inline function patcher (AArch64, /proc/self/mem)
 *
 * Trampoline layout written at target (16 bytes):
 *   LDR x17, #8   ; load 64-bit hook address from +8
 *   BR  x17       ; jump there
 *   <8 bytes: hook address>
 *
 * make_trampoline() additionally builds a 32-byte executable stub so the
 * hook can call the original:  [saved 16 bytes] + [LDR x17,#8 / BR / addr]
 * Returns NULL if the first 16 bytes contain PC-relative instructions.
 * ══════════════════════════════════════════════════════════════════════════ */

static int selfmem_write(void *target, const void *src, size_t len)
{
    /* Try mprotect first: make page temporarily RWX, memcpy, restore RX.
     * This works even as an unprivileged UID inside the process.
     * /proc/self/mem O_RDWR writes to r-xp pages fail as non-root on this kernel. */
    long pgsz = sysconf(_SC_PAGESIZE);
    uintptr_t page = (uintptr_t)target & ~(uintptr_t)(pgsz - 1);
    size_t region = (((uintptr_t)target + len - page) + pgsz - 1) & ~(size_t)(pgsz - 1);

    if (mprotect((void *)page, region, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy(target, src, len);
        mprotect((void *)page, region, PROT_READ | PROT_EXEC);
        __builtin___clear_cache(target, (char *)target + len);
        return 0;
    }

    /* Fallback: /proc/self/mem (works when running as root) */
    int fd = open("/proc/self/mem", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "[altscreen_hook] selfmem_write: mprotect errno=%d, "
                "/proc/self/mem open errno=%d\n", errno, errno);
        return -1;
    }
    lseek(fd, (off_t)(uintptr_t)target, SEEK_SET);
    ssize_t n = write(fd, src, len);
    close(fd);
    __builtin___clear_cache(target, (char *)target + len);
    return (n == (ssize_t)len) ? 0 : -1;
}

static int selfmem_patch(void *target, void *hook)
{
    uint8_t patch[16];
    uint32_t insns[2] = { 0x58000051, 0xD61F0220 }; /* LDR x17,#8 / BR x17 */
    memcpy(patch, insns, 8);
    uint64_t addr = (uint64_t)(uintptr_t)hook;
    memcpy(patch + 8, &addr, 8);
    return selfmem_write(target, patch, 16);
}

/* Returns executable trampoline to call original, or NULL on failure. */
static void *make_trampoline(void *target)
{
    /* Reject PC-relative instructions in the first 4 words */
    uint32_t *w = (uint32_t *)target;
    for (int i = 0; i < 4; i++) {
        uint32_t op = w[i];
        const char *kind = NULL;
        if ((op & 0x9F000000) == 0x90000000) kind = "ADRP";
        else if ((op & 0x9F000000) == 0x10000000) kind = "ADR";
        else if ((op & 0xFC000000) == 0x14000000) kind = "B/BL";
        else if ((op & 0xFF000010) == 0x54000000) kind = "B.cond";
        else if ((op & 0x7E000000) == 0x34000000) kind = "CBZ/CBNZ";
        else if ((op & 0x7E000000) == 0x36000000) kind = "TBZ/TBNZ";
        if (kind) {
            fprintf(stderr,
                "[altscreen_hook] make_trampoline reject: target=%p word[%d]=0x%08x (%s) "
                "bytes=%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                target, i, op, kind,
                ((uint8_t *)target)[0],  ((uint8_t *)target)[1],  ((uint8_t *)target)[2],  ((uint8_t *)target)[3],
                ((uint8_t *)target)[4],  ((uint8_t *)target)[5],  ((uint8_t *)target)[6],  ((uint8_t *)target)[7],
                ((uint8_t *)target)[8],  ((uint8_t *)target)[9],  ((uint8_t *)target)[10], ((uint8_t *)target)[11],
                ((uint8_t *)target)[12], ((uint8_t *)target)[13], ((uint8_t *)target)[14], ((uint8_t *)target)[15]);
            return NULL;
        }
    }

    uint8_t *buf = mmap(NULL, 32,
                        PROT_READ | PROT_WRITE | PROT_EXEC,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) return NULL;

    /* [0..15]  saved original first 16 bytes */
    memcpy(buf, target, 16);

    /* [16..31]  jump back to target+16 */
    uint32_t jmp[4] = { 0x58000051, 0xD61F0220, 0, 0 };
    uint64_t ret = (uint64_t)(uintptr_t)target + 16;
    memcpy(&jmp[2], &ret, 8);
    memcpy(buf + 16, jmp, 16);

    mprotect(buf, 32, PROT_READ | PROT_EXEC);
    return buf;
}


/* ══════════════════════════════════════════════════════════════════════════
 * § 2  AirPlayReceiverSessionHasFeatureAltScreen — force true
 *
 * Called from onEvent_sessionCreated (0x2bee24 in this build).
 * Its return value is stored in CDIOManager+0x202 and propagates as the
 * first bool into createMainDisplaysDictionary, which sets the AltScreen
 * capability bit in the session dictionary sent to the iPhone.
 * ══════════════════════════════════════════════════════════════════════════ */

static int altscreen_patch_ok = 0;

static int hook_HasFeatureAltScreen(void *session)
{
    (void)session;
    return 1;
}


/* ══════════════════════════════════════════════════════════════════════════
 * § 3  AirPlayReceiverSessionSetDelegate — session tracking
 * ══════════════════════════════════════════════════════════════════════════ */

static void   *session_table[MAX_SESSIONS];
static int     session_count = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef void (*real_setdelegate_t)(void *session, void *delegate);
static real_setdelegate_t trampoline_setdelegate = NULL;

static void hook_SessionSetDelegate(void *session, void *delegate)
{
    pthread_mutex_lock(&session_mutex);
    int found = 0;
    for (int i = 0; i < session_count; i++)
        if (session_table[i] == session) { found = 1; break; }
    if (!found && session_count < MAX_SESSIONS) {
        session_table[session_count++] = session;
        fprintf(stderr, "[altscreen_hook] new AirPlay session %p (total %d)\n",
                session, session_count);
    }
    pthread_mutex_unlock(&session_mutex);

    if (trampoline_setdelegate)
        trampoline_setdelegate(session, delegate);
}


/* ══════════════════════════════════════════════════════════════════════════
 * § 4  addMainDisplay hook → inject addAltDisplay call
 *
 * dio::display::addMainDisplay is called once per session from getDisplays()
 * with the resolved PixelResolution / PhysicalResolution / Fps enums and the
 * capabilities mask.  We intercept it, call the original, then call
 * dio::display::addAltDisplay with the same resolution/fps enums so the
 * iPhone receives a proper AltScreen screen entry alongside the main one.
 *
 * Both symbols are exported (T) from dio_manager, so dlsym(RTLD_DEFAULT)
 * finds them without any ELF patching.
 *
 * GCC SSO std::string layout (len <= 15 stored inline):
 *   struct { char buf[16]; size_t len; size_t capacity; }
 *   capacity == 15 signals SSO mode.
 * ══════════════════════════════════════════════════════════════════════════ */

/* forward-declared here; defined in § 6 (after feedlog ring buffer) */
static void feedlog_append(const char *fmt, ...) __attribute__((format(printf,1,2)));

/* Mangled names from nm -D dio_manager */
#define SYM_addMainDisplay \
    "_ZN3dio7display14addMainDisplayERP13CFLDictionaryRKNSt7__cxx11" \
    "12basic_stringIcSt11char_traitsIcESaIcEEENS_15PixelResolutionE" \
    "SC_NS_3FpsENS_18PhysicalResolutionESE_jNS0_21EDisplayPrimaryDeviceE"
#define SYM_addAltDisplay \
    "_ZN3dio7display13addAltDisplayERP13CFLDictionaryRKNSt7__cxx11" \
    "12basic_stringIcSt11char_traitsIcESaIcEEENS_15PixelResolutionE" \
    "SC_NS_3FpsENS_18PhysicalResolutionESE_SB_"

/* GCC libstdc++ std::__cxx11::basic_string layout (32 bytes total):
 *   offset 0:  char *_M_p          → points to _M_local_buf for SSO (len≤15)
 *   offset 8:  size_t _M_length
 *   offset 16: char _M_local_buf[16]   (SSO inline storage)
 *
 * convert2CFString does CFStringCreateWithCString(0, *param_1, ...) which
 * reads the FIRST 8 bytes as a char * — i.e. _M_p.  For SSO strings _M_p
 * must point to _M_local_buf (not inline chars at offset 0). */
typedef struct {
    char   *_M_p;           /* offset  0: always valid char * */
    size_t  _M_length;      /* offset  8 */
    char    _M_local_buf[16]; /* offset 16: SSO inline storage */
} cxx11_string_t;

static void cxx11_string_init(cxx11_string_t *s, const char *str)
{
    size_t n = strlen(str);
    if (n > 15) n = 15;
    s->_M_p = s->_M_local_buf;          /* SSO: point into own struct */
    s->_M_length = n;
    memcpy(s->_M_local_buf, str, n);
    s->_M_local_buf[n] = '\0';
}

/* Signatures — enums are passed as int-sized values (w registers on AArch64) */
typedef int (*fn_addMainDisplay_t)(void **dict_ref,
                                   const void *name_str,
                                   int pix_w, int pix_h,
                                   int fps,
                                   int phys_w, int phys_h,
                                   unsigned int caps,
                                   int primary_device);
typedef int (*fn_addAltDisplay_t) (void **dict_ref,
                                   const cxx11_string_t *name_str,
                                   int pix_w, int pix_h,
                                   int fps,
                                   int phys_w, int phys_h,
                                   const cxx11_string_t *id_str);

static fn_addMainDisplay_t trampoline_addMainDisplay = NULL;
static int                 altdisplay_patch_ok       = 0;

/* Custom altscreen dims — -1 means "use same as main display" (default).
 * Set via cmd socket: "altdisplay <pix_w> <pix_h> <fps> <phys_w> <phys_h>" */
static int g_alt_pix_w  = -1;
static int g_alt_pix_h  = -1;
static int g_alt_fps    = -1;
static int g_alt_phys_w = -1;
static int g_alt_phys_h = -1;

/* ── CFL/Apple Foundation introspection ──────────────────────────────────
 *
 * dio_manager links against libairplay.so / Apple's CFLite-style runtime,
 * which provides standard CFCopyDescription / CFShow / CFStringGetCString.
 * We resolve them lazily via dlsym so we don't link-depend on them in our
 * .so build environment.  Used to dump display-dictionary contents and see
 * exactly what gets shipped to the iPhone. */
typedef void *      CFTypeRef;
typedef const void *CFStringRef;
typedef long        CFIndex;

typedef CFStringRef (*cf_copy_description_t)(CFTypeRef);
typedef int         (*cf_string_get_c_string_t)(CFStringRef, char *, CFIndex, unsigned);
typedef CFIndex     (*cf_string_get_length_t)(CFStringRef);
typedef void        (*cf_release_t)(CFTypeRef);
typedef long        (*cf_get_type_id_t)(CFTypeRef);
typedef void        (*cf_show_t)(CFTypeRef);
typedef CFIndex     (*cf_dict_get_count_t)(CFTypeRef);
typedef void        (*cf_dict_get_keys_values_t)(CFTypeRef, const void **, const void **);

#define CF_ENC_UTF8  0x08000100u

static void dump_cfobj(const char *label, CFTypeRef obj)
{
    if (!obj) {
        fprintf(stderr, "[altscreen_hook] %s: NULL\n", label);
        return;
    }
    static cf_copy_description_t   cf_copy_desc  = NULL;
    static cf_string_get_c_string_t cf_get_cstr  = NULL;
    static cf_release_t             cf_release   = NULL;
    static cf_show_t                cf_show      = NULL;
    static cf_dict_get_count_t      cf_dict_cnt  = NULL;
    static cf_dict_get_keys_values_t cf_dict_kv  = NULL;
    static cf_get_type_id_t         cf_type_id   = NULL;
    static int                      lookup_done  = 0;
    if (!lookup_done) {
        cf_copy_desc = (cf_copy_description_t)dlsym(RTLD_DEFAULT, "CFCopyDescription");
        if (!cf_copy_desc) cf_copy_desc = (cf_copy_description_t)dlsym(RTLD_DEFAULT, "CFLCopyDescription");
        cf_get_cstr  = (cf_string_get_c_string_t)dlsym(RTLD_DEFAULT, "CFStringGetCString");
        cf_release   = (cf_release_t)dlsym(RTLD_DEFAULT, "CFRelease");
        cf_show      = (cf_show_t)dlsym(RTLD_DEFAULT, "CFShow");
        cf_dict_cnt  = (cf_dict_get_count_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetCount");
        cf_dict_kv   = (cf_dict_get_keys_values_t)dlsym(RTLD_DEFAULT, "CFDictionaryGetKeysAndValues");
        cf_type_id   = (cf_get_type_id_t)dlsym(RTLD_DEFAULT, "CFGetTypeID");
        lookup_done = 1;
        fprintf(stderr, "[altscreen_hook] CF lookup: copy_desc=%p show=%p "
                "dict_count=%p dict_kv=%p type_id=%p get_cstr=%p\n",
                (void *)cf_copy_desc, (void *)cf_show,
                (void *)cf_dict_cnt, (void *)cf_dict_kv,
                (void *)cf_type_id, (void *)cf_get_cstr);
    }

    /* Path A: CFCopyDescription → CFStringGetCString (cleanest if available) */
    if (cf_copy_desc && cf_get_cstr) {
        CFStringRef desc = cf_copy_desc(obj);
        if (desc) {
            char buf[8192];
            if (cf_get_cstr(desc, buf, sizeof(buf), CF_ENC_UTF8)) {
                fprintf(stderr, "[altscreen_hook] %s ↓↓↓ [CFCopyDescription]\n%s\n"
                        "[altscreen_hook] %s ↑↑↑\n", label, buf, label);
            }
            if (cf_release) cf_release((CFTypeRef)desc);
            return;
        }
    }

    /* Path B: CFShow — prints to stderr, which we already capture */
    if (cf_show) {
        fprintf(stderr, "[altscreen_hook] %s ↓↓↓ [CFShow]\n", label);
        cf_show(obj);
        fprintf(stderr, "\n[altscreen_hook] %s ↑↑↑\n", label);
        return;
    }

    /* Path C: walk the dict manually if iteration primitives are present */
    if (cf_dict_cnt && cf_dict_kv) {
        CFIndex n = cf_dict_cnt(obj);
        fprintf(stderr, "[altscreen_hook] %s ↓↓↓ [walk, %ld keys]\n", label, (long)n);
        if (n > 0 && n < 256) {
            const void **keys   = (const void **)calloc((size_t)n, sizeof(void *));
            const void **values = (const void **)calloc((size_t)n, sizeof(void *));
            if (keys && values) {
                cf_dict_kv(obj, keys, values);
                for (CFIndex i = 0; i < n; i++) {
                    /* Try to print the key as a CFString — best-effort */
                    char kbuf[256] = "<?>";
                    if (cf_get_cstr) cf_get_cstr((CFStringRef)keys[i], kbuf, sizeof(kbuf), CF_ENC_UTF8);
                    long ktid = cf_type_id ? cf_type_id((CFTypeRef)keys[i])  : -1;
                    long vtid = cf_type_id ? cf_type_id((CFTypeRef)values[i]) : -1;
                    fprintf(stderr, "  [%ld] key='%s' (typeID=%ld) value=%p (typeID=%ld)\n",
                            (long)i, kbuf, ktid, values[i], vtid);
                }
            }
            free(keys); free(values);
        }
        fprintf(stderr, "[altscreen_hook] %s ↑↑↑\n", label);
        return;
    }

    fprintf(stderr, "[altscreen_hook] %s: no CF dump symbols found, ptr=%p\n", label, obj);
}

static int hook_addMainDisplay(void **dict_ref,
                               const void *name_str,
                               int pix_w, int pix_h,
                               int fps,
                               int phys_w, int phys_h,
                               unsigned int caps,
                               int primary_device)
{
    feedlog_append("main: pix=%dx%d fps=%d phys=%dx%d caps=0x%x",
                   pix_w, pix_h, fps, phys_w, phys_h, caps);
    int rc = 0;
    if (trampoline_addMainDisplay)
        rc = trampoline_addMainDisplay(dict_ref, name_str,
                                       pix_w, pix_h, fps,
                                       phys_w, phys_h,
                                       caps, primary_device);

    /* Inject AltScreen display entry alongside the main one.
     * addAltDisplay is NEVER called natively by dio_manager, so we must do it.
     * convert2CFString reads *param = _M_p, so we need a proper cxx11 string
     * where _M_p points to _M_local_buf — NOT raw inline bytes at offset 0. */
    fn_addAltDisplay_t fn_alt =
        (fn_addAltDisplay_t)dlsym(RTLD_DEFAULT, SYM_addAltDisplay);
    if (fn_alt && dict_ref) {
        int a_pw = (g_alt_pix_w  >= 0) ? g_alt_pix_w  : pix_w;
        int a_ph = (g_alt_pix_h  >= 0) ? g_alt_pix_h  : pix_h;
        int a_fps= (g_alt_fps    >= 0) ? g_alt_fps    : fps;
        int a_ww = (g_alt_phys_w >= 0) ? g_alt_phys_w : phys_w;
        int a_wh = (g_alt_phys_h >= 0) ? g_alt_phys_h : phys_h;
        cxx11_string_t disp_name, disp_id;
        cxx11_string_init(&disp_name, ALTSCREEN_DISPLAY_NAME);
        cxx11_string_init(&disp_id,   ALTSCREEN_DISPLAY_ID);
        feedlog_append("addAltDisplay pix=%dx%d fps=%d phys=%dx%d%s",
                       a_pw, a_ph, a_fps, a_ww, a_wh,
                       (g_alt_pix_w >= 0) ? " [custom]" : " [same as main]");
        fn_alt(dict_ref, &disp_name, a_pw, a_ph, a_fps, a_ww, a_wh, &disp_id);
        feedlog_append("addAltDisplay done");
    } else {
        feedlog_append("addAltDisplay skip fn_alt=%p dict=%p", fn_alt, dict_ref);
    }

    return rc;
}

/* ══════════════════════════════════════════════════════════════════════════
 * § 6  ScreenStreamProcessData — NAL forwarding
 *
 * ScreenStreamProcessData (libairplay.so) receives each encoded frame before
 * NAL extraction.  Its prolog has no PC-relative instructions in the first
 * four words, so make_trampoline succeeds and the original is called for all
 * streams (main + AltScreen) — main screen is unaffected.
 *
 * AltScreen identification: track unique stream context pointers seen in the
 * hook.  Index 0 = main screen (first caller), index 1+ = AltScreen.
 * Forward the raw buffer for index >= ALTSCREEN_STREAM_INDEX.
 * ══════════════════════════════════════════════════════════════════════════ */

/* In-memory log ring — written by hooks, read via "feedlog" cmd */
#define FEEDLOG_LINES  8
#define FEEDLOG_LINE   128
static char    feedlog_buf[FEEDLOG_LINES][FEEDLOG_LINE];
static int     feedlog_n   = 0;
static pthread_mutex_t feedlog_mutex = PTHREAD_MUTEX_INITIALIZER;

static void feedlog_append(const char *fmt, ...) __attribute__((format(printf,1,2)));
static void feedlog_append(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pthread_mutex_lock(&feedlog_mutex);
    int slot = feedlog_n % FEEDLOG_LINES;
    vsnprintf(feedlog_buf[slot], FEEDLOG_LINE, fmt, ap);
    feedlog_n++;
    pthread_mutex_unlock(&feedlog_mutex);
    va_end(ap);
}

#define SYM_stream_process_data "ScreenStreamProcessData"
#define SYM_set_avcc            "ScreenStreamSetAVCC"

/* x0=stream, x1=data, x2=len, x3=displayTime, x4=?, x5=avcc, x6=ctx */
typedef int (*real_ssp_t)(void *stream, const uint8_t *data,
                          size_t len, uint64_t ts,
                          void *unk4, void *avcc, void *ctx);

/* x0=stream, x1=avcc_config_ptr, x2=avcc_config_len */
typedef int (*real_setavcc_t)(void *stream, const uint8_t *cfg, size_t cfg_len);
static real_setavcc_t trampoline_setavcc = NULL;

/* ScreenStream CF-object field offsets (confirmed by disassembly of libairplay.so) */
#define SS_OFF_STREAM_TYPE  72   /* uint32: 110=main screen, 111=AltScreen          */
#define SS_OFF_RUNNING      76   /* uint8:  1 when ScreenStreamStart succeeded       */
#define SS_OFF_NAL_CB       96   /* void*:  per-NAL callback (NULL by default)       */
/* ScreenStreamSetAVCC stores H264ConvertAVCCtoAnnexBHeader output here:            */
#define SS_OFF_ANNEXB_PTR   16   /* void*:  malloc'd Annex B header (SPS+PPS bytes) */
#define SS_OFF_ANNEXB_LEN   24   /* size_t: bytes written to the Annex B buffer      */

static uint32_t ss_stream_type(void *ss)
{
    if (!ss) return 0;
    return *(uint32_t *)((char *)ss + SS_OFF_STREAM_TYPE);
}

static real_ssp_t      real_ssp      = NULL;
static int             ssp_patch_ok  = 0;

typedef struct { void *ptr; uint32_t type; } StreamEntry;
static StreamEntry     stream_table[MAX_STREAMS];
static int             stream_count  = 0;
static pthread_mutex_t stream_mutex  = PTHREAD_MUTEX_INITIALIZER;
static int             forward_fd    = -1;
static int             forward_enabled = 1;
static int             g_fwd_min_idx = 0;   /* 0=all streams, 111=AltScreen only */
static int             g_config_pending = 0; /* under fwd_mutex: resend SPS/PPS on next NAL */
static pthread_mutex_t fwd_mutex     = PTHREAD_MUTEX_INITIALIZER;

/* Send Annex B header cached in a ScreenStream object as AVCC-framed NALs.
 * Called directly with fd (caller holds fwd_mutex).                         */
static void send_stream_config_to_fd(int fd, void *ss)
{
    if (!ss || fd < 0) return;
    const uint8_t *ab  = *(const uint8_t **)((char *)ss + SS_OFF_ANNEXB_PTR);
    size_t         alen = *(size_t *)((char *)ss + SS_OFF_ANNEXB_LEN);
    if (!ab || alen == 0 || alen > 4096) return;

    feedlog_append("dumpconfig: ss=%p ab=%p alen=%zu b0=%02x", ss, ab, alen, ab[0]);

    /* Walk Annex B start codes (00 00 01 or 00 00 00 01), emit each NAL as
     * [4-byte outer frame len][4-byte AVCC NAL len][NAL bytes].             */
    const uint8_t *p = ab, *end = ab + alen;
    while (p < end) {
        /* Find start code */
        const uint8_t *sc = NULL;
        for (const uint8_t *q = p; q + 3 <= end; q++) {
            if (q[0] == 0 && q[1] == 0 &&
                (q[2] == 1 || (q + 4 <= end && q[2] == 0 && q[3] == 1))) {
                sc = q; break;
            }
        }
        if (!sc) break;
        p = sc + (sc[2] == 1 ? 3 : 4);
        if (p >= end) break;

        /* Find next start code to bound this NAL */
        const uint8_t *next_sc = NULL;
        for (const uint8_t *q = p + 1; q + 3 <= end; q++) {
            if (q[0] == 0 && q[1] == 0 &&
                (q[2] == 1 || (q + 4 <= end && q[2] == 0 && q[3] == 1))) {
                next_sc = q; break;
            }
        }
        size_t nal_len = next_sc ? (size_t)(next_sc - p) : (size_t)(end - p);
        if (nal_len > 0 && nal_len <= 2048) {
            uint8_t type = p[0] & 0x1F;
            feedlog_append("dumpconfig: nal type=%u len=%zu", type, nal_len);
            uint32_t inner = htonl((uint32_t)nal_len);
            uint32_t outer = htonl((uint32_t)(4 + nal_len));
            send(fd, &outer, 4, MSG_NOSIGNAL);
            send(fd, &inner, 4, MSG_NOSIGNAL);
            send(fd, p, nal_len, MSG_NOSIGNAL);
        }
        p = next_sc ? next_sc : end;
    }
}

/* Resend cached SPS/PPS from all tracked streams — called without fwd_mutex. */
static void resend_stream_configs(void)
{
    void *ptrs[MAX_STREAMS]; uint32_t types[MAX_STREAMS]; int n;

    pthread_mutex_lock(&stream_mutex);
    n = stream_count;
    for (int i = 0; i < n; i++) { ptrs[i] = stream_table[i].ptr; types[i] = stream_table[i].type; }
    pthread_mutex_unlock(&stream_mutex);

    pthread_mutex_lock(&fwd_mutex);
    int fd = forward_fd;
    if (fd >= 0) {
        for (int i = 0; i < n; i++) {
            if (ptrs[i] && (int)types[i] >= g_fwd_min_idx)
                send_stream_config_to_fd(fd, ptrs[i]);
        }
    }
    pthread_mutex_unlock(&fwd_mutex);
}

static void try_connect_locked(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return;
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(FORWARD_PORT);
    inet_pton(AF_INET, FORWARD_HOST, &sa.sin_addr);
    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0) {
        forward_fd = fd;
        g_config_pending = 1;   /* trigger SPS/PPS resend before first NAL */
        fprintf(stderr, "[altscreen_hook] connected to %s:%d\n",
                FORWARD_HOST, FORWARD_PORT);
    } else {
        close(fd);
    }
}

static void forward_nal(const uint8_t *data, uint32_t len)
{
    pthread_mutex_lock(&fwd_mutex);
    if (forward_fd < 0) try_connect_locked();
    /* On new connection, resend cached SPS/PPS before any NAL data.
     * Release fwd_mutex first to avoid deadlock with stream_mutex ordering. */
    if (g_config_pending && forward_fd >= 0) {
        g_config_pending = 0;
        pthread_mutex_unlock(&fwd_mutex);
        resend_stream_configs();      /* acquires stream_mutex then fwd_mutex */
        pthread_mutex_lock(&fwd_mutex);
    }
    if (forward_fd >= 0) {
        uint32_t be_len = htonl(len);
        ssize_t r1 = send(forward_fd, &be_len, 4, MSG_NOSIGNAL);
        ssize_t r2 = (r1 == 4) ? send(forward_fd, data, len, MSG_NOSIGNAL) : -1;
        static int fwd_log_n = 0;
        if (fwd_log_n < 5) {
            feedlog_append("fwd: fd=%d len=%u r1=%d r2=%d errno=%d",
                           forward_fd, len, (int)r1, (int)r2, errno);
            fwd_log_n++;
        }
        if (r1 != 4 || r2 != (ssize_t)len) {
            close(forward_fd);
            forward_fd = -1;
        }
    } else {
        static int no_fd_n = 0;
        if (no_fd_n < 3) {
            feedlog_append("fwd: no fd, len=%u", len);
            no_fd_n++;
        }
    }
    pthread_mutex_unlock(&fwd_mutex);
}

/* Track unique stream pointers; record type once while object is live */
static void track_stream(void *self)
{
    pthread_mutex_lock(&stream_mutex);
    for (int i = 0; i < stream_count; i++) {
        if (stream_table[i].ptr == self) {
            pthread_mutex_unlock(&stream_mutex);
            return;
        }
    }
    if (stream_count < MAX_STREAMS) {
        uint32_t stype = ss_stream_type(self);
        stream_table[stream_count].ptr  = self;
        stream_table[stream_count].type = stype;
        stream_count++;
        fprintf(stderr,
                "[altscreen_hook] new ScreenStream @ %p type=%u%s\n",
                self, stype,
                stype == 111 ? " ← AltScreen" : " (main)");
    }
    pthread_mutex_unlock(&stream_mutex);
}

/* ── ScreenStreamSetAVCC hook ───────────────────────────────────────────────
 * SPS/PPS are delivered out-of-band via ScreenStreamSetAVCC(stream, cfg, len)
 * where cfg is an AVCDecoderConfigurationRecord (AVCC extradata box).
 * They NEVER appear in ScreenStreamProcessData's data buffer.
 * We parse the config record and forward each SPS/PPS NAL wrapped in a
 * single-NAL AVCC frame ([4-byte nal_len][nal_data]) so altscreen_stream's
 * existing AVCC parser receives them before the first video frame.
 * ────────────────────────────────────────────────────────────────────────── */

/* Wrap one bare NAL in a 4-byte AVCC length prefix and send via forward_nal */
static void forward_avcc_nal(const uint8_t *nal, size_t nal_len)
{
    if (nal_len == 0 || nal_len > 2048) return;
    uint8_t buf[2052];
    uint32_t be = htonl((uint32_t)nal_len);
    memcpy(buf, &be, 4);
    memcpy(buf + 4, nal, nal_len);
    forward_nal(buf, (uint32_t)(4 + nal_len));
}

/* Parse AVCDecoderConfigurationRecord, forward SPS+PPS as AVCC-wrapped NALs */
static void forward_avcc_config(const uint8_t *cfg, size_t clen)
{
    if (!cfg || clen < 7) return;
    /* byte[5]: 0b111xxxxx | numSPS */
    int num_sps = cfg[5] & 0x1f;
    size_t off = 6;
    for (int i = 0; i < num_sps && off + 2 <= clen; i++) {
        uint16_t slen = ((uint16_t)cfg[off] << 8) | cfg[off + 1];
        off += 2;
        if (slen == 0 || off + slen > clen) break;
        feedlog_append("setavcc: SPS len=%u byte0=0x%02x", slen, cfg[off]);
        forward_avcc_nal(cfg + off, slen);
        off += slen;
    }
    if (off >= clen) return;
    int num_pps = cfg[off++] & 0xff;
    for (int i = 0; i < num_pps && off + 2 <= clen; i++) {
        uint16_t plen = ((uint16_t)cfg[off] << 8) | cfg[off + 1];
        off += 2;
        if (plen == 0 || off + plen > clen) break;
        feedlog_append("setavcc: PPS len=%u byte0=0x%02x", plen, cfg[off]);
        forward_avcc_nal(cfg + off, plen);
        off += plen;
    }
}

static int hook_SetAVCC(void *stream, const uint8_t *cfg, size_t cfg_len)
{
    if (cfg && cfg_len > 0 && forward_enabled) {
        uint32_t stype = ss_stream_type(stream);
        if ((int)stype >= g_fwd_min_idx)
            forward_avcc_config(cfg, cfg_len);
        feedlog_append("setavcc: stream=%p stype=%u cfg_len=%zu",
                       stream, stype, cfg_len);
    }
    return trampoline_setavcc ? trampoline_setavcc(stream, cfg, cfg_len) : 0;
}

static int hook_ssp(void *stream, const uint8_t *data,
                    size_t len, uint64_t ts,
                    void *unk4, void *avcc, void *ctx)
{
    track_stream(stream);

    /* Use stream_type field at SS_OFF_STREAM_TYPE (+72) — directly set by
     * ScreenStreamStart — instead of fragile first-seen index tracking.
     * g_fwd_min_idx=0 → all streams, =111 → AltScreen only (default). */
    uint32_t stype = ss_stream_type(stream);
    if ((int)stype >= g_fwd_min_idx && data && len > 0 && forward_enabled) {
        static int log_n = 0;
        if (log_n < 3) {
            feedlog_append("ssp type=%u stream=%p len=%zu ts=%llu",
                           stype, stream, len, (unsigned long long)ts);
            if (len >= 4)
                feedlog_append("ssp bytes: %02x %02x %02x %02x",
                               data[0], data[1], data[2], data[3]);
            log_n++;
        }
        forward_nal(data, (uint32_t)len);
    }

    if (real_ssp)
        return real_ssp(stream, data, len, ts, unk4, avcc, ctx);
    return 0;
}


/* ══════════════════════════════════════════════════════════════════════════
 * § 7  Fake displayable
 * ══════════════════════════════════════════════════════════════════════════ */

typedef int  (*real_dint_create_t)(int, const char *, int, int,
                                   void **, const char *, void *);
typedef void (*real_dint_destroy_t)(void *);
typedef int  (*real_px_t)(void *, ...);

static real_dint_create_t  real_dint_create  = NULL;
static real_dint_destroy_t real_dint_destroy = NULL;
static real_px_t           real_px_init      = NULL;
static real_px_t           real_px_feed      = NULL;
static void               *fake_disp[MAX_FAKE_DISP];
static int                 fake_disp_count   = 0;
static pthread_mutex_t     fake_mutex        = PTHREAD_MUTEX_INITIALIZER;

static int is_fake(void *ptr)
{
    for (int i = 0; i < fake_disp_count; i++)
        if (fake_disp[i] == ptr) return 1;
    return 0;
}

int dint_create_displayable_media(int mode, const char *name,
                                  int width, int height, void **out,
                                  const char *extra_str, void *ctx)
{
    if (!real_dint_create)
        real_dint_create = (real_dint_create_t)dlsym(RTLD_NEXT,
                               "dint_create_displayable_media");
    int result = 0;
    if (real_dint_create)
        result = real_dint_create(mode, name, width, height,
                                  out, extra_str, ctx);
    if (result == 0 && out) {
        void *stub = calloc(1, FAKE_DISP_SIZE);
        if (stub) {
            *out = stub;
            pthread_mutex_lock(&fake_mutex);
            if (fake_disp_count < MAX_FAKE_DISP)
                fake_disp[fake_disp_count++] = stub;
            pthread_mutex_unlock(&fake_mutex);
            fprintf(stderr,
                    "[altscreen_hook] dint_create failed for \"%s\""
                    " — injected fake displayable %p\n",
                    name ? name : "(null)", stub);
            result = 1;
        }
    }
    return result;
}

void dint_destroy_displayable(void *displayable)
{
    pthread_mutex_lock(&fake_mutex);
    int fake = is_fake(displayable);
    if (fake) {
        for (int i = 0; i < fake_disp_count; i++) {
            if (fake_disp[i] == displayable) {
                fake_disp[i] = fake_disp[--fake_disp_count];
                break;
            }
        }
    }
    pthread_mutex_unlock(&fake_mutex);
    if (fake) { free(displayable); return; }
    if (!real_dint_destroy)
        real_dint_destroy = (real_dint_destroy_t)dlsym(RTLD_NEXT,
                                "dint_destroy_displayable");
    if (real_dint_destroy)
        real_dint_destroy(displayable);
}

int pxInit(void *arg1, void *arg2, int arg3)
{
    pthread_mutex_lock(&fake_mutex);
    int fake = is_fake(arg1);
    pthread_mutex_unlock(&fake_mutex);
    if (fake) return 0;
    if (!real_px_init)
        real_px_init = (real_px_t)dlsym(RTLD_NEXT, "pxInit");
    return real_px_init ? real_px_init(arg1, arg2, arg3) : 0;
}

int pxFeed(void *arg1, void *arg2)
{
    pthread_mutex_lock(&fake_mutex);
    int fake = is_fake(arg1);
    pthread_mutex_unlock(&fake_mutex);
    if (fake) return 0;
    if (!real_px_feed)
        real_px_feed = (real_px_t)dlsym(RTLD_NEXT, "pxFeed");
    return real_px_feed ? real_px_feed(arg1, arg2) : 0;
}


/* ══════════════════════════════════════════════════════════════════════════
 * § 8  TCP command socket  :19821
 *
 * Commands (newline-terminated):
 *   status         — pid, patch state, decoder/session/fwd counts
 *   decoders       — list gfx::Decoder instances with indices
 *   sessions       — list captured AirPlay session pointers
 *   altscreen on   — enable NAL forwarding
 *   altscreen off  — disable NAL forwarding
 * ══════════════════════════════════════════════════════════════════════════ */

static void cmd_handle(int cfd)
{
    char line[256];
    int  pos = 0;
    char ch;
    while (recv(cfd, &ch, 1, 0) == 1) {
        if (ch == '\n' || ch == '\r') {
            line[pos] = '\0';
            pos = 0;
            if (!line[0]) continue;

            char resp[2048];
            int  n = 0;

            if (strcmp(line, "status") == 0) {
                pthread_mutex_lock(&stream_mutex);
                int nst = stream_count;
                pthread_mutex_unlock(&stream_mutex);
                pthread_mutex_lock(&session_mutex);
                int sc = session_count;
                pthread_mutex_unlock(&session_mutex);
                pthread_mutex_lock(&fwd_mutex);
                int ffd = forward_fd;
                pthread_mutex_unlock(&fwd_mutex);
                pthread_mutex_lock(&fake_mutex);
                int fc = fake_disp_count;
                pthread_mutex_unlock(&fake_mutex);
                n = snprintf(resp, sizeof(resp),
                    "pid=%d altscreen_patch=%s altdisplay_patch=%s ssp_patch=%s"
                    " forward_enabled=%d fwd_min_idx=%d streams=%d sessions=%d"
                    " forward_fd=%d fake_disps=%d\n",
                    (int)getpid(),
                    altscreen_patch_ok  ? "OK" : "FAIL",
                    altdisplay_patch_ok ? "OK" : "FAIL",
                    ssp_patch_ok        ? "OK" : "FAIL",
                    forward_enabled, g_fwd_min_idx, nst, sc, ffd, fc);

            } else if (strcmp(line, "streams") == 0) {
                pthread_mutex_lock(&stream_mutex);
                n += snprintf(resp+n, sizeof(resp)-n,
                              "streams: %d  fwd_min_type=%d\n",
                              stream_count, g_fwd_min_idx);
                for (int i = 0; i < stream_count; i++) {
                    n += snprintf(resp+n, sizeof(resp)-n,
                        "  [%d] %p type=%u%s\n", i, stream_table[i].ptr,
                        stream_table[i].type,
                        stream_table[i].type == 111 ? " ← AltScreen" : " (main)");
                }
                pthread_mutex_unlock(&stream_mutex);

            } else if (strcmp(line, "sessions") == 0) {
                pthread_mutex_lock(&session_mutex);
                n += snprintf(resp+n, sizeof(resp)-n,
                              "sessions: %d\n", session_count);
                for (int i = 0; i < session_count; i++)
                    n += snprintf(resp+n, sizeof(resp)-n,
                        "  [%d] %p\n", i, session_table[i]);
                pthread_mutex_unlock(&session_mutex);

            } else if (strcmp(line, "altscreen on") == 0) {
                forward_enabled = 1;
                n = snprintf(resp, sizeof(resp), "altscreen forwarding ON\n");
            } else if (strcmp(line, "altscreen off") == 0) {
                forward_enabled = 0;
                n = snprintf(resp, sizeof(resp), "altscreen forwarding OFF\n");
            } else if (strncmp(line, "stream ", 7) == 0) {
                /* Accept a stream_type threshold: 0=all, 110=main+alt, 111=AltScreen only */
                int t = atoi(line + 7);
                if (t < 0) {
                    n = snprintf(resp, sizeof(resp), "stream: value must be >= 0\n");
                } else {
                    g_fwd_min_idx = t;
                    n = snprintf(resp, sizeof(resp),
                        "forwarding stream_type >= %d  (0=all 110=main+alt 111=alt only)\n",
                        g_fwd_min_idx);
                }
            } else if (strcmp(line, "dumpconfig") == 0) {
                /* Read cached Annex B (SPS+PPS) from each stream object and
                 * re-send to altscreen_stream — useful when hook loaded after
                 * ScreenStreamSetAVCC was already called.                   */
                void *ptrs[MAX_STREAMS]; uint32_t types[MAX_STREAMS]; int nst;
                pthread_mutex_lock(&stream_mutex);
                nst = stream_count;
                for (int i = 0; i < nst; i++) {
                    ptrs[i] = stream_table[i].ptr;
                    types[i] = stream_table[i].type;
                }
                pthread_mutex_unlock(&stream_mutex);

                pthread_mutex_lock(&fwd_mutex);
                int ffd = forward_fd;
                if (ffd >= 0) {
                    for (int i = 0; i < nst; i++) {
                        if (ptrs[i] && (int)types[i] >= g_fwd_min_idx)
                            send_stream_config_to_fd(ffd, ptrs[i]);
                    }
                }
                pthread_mutex_unlock(&fwd_mutex);
                n = snprintf(resp, sizeof(resp),
                             "dumpconfig: sent %d stream(s) fwd_fd=%d\n", nst, ffd);

            } else if (strcmp(line, "feedlog") == 0) {
                pthread_mutex_lock(&feedlog_mutex);
                int total = feedlog_n;
                int start = (total > FEEDLOG_LINES) ? (total % FEEDLOG_LINES) : 0;
                int count = (total > FEEDLOG_LINES) ? FEEDLOG_LINES : total;
                n += snprintf(resp+n, sizeof(resp)-n, "feedlog: %d lines\n", total);
                for (int i = 0; i < count && n < (int)sizeof(resp) - 2; i++) {
                    int slot = (start + i) % FEEDLOG_LINES;
                    n += snprintf(resp+n, sizeof(resp)-n, "  %s\n", feedlog_buf[slot]);
                }
                pthread_mutex_unlock(&feedlog_mutex);
            } else if (strcmp(line, "prolog") == 0) {
                void *fn = dlsym(RTLD_DEFAULT, SYM_stream_process_data);
                if (fn) {
                    uint32_t *w = (uint32_t *)fn;
                    n = snprintf(resp, sizeof(resp),
                        "ScreenStreamProcessData @ %p prolog: %08x %08x %08x %08x\n",
                        fn, w[0], w[1], w[2], w[3]);
                } else {
                    n = snprintf(resp, sizeof(resp),
                        "ScreenStreamProcessData: dlsym failed\n");
                }

            } else if (strncmp(line, "altdisplay", 10) == 0) {
                /* altdisplay                        — show current config
                 * altdisplay same                   — reset to main-display passthrough
                 * altdisplay <pw> <ph> <fps> <ww> <wh>  — set custom enum values */
                const char *arg = line + 10;
                while (*arg == ' ') arg++;
                if (*arg == '\0') {
                    /* show current */
                    if (g_alt_pix_w < 0)
                        n = snprintf(resp, sizeof(resp),
                            "altdisplay: same as main display (passthrough)\n");
                    else
                        n = snprintf(resp, sizeof(resp),
                            "altdisplay: pix_w=%d pix_h=%d fps=%d phys_w=%d phys_h=%d\n",
                            g_alt_pix_w, g_alt_pix_h, g_alt_fps,
                            g_alt_phys_w, g_alt_phys_h);
                } else if (strcmp(arg, "same") == 0) {
                    g_alt_pix_w = g_alt_pix_h = g_alt_fps =
                    g_alt_phys_w = g_alt_phys_h = -1;
                    n = snprintf(resp, sizeof(resp),
                        "altdisplay: reset to same as main display\n");
                } else {
                    int pw, ph, fr, ww, wh;
                    if (sscanf(arg, "%d %d %d %d %d", &pw, &ph, &fr, &ww, &wh) == 5) {
                        g_alt_pix_w  = pw;
                        g_alt_pix_h  = ph;
                        g_alt_fps    = fr;
                        g_alt_phys_w = ww;
                        g_alt_phys_h = wh;
                        n = snprintf(resp, sizeof(resp),
                            "altdisplay: set pix=%dx%d fps=%d phys=%dx%d"
                            " (takes effect on next CarPlay session)\n",
                            pw, ph, fr, ww, wh);
                    } else {
                        n = snprintf(resp, sizeof(resp),
                            "usage: altdisplay [same | <pix_w> <pix_h> <fps> <phys_w> <phys_h>]\n");
                    }
                }

            } else {
                n = snprintf(resp, sizeof(resp),
                    "cmds: status | streams | sessions"
                    " | altscreen on | altscreen off | stream <N>"
                    " | altdisplay [same | <pw> <ph> <fps> <ww> <wh>]"
                    " | feedlog | prolog | dumpconfig\n");
            }

            send(cfd, resp, n, MSG_NOSIGNAL);
        } else {
            if (pos < (int)sizeof(line) - 1)
                line[pos++] = ch;
        }
    }
    close(cfd);
}

/* ── Log forwarding ─────────────────────────────────────────────────────── */
static int              g_log_sock  = -1;
static pthread_mutex_t  g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *log_forward_thread(void *arg)
{
    int rfd = (int)(intptr_t)arg;
    char buf[4096];
    for (;;) {
        ssize_t n = read(rfd, buf, sizeof(buf));
        if (n <= 0) break;
        pthread_mutex_lock(&g_log_mutex);
        if (g_log_sock >= 0)
            send(g_log_sock, buf, (size_t)n, MSG_NOSIGNAL);
        pthread_mutex_unlock(&g_log_mutex);
    }
    return NULL;
}

static void *cmd_thread(void *arg)
{
    (void)arg;
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) return NULL;
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in sa = {0};
    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(CMD_PORT);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(srv, (struct sockaddr *)&sa, sizeof(sa)) < 0 ||
        listen(srv, 4) < 0) { close(srv); return NULL; }
    fprintf(stderr, "[altscreen_hook] cmd socket on :%d\n", CMD_PORT);
    while (1) {
        int cfd = accept(srv, NULL, NULL);
        if (cfd >= 0) cmd_handle(cfd);
    }
    return NULL;
}


/* ══════════════════════════════════════════════════════════════════════════
 * § 9  Constructor
 * ══════════════════════════════════════════════════════════════════════════ */

__attribute__((constructor))
static void hook_init(void)
{
    /* ── Pipe stderr → TCP → Mac log receiver ── */
    {
        int pfd[2];
        if (pipe(pfd) == 0) {
            dup2(pfd[1], STDERR_FILENO);
            close(pfd[1]);
            setvbuf(stderr, NULL, _IOLBF, 0);
            int lsock = socket(AF_INET, SOCK_STREAM, 0);
            if (lsock >= 0) {
                struct sockaddr_in lsa = {0};
                lsa.sin_family = AF_INET;
                lsa.sin_port   = htons(LOG_PORT);
                inet_pton(AF_INET, LOG_HOST, &lsa.sin_addr);
                if (connect(lsock, (struct sockaddr *)&lsa, sizeof(lsa)) == 0)
                    g_log_sock = lsock;
                else
                    close(lsock);
            }
            pthread_t lt;
            pthread_create(&lt, NULL, log_forward_thread,
                           (void *)(intptr_t)pfd[0]);
            pthread_detach(lt);
        }
    }
    fprintf(stderr, "[altscreen_hook] 1 loaded pid=%d log_sock=%d\n",
            (int)getpid(), g_log_sock);

    feedlog_append("Cosmin 2222");

    /* ── Patch AirPlayReceiverSessionHasFeatureAltScreen → return 1 ── */
    void *fn_altscreen = dlsym(RTLD_DEFAULT,
                               "AirPlayReceiverSessionHasFeatureAltScreen");
    if (fn_altscreen) {
        if (selfmem_patch(fn_altscreen, hook_HasFeatureAltScreen) == 0) {
            altscreen_patch_ok = 1;
            fprintf(stderr,
                    "[altscreen_hook] patched HasFeatureAltScreen @ %p\n",
                    fn_altscreen);
        } else {
            fprintf(stderr,
                    "[altscreen_hook] FAILED to patch HasFeatureAltScreen\n");
        }
    } else {
        fprintf(stderr,
                "[altscreen_hook] HasFeatureAltScreen not found in process\n");
    }

    /* ── Patch AirPlayReceiverSessionSetDelegate for session tracking ── */
    void *fn_setdel = dlsym(RTLD_DEFAULT,
                            "AirPlayReceiverSessionSetDelegate");
    if (fn_setdel) {
        void *tramp = make_trampoline(fn_setdel);
        if (tramp) {
            trampoline_setdelegate = (real_setdelegate_t)tramp;
            if (selfmem_patch(fn_setdel, hook_SessionSetDelegate) == 0)
                fprintf(stderr,
                        "[altscreen_hook] patched SessionSetDelegate @ %p"
                        " trampoline=%p\n", fn_setdel, tramp);
            else {
                munmap(tramp, 32);
                trampoline_setdelegate = NULL;
            }
        } else {
            fprintf(stderr,
                    "[altscreen_hook] SessionSetDelegate: trampoline failed"
                    " (PC-relative prolog) — session tracking disabled\n");
        }
    }

    /* ── Patch addMainDisplay → also registers AltScreen display entry ── */
    void *fn_main = dlsym(RTLD_DEFAULT, SYM_addMainDisplay);
    if (fn_main) {
        feedlog_append("addMainDisplay init");
        void *tramp = make_trampoline(fn_main);
        if (tramp) {
            trampoline_addMainDisplay = (fn_addMainDisplay_t)tramp;
            if (selfmem_patch(fn_main, hook_addMainDisplay) == 0) {
                altdisplay_patch_ok = 1;
                feedlog_append("addMainDisplay patch=OK");
                fprintf(stderr,
                        "[altscreen_hook] patched addMainDisplay @ %p"
                        " trampoline=%p\n", fn_main, tramp);
            } else {
                feedlog_append("addMainDisplay patch=FAIL");
                munmap(tramp, 32);
                trampoline_addMainDisplay = NULL;
            }
        } else {
            feedlog_append("addMainDisplay trampoline=FAIL");
            fprintf(stderr,
                    "[altscreen_hook] addMainDisplay: trampoline failed"
                    " (PC-relative prolog)\n");
        }
    } else {
        feedlog_append("addMainDisplay dlsym=FAIL");
        fprintf(stderr, "[altscreen_hook] addMainDisplay not found\n");
    }

    /* ── Patch ScreenStreamProcessData → intercept AltScreen NAL units ── */
    void *fn_ssp = dlsym(RTLD_DEFAULT, SYM_stream_process_data);
    if (fn_ssp) {
        void *tramp = make_trampoline(fn_ssp);
        if (tramp) {
            real_ssp = (real_ssp_t)tramp;
            if (selfmem_patch(fn_ssp, hook_ssp) == 0) {
                ssp_patch_ok = 1;
                fprintf(stderr,
                        "[altscreen_hook] patched ScreenStreamProcessData @ %p"
                        " trampoline=%p\n", fn_ssp, tramp);
            } else {
                munmap(tramp, 32);
                real_ssp = NULL;
                fprintf(stderr,
                        "[altscreen_hook] ScreenStreamProcessData: selfmem_patch failed\n");
            }
        } else {
            fprintf(stderr,
                    "[altscreen_hook] ScreenStreamProcessData: trampoline failed"
                    " (PC-relative prolog)\n");
        }
    } else {
        fprintf(stderr,
                "[altscreen_hook] ScreenStreamProcessData not found\n");
    }

    /* ── Patch ScreenStreamSetAVCC → capture SPS+PPS from AVCC config ── */
    void *fn_avcc = dlsym(RTLD_DEFAULT, SYM_set_avcc);
    if (fn_avcc) {
        void *tramp = make_trampoline(fn_avcc);
        if (tramp) {
            trampoline_setavcc = (real_setavcc_t)tramp;
            if (selfmem_patch(fn_avcc, hook_SetAVCC) == 0)
                fprintf(stderr,
                        "[altscreen_hook] patched ScreenStreamSetAVCC @ %p"
                        " trampoline=%p\n", fn_avcc, tramp);
            else {
                munmap(tramp, 32);
                trampoline_setavcc = NULL;
                fprintf(stderr,
                        "[altscreen_hook] ScreenStreamSetAVCC: selfmem_patch failed\n");
            }
        } else {
            fprintf(stderr,
                    "[altscreen_hook] ScreenStreamSetAVCC: trampoline failed\n");
        }
    } else {
        fprintf(stderr, "[altscreen_hook] ScreenStreamSetAVCC not found\n");
    }

    /* ── Start command socket thread ── */
    pthread_t t;
    pthread_create(&t, NULL, cmd_thread, NULL);
    pthread_detach(t);

    /* ── Beacon to log (stderr → /tmp/altscreen_hook.log) ── */
    {
        char exe[256] = "(unknown)";
        readlink("/proc/self/exe", exe, sizeof(exe) - 1);
        fprintf(stderr,
                "[altscreen_hook] HOOK_LOADED pid=%d exe=%s"
                " altscreen_patch=%s altdisplay_patch=%s ssp_patch=%s"
                " cmd_port=%d\n",
                (int)getpid(), exe,
                altscreen_patch_ok  ? "OK" : "FAIL",
                altdisplay_patch_ok ? "OK" : "FAIL",
                ssp_patch_ok        ? "OK" : "FAIL",
                CMD_PORT);
    }
}
