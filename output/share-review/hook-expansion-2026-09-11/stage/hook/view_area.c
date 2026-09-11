/* Copyright (c) 2026, original project author. */
/* Portable adaptation of the visible-rectangle + nested-safe-rectangle builder.
 * Validation and diagnostic formatting are new; platform objects are omitted.
 */
#include "view_area.h"
#include <inttypes.h>
#include <stdio.h>

int view_area_valid(const view_area *area)
{
    const view_rect *v, *s;
    if (!area) return 0;
    v=&area->visible; s=&area->safe;
    if (v->x<0 || v->y<0 || v->width<=0 || v->height<=0 ||
        s->x<v->x || s->y<v->y || s->width<=0 || s->height<=0) return 0;
    return (int64_t)s->x+s->width <= (int64_t)v->x+v->width &&
           (int64_t)s->y+s->height <= (int64_t)v->y+v->height;
}

int view_area_init(view_area *out, view_rect visible, view_rect safe)
{
    view_area candidate={visible,safe};
    if (!out || !view_area_valid(&candidate)) return -1;
    *out=candidate;
    return 0;
}

int view_area_from_insets(view_area *out, int32_t width, int32_t height,
                          int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    view_rect visible={0,0,width,height}, safe;
    if (width<=0 || height<=0 || left<0 || top<0 || right<0 || bottom<0 ||
        (int64_t)left+right>=width || (int64_t)top+bottom>=height) return -1;
    safe.x=left; safe.y=top;
    safe.width=width-left-right; safe.height=height-top-bottom;
    return view_area_init(out,visible,safe);
}

int view_area_write_json(const view_area *area, char *out, size_t capacity)
{
    const view_rect *v, *s;
    if ((!out && capacity) || !view_area_valid(area)) {
        if (out && capacity) out[0]='\0';
        return -1;
    }
    v=&area->visible; s=&area->safe;
    return snprintf(out,capacity,
        "{\"originXPixels\":%" PRId32 ",\"originYPixels\":%" PRId32
        ",\"widthPixels\":%" PRId32 ",\"heightPixels\":%" PRId32
        ",\"safeArea\":{\"originXPixels\":%" PRId32 ",\"originYPixels\":%" PRId32
        ",\"widthPixels\":%" PRId32 ",\"heightPixels\":%" PRId32 "}}",
        v->x,v->y,v->width,v->height,s->x,s->y,s->width,s->height);
}
