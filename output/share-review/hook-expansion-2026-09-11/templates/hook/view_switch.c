/* Copyright (c) 2026, original project author. */
/* New standalone model; not a verbatim copy of a production command sender. */
#include "view_switch.h"
#include <string.h>

int view_switch_init(view_switch *s, size_t count, size_t initial)
{
    if (!s || !count || initial>=count) return -1;
    memset(s,0,sizeof(*s));
    s->count=count; s->desired=initial; s->applied=VIEW_AREA_UNKNOWN;
    return 0;
}

int view_switch_begin(view_switch *s)
{
    if (!s || !s->count || s->active || s->generation==UINT64_MAX) return -1;
    ++s->generation;
    s->active=1; s->pending=0; s->applied=VIEW_AREA_UNKNOWN;
    return 0;
}

void view_switch_end(view_switch *s)
{
    if (!s) return;
    s->active=0; s->pending=0; s->applied=VIEW_AREA_UNKNOWN;
}

int view_switch_select(view_switch *s, size_t area)
{
    if (!s || area>=s->count) return -1;
    s->desired=area;
    return 0;
}

int view_switch_next(view_switch *s, view_request *out)
{
    if (!s || !out || !s->count) return -1;
    if (!s->active || s->pending || s->desired==s->applied) return 0;
    if (s->serial==UINT64_MAX) return -1;
    s->request.generation=s->generation;
    s->request.serial=++s->serial;
    s->request.area=s->desired;
    s->pending=1;
    *out=s->request;
    return 1;
}

int view_switch_complete(view_switch *s, view_request r, int success)
{
    if (!s || !s->active || !s->pending || r.generation!=s->generation ||
        r.serial!=s->request.serial || r.area!=s->request.area) return 0;
    s->pending=0;
    s->applied=success ? r.area : VIEW_AREA_UNKNOWN;
    return 1;
}
