/*
 * decoder_optimize - dio video-decode feed tuning for the stock libairplay 210.81.
 *
 * What stock does with a CarPlay video frame before the hardware sees it
 * (reversed 2026-10-19, full write-up in dio_asm_to_c/_legacy_attempt/
 * libairplay-mhi2q/reversed/04_video_decoder_optimization.md; the decode chain
 * itself is reversed/01_screen_decode_chain.md):
 *
 *   engine thread: AVCC->Annex-B rewrite ............... copy 1
 *     dio::OmxVideoImpl::initialize (libairplay @0x8c638) builds the pipeline:
 *       Queue B  MOV R0,#4 @0x8c694  -> CScreenRender::create(4) -> CCommandThread(4)
 *       Queue A  MOV R3,#4 @0x8c6f4  -> COMXVideoDecoder::init(.., 4)
 *   COMXVideoDecoder::decode: frame -> heap CBlob ...... copy 2, Queue A (depth 4)
 *   videoDecoderThread (prio $SCREEN_RECEIVER_THREAD_PRIO, default 17):
 *     blob -> OMX input buffer ......................... copy 3, EmptyThisBuffer
 *   HW: OMX.qcom.video.decoder.avc -> gralloc (zero-copy present)
 *   FillBufferDone -> CRenderFrameCommand -> Queue B -> screen_post_window
 *
 * The decode is hardware and the present is zero-copy; what software adds is
 * queueing (two depth-4 command queues = up to ~4 frames of pipeline latency at
 * 30 fps) and scheduling (decoder thread at priority 17).  This module shrinks
 * both queues to depth 2; the thread priority is controlled natively by stock
 * through SCREEN_RECEIVER_THREAD_PRIO.
 *
 * Patch sites are inside dio::OmxVideoImpl::initialize, before any decoder
 * thread exists, and on_init runs at the first Cinemo boundary - long before
 * the first CarPlay SETUP creates a stream.  Safety rules, in order:
 *
 *   1. Base gate by deltas.  Three exports of known stock vaddrs must agree
 *      pairwise to the byte.  Deltas alone prove only the mutual layout, NOT
 *      that the symbols sit at the stock offsets inside their module: on a
 *      foreign lib they can all be shifted together and the derived base point
 *      anywhere.  The delta gate is therefore only a cheap first filter -
 *      every memory access after it is separately made safe by rule 2.
 *   2. Readability before every read.  Nothing is ever dereferenced without
 *      first probing the exact byte range with a kernel copy: write() into an
 *      owned pipe faults with EFAULT on unreadable memory and has no side
 *      effects on foreign pages (an mprotect probe would strip W from a
 *      foreign RW page).  The ELF identity (magic, ET_DYN, EM_ARM) and both
 *      12-byte signatures are probe-then-read, so any foreign library is
 *      refused without a fault, no matter where the derived base landed.
 *   3. Both patch sites are verified before the first write; the writes share
 *      one mprotect session, so a refused site can never coexist with a
 *      patched one.
 *   4. The mprotect restore is checked: a failed restore is reported as a
 *      failure, never as a quiet success that leaves the text page writable.
 *
 *   P5087 MU1316 US + P5092 MU1329 EU: byte-identical libairplay (763420 B),
 *     anchors 0x201e4/0x20b40/0x220d0, both sites match.
 *   K1004 MU1003 CN: different lib - deltas/identity/signatures refuse.
 *
 * The installer patches the unit's own file by signature because production
 * QNX refuses writes to shared text. Already-patched queues are verified at
 * depth 2; runtime patching is a fallback where text protection permits it.
 * scripts/test_decoder_optimize_site.sh re-verifies the signatures against
 * the unpacked firmware trees; tests/decoder_optimize_hook_test.c drives base
 * gating and decoder_optimize_apply on the host, including the partial-patch
 * and unreadable-page refusals.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "decoder_optimize_hook.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

DEFINE_LOG_MODULE(DECODER_OPTIMIZE);

/* libairplay 210.81 (P5087/P5092): three exports of known vaddr. The wireless
 * bridge interposes all three, so resolve from the explicit library handle;
 * RTLD_DEFAULT and RTLD_NEXT can both select the bridge in a preload chain. */
