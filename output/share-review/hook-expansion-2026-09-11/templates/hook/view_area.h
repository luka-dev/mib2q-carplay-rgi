/* Copyright (c) 2026, original project author. */
#ifndef SHARE_VIEW_AREA_H
#define SHARE_VIEW_AREA_H
#include <stddef.h>
#include <stdint.h>

typedef struct { int32_t x, y, width, height; } view_rect;
typedef struct { view_rect visible, safe; } view_area;

/* Same pixel coordinate system for both rectangles: origin at the top left,
 * x increases right, y increases down; right/bottom edges are exclusive.
 * Positive sizes, nonnegative origins, safe contained within visible.
 * Validation is deliberately a local contract, not protocol negotiation.
 */
int view_area_valid(const view_area *area);
/* Return 0 on success, -1 on invalid input; leave *out unchanged on failure. */
int view_area_init(view_area *out, view_rect visible, view_rect safe);
int view_area_from_insets(view_area *out, int32_t width, int32_t height,
                          int32_t left, int32_t top, int32_t right, int32_t bottom);

/* Diagnostic JSON using the rectangle keys from the selected local builder.
 * Returns required bytes excluding NUL, or -1 for invalid input/format error.
 * A result >= capacity means truncation. NULL is allowed only with capacity 0.
 * This is a debug representation, not a transport encoder or sender.
 */
int view_area_write_json(const view_area *area, char *out, size_t capacity);
#endif
