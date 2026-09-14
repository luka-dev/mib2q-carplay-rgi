#include "cursor_plane.h"
#include <string.h>

static unsigned u16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }
static uint32_t u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
void cursor_put32(uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

int cursor_decode(cursor_frame *f, const uint8_t *p, size_t len) {
    if (!p || len != CURSOR_FRAME_SIZE || p[0] != 2 || p[1] > CURSOR_LOADER
            || u16(p + 2) != CURSOR_DISPLAYABLE || p[29] || p[30] || p[31]) return -1;
    memset(f, 0, sizeof(*f));
    f->mode = p[1]; f->seq = u32(p + 4); f->token = u32(p + 8);
    if (f->mode < CURSOR_SHOW) return 0;
    f->width = u16(p + 12); f->height = u16(p + 14);
    f->vx = u16(p + 16); f->vy = u16(p + 18);
    f->vw = u16(p + 20); f->vh = u16(p + 22);
    f->x = u16(p + 24); f->y = u16(p + 26); f->flags = p[28];
    f->activity = u32(p + 32); f->revision = u32(p + 36);
    if (f->width < 320 || f->width > 1920 || f->height < 200 || f->height > 1080
            || !f->vw || !f->vh || f->vx + f->vw > f->width || f->vy + f->vh > f->height
            || (f->mode == CURSOR_SHOW && (f->x >= f->vw || f->y >= f->vh || f->flags > CURSOR_INPUT_FORCE_HIDE))) return -1;
    if (f->mode == CURSOR_LOADER && f->flags > 1) return -1;
    return 0;
}
