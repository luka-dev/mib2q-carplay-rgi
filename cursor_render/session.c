#include "session.h"
#include <string.h>

static uint32_t rotate(cursor_session *s) {
    if (++s->token == 0) ++s->token;
    return s->token;
}
static int ack(cursor_session *s, uint8_t *p, uint32_t seq, int status, uint32_t token, uint64_t now) {
    memset(p, 0, CURSOR_ACK_SIZE);
    cursor_put32(p, seq); cursor_put32(p + 4, (uint32_t)status);
    cursor_put32(p + 8, token); cursor_put32(p + 12, CURSOR_LEASE_MS);
    if (status == 0 && s->engine.initialized) {
        unsigned phase = s->open ? s->displayed.mode : CURSOR_HIDE;
        if (s->engine.command.mode == CURSOR_LOADER && s->engine.done) phase = CURSOR_LOADER_DONE;
        cursor_put32(p + 16, phase);
        p[20] = s->displayed.x >> 8; p[21] = s->displayed.x;
        p[22] = s->displayed.y >> 8; p[23] = s->displayed.y;
        cursor_put32(p + 24, s->open ? cursor_engine_click_ttl(&s->engine, now) : 0);
        cursor_put32(p + 28, s->engine.command.revision);
    }
    return 1;
}
void cursor_session_init(cursor_session *s, uint32_t seed) {
    memset(s, 0, sizeof(*s)); s->token = seed | 1u;
}
void cursor_session_reset(cursor_session *s) {
    cursor_plane_close();
    s->awaiting = s->open = 0; s->token_until = s->next_draw = 0;
    cursor_engine_reset(&s->engine); memset(&s->displayed, 0, sizeof(s->displayed));
    rotate(s);
}
int cursor_session_receive(cursor_session *s, const uint8_t *p, size_t len,
                           uint64_t now, uint8_t *reply) {
    cursor_frame f;
    if (cursor_decode(&f, p, len) != 0 || !f.seq) return -1;
    if (f.mode == CURSOR_HIDE || f.mode == CURSOR_PROBE) {
        cursor_session_reset(s);
        s->token_until = now + CURSOR_LEASE_MS;
        return ack(s, reply, f.seq, f.mode == CURSOR_PROBE ? 1 : 0, s->token, now);
    }
    if (!s->token_until || now >= s->token_until || f.token != s->token)
        return ack(s, reply, f.seq, -2, 0, now);
    if (s->awaiting) return -1;
    cursor_engine_apply(&s->engine, &f, now);
    s->pending = f; s->awaiting = 1; s->next_draw = 0;
    return 0;
}
int cursor_session_tick(cursor_session *s, uint64_t now, uint8_t *reply) {
    uint32_t seq = s->pending.seq;
    if (s->token_until && now >= s->token_until) {
        int had_pending = s->awaiting;
        cursor_session_reset(s);
        return had_pending ? ack(s, reply, seq, -2, 0, now) : 0;
    }
    if (!s->engine.initialized || now < s->next_draw) return 0;
    cursor_frame frame;
    cursor_engine_frame(&s->engine, now, &frame);
    if (frame.alpha) {
        if (cursor_plane_open(frame.width, frame.height) != 0) goto failure;
        s->open = 1;
        if (!cursor_plane_managed()) return 0; // Keep receiving cancellation during adoption.
        if (cursor_plane_draw(&frame) != 0) goto failure;
    } else if (s->open) {
        cursor_plane_close(); s->open = 0; // Remove even on a static underlying video.
    }
    s->displayed = frame; s->next_draw = now + 33;
    if (!s->awaiting) return 0;
    s->awaiting = 0; s->token_until = now + CURSOR_LEASE_MS;
    return ack(s, reply, seq, 0, rotate(s), now);
failure: {
    int had_pending = s->awaiting;
    cursor_session_reset(s);
    return had_pending ? ack(s, reply, seq, -1, 0, now) : 0;
    }
}
