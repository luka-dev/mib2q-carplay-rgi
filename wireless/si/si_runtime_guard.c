#include "si_runtime_guard.h"
#include "si_protocol.h"
#include <string.h>
#include "si_candidate_guard.h"

static int pointer_ok(uint32_t p) { return p >= 0x10000u && p < 0xffffe000u && !(p & 3u); }

static int read_word(si_read_memory read_memory, void *ctx, uint32_t va, uint32_t *value) {
    unsigned char b[4];
    if (!pointer_ok(va) || !read_memory(ctx, va, b, sizeof(b))) return 0;
    *value = (uint32_t)b[0] | (uint32_t)b[1] << 8 |
             (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    return 1;
}

int si_guard_candidate(si_read_memory read_memory, void *ctx) {
    unsigned char b[128];
    uint32_t root;
    if (!read_memory) return 0;
    for (size_t i = 0; i < sizeof(si_guard_ranges) / sizeof(si_guard_ranges[0]); ++i) {
        const struct si_guard_range *r = &si_guard_ranges[i];
        for (size_t n = 0; n < r->size;) {
            size_t size = r->size - n;
            if (size > sizeof(b)) size = sizeof(b);
            if (!read_memory(ctx, r->va + (uint32_t)n, b, size) ||
                memcmp(b, r->bytes + n, size)) return 0;
            n += size;
        }
    }
    return read_word(read_memory, ctx, SI_HOOK_DEFAULT_ROOT_VA, &root) && root == 0;
}

int si_guard_ready(si_read_memory read_memory, void *ctx,
                   struct si_ready_snapshot *snapshot) {
    uint32_t c, impl, state, listener, vt, dispatch, again;
    if (!read_memory || !snapshot) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    /* Controller singleton dynsym; ctor stores its DSI implementation at
     * +0xcc4 (0x16637c). Startup pending is 2 (0x1809e8); the completion
     * callback sets 0 (0x180724). Live HU confirms state 0 and listener=c+0xd8.
     * 0x128978's GOT relocation resolves the job-listener array at 0x1c3c90;
     * 0x128dd4/0x128de4 dispatch through listener+4's vtable slot 24. */
    if (!read_word(read_memory, ctx, SI_CONTROLLER_PTR, &c) || !pointer_ok(c) ||
        !read_word(read_memory, ctx, c + SI_CONTROLLER_IMPL_OFF, &impl) || !pointer_ok(impl) ||
        !read_word(read_memory, ctx, impl + SI_IMPL_STATE_OFF, &state) || state != 0 ||
        !read_word(read_memory, ctx, SI_REGISTRATION_ARRAY + 4 * SI_VA_CTRL_AO_ID, &listener) ||
        !pointer_ok(listener) || listener != c + 0xd8u ||
        !read_word(read_memory, ctx, listener + 4, &vt) || !pointer_ok(vt) ||
        !read_word(read_memory, ctx, vt + 24, &dispatch) || dispatch != 0x1653c0u ||
        !read_word(read_memory, ctx, SI_CONTROLLER_PTR, &again) || again != c ||
        !read_word(read_memory, ctx, c + SI_CONTROLLER_IMPL_OFF, &again) || again != impl ||
        !read_word(read_memory, ctx, SI_REGISTRATION_ARRAY + 4 * SI_VA_CTRL_AO_ID, &again) || again != listener ||
        !read_word(read_memory, ctx, impl + SI_IMPL_STATE_OFF, &again) || again != 0) return 0;
    snapshot->controller = c;
    snapshot->impl = impl;
    snapshot->listener = listener;
    return 1;
}
