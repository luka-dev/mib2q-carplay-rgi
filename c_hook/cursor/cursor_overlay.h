/*
 * Cursor Overlay — independent QNX Screen window above the CarPlay
 * video plane.  Lets us composite a cursor (or any UI) on top of the
 * decoded NV12 stream WITHOUT writing into the decoder's reference
 * frame, which is what was causing the ghost trail with in-place blit.
 *
 * Lifecycle: the window is owned by the dio_manager process via this
 * LD_PRELOAD library.  It is created lazily on the first OMX
 * fillBufferDone after we already know the CarPlay parent window
 * (tracked in cursor_hook.c::g_carplay_window) — so the overlay only
 * exists when CarPlay is actively producing frames.  When the user
 * unplugs the phone, dio_manager exits, our .so unloads, and the
 * window is torn down with the process.
 *
 * The overlay can also be hidden mid-session via SCREEN_PROPERTY_VISIBLE
 * = 0 — used while the cursor is faded out, to save compositor work.
 */

#ifndef CARPLAY_CURSOR_OVERLAY_H
#define CARPLAY_CURSOR_OVERLAY_H

#include "../framework/common.h"

/* Opaque handle to a screen_window_t (we don't pull in <screen/screen.h>
 * here to keep the public API lightweight). */
typedef void* cursor_overlay_window_handle;

/* Idempotent: only the first call with a non-null carplay_win and
 * positive (w, h) actually creates the window.  Returns 0 on success,
 * -1 if creation failed (one-shot — won't retry).  Safe to call on
 * every frame. */
int  cursor_overlay_try_create(cursor_overlay_window_handle carplay_win,
                               int w, int h);

/* Initial test render: draws a red opaque square at top-left of the
 * overlay so we can visually confirm the window is composited above
 * the CarPlay video plane.  Replace with cursor_overlay_draw_cursor
 * once the prototype is verified. */
void cursor_overlay_render_test(void);

/* Future: draw cursor sprite at (cx, cy) with overall alpha. */
void cursor_overlay_draw_cursor(int cx, int cy, int alpha);

/* Hide the overlay (compositor skips it without us posting empty
 * buffers).  Faster than clearing + posting when cursor faded out. */
void cursor_overlay_set_visible(int visible);

/* Best-effort teardown — runs in module destructor. */
void cursor_overlay_destroy(void);

#endif /* CARPLAY_CURSOR_OVERLAY_H */
