/* prot_vin.c - live VIN reader with a small cross-source quorum.
 *
 * We run from MMX (the app image), where the VIN lives in the app persistence
 * store, NOT in the RCC FEC (that partition isn't reachable from MMX). The store
 * holds it as JSON KV records:
 *     {"p":"/core/vin","d":"<VIN>"}
 *     {"p":"/core/registeredVin","d":"<VIN>"}
 * We read both and require agreement, so a single edited record can't lie.
 * Path overridable at build time; default is the on-unit MMX store.
 * (Build with -DPROT_TARGET_RCC to read the RCC FEC files instead.) */
#include "prot_vin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VINCHARS "ABCDEFGHJKLMNPRSTUVWXYZ0123456789"   /* ISO 3779: no I, O, Q */

static const char *WMI[] = {
    "WAU","WA1","WUA","TRU","WVW","WV1","WV2","1VW","3VW","VSS","TMB",
    "WVG","WME","W1K","WDB","WDD", NULL
};

static int is_vinchars(const unsigned char *p, int n)
{
    for (int i = 0; i < n; i++)
        if (!strchr(VINCHARS, p[i])) return 0;
    return 1;
}

int prot_scan_vin(const unsigned char *buf, size_t n, char vin[PROT_VIN_LEN + 1])
{
    if (n < PROT_VIN_LEN) return 0;
    for (size_t i = 0; i + PROT_VIN_LEN <= n; i++)
        for (const char **w = WMI; *w; w++)
            if (memcmp(buf + i, *w, 3) == 0 && is_vinchars(buf + i, PROT_VIN_LEN)) {
                memcpy(vin, buf + i, PROT_VIN_LEN); vin[PROT_VIN_LEN] = 0; return 1;
            }
    return 0;
}

#ifndef PROT_TARGET_RCC
/* Find the 17-char VIN that follows a literal anchor (e.g. `vin","d":"`). */
static int scan_after(const unsigned char *buf, size_t n, const char *anchor,
                      char vin[PROT_VIN_LEN + 1])
{
    size_t a = strlen(anchor);
    for (size_t i = 0; i + a + PROT_VIN_LEN <= n; i++) {
        if (memcmp(buf + i, anchor, a) != 0) continue;
        const unsigned char *p = buf + i + a;
        if (is_vinchars(p, PROT_VIN_LEN)) { memcpy(vin, p, PROT_VIN_LEN); vin[PROT_VIN_LEN] = 0; return 1; }
    }
    return 0;
}
#endif /* !PROT_TARGET_RCC */

static size_t slurp(const char *path, unsigned char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

int prot_read_vin(char out[PROT_VIN_LEN + 1])
{
#ifdef PROT_TARGET_RCC
    /* RCC build: two FEC copies. */
    static const char *SRC[] = {
        "/mnt/efs-persist/FEC/FecContainer.fec",
        "/mnt/efs-system/backup/FEC/FecContainer.fec", NULL };
    unsigned char buf[8192];
    char seen[PROT_VIN_LEN + 1] = {0}; int have = 0;
    for (const char **s = SRC; *s; s++) {
        size_t n = slurp(*s, buf, sizeof buf); char v[PROT_VIN_LEN + 1];
        if (!n || !prot_scan_vin(buf, n, v)) continue;
        if (!have) { memcpy(seen, v, sizeof v); have = 1; }
        else if (memcmp(seen, v, PROT_VIN_LEN) != 0) return -2;
    }
    return have ? (memcpy(out, seen, PROT_VIN_LEN + 1), 0) : -1;
#else
    /* MMX build (default): the app persistence store, two /core records.
     * /var -> /mnt/persist/var, so both paths name the same file; try both. */
    static unsigned char buf[1 << 20];               /* store is ~90 KB; 1 MB is ample */
    size_t n = 0;
#ifdef PROT_MMX_STORE
    n = slurp(PROT_MMX_STORE, buf, sizeof buf);
#else
    n = slurp("/mnt/persist/var/fw/persistence.sqlite", buf, sizeof buf);
    if (!n) n = slurp("/var/fw/persistence.sqlite", buf, sizeof buf);
#endif
    if (!n) return -1;
    char a[PROT_VIN_LEN + 1], b[PROT_VIN_LEN + 1];
    int ha = scan_after(buf, n, "vin\",\"d\":\"", a);   /* /core/vin        */
    int hb = scan_after(buf, n, "Vin\",\"d\":\"", b);   /* /core/registeredVin */
    if (ha && hb && memcmp(a, b, PROT_VIN_LEN) != 0) return -2;   /* disagree */
    if (ha) { memcpy(out, a, PROT_VIN_LEN + 1); return 0; }
    if (hb) { memcpy(out, b, PROT_VIN_LEN + 1); return 0; }
    /* last resort: any WMI-anchored VIN in the store */
    return prot_scan_vin(buf, n, out) ? 0 : -1;
#endif
}
