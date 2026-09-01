/*
 * cluster_surface — managed QNX Screen window for the cluster renderers.
 * See cluster_surface.h.  Uses screen_* directly (link -lscreen).  QNX 6.5
 * ARMv7 gcc: no __thread; plain struct + fprintf.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "cluster_surface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* DM manager group we request + the MANAGER_STRING the DM stamps once it adopts
 * us.  Both verified by RE (see cluster_surface.h). */
#define CS_MANAGE_GROUP    "How are you gentlemen?"
#define CS_MANAGER_STRING  "All your base are belong to us!"

/* Property / value numbers not present in every QNX 6.5 screen.h — verified
 * stable by RE (maneuver_render used the same literals). */
#ifndef SCREEN_PROPERTY_TRANSPARENCY
#define SCREEN_PROPERTY_TRANSPARENCY     17
#endif
#ifndef SCREEN_PROPERTY_MANAGER_STRING
#define SCREEN_PROPERTY_MANAGER_STRING   152
#endif
#ifndef SCREEN_TRANSPARENCY_SOURCE_OVER
#define SCREEN_TRANSPARENCY_SOURCE_OVER  2
#endif

struct cluster_surface {
    cluster_surface_cfg cfg;
    screen_context_t    ctx;
    screen_window_t     win;
    int                 expected;      /* had a window at least once            */
    int                 manager_seen;  /* observed CS_MANAGER_STRING at least once */
    struct timespec     last_recreate;
};

/* Create + configure + manage + buffer a fresh window on s->ctx.  0 on success. */
static int cs_open_window(cluster_surface_t *s) {
    cluster_surface_cfg *c = &s->cfg;
    int visible = 1, size[2];
    char idstr[16];

    if (screen_create_window(&s->win, s->ctx) != 0) {
        fprintf(stderr, "cluster_surface: screen_create_window failed errno=%d\n", errno);
        s->win = NULL;
        return -1;
    }

    snprintf(idstr, sizeof(idstr), "%d", c->id);
#define CS_REQUIRED(call, label) do {                                            \
        errno = 0;                                                               \
        if ((call) != 0) {                                                       \
            fprintf(stderr, "cluster_surface: %s failed errno=%d\n", label, errno); \
            goto fail_window;                                                    \
        }                                                                        \
    } while (0)
    CS_REQUIRED(screen_set_window_property_cv(s->win, SCREEN_PROPERTY_ID_STRING,
                                               strlen(idstr), idstr), "set ID_STRING");
    CS_REQUIRED(screen_set_window_property_iv(s->win, SCREEN_PROPERTY_VISIBLE, &visible),
                "set VISIBLE");
    CS_REQUIRED(screen_set_window_property_iv(s->win, SCREEN_PROPERTY_FORMAT, &c->format),
                "set FORMAT");
    CS_REQUIRED(screen_set_window_property_iv(s->win, SCREEN_PROPERTY_USAGE, &c->usage),
                "set USAGE");
    size[0] = c->width; size[1] = c->height;
    CS_REQUIRED(screen_set_window_property_iv(s->win, SCREEN_PROPERTY_SIZE, size), "set SIZE");
    CS_REQUIRED(screen_set_window_property_iv(s->win, SCREEN_PROPERTY_BUFFER_SIZE, size),
                "set BUFFER_SIZE");

    if (c->transparent) {
        int t = SCREEN_TRANSPARENCY_SOURCE_OVER;
        CS_REQUIRED(screen_set_window_property_iv(s->win, SCREEN_PROPERTY_TRANSPARENCY, &t),
                    "set TRANSPARENCY");
    }

    /* Managed-client handshake — DM adopts us into m_surfaceSources[id]. */
    CS_REQUIRED(screen_manage_window(s->win, CS_MANAGE_GROUP), "screen_manage_window");

    if (screen_create_window_buffers(s->win, c->nbuffers) != 0) {
        fprintf(stderr, "cluster_surface: create_window_buffers failed errno=%d\n", errno);
        goto fail_window;
    }

    fprintf(stderr, "cluster_surface: window id=%d %dx%d fmt=%d usage=0x%x nbuf=%d managed\n",
            c->id, c->width, c->height, c->format, c->usage, c->nbuffers);
#undef CS_REQUIRED
    return 0;

fail_window:
#undef CS_REQUIRED
    screen_destroy_window(s->win);
    s->win = NULL;
    return -1;
}

cluster_surface_t *cluster_surface_create(const cluster_surface_cfg *cfg) {
    cluster_surface_t *s;
    if (!cfg) return NULL;
    s = (cluster_surface_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->cfg = *cfg;

    if (screen_create_context(&s->ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        fprintf(stderr, "cluster_surface: screen_create_context failed errno=%d\n", errno);
        free(s);
        return NULL;
    }
    if (cs_open_window(s) != 0) {
        screen_destroy_context(s->ctx);
        free(s);
        return NULL;
    }
    s->expected = 1;
    return s;
}

screen_window_t  cluster_surface_window(cluster_surface_t *s)  { return s ? s->win : NULL; }
screen_context_t cluster_surface_context(cluster_surface_t *s) { return s ? s->ctx : NULL; }

int cluster_surface_lost(cluster_surface_t *s) {
    int visible = -1;
    char mgr[64];

    if (!s || !s->expected) return 0;
    if (!s->win) return 1;                 /* a previous recreate failed */

    /* Probe 1: our window struct is still valid in this process. */
    errno = 0;
    if (screen_get_window_property_iv(s->win, SCREEN_PROPERTY_VISIBLE, &visible) != 0) {
        int e = errno;
        if (e == ENOENT || e == EBADF || e == EINVAL) {
            fprintf(stderr, "cluster_surface: window invalidated errno=%d\n", e);
            return 1;
        }
    }

    /* Probe 2: still in the DM managed group.  Only treat a mismatch as a genuine
     * disown after we have seen the expected value at least once (some drivers may
     * not populate MANAGER_STRING — never disown on that alone). */
    mgr[0] = '\0';
    errno = 0;
    if (screen_get_window_property_cv(s->win, SCREEN_PROPERTY_MANAGER_STRING,
                                      (int)sizeof(mgr) - 1, mgr) == 0) {
        if (strcmp(mgr, CS_MANAGER_STRING) == 0) {
            s->manager_seen = 1;
        } else if (s->manager_seen) {
            fprintf(stderr, "cluster_surface: disowned (manager='%s')\n", mgr);
            return 1;
        }
    }
    return 0;
}

int cluster_surface_recreate(cluster_surface_t *s) {
    struct timespec now;
    long dms;

    if (!s) return -1;

    /* Backoff: at most one recreate per 100 ms (avoid tit-for-tat flapping). */
    clock_gettime(CLOCK_MONOTONIC, &now);
    dms = (long)(now.tv_sec - s->last_recreate.tv_sec) * 1000
        + (long)(now.tv_nsec - s->last_recreate.tv_nsec) / 1000000;
    if (s->last_recreate.tv_sec != 0 && dms < 100) return -1;
    s->last_recreate = now;

    if (s->win) { screen_destroy_window(s->win); s->win = NULL; }
    /* Adoption is asynchronous for each new native window.  Do not compare its
     * initially-empty manager string with the previous window's observed owner. */
    s->manager_seen = 0;
    return cs_open_window(s);
}

void cluster_surface_destroy(cluster_surface_t *s) {
    if (!s) return;
    if (s->win) { screen_destroy_window(s->win); s->win = NULL; }
    if (s->ctx) { screen_destroy_context(s->ctx); s->ctx = NULL; }
    free(s);
}
