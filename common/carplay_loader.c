/*
 * Shared startup scene used while the iPhone negotiates its video streams.
 * Integer-only drawing keeps this usable in the QNX 6.5 hook: no libm, no TLS,
 * no allocation and no platform headers.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "carplay_loader.h"

#include <limits.h>

#define LOADER_ORBIT_POINTS 12
#define LOADER_GRADIENT_BANDS 24

typedef struct {
    int x;
    int y;
} loader_point_t;

/* Unit-circle coordinates in Q10 fixed point, clockwise from 12 o'clock. */
static const loader_point_t k_orbit[LOADER_ORBIT_POINTS] = {
    {    0, -1024 }, {  512, -887 }, {  887, -512 }, { 1024,    0 },
    {  887,   512 }, {  512,  887 }, {    0, 1024 }, { -512,  887 },
    { -887,   512 }, {-1024,    0 }, { -887, -512 }, { -512, -887 }
};

static int loader_min(int a, int b) { return a < b ? a : b; }

static int loader_max(int a, int b) { return a > b ? a : b; }

static int loader_scale(int value, int numerator, int denominator) {
    return (int)(((int64_t)value * numerator) / denominator);
}

static uint8_t loader_lerp_u8(uint8_t from, uint8_t to, unsigned amount) {
    unsigned a = amount > 255U ? 255U : amount;
    return (uint8_t)(((unsigned)from * (255U - a) + (unsigned)to * a + 127U) /
                     255U);
}

static carplay_loader_color_t loader_color(uint8_t r, uint8_t g, uint8_t b) {
    carplay_loader_color_t c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = 255U;
    return c;
}

static carplay_loader_color_t loader_lerp_color(carplay_loader_color_t from,
                                                 carplay_loader_color_t to,
                                                 unsigned amount) {
    carplay_loader_color_t c;
    c.r = loader_lerp_u8(from.r, to.r, amount);
    c.g = loader_lerp_u8(from.g, to.g, amount);
    c.b = loader_lerp_u8(from.b, to.b, amount);
    c.a = loader_lerp_u8(from.a, to.a, amount);
    return c;
}

unsigned carplay_loader_breathing_level(uint64_t elapsed_ms) {
    unsigned pulse = (unsigned)(elapsed_ms % 1200U);
    if (pulse > 600U) pulse = 1200U - pulse;
    return (pulse * 255U) / 600U;
}

static void loader_fill_pill(const carplay_loader_canvas_t *canvas,
                             int center_x, int center_y,
                             int width, int height,
                             carplay_loader_color_t color) {
    int radius = height / 2;
    int body_width = width - height;

    if (radius < 1) radius = 1;
    if (body_width < 0) body_width = 0;
    canvas->fill_rect(canvas->userdata, center_x - body_width / 2,
                      center_y - radius, body_width, radius * 2 + 1, color);
    canvas->fill_circle(canvas->userdata, center_x - body_width / 2,
                        center_y, radius, color);
    canvas->fill_circle(canvas->userdata, center_x + body_width / 2,
                        center_y, radius, color);
}

int carplay_loader_render(const carplay_loader_canvas_t *canvas,
                          uint64_t elapsed_ms) {
    const carplay_loader_color_t bg_top = loader_color(4U, 9U, 14U);
    const carplay_loader_color_t bg_bottom = loader_color(12U, 24U, 33U);
    const carplay_loader_color_t dot_dim = loader_color(22U, 52U, 70U);
    const carplay_loader_color_t dot_bright = loader_color(105U, 211U, 255U);
    const carplay_loader_color_t core_dark = loader_color(7U, 16U, 23U);
    const carplay_loader_color_t core_bright = loader_color(72U, 178U, 226U);
    int min_dimension;
    int center_x;
    int center_y;
    int orbit_radius;
    int dot_radius;
    int core_radius;
    unsigned phase;
    unsigned pulse;
    int i;

    if (!canvas || canvas->width <= 0 || canvas->height <= 0 ||
        !canvas->clear || !canvas->fill_rect || !canvas->fill_circle) {
        return -1;
    }

    min_dimension = loader_min(canvas->width, canvas->height);
    center_x = canvas->width / 2;
    center_y = canvas->height / 2;
    orbit_radius = loader_max(8, loader_scale(min_dimension, 16, 100));
    dot_radius = loader_max(1, loader_scale(min_dimension, 11, 1000));
    core_radius = loader_max(4, loader_scale(min_dimension, 45, 1000));

    /* A small number of bands keeps tiled backends cheap while still avoiding
     * a flat placeholder-looking background. */
    canvas->clear(canvas->userdata, bg_top);
    for (i = 0; i < LOADER_GRADIENT_BANDS; ++i) {
        int y0 = loader_scale(canvas->height, i, LOADER_GRADIENT_BANDS);
        int y1 = loader_scale(canvas->height, i + 1, LOADER_GRADIENT_BANDS);
        unsigned amount = (unsigned)((i * 255) / (LOADER_GRADIENT_BANDS - 1));
        carplay_loader_color_t band = loader_lerp_color(bg_top, bg_bottom, amount);
        canvas->fill_rect(canvas->userdata, 0, y0, canvas->width, y1 - y0, band);
    }

    /* Slow breathing center mark.  A triangle wave avoids float/libm. */
    pulse = carplay_loader_breathing_level(elapsed_ms);

    canvas->fill_circle(canvas->userdata, center_x, center_y,
                        core_radius + dot_radius,
                        loader_lerp_color(dot_dim, core_bright, pulse / 2U));
    canvas->fill_circle(canvas->userdata, center_x, center_y,
                        core_radius, core_dark);
    loader_fill_pill(canvas, center_x, center_y,
                     loader_max(8, loader_scale(min_dimension, 7, 100)),
                     loader_max(3, loader_scale(min_dimension, 18, 1000)),
                     loader_lerp_color(loader_color(78U, 151U, 181U),
                                       dot_bright, pulse));

    /* Twelve-dot comet.  One step every 90 ms gives motion without asking the
     * startup path to redraw at video rate. */
    phase = (unsigned)((elapsed_ms / 90U) % LOADER_ORBIT_POINTS);
    for (i = 0; i < LOADER_ORBIT_POINTS; ++i) {
        unsigned age = (phase + LOADER_ORBIT_POINTS - (unsigned)i) %
                       LOADER_ORBIT_POINTS;
        unsigned strength;
        int radius = dot_radius;
        int x;
        int y;

        if (age == 0U) {
            strength = 255U;
            radius += loader_max(1, dot_radius / 3);
        } else if (age < 8U) {
            strength = 220U - age * 25U;
        } else {
            strength = 18U;
        }

        x = center_x + (k_orbit[i].x * orbit_radius) / 1024;
        y = center_y + (k_orbit[i].y * orbit_radius) / 1024;
        canvas->fill_circle(canvas->userdata, x, y, radius,
                            loader_lerp_color(dot_dim, dot_bright, strength));
    }

    return 0;
}

