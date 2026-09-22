/* prot_vin.h - read the LIVE VIN on the unit, from independent sources, with a
 * simple quorum so a single edited source can't lie. Used by prot_arm_auto().
 *
 * Sources (best-effort; a missing one is skipped):
 *   /mnt/efs-persist/FEC/FecContainer.fec           active FEC
 *   /mnt/efs-system/backup/FEC/FecContainer.fec     backup FEC
 * Agreement -> that VIN. Disagreement or none -> failure (caller degrades to
 * stock / inert). See docs/build-and-deploy/VIN_PROTECTION.md for the full picture. */
#ifndef PROT_VIN_H
#define PROT_VIN_H

#include <stddef.h>

#define PROT_VIN_LEN 17

/* Write the agreed VIN (17 chars + NUL) into out[18]. Returns:
 *   0  agreement across the sources that were present
 *  -1  no VIN found in any source
 *  -2  sources disagree (do not trust) */
int prot_read_vin(char out[PROT_VIN_LEN + 1]);

/* Extract the first WMI-anchored VIN from a buffer (used internally and testable).
 * Returns 1 and fills vin[18] on success, 0 otherwise. */
int prot_scan_vin(const unsigned char *buf, size_t n, char vin[PROT_VIN_LEN + 1]);

#endif /* PROT_VIN_H */
