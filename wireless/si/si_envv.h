/* Portable envv editing for the SI adapter spawn interposer.
 * Pure C, no QNX or host assumptions; unit-tested natively (see
 * wireless/si/test_si_stage1.py). Boundaries are hard: the output array and
 * strings live in caller-provided arenas, and overflow is an error, never a
 * truncation. */
#ifndef MHI2Q_SI_ENVV_H
#define MHI2Q_SI_ENVV_H

#include <stddef.h>

#define SI_ENVV_MAX_ENTRIES 128

/* One edit pass over an envp array.
 *   drop_prefix: remove this NAME and its NAME_* namespace. NULL/"" disables.
 *   inject:      replace all existing occurrences of each injected NAME.
 *                Duplicate/malformed injected names are rejected.
 *   arena/arena_size: backing storage for rewritten entries (remove is
 *                pointer-only; only injection copies into the arena).
 * Output: out_entries (NULL-terminated) and the number of dropped/injected
 * items. Returns 0 on success, -1 on overflow or malformed input. Every
 * output entry is either a pointer from envp or a NUL-terminated string in
 * the arena — ownership never crosses arenas. */
int si_envv_apply(char *const *envp,
                  const char *drop_prefix,
                  int inject,
                  const char *const *inject_entries, int inject_count,
                  char *arena, size_t arena_size,
                  const char **out_entries, int *out_count,
                  int *dropped, int *appended);

/* Exact path when configured with '/', otherwise exact basename. */
int si_envv_path_matches(const char *path, const char *substring);
/* Native transport and admission are independent. Reject a stale WLAN
 * spawn instead of selecting wired when its admission has disappeared. */
int si_envv_child_mode(int transport, int wireless_enabled, int admitted);

#endif
