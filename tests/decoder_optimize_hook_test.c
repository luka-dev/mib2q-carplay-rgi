/* decoder_optimize: the patcher's decision logic runs on the host (no OMX needed).
 *
 * Base rule: consistent anchor deltas prove only the mutual symbol layout, not
 * that the symbols sit at the stock offsets - a foreign lib can shift all
 * three together, so the derived base must be probed for readability (kernel
 * copy through an owned pipe) and ELF-identified (magic, ET_DYN, EM_ARM)
 * before anything dereferences it.  The shifted-base case below points at a
 * PROT_NONE page: reaching the refusal at all proves no read happened there.
 *
 * decoder_optimize_apply is exercised on a fake mapped module: the success path, the
 * fixed depth 2, partial-patch rule (one bad signature must leave the other
 * site unwritten), unreadable-site refusal and both protection failures. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include "decoder_optimize/decoder_optimize_hook.h"
#include "framework/logging.h"

/* statics of the module under test */
uintptr_t dec_base_from_anchors(uintptr_t delegate_, uintptr_t create, uintptr_t control);
uintptr_t dec_base_from_library(void* library, void* (*lookup)(void*, const char*));
bool dec_site_matches(const uint8_t* site, const uint8_t sig[12]);
bool dec_readable(const void* addr, size_t len);
bool decoder_optimize_apply(uintptr_t base);

/* stock 210.81 (P5087/P5092): nm -D vaddrs, the two mov-immediate sites, and
 * the exact 12-byte signatures (site bytes cross-checked against the unpacked
 * firmware trees by scripts/test_decoder_optimize_site.sh) */
#define OFF_DELEGATE 0x201e4u
#define OFF_CREATE   0x20b40u
#define OFF_CONTROL  0x220d0u
#define SITE_A 0x8c6f4u
#define SITE_B 0x8c694u
static const uint8_t SIG_A[12] = { 0x04,0x30,0xa0,0xe3, 0x01,0x10,0x84,0xe0, 0xbc,0x36,0xfe,0xeb };
static const uint8_t SIG_B[12] = { 0x04,0x00,0xa0,0xe3, 0x3a,0x38,0xfe,0xeb, 0x00,0x00,0x50,0xe3 };
/* K1004 MU1003 CN: what actually sits at +SITE_A there - must never match */
static const uint8_t CN_AT_A[12] = { 0x01,0x00,0x73,0xe3, 0x0b,0x00,0x00,0x0a, 0x50,0x10,0x9f,0xe5 };

static uint8_t elf_page[0x2000] __attribute__((aligned(0x1000)));

static void elf_identity(uint8_t* page) {
    memcpy(page, "\x7f" "ELF", 4);
    page[16] = 3; page[17] = 0;    /* e_type = ET_DYN, LE */
    page[18] = 40; page[19] = 0;   /* e_machine = EM_ARM, LE */
}

