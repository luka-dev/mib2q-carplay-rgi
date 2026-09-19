/* carplay_prot.h - one-call VIN gate for the CarPlay .so artifacts.
 * Guarded by CARPLAY_PROT; when not defined the callers compile to nothing. */
#ifndef CARPLAY_PROT_H
#define CARPLAY_PROT_H
#ifdef __cplusplus
extern "C" {
#endif
/* Arm once (idempotent) against the live MMX VIN + this .so's own image at
 * self_path. Returns 1 iff this unit's VIN matches the VIN this build was
 * stamped for; 0 otherwise (inert). Fail-open: any error (no/disagreeing VIN,
 * unreadable image, unstamped) -> 0, i.e. stock behavior, never a brick. */
int carplay_prot_active(const char *self_path);
#ifdef __cplusplus
}
#endif
#endif