#define ANCHOR_DELEGATE_NAME  "AirPlayReceiverServerSetDelegate"
#define ANCHOR_DELEGATE_OFF   0x201e4u
#define ANCHOR_CREATE_NAME    "AirPlayReceiverServerCreate"
#define ANCHOR_CREATE_OFF     0x20b40u
#define ANCHOR_CONTROL_NAME   "AirPlayReceiverServerControl"
#define ANCHOR_CONTROL_OFF    0x220d0u

/* dio::OmxVideoImpl::initialize: two `mov rX, #4` immediates, each followed by
 * its unique call pair.  One byte (the ARM imm8, bits [7:0] of the word) per
 * site is all a depth change needs. */
#define SITE_QUEUE_B 0x8c694u   /* present-side queue: mov r0,#4; bl CScreenRender::create; cmp r0,#0 */
#define SITE_QUEUE_A 0x8c6f4u   /* decode-feed queue:  mov r3,#4; add r1,r4,r1; bl init          */

static const uint8_t SIG_QUEUE_B[12] = {
    0x04, 0x00, 0xa0, 0xe3,   /* mov r0, #4 */
    0x3a, 0x38, 0xfe, 0xeb,   /* bl  0x1a788 (CScreenRender::create) */
    0x00, 0x00, 0x50, 0xe3,   /* cmp r0, #0 */
};
static const uint8_t SIG_QUEUE_A[12] = {
    0x04, 0x30, 0xa0, 0xe3,   /* mov r3, #4 */
    0x01, 0x10, 0x84, 0xe0,   /* add r1, r4, r1 */
    0xbc, 0x36, 0xfe, 0xeb,   /* bl  0x1a1f4 (COMXVideoDecoder::init) */
};

#ifdef DECODER_OPTIMIZE_HOOK_TESTING
#define DECODER_OPTIMIZE_TESTABLE
#else
#define DECODER_OPTIMIZE_TESTABLE static
#endif

/* ---- rule 2: kernel-copy readability probe --------------------------------
 * An owned pipe turns "is this range readable?" into a plain write(): the
 * kernel copies from the probe address and returns EFAULT on any unreadable
 * byte.  It never modifies foreign page protections and never faults.  Runs
 * once per process from on_init, single-threaded - no locking.  The drain
 * reads back exactly the bytes a full probe wrote, so it cannot block. */
static int dec_probe_fd[2] = { -1, -1 };   /* [0] = read end, [1] = write end */

DECODER_OPTIMIZE_TESTABLE bool dec_readable(const void* addr, size_t len) {
    uint8_t drain[64];
    size_t left = len;
    if (len == 0 || len > sizeof(drain)) return false;
    if (dec_probe_fd[0] < 0 && pipe(dec_probe_fd) != 0) return false;
    if (write(dec_probe_fd[1], addr, len) != (ssize_t)len) return false;
    while (left) {
        ssize_t k = read(dec_probe_fd[0], drain, left);
        if (k <= 0) return false;
        left -= (size_t)k;
    }
    return true;
}

static void dec_probe_close(void) {
    if (dec_probe_fd[0] >= 0) close(dec_probe_fd[0]);
    if (dec_probe_fd[1] >= 0) close(dec_probe_fd[1]);
    dec_probe_fd[0] = dec_probe_fd[1] = -1;
}

/* ELF identity of a QNX ARM shared object at a probed base: magic, ET_DYN,
 * EM_ARM (little-endian e_type@0x10, e_machine@0x12). */
static bool dec_is_elf_image(const void* base) {
    const uint8_t* p = (const uint8_t*)base;
    if (!dec_readable(p, 20)) return false;
    if (memcmp(p, "\x7f" "ELF", 4) != 0) return false;
    if (p[16] != 3 || p[17] != 0) return false;   /* e_type  = ET_DYN */
    if (p[18] != 40 || p[19] != 0) return false;  /* e_machine = EM_ARM */
    return true;
}

