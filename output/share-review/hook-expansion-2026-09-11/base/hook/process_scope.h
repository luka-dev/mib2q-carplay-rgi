/* Copyright (c) 2026, original project author. */
#ifndef EXCERPT_PROCESS_SCOPE_H
#define EXCERPT_PROCESS_SCOPE_H

enum {
    HOOK_PROCESS_UNKNOWN = -1,
    HOOK_PROCESS_OTHER = 0,
    HOOK_PROCESS_MATCH = 1
};

/* Match a nonempty basename against a NUL-terminated executable path.
 * No substring match: "example_host_helper" is not "example_host".
 * Invalid arguments return UNKNOWN. Paths use '/' as their separator.
 */
int hook_process_name_matches(const char *executable, const char *expected_name);

/* Best-effort first-argument probe on systems exposing /proc/self/cmdline.
 * Unavailable, empty or truncated input returns UNKNOWN, not MATCH.
 * argv[0] is caller-controlled: this is an operational filter, not an
 * authentication or security boundary. The caller chooses UNKNOWN policy.
 * No production process name is embedded in this helper.
 */
int hook_process_matches(const char *expected_name);
#endif
