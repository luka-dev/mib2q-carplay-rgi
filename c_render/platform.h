/*
 * Platform abstraction for windowing and GL context.
 *
 * macOS: GLFW + OpenGL 2.1
 * QNX:   libdisplayinit.so + EGL + GLES2
 */

#ifndef CR_PLATFORM_H
#define CR_PLATFORM_H

/* Initialize platform window and GL context.
 * Returns 0 on success, -1 on failure. */
int platform_init(int width, int height);

/* Swap buffers (present frame). */
void platform_swap(void);

/* Poll events (keyboard, window close, etc.).
 * Call once per frame. */
void platform_poll(void);

/* Returns 1 if the window should close. */
int platform_should_close(void);

/* Shutdown platform, restore display state. */
void platform_shutdown(void);

/* Get actual framebuffer size (may differ from window size on HiDPI). */
void platform_get_framebuffer_size(int *width, int *height);

/* Get the active routing IDs used by the platform backend.
 * On macOS this returns the protocol defaults. */
void platform_get_routing_ids(int *display_id, int *context_id, int *displayable_id);

/* Ensure the renderer context is active on the target display.
 * QNX uses dmdt gs/sc; other platforms no-op. */
void platform_ensure_focus(void);

/* Re-claim the displayable binding (QNX: screen_manage_window on our
 * window handle).  Defends against another process — native nav's
 * libRenderSystem — re-registering a window with the same ID and
 * stealing our binding in displaymanager's m_surfaceSources.
 * No-op on non-QNX platforms. */
void platform_reclaim_displayable(void);

/* Release the displayable binding back to the native owner (QNX:
 * explicit screen_destroy_window on our window).  Counterpart to
 * platform_reclaim_displayable — lets displaymanager re-bind
 * m_surfaceSources to whatever managed window remains for that ID
 * (typically the native KOMO RG widget's window).
 * Called from platform_shutdown.  No-op on non-QNX platforms. */
void platform_release_displayable(void);

/* Key codes for test navigation */
#define CR_KEY_LEFT   0
#define CR_KEY_RIGHT  1
#define CR_KEY_UP     2
#define CR_KEY_DOWN   3
#define CR_KEY_P      4
#define CR_KEY_SPACE  5
#define CR_KEY_S      6
#define CR_KEY_A      7
#define CR_KEY_LBRACKET 8
#define CR_KEY_RBRACKET 9
#define CR_KEY_D     10
#define CR_KEY_MAX   11

/* Returns 1 if key was tapped since last poll, clears the tap state. */
int platform_key_tap(int key);

#endif /* CR_PLATFORM_H */