/* Base of the loaded libairplay from three anchor exports.  The delta gate
 * proves the stock mutual layout; the ELF identity - probed, then read - must
 * confirm the derived base before anything trusts it.  0 on any doubt. */
DECODER_OPTIMIZE_TESTABLE uintptr_t dec_base_from_anchors(uintptr_t delegate_, uintptr_t create,
                                                   uintptr_t control) {
    uintptr_t base;

    if (!delegate_ || !create || !control) return 0;
    if (create - delegate_ != (uintptr_t)(ANCHOR_CREATE_OFF - ANCHOR_DELEGATE_OFF)) return 0;
    if (control - delegate_ != (uintptr_t)(ANCHOR_CONTROL_OFF - ANCHOR_DELEGATE_OFF)) return 0;
    base = delegate_ - ANCHOR_DELEGATE_OFF;
    if (!dec_is_elf_image((const void*)base)) return 0;
    return base;
}

DECODER_OPTIMIZE_TESTABLE bool dec_site_matches(const uint8_t* site, const uint8_t sig[12]) {
    return memcmp(site, sig, 12) == 0;
}

/* icache maintenance after writing instructions.  Host builds (the test) never
 * write real code, and __clear_cache is a GCC-ism the QNX build defines. */
static void dec_flush_icache(void* from, void* to) {
#ifdef __QNX__
    __clear_cache(from, to);
#else
    (void)from; (void)to;
#endif
}

/* The one text-protection seam, so the host test can drive the mprotect
 * failure paths (rules 3/4) against a fake module.  Production: real
 * mprotect, errno-checked; `writable` selects the RWX open and the R|X
 * restore. */
#ifdef DECODER_OPTIMIZE_HOOK_TESTING
bool dec_protect(void* addr, size_t len, bool writable);
#else
static bool dec_protect(void* addr, size_t len, bool writable) {
    int prot = writable ? (PROT_READ | PROT_WRITE | PROT_EXEC)
                        : (PROT_READ | PROT_EXEC);
    return mprotect(addr, len, prot) == 0;
}
#endif

/* Both sites probed, verified and written under one mprotect session, or
 * nothing is written.  Every failure path logs and returns false. */
