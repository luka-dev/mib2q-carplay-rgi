#include "si_envv.h"
int si_envv_child_mode(int transport, int wireless_enabled, int admitted) {
    if (transport==0) return 0;
    return transport==1 && wireless_enabled && admitted ? 1:-1;
}

#include <string.h>

static int starts_with_name(const char *entry, const char *prefix) {
    /* match '<prefix>' against the NAME part of 'NAME=VALUE' only */
    size_t n = strlen(prefix);
    if (n == 0) return 0;
    if (strncmp(entry, prefix, n) != 0) return 0;
    return entry[n] == '=' || entry[n] == '_' || entry[n] == '\0';
}

int si_envv_path_matches(const char *path, const char *substring) {
    if (!path || !substring || !*substring) return 0;
    if (strchr(substring, '/')) return strcmp(path, substring) == 0;
    const char *base = strrchr(path, '/');
    return strcmp(base ? base + 1 : path, substring) == 0;
}

int si_envv_apply(char *const *envp,
                  const char *drop_prefix,
                  int inject,
                  const char *const *inject_entries, int inject_count,
                  char *arena, size_t arena_size,
                  const char **out_entries, int *out_count,
                  int *dropped, int *appended) {
    size_t arena_used = 0;
    int count = 0, drop = 0, app = 0;

    if (!envp || !out_entries || !out_count || !dropped || !appended) return -1;
    out_entries[0] = NULL;
    *out_count = *dropped = *appended = 0;
    if (inject_count < 0 || inject_count > SI_ENVV_MAX_ENTRIES) return -1;
    if (inject && inject_count > 0 && !inject_entries) return -1;
    if (inject && inject_count > 0 && !arena) return -1;

    if (inject) for (int j = 0; j < inject_count; ++j) {
        const char *s = inject_entries[j];
        const char *eq = s ? strchr(s, '=') : NULL;
        if (!eq || eq == s) return -1;
        for (const char *p = s; p < eq; ++p)
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  *p == '_' || (p > s && *p >= '0' && *p <= '9'))) return -1;
        for (int k = 0; k < j; ++k)
            if (strncmp(s, inject_entries[k], (size_t)(eq - s) + 1) == 0) return -1;
        if (drop_prefix && starts_with_name(s, drop_prefix)) return -1;
    }

    for (int i = 0; envp[i] != NULL; ++i) {
        if (i >= SI_ENVV_MAX_ENTRIES) return -1;
        if (drop_prefix && starts_with_name(envp[i], drop_prefix)) {
            ++drop;
            continue;
        }
        int replaced = 0;
        if (inject) for (int j = 0; j < inject_count; ++j) {
            size_t key_len = (size_t)(strchr(inject_entries[j], '=') - inject_entries[j]) + 1;
            if (strncmp(envp[i], inject_entries[j], key_len) == 0) replaced = 1;
        }
        if (replaced) { ++drop; continue; }
        out_entries[count++] = envp[i];
    }
    if (inject && inject_count > 0) {
        for (int j = 0; j < inject_count; ++j) {
            const char *src = inject_entries[j];
            if (!src) return -1;
            size_t len = strlen(src) + 1;
            if (count >= SI_ENVV_MAX_ENTRIES) return -1;
            if (arena_used + len > arena_size) return -1;
            memcpy(arena + arena_used, src, len);
            arena_used += len;
            out_entries[count++] = arena + (arena_used - len);
            ++app;
        }
    }
    out_entries[count] = NULL;
    *out_count = count;
    *dropped = drop;
    *appended = app;
    return 0;
}