static void test_base_rule(void) {
    uintptr_t base = (uintptr_t)elf_page;
    elf_identity(elf_page);

    /* stock layout over a real probed ELF image: accepted, base recovered */
    assert(dec_base_from_anchors(base + OFF_DELEGATE, base + OFF_CREATE,
                                 base + OFF_CONTROL) == base);

    /* the review case: all three anchors shifted together so the deltas stay
     * consistent while the derived base lands on unreadable memory.  The page
     * after the mapping is PROT_NONE; touching base (or anything derived)
     * would fault - reaching the 0 return proves probe-before-read. */
    {
        long ps = sysconf(_SC_PAGESIZE);
        uint8_t* none_page = mmap(NULL, (size_t)ps, PROT_NONE,
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(none_page != MAP_FAILED);
        assert(dec_base_from_anchors((uintptr_t)none_page + OFF_DELEGATE,
                                     (uintptr_t)none_page + OFF_CREATE,
                                     (uintptr_t)none_page + OFF_CONTROL) == 0);
        munmap(none_page, (size_t)ps);
    }

    /* readable but a foreign ELF identity: right magic, wrong type */
    elf_page[16] = 2;   /* ET_EXEC */
    assert(dec_base_from_anchors(base + OFF_DELEGATE, base + OFF_CREATE,
                                 base + OFF_CONTROL) == 0);
    elf_identity(elf_page);

    /* two of three anchors agree but the third is from somewhere else */
    assert(dec_base_from_anchors(base + OFF_DELEGATE, base + OFF_CREATE,
                                 base + OFF_CONTROL + 4) == 0);

    /* deltas agree, base readable, but no ELF image: zeroed page */
    memset(elf_page, 0, sizeof(elf_page));
    assert(dec_base_from_anchors(base + OFF_DELEGATE, base + OFF_CREATE,
                                 base + OFF_CONTROL) == 0);

    /* missing anchors */
    assert(dec_base_from_anchors(0, base + OFF_CREATE, base + OFF_CONTROL) == 0);

    /* create below delegate: unsigned wrap must refuse, not alias into range */
    elf_identity(elf_page);
    assert(dec_base_from_anchors(base + OFF_CREATE, base + OFF_DELEGATE,
                                 base + OFF_CONTROL) == 0);

    printf("  base rule: deltas gate, probe+ELF identity confirm; shifted base refused unread\n");
}

static int library_token, lookup_calls;
static void* scoped_lookup(void* handle, const char* name) {
    uintptr_t base = (uintptr_t)elf_page;
    assert(handle == &library_token);
    ++lookup_calls;
    if (strcmp(name, "AirPlayReceiverServerSetDelegate") == 0) return (void*)(base + OFF_DELEGATE);
    if (strcmp(name, "AirPlayReceiverServerCreate") == 0) return (void*)(base + OFF_CREATE);
    if (strcmp(name, "AirPlayReceiverServerControl") == 0) return (void*)(base + OFF_CONTROL);
    assert(0);
    return NULL;
}
static void test_library_scope(void) {
    elf_identity(elf_page);
    assert(dec_base_from_library(NULL, scoped_lookup) == 0);
    assert(dec_base_from_library(&library_token, NULL) == 0);
    assert(lookup_calls == 0);
    assert(dec_base_from_library(&library_token, scoped_lookup) == (uintptr_t)elf_page);
    assert(lookup_calls == 3);
    printf("  scope: all anchors resolved through explicit library handle\n");
}

static void test_site_match(void) {
    const uint8_t good[12] = { 0x04,0x30,0xa0,0xe3, 0x01,0x10,0x84,0xe0, 0xbc,0x36,0xfe,0xeb };
    uint8_t flip[12];
    memcpy(flip, good, 12);
    assert(dec_site_matches(good, SIG_A));
    flip[7] ^= 0x01;                                       /* bl target off by one */
    assert(!dec_site_matches(flip, SIG_A));
    assert(!dec_site_matches(CN_AT_A, SIG_A));             /* CN train fails inert */
    assert(dec_site_matches(SIG_B, SIG_B));
    printf("  site match: 12 exact bytes, imm8 tail, CN refuses\n");
}

/* a fake "loaded libairplay": ELF identity at 0, stock signature words at both
 * sites, size just past the higher site.  Plain RW memory - dec_protect is
 * stubbed below, so no host W^X constraints apply. */
#define FAKE_SIZE (0x8d000u)

static uint8_t* fake_module(void) {
    uint8_t* m = malloc(FAKE_SIZE);
    assert(m);
    elf_identity(m);
    memcpy(m + SITE_A, SIG_A, 12);
    memcpy(m + SITE_B, SIG_B, 12);
    return m;
}

/* dec_protect stub: bounds-checks against the fake module, counts calls and
 * fails on demand, so the mprotect-open and mprotect-restore failure paths of
 * rules 3/4 are exercised by the permanent suite, not a throwaway bench.
 * Zero calls at the end of a refusal case proves no protection session was
 * ever opened, i.e. nothing was written. */
static uint8_t* prot_region;
static int prot_fail_open, prot_fail_restore, prot_calls;
bool dec_protect(void* addr, size_t len, bool writable) {
    uint8_t* p = addr;
    /* the session is page-framed and must span both patch sites */
    assert(p <= prot_region + SITE_A && p + len >= prot_region + SITE_A + 12);
    assert(p <= prot_region + SITE_B && p + len >= prot_region + SITE_B + 12);
    if (writable ? prot_fail_open : prot_fail_restore) { prot_calls++; return false; }
    prot_calls++;
    return true;
}

static void test_apply(void) {
    uint8_t* m = fake_module();
    prot_region = m;

    /* success: both imm8 bytes swapped, call-pair tails intact */
    assert(decoder_optimize_apply((uintptr_t)m) == true);
    assert(prot_calls == 2);                    /* open + restore, one session */
    assert(m[SITE_A] == 2 && memcmp(m + SITE_A + 4, SIG_A + 4, 8) == 0);
    assert(m[SITE_B] == 2 && memcmp(m + SITE_B + 4, SIG_B + 4, 8) == 0);
    printf("  apply: both sites patched to 2, tails intact\n");

    /* File-patched production image: both full signatures verified, no mprotect. */
    prot_calls = 0;
    prot_fail_open = 1;
    assert(decoder_optimize_apply((uintptr_t)m) == true);
    assert(prot_calls == 0);
    /* An optimized immediate cannot hide corruption in the rest of the MOV. */
    m[SITE_A + 1] ^= 1;
    assert(decoder_optimize_apply((uintptr_t)m) == false);
    assert(prot_calls == 0);
    m[SITE_A + 1] ^= 1;
    /* Mixed depths are refused in either direction without writes. */
    m[SITE_A] = 4;
    assert(decoder_optimize_apply((uintptr_t)m) == false);
    assert(prot_calls == 0 && m[SITE_A] == 4 && m[SITE_B] == 2);
    m[SITE_A] = 2; m[SITE_B] = 4;
    assert(decoder_optimize_apply((uintptr_t)m) == false);
    assert(prot_calls == 0 && m[SITE_A] == 2 && m[SITE_B] == 4);
    prot_fail_open = 0;

    /* reset to stock for the refusal cases */
    m[SITE_A] = SIG_A[0];
    m[SITE_B] = SIG_B[0];

    /* partial-patch rule: the higher site's signature corrupted must leave the
     * other site unwritten - no mixed depths */
    m[SITE_B + 7] ^= 0x01;
    prot_calls = 0;
    assert(decoder_optimize_apply((uintptr_t)m) == false);
    assert(prot_calls == 0);                               /* never opened a session */
    assert(m[SITE_A] == SIG_A[0]);                         /* decode feed untouched */
    m[SITE_B + 7] ^= 0x01;

    /* unreadable site: the kernel-copy probe refuses before anything else.
     * Both sites share one page (as in the real lib), so the module here is a
     * page-aligned mapping and its site page goes PROT_NONE - reading it back
     * would fault, so "nothing written" is asserted via prot_calls. */
    {
        long ps = sysconf(_SC_PAGESIZE);
        uint8_t* m2 = mmap(NULL, FAKE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        uint8_t* site_page;
        assert(m2 != MAP_FAILED);
        elf_identity(m2);
        memcpy(m2 + SITE_A, SIG_A, 12);
        memcpy(m2 + SITE_B, SIG_B, 12);
        site_page = m2 + (SITE_B & ~(uintptr_t)0xfffu);
        prot_region = m2;
        prot_calls = 0;
        assert(mprotect(site_page, (size_t)ps, PROT_NONE) == 0);
        assert(decoder_optimize_apply((uintptr_t)m2) == false);
        assert(prot_calls == 0);
        assert(mprotect(site_page, (size_t)ps, PROT_READ | PROT_WRITE) == 0);
        munmap(m2, FAKE_SIZE);
        prot_region = m;                       /* back to the main fake module */
    }

    /* mprotect open failure: refused with nothing written */
    prot_fail_open = 1;
    prot_calls = 0;
    assert(decoder_optimize_apply((uintptr_t)m) == false);
    assert(m[SITE_A] == SIG_A[0] && m[SITE_B] == SIG_B[0]);
    prot_fail_open = 0;

    /* mprotect restore failure: writes happened and the module reports
     * failure - never a quiet success with a writable text page */
    prot_fail_restore = 1;
    assert(decoder_optimize_apply((uintptr_t)m) == false);
    assert(m[SITE_A] == 2 && m[SITE_B] == 2);
    prot_fail_restore = 0;

    free(m);
    prot_region = NULL;
    printf("  apply: partial patch, unreadable page, mprotect open/restore refusals OK\n");
}

static void test_readable_probe(void) {
    long ps = sysconf(_SC_PAGESIZE);
    uint8_t* p = mmap(NULL, (size_t)ps, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(p != MAP_FAILED);
    assert(dec_readable(p, 12));
    assert(mprotect(p, (size_t)ps, PROT_NONE) == 0);
    assert(!dec_readable(p, 12));                          /* EFAULT, not a fault */
    assert(mprotect(p, (size_t)ps, PROT_READ | PROT_WRITE) == 0);
    assert(!dec_readable(p, 0));                           /* degenerate length */
    munmap(p, (size_t)ps);
    printf("  probe: readable ranges pass, PROT_NONE fails EFAULT, no signal\n");
}

int main(void) {
    /* Built twice by run_tests.sh: ENABLE_LOGGING=0 (pure logic) and =1 with
     * the real logging.c, as in the shipping .so.  The logging build is what
     * keeps the refusal branches honest: their log arguments are evaluated
     * for real, so a branch that dereferenced an unprobed address would
     * SIGBUS right here (that bug shipped once already). */
    log_config_t lc = LOG_CONFIG_DEFAULT;
    lc.log_path = "/tmp/decoder_optimize_hook_test.log";
    lc.include_timestamp = true;
    assert(log_init(&lc) == HOOK_OK);

    test_base_rule();
    test_library_scope();
    test_site_match();
    test_readable_probe();
    test_apply();
    log_shutdown();
    printf("decoder_optimize_hook_test PASS\n");
    return 0;
}
