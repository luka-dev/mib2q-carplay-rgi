/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef CARPLAY_LOADER_H
#define CARPLAY_LOADER_H

/*
 * carplay_loader — platform-neutral CarPlay startup animation.
 *
 * The scene owns no window, EGL, Screen or OMX objects.  A consumer supplies a
 * tiny drawing backend, so dio's Qualcomm-32m path and altscreen_render's RGBA
 * path can render the same animation into their own native buffers.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARPLAY_LOADER_FRAME_RATE        30U
#define CARPLAY_LOADER_FRAME_INTERVAL_NS 33333333UL
#define CARPLAY_LOADER_FRAME_INTERVAL_MS 33U /* Convenience for millisecond APIs. */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} carplay_loader_color_t;

typedef struct {
    int width;
    int height;
    void *userdata;

    void (*clear)(void *userdata, carplay_loader_color_t color);
    void (*fill_rect)(void *userdata, int x, int y, int width, int height,
                      carplay_loader_color_t color);
    void (*fill_circle)(void *userdata, int center_x, int center_y, int radius,
                        carplay_loader_color_t color);
} carplay_loader_canvas_t;

/* Shared 1.2-second triangle-wave phase used by native fallback backends. */
unsigned carplay_loader_breathing_level(uint64_t elapsed_ms);

/* Render one deterministic frame. elapsed_ms is time since this loader started. */
int carplay_loader_render(const carplay_loader_canvas_t *canvas,
                          uint64_t elapsed_ms);

/* Reference linear backend. Pixels are byte-ordered R,G,B,A. */
int carplay_loader_render_rgba8888(uint8_t *pixels,
                                   int width,
                                   int height,
                                   size_t stride_bytes,
                                   uint64_t elapsed_ms);

#ifdef __cplusplus
}
#endif

#endif /* CARPLAY_LOADER_H */
