/* prot_rt.c - runtime side of the build-to-VIN binding. See prot_rt.h.
 *
 * What gets checksummed: the ENTIRE on-disk image, with only the slot's mutable
 * fields (CHK/LEN/ENC) zeroed. So any binary/ASM patch anywhere in the library
 * shifts the CRC and the payload decodes to garbage. We CRC the file-on-disk
 * bytes (the flashed, immutable image) rather than in-memory .text, which
 * sidesteps relocation differences between file and memory.
 *
 * PROT_SECRET must be identical here and in protect.py. Supply it at build time,
 * e.g. -DPROT_SECRET='"…"'; the default below is a placeholder for the demo.
 */
#include "prot_rt.h"
#include "prot_crypto.h"
#include "prot_vin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* _exit, readlink */
#include <sys/types.h>

#ifndef PROT_SECRET
#define PROT_SECRET "CHANGE-ME-per-build-secret"
#endif

#define PROT_MAX_IMG (8u * 1024u * 1024u)   /* cap; the RCC apps are well under this */

/* The 8-byte magic may appear more than once in the image (the compiler can
 * materialize the same constant as a .rodata immediate). We don't fight that -
 * we try every candidate and accept the one whose CRC actually validates. */
static void make_magic(uint8_t m[PROT_SLOT_MAGLEN])
{
    m[0]='P'; m[1]='R'; m[2]='O'; m[3]='T';
    m[4]=0;   m[5]='S'; m[6]='L'; m[7]='T';
}

/* Core: validate the image CRC for candidate slot s, derive K from the live VIN,
 * and unblind the LEN stored bytes into `body`. Returns 0 iff the image is
 * intact (this is the genuine slot). Does NOT judge whether the VIN is "right" -
 * a wrong VIN also returns 0 with garbage in body, on purpose. */
static int slot_core(uint8_t *img, size_t n, size_t s, const char *vin,
                     uint8_t *body, size_t bodycap, size_t *body_len)
{
    uint8_t *slot = img + s;
    uint32_t chk = (uint32_t)slot[8] | (uint32_t)slot[9]<<8 |
                   (uint32_t)slot[10]<<16 | (uint32_t)slot[11]<<24;
    uint16_t len = (uint16_t)(slot[12] | slot[13]<<8);   /* body length (incl. tag in token mode) */
    if (len == 0 || len > bodycap) return -1;

    uint8_t save[4 + 2 + PROT_TAG + PROT_SLOT_CAP];
    memcpy(save, slot + 8, 4 + 2 + PROT_TAG + PROT_SLOT_CAP);
    memset(slot + 8, 0, 4 + 2 + PROT_TAG + PROT_SLOT_CAP);
    uint32_t crc_now = prot_crc32(img, n);           /* whole file, slot fields zeroed */
    memcpy(slot + 8, save, 4 + 2 + PROT_TAG + PROT_SLOT_CAP);
    if (crc_now != chk) return -1;                   /* not this slot, or code patched */

    uint8_t msg[17 + 4], K[32];
    memcpy(msg, vin, 17);
    msg[17]=(uint8_t)chk; msg[18]=(uint8_t)(chk>>8);
    msg[19]=(uint8_t)(chk>>16); msg[20]=(uint8_t)(chk>>24);
    prot_hmac((const uint8_t *)PROT_SECRET, strlen(PROT_SECRET), msg, sizeof msg, K);

    uint8_t ks[PROT_TAG + PROT_SLOT_CAP];
    prot_keystream(K, ks, len);
    for (size_t i = 0; i < len; i++) body[i] = slot[14 + i] ^ ks[i];
    *body_len = len;
    return 0;
}

/* Iterate all magic candidates, run fn on the first CRC-valid one. */
static int for_genuine_slot(const char *self_path, const char *vin,
                            uint8_t *body, size_t bodycap, size_t *body_len)
{
    if (!vin || strlen(vin) != 17) return 1;
    FILE *f = fopen(self_path, "rb");
    if (!f) return 2;
    uint8_t *img = (uint8_t *)malloc(PROT_MAX_IMG);
    if (!img) { fclose(f); return 3; }
    size_t n = fread(img, 1, PROT_MAX_IMG, f);
    fclose(f);
    if (n < PROT_SLOT_SIZE) { free(img); return 4; }

    uint8_t mag[PROT_SLOT_MAGLEN]; make_magic(mag);
    int rc = 5;
    for (size_t i = 0; i + PROT_SLOT_SIZE <= n; i++) {
        if (memcmp(img + i, mag, PROT_SLOT_MAGLEN) != 0) continue;
        if (slot_core(img, n, i, vin, body, bodycap, body_len) == 0) { rc = 0; break; }
        rc = 7;                                      /* magic found but CRC failed -> patched */
    }
    free(img);
    return rc;
}

/* ---- one-shot cache. Unlock once at init; hot paths read this, no crypto. ---- */
static uint8_t  g_payload[PROT_SLOT_CAP];
static size_t   g_len = 0;
static int      g_ready = 0;

int prot_init(const char *self_path, const char *vin)
{
    g_ready = 0; g_len = 0;
    int rc = prot_unlock(self_path, vin, g_payload, sizeof g_payload, &g_len);
    if (rc == 0) g_ready = 1;
    return rc;
}
int prot_ready(void) { return g_ready; }
uint8_t prot_gate(size_t i) { return (g_ready && i < g_len) ? g_payload[i] : 0; }
size_t  prot_gate_len(void) { return g_ready ? g_len : 0; }

