#ifndef CARPLAY_CURSOR_PLANE_H
#define CARPLAY_CURSOR_PLANE_H
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#define CURSOR_DISPLAYABLE 103
#define CURSOR_FRAME_SIZE 40
#define CURSOR_ACK_SIZE 32
#define CURSOR_LEASE_MS 500
#define CURSOR_GLASS_EXTENT 112
enum { CURSOR_HIDE = 0, CURSOR_PROBE = 1, CURSOR_SHOW = 2, CURSOR_LOADER = 3 };

enum { CURSOR_INPUT_IDLE = 0, CURSOR_INPUT_TOUCH = 1, CURSOR_INPUT_RELEASED = 2, CURSOR_INPUT_DISMISS = 3, CURSOR_INPUT_FORCE_HIDE = 4 };
#define CURSOR_LOADER_DONE 4
typedef struct {
    uint32_t seq, token, activity, revision;
    unsigned flags;
    unsigned mode, width, height, vx, vy, vw, vh, x, y, alpha;
    float motion_x, motion_y, light_phase; // Native scene uniforms, never transmitted by Java.
} cursor_frame;
int cursor_decode(cursor_frame *f, const uint8_t *data, size_t len);
void cursor_put32(uint8_t *p, uint32_t v);
int cursor_plane_open(unsigned width, unsigned height);
int cursor_plane_managed(void);
int cursor_plane_draw(const cursor_frame *f);
void cursor_plane_close(void);
#endif
