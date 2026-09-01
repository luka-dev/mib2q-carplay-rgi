/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#ifndef CLUSTER_SURFACE_H
#define CLUSTER_SURFACE_H
/*
 * cluster_surface — shared managed-window primitive for the MHI2Q cluster
 * renderer (maneuver_render = GL overlay).
 *
 * Both open ONE QNX Screen window and hand it to the DisplayManager as a managed
 * client (screen_manage_window into the "How are you gentlemen?" group) under a
 * numeric SCREEN_PROPERTY_ID_STRING (the displayable id, e.g. 98 / 99).  The DM
 * composites the active cluster context (a dc[] referencing that id) and the MOST
 * encoder captures the composite → VC.  Context routing (which context is live) is
 * Java's job (DisplayManager.switchContext) — this module runs NO dmdt.
 *
 * The one lifecycle concern is disown recovery: the DM owns our window and can
 * disown it on context transitions (it stamps SCREEN_PROPERTY_MANAGER_STRING).
 * cluster_surface_lost() detects loss/disown so the caller can rebuild its
 * EGL surface on a freshly recreated window.
 *
 * Group/manager strings verified by RE:
 *   libdisplayinit.so  display_create_window → screen_manage_window(win,"How are you gentlemen?")
 *   libdm_modMain.so   CScreenHandler stamps MANAGER_STRING="All your base are belong to us!"
 */
#include <screen/screen.h>

typedef struct {
    int id;          /* SCREEN_PROPERTY_ID_STRING value = displayable number (98/99) */
    int width, height;
    int format;      /* SCREEN_FORMAT_* (cluster scanout renderers use RGBA8888=8) */
    int usage;       /* SCREEN_USAGE_* (OPENGL_ES2 for RGBA EGL/GLES renderers)     */
    int nbuffers;
    int transparent; /* 1 = TRANSPARENCY_SOURCE_OVER overlay (maneuver); 0 = opaque  */
} cluster_surface_cfg;

typedef struct cluster_surface cluster_surface_t;

/* Create screen context + managed window + buffers.  NULL on failure. */
cluster_surface_t *cluster_surface_create(const cluster_surface_cfg *cfg);

/* Native handles for the caller's pixel pipeline (eglCreateWindowSurface). */
screen_window_t    cluster_surface_window(cluster_surface_t *s);
screen_context_t   cluster_surface_context(cluster_surface_t *s);

/* Cheap health probe (~every 5 s): 1 = window lost/disowned → caller should tear
 * down its EGL surface, call cluster_surface_recreate(), then rebind. */
int  cluster_surface_lost(cluster_surface_t *s);

/* Destroy the window and open a fresh managed one (same cfg).  Caller rebinds its
 * surface to the new cluster_surface_window() afterwards.  100 ms backoff. 0=ok. */
int  cluster_surface_recreate(cluster_surface_t *s);

/* Destroy window + context and free the handle. */
void cluster_surface_destroy(cluster_surface_t *s);

#endif /* CLUSTER_SURFACE_H */