typedef struct {
    uint8_t *pixels;
    int width;
    int height;
    size_t stride;
} loader_rgba_surface_t;

static void loader_rgba_fill_rect(void *userdata, int x, int y,
                                  int width, int height,
                                  carplay_loader_color_t color) {
    loader_rgba_surface_t *surface = (loader_rgba_surface_t *)userdata;
    int x0 = loader_max(0, x);
    int y0 = loader_max(0, y);
    int x1 = loader_min(surface->width, x + width);
    int y1 = loader_min(surface->height, y + height);
    int py;

    if (x0 >= x1 || y0 >= y1) return;
    for (py = y0; py < y1; ++py) {
        uint8_t *p = surface->pixels + (size_t)py * surface->stride +
                     (size_t)x0 * 4U;
        int px;
        for (px = x0; px < x1; ++px) {
            p[0] = color.r;
            p[1] = color.g;
            p[2] = color.b;
            p[3] = color.a;
            p += 4;
        }
    }
}

static void loader_rgba_clear(void *userdata, carplay_loader_color_t color) {
    loader_rgba_surface_t *surface = (loader_rgba_surface_t *)userdata;
    loader_rgba_fill_rect(userdata, 0, 0, surface->width, surface->height, color);
}

static void loader_rgba_fill_circle(void *userdata, int center_x, int center_y,
                                    int radius,
                                    carplay_loader_color_t color) {
    loader_rgba_surface_t *surface = (loader_rgba_surface_t *)userdata;
    int y0;
    int y1;
    int py;
    int64_t radius_squared;

    if (radius < 0) return;
    y0 = loader_max(0, center_y - radius);
    y1 = loader_min(surface->height - 1, center_y + radius);
    radius_squared = (int64_t)radius * radius;

    for (py = y0; py <= y1; ++py) {
        int64_t dy = (int64_t)py - center_y;
        int x_extent = 0;
        int x0;
        int x1;

        /* Integer square-root by monotonic scan.  Loader circles are small and
         * this runs only during startup, keeping the shared code libm-free. */
        while ((int64_t)(x_extent + 1) * (x_extent + 1) + dy * dy <=
               radius_squared) {
            ++x_extent;
        }
        x0 = loader_max(0, center_x - x_extent);
        x1 = loader_min(surface->width - 1, center_x + x_extent);
        if (x0 <= x1) {
            loader_rgba_fill_rect(userdata, x0, py, x1 - x0 + 1, 1, color);
        }
    }
}

int carplay_loader_render_rgba8888(uint8_t *pixels,
                                   int width,
                                   int height,
                                   size_t stride_bytes,
                                   uint64_t elapsed_ms) {
    loader_rgba_surface_t surface;
    carplay_loader_canvas_t canvas;

    if (!pixels || width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / 4U || stride_bytes < (size_t)width * 4U) {
        return -1;
    }

    surface.pixels = pixels;
    surface.width = width;
    surface.height = height;
    surface.stride = stride_bytes;

    canvas.width = width;
    canvas.height = height;
    canvas.userdata = &surface;
    canvas.clear = loader_rgba_clear;
    canvas.fill_rect = loader_rgba_fill_rect;
    canvas.fill_circle = loader_rgba_fill_circle;
    return carplay_loader_render(&canvas, elapsed_ms);
}