/* Blind mode: unblind the stored constants in place. Silent by design - a wrong
 * VIN returns 0 with garbage in consts (the wrongness is the protection). */
int prot_unblind(const char *self_path, const char *vin, uint8_t *consts, size_t n)
{
    uint8_t body[PROT_TAG + PROT_SLOT_CAP];
    size_t bl = 0;
    int rc = for_genuine_slot(self_path, vin, body, sizeof body, &bl);
    if (rc != 0) return rc;                            /* patched image / malformed */
    if (bl < n) return 8;                              /* stored fewer bytes than asked */
    memcpy(consts, body, n);
    return 0;
}

/* Token mode: body = TAG || payload. Wrong VIN -> tag mismatch -> rejected. */
int prot_unlock(const char *self_path, const char *vin,
                uint8_t *out, size_t cap, size_t *out_len)
{
    uint8_t body[PROT_TAG + PROT_SLOT_CAP];
    size_t bl = 0;
    int rc = for_genuine_slot(self_path, vin, body, sizeof body, &bl);
    if (rc != 0) return rc;

    static const uint8_t TAG[PROT_TAG] = { 0xA5, 0x5A, 0xC3, 0x3C };
    if (bl < PROT_TAG || memcmp(body, TAG, PROT_TAG) != 0) return 7;  /* wrong VIN/secret */
    size_t plen = bl - PROT_TAG;
    if (plen > cap) return 6;
    memcpy(out, body + PROT_TAG, plen);
    *out_len = plen;
    return 0;
}

/* Formality tripwire: exit if the VIN doesn't match a token-stamped tag.
 * Cosmetic - the real defense is broken math via prot_unblind. Needs a token-
 * mode slot; on a blind-only image it will _exit (no tag), so use only with token. */
void prot_or_die(const char *self_path, const char *vin)
{
    uint8_t scratch[PROT_SLOT_CAP]; size_t nn = 0;
    if (prot_unlock(self_path, vin, scratch, sizeof scratch, &nn) != 0)
        _exit(42);                                     /* VIN/integrity failed */
}

/* Graceful fail-open. Premium only when the VIN is right AND the image intact
 * AND the sentinel matches; every other outcome degrades to `defaults` (stock).
 * The premium constants never appear on a failure path. */
prot_mode_t prot_select(const char *self_path, const char *vin, uint8_t sentinel,
                        const uint8_t *defaults, uint8_t *out, size_t n)
{
    uint8_t body[PROT_TAG + PROT_SLOT_CAP];
    size_t bl = 0;
    /* +1 for the leading sentinel byte in the stamped constants */
    int rc = for_genuine_slot(self_path, vin, body, sizeof body, &bl);
    if (rc == 0 && bl >= n + 1 && body[0] == sentinel) {
        memcpy(out, body + 1, n);                      /* right VIN -> premium */
        return PROT_PREMIUM;
    }
    memcpy(out, defaults, n);                          /* anything else -> stock */
    return PROT_STOCK;
}

/* Hook gating: resolve once at load, cache mode + constants. Hooks then read
 * prot_active()/prot_gate() with no crypto. Inactive -> hooks call through to the
 * original, stock path untouched. */
static prot_mode_t g_mode = PROT_STOCK;
prot_mode_t prot_arm(const char *self_path, const char *vin, uint8_t sentinel,
                     const uint8_t *defaults, size_t n)
{
    if (n > sizeof g_payload) n = sizeof g_payload;
    g_mode = prot_select(self_path, vin, sentinel, defaults, g_payload, n);
    g_len = n;
    g_ready = 1;                                       /* constants cached either way */
    return g_mode;
}
int prot_active(void) { return g_mode == PROT_PREMIUM; }

/* Best-effort path to our own on-disk image. */
int prot_self_path(char *buf, size_t cap)
{
    ssize_t r;
    /* QNX Neutrino */
    if ((r = readlink("/proc/self/exefile", buf, cap - 1)) > 0) { buf[r] = 0; return 0; }
    /* Linux (host tests / other targets) */
    if ((r = readlink("/proc/self/exe", buf, cap - 1)) > 0) { buf[r] = 0; return 0; }
#ifdef PROT_SELF_OVERRIDE   /* build-time path: tests, or platforms without /proc */
    { size_t k = strlen(PROT_SELF_OVERRIDE); if (k < cap) { memcpy(buf, PROT_SELF_OVERRIDE, k+1); return 0; } }
#endif
    return -1;
}

/* Production: arm from the live quorum VIN + our own image. Any failure -> STOCK. */
prot_mode_t prot_arm_auto(uint8_t sentinel, const uint8_t *defaults, size_t n)
{
    char self[1024], vin[PROT_VIN_LEN + 1];
    if (prot_self_path(self, sizeof self) != 0 || prot_read_vin(vin) != 0) {
        /* can't locate self, or no/disagreeing VIN -> degrade to stock/inert */
        if (n > sizeof g_payload) n = sizeof g_payload;
        memcpy(g_payload, defaults, n);
        g_len = n; g_ready = 1; g_mode = PROT_STOCK;
        return PROT_STOCK;
    }
    return prot_arm(self, vin, sentinel, defaults, n);
}