DECODER_OPTIMIZE_TESTABLE bool decoder_optimize_apply(uintptr_t base) {
    const unsigned depth = 2;
    const struct { uint32_t off; const uint8_t* sig; const char* what; } sites[2] = {
        { SITE_QUEUE_A, SIG_QUEUE_A, "decode feed" },
        { SITE_QUEUE_B, SIG_QUEUE_B, "present" },
    };
    uintptr_t lo = base + (SITE_QUEUE_A < SITE_QUEUE_B ? SITE_QUEUE_A : SITE_QUEUE_B);
    uintptr_t hi = base + (SITE_QUEUE_A < SITE_QUEUE_B ? SITE_QUEUE_B : SITE_QUEUE_A);
    uintptr_t page_lo = lo & ~(uintptr_t)0xfffu;
    uintptr_t page_hi = hi & ~(uintptr_t)0xfffu;
    size_t len = (size_t)(page_hi + 0x1000 - page_lo);
    unsigned i, done = 0;

    /* Rule 2+3: all probing and verification before the first write.  The
     * byte-level log line is only reachable after the probe succeeded - a
     * failed probe logs the address, never the bytes. */
    for (i = 0; i < 2; ++i) {
        const uint8_t* p = (const uint8_t*)(base + sites[i].off);
        bool readable = dec_readable(p, 12);
        if (readable && dec_site_matches(p, sites[i].sig)) continue;
        if (readable && p[0] == depth && memcmp(p + 1, sites[i].sig + 1, 11) == 0) {
            ++done;
            continue;
        }
        if (!readable)
            LOG_WARN(LOG_MODULE, "%s (+0x%x) is unreadable at base %p -"
                                 " not the supported libairplay build, no patch",
                     sites[i].what, sites[i].off, (void*)base);
        else
            LOG_WARN(LOG_MODULE, "signature mismatch at %s (+0x%x): %02x %02x %02x %02x -"
                                 " not the supported libairplay build, no patch",
                     sites[i].what, sites[i].off, p[0], p[1], p[2], p[3]);
        return false;
    }

    if (done == 2) {
        LOG_WARN(LOG_MODULE, "queue depth %u already installed (decode feed +0x%x, present +0x%x)",
                 depth, SITE_QUEUE_A, SITE_QUEUE_B);
        return true;
    }
    if (done) {
        LOG_WARN(LOG_MODULE, "mixed queue depths - refusing partial patch");
        return false;
    }

    if (!dec_protect((void*)page_lo, len, true)) {
        LOG_WARN(LOG_MODULE, "mprotect RWX failed (+0x%x..+0x%x), no patch",
                 (unsigned)(page_lo - base), (unsigned)(page_lo - base + len));
        return false;
    }
    for (i = 0; i < 2; ++i) {
        uint8_t* p = (uint8_t*)(base + sites[i].off);
        p[0] = (uint8_t)depth;                /* MOV rX,#imm: imm8 = bits [7:0] */
        dec_flush_icache(p, p + 12);
    }

    /* Rule 4: a failed restore must not read as success. */
    if (!dec_protect((void*)page_lo, len, false)) {
        LOG_WARN(LOG_MODULE, "PATCHED depth %u but the text page stays writable"
                             " (mprotect restore failed)", depth);
        return false;
    }

    for (i = 0; i < 2; ++i) {
        uint8_t* p = (uint8_t*)(base + sites[i].off);
        if (p[0] == depth && memcmp(p + 1, sites[i].sig + 1, 11) == 0) continue;
        LOG_WARN(LOG_MODULE, "write-back verify failed at %s (+0x%x)", sites[i].what, sites[i].off);
        return false;
    }
    LOG_WARN(LOG_MODULE, "queue depth 4 -> %u (decode feed +0x%x, present +0x%x)",
             depth, SITE_QUEUE_A, SITE_QUEUE_B);
    return true;
}

DECODER_OPTIMIZE_TESTABLE uintptr_t dec_base_from_library(
    void *library, void *(*lookup)(void *, const char *)) {
    uintptr_t delegate_, create, control, base;
    if (!library || !lookup) return 0;
    delegate_ = (uintptr_t)lookup(library, ANCHOR_DELEGATE_NAME);
    create    = (uintptr_t)lookup(library, ANCHOR_CREATE_NAME);
    control   = (uintptr_t)lookup(library, ANCHOR_CONTROL_NAME);
    base = dec_base_from_anchors(delegate_, create, control);
    return base;
}

static void decoder_optimize_init(void) {
    /* DIO already needs libairplay. Retain this handle for process lifetime:
     * the patched code and all three anchors must keep the same owner. */
    static void *library;
    if (!library) library = dlopen("/mnt/app/eso/lib/libairplay.so", RTLD_NOW | RTLD_LOCAL);
    uintptr_t base = dec_base_from_library(library, dlsym);
    if (!base) {
        LOG_WARN(LOG_MODULE, "anchors %s/%s/%s do not resolve to a stock 210.81 libairplay -"
                             " unsupported build, staying stock",
                 ANCHOR_DELEGATE_NAME, ANCHOR_CREATE_NAME, ANCHOR_CONTROL_NAME);
        dec_probe_close();                     /* the base probes opened it */
        return;
    }

    if (getenv("SCREEN_RECEIVER_THREAD_PRIO"))
        LOG_INFO(LOG_MODULE, "SCREEN_RECEIVER_THREAD_PRIO='%s' (stock applies it natively)",
                 getenv("SCREEN_RECEIVER_THREAD_PRIO"));

    decoder_optimize_apply(base);
    dec_probe_close();
}

static void decoder_optimize_shutdown(void) {}

const hook_module_def_t decoder_optimize_module_def = {
    .name = "decoder_optimize",
    .priority = HOOK_PRIORITY_LAST,
    .on_init = decoder_optimize_init,
    .on_shutdown = decoder_optimize_shutdown,
};
