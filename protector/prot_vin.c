/* prot_vin.c - live VIN reader with a small cross-source quorum. See prot_vin.h. */
#include "prot_vin.h"
#include <stdio.h>
#include <string.h>

#define VINCHARS "ABCDEFGHJKLMNPRSTUVWXYZ0123456789"   /* ISO 3779: no I, O, Q */

/* World Manufacturer Identifier prefixes we anchor on. Extend for other makes. */
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
    for (size_t i = 0; i + PROT_VIN_LEN <= n; i++) {
        for (const char **w = WMI; *w; w++) {
            if (memcmp(buf + i, *w, 3) == 0 && is_vinchars(buf + i, PROT_VIN_LEN)) {
                memcpy(vin, buf + i, PROT_VIN_LEN);
                vin[PROT_VIN_LEN] = 0;
                return 1;
            }
        }
    }
    return 0;
}

static int vin_from_file(const char *path, char vin[PROT_VIN_LEN + 1])
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[8192];
    size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    return prot_scan_vin(buf, n, vin);
}

int prot_read_vin(char out[PROT_VIN_LEN + 1])
{
    /* Live sources: the two FEC copies only. (No /dev/shmem/VIN.txt - that is the
     * volatile bus VIN, absent on the bench and not the binding we key on.)
     * Paths overridable at build time for tests / unusual layouts. */
#ifndef PROT_FEC_ACTIVE
#define PROT_FEC_ACTIVE "/mnt/efs-persist/FEC/FecContainer.fec"
#endif
#ifndef PROT_FEC_BACKUP
#define PROT_FEC_BACKUP "/mnt/efs-system/backup/FEC/FecContainer.fec"
#endif
    static const char *SRC[] = { PROT_FEC_ACTIVE, PROT_FEC_BACKUP, NULL };
    char seen[PROT_VIN_LEN + 1] = {0};
    int have = 0;
    for (const char **s = SRC; *s; s++) {
        char v[PROT_VIN_LEN + 1];
        if (!vin_from_file(*s, v)) continue;
        if (!have) { memcpy(seen, v, sizeof v); have = 1; }
        else if (memcmp(seen, v, PROT_VIN_LEN) != 0) return -2;  /* disagree */
    }
    if (!have) return -1;
    memcpy(out, seen, PROT_VIN_LEN + 1);
    return 0;
}
