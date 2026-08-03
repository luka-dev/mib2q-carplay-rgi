/*
 * Qualcomm 32m presentation without changing OMX's allocator:
 *
 *   tiled Screen WINDOW buffer (OMX output)
 *      -> screen_post_window(staging)
 *      -> screen_flush_context(SCREEN_WAIT_IDLE)
 *      -> screen_read_window(..., Screen-owned RGBA window buffer)
 *      -> screen_post_window(RGBA plane 99)
 *
 * Stock libairplay 210 uses Screen window buffers as OMX output.  Do not replace
 * them with separately allocated pixmaps: the APQ8064 decoder cannot complete
 * Loaded->Idle with those allocations and may starve dio_manager's OMX pool.
 */
#include "screen_readback_present.h"
#include "cluster_surface.h"

#include <screen/screen.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RB_DISPLAYABLE_ID       99
#define RB_RGBA_BUFFER_COUNT     3
#define RB_QCOM_TILED_NV12       0x1000c
#define RB_QCOM_VIDEO_USAGE      0x484
#define RB_STAGING_ID            "AA_ALTSCREEN_STAGING"

/* QNX 6.5 headers differ slightly between SDK/BSP revisions. */
#ifndef SCREEN_WAIT_IDLE
#define SCREEN_WAIT_IDLE (1 << 0)
#endif

typedef int (*rb_read_window_fn)(screen_window_t, screen_buffer_t, int,
                                 const int *, int);

struct screen_readback_present {
    int width;
    int height;
    unsigned int source_count;
    unsigned int rgba_count;
    unsigned int next_rgba;

    cluster_surface_t *rgba_surface;
    screen_context_t context;
    screen_window_t rgba_window;
    screen_window_t staging_window;

    screen_buffer_t *source_buffers;
    void **source_pointers;
    screen_buffer_t *rgba_buffers;
    void **rgba_pointers;
    int *rgba_strides;

    rb_read_window_fn read_window;
    unsigned long frames;
    unsigned long failures;
    int first_success_logged;
};

static uint32_t rb_sample_hash(const screen_readback_present_t *p,
                               unsigned int index) {
    const uint8_t *base;
    uint32_t hash = 2166136261u;
    int y, x, stride;
    if (!p || index >= p->rgba_count || !p->rgba_pointers[index]) return 0;
    base = (const uint8_t *)p->rgba_pointers[index];
    stride = p->rgba_strides[index];
    if (stride <= 0) stride = p->width * 4;

    /* Sparse deterministic sample: diagnostic only, never on the hot pixel path. */
    for (y = 0; y < p->height; y += (p->height / 8) + 1) {
        const uint8_t *row = base + (size_t)y * (size_t)stride;
        for (x = 0; x < p->width * 4; x += ((p->width * 4) / 16) + 1) {
            hash ^= row[x];
            hash *= 16777619u;
        }
    }
    return hash;
}

static int rb_get_window_buffers(screen_window_t window, unsigned int count,
                                 screen_buffer_t **out_buffers, void ***out_pointers,
                                 int **out_strides, int require_pointers,
                                 const char *label) {
    screen_buffer_t *buffers;
    void **pointers;
    int *strides = NULL;
    unsigned int i;

    buffers = (screen_buffer_t *)calloc(count, sizeof(*buffers));
    pointers = (void **)calloc(count, sizeof(*pointers));
    if (out_strides) strides = (int *)calloc(count, sizeof(*strides));
    if (!buffers || !pointers || (out_strides && !strides)) goto fail;

    if (screen_get_window_property_pv(window, SCREEN_PROPERTY_RENDER_BUFFERS,
                                      (void **)buffers) != 0) {
        fprintf(stderr, "altscreen_render[readback]: %s render buffers failed errno=%d\n",
                label, errno);
        goto fail;
    }
    for (i = 0; i < count; ++i) {
        if (screen_get_buffer_property_pv(buffers[i], SCREEN_PROPERTY_POINTER,
                                          &pointers[i]) != 0 || !pointers[i]) {
            pointers[i] = NULL;
            if (!require_pointers) {
                fprintf(stderr,
                        "altscreen_render[readback]: %s buffer %u pointer unavailable; checksum disabled\n",
                        label, i);
                continue;
            }
            fprintf(stderr,
                    "altscreen_render[readback]: %s buffer %u pointer failed errno=%d\n",
                    label, i, errno);
            goto fail;
        }
        if (strides && pointers[i] &&
            screen_get_buffer_property_iv(buffers[i], SCREEN_PROPERTY_STRIDE,
                                          &strides[i]) != 0) {
            if (!require_pointers) {
                strides[i] = 0;
                continue;
            }
            fprintf(stderr,
                    "altscreen_render[readback]: %s buffer %u stride failed errno=%d\n",
                    label, i, errno);
            goto fail;
        }
    }

    *out_buffers = buffers;
    *out_pointers = pointers;
    if (out_strides) *out_strides = strides;
    return 0;

fail:
    free(buffers);
    free(pointers);
    free(strides);
    return -1;
}

static int rb_create_staging(screen_readback_present_t *p) {
    int format = RB_QCOM_TILED_NV12;
    int usage = RB_QCOM_VIDEO_USAGE;
    int size[2] = { p->width, p->height };
    int manage_rc;

    if (screen_create_window(&p->staging_window, p->context) != 0) {
        fprintf(stderr, "altscreen_render[readback]: create staging window failed errno=%d\n",
                errno);
        return -1;
    }
#define RB_REQUIRED(call, label) do {                                             \
        errno = 0;                                                               \
        if ((call) != 0) {                                                       \
            fprintf(stderr, "altscreen_render[readback]: %s failed errno=%d\n", \
                    label, errno);                                               \
            return -1;                                                          \
        }                                                                        \
    } while (0)
    RB_REQUIRED(screen_set_window_property_cv(p->staging_window,
                                               SCREEN_PROPERTY_ID_STRING,
                                               (int)strlen(RB_STAGING_ID),
                                               RB_STAGING_ID), "set staging ID");
    RB_REQUIRED(screen_set_window_property_iv(p->staging_window,
                                               SCREEN_PROPERTY_FORMAT,
                                               &format), "set staging FORMAT");
    RB_REQUIRED(screen_set_window_property_iv(p->staging_window,
                                               SCREEN_PROPERTY_USAGE,
                                               &usage), "set staging USAGE");
    RB_REQUIRED(screen_set_window_property_iv(p->staging_window,
                                               SCREEN_PROPERTY_SIZE,
                                               size), "set staging SIZE");
    RB_REQUIRED(screen_set_window_property_iv(p->staging_window,
                                               SCREEN_PROPERTY_BUFFER_SIZE,
                                               size), "set staging BUFFER_SIZE");

    /* The nonnumeric ID prevents collision with a displayable.  mh2p-cluster's
     * working readback path registers its staging window the same way.  Some BSP
     * revisions do not require management for an offscreen source, so log but do
     * not reject a management failure. */
    errno = 0;
    manage_rc = screen_manage_window(p->staging_window, "How are you gentlemen?");
    fprintf(stderr,
            "altscreen_render[readback]: staging manage rc=%d errno=%d id=%s\n",
            manage_rc, manage_rc ? errno : 0, RB_STAGING_ID);

    RB_REQUIRED(screen_create_window_buffers(p->staging_window,
                                              (int)p->source_count),
                "create staging buffers");
#undef RB_REQUIRED

    if (rb_get_window_buffers(p->staging_window, p->source_count,
                              &p->source_buffers, &p->source_pointers,
                              NULL, 1, "staging") != 0) return -1;
    return 0;
}

screen_readback_present_t *screen_readback_present_create(int width, int height,
                                                           unsigned int source_count) {
    screen_readback_present_t *p;
    cluster_surface_cfg cfg;

    if (width <= 0 || height <= 0 || source_count == 0) return NULL;
    p = (screen_readback_present_t *)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->width = width;
    p->height = height;
    p->source_count = source_count;
    p->rgba_count = RB_RGBA_BUFFER_COUNT;

    p->read_window = (rb_read_window_fn)dlsym(RTLD_DEFAULT, "screen_read_window");
    if (!p->read_window) {
        fprintf(stderr,
                "altscreen_render[readback]: screen_read_window unavailable; refusing unsafe fallback\n");
        goto fail;
    }

    memset(&cfg, 0, sizeof(cfg));
    cfg.id = RB_DISPLAYABLE_ID;
    cfg.width = width;
    cfg.height = height;
    cfg.format = SCREEN_FORMAT_RGBA8888;
    /* NATIVE is required by screen_read_window. READ permits the one-time
     * checksum diagnostic; WRITE is the compositor destination; OPENGL_ES2
     * retains the already-proven RGBA allocation class for plane 99. */
    cfg.usage = SCREEN_USAGE_READ | SCREEN_USAGE_WRITE |
                SCREEN_USAGE_NATIVE | SCREEN_USAGE_OPENGL_ES2;
    cfg.nbuffers = (int)p->rgba_count;
    cfg.transparent = 0;
    p->rgba_surface = cluster_surface_create(&cfg);
    if (!p->rgba_surface) goto fail;
    p->context = cluster_surface_context(p->rgba_surface);
    p->rgba_window = cluster_surface_window(p->rgba_surface);

    if (rb_get_window_buffers(p->rgba_window, p->rgba_count,
                              &p->rgba_buffers, &p->rgba_pointers,
                              &p->rgba_strides, 0, "RGBA") != 0) goto fail;
    if (rb_create_staging(p) != 0) goto fail;

    fprintf(stderr,
            "altscreen_render[readback]: ready %dx%d, %u tiled OMX window buffers -> %u RGBA plane-99 buffers\n",
            width, height, source_count, p->rgba_count);
    return p;

fail:
    screen_readback_present_destroy(p);
    return NULL;
}

void *screen_readback_present_source_pointer(screen_readback_present_t *p,
                                              unsigned int index) {
    if (!p || index >= p->source_count) return NULL;
    return p->source_pointers[index];
}

int screen_readback_present_frame(screen_readback_present_t *p,
                                  unsigned int source_index,
                                  unsigned int filled_len) {
    unsigned int dst;
    int rect[4];
    int rc;
    uint32_t hash;
    const char *stage = "validate";

    if (!p || source_index >= p->source_count || filled_len == 0) return -1;
    rect[0] = 0;
    rect[1] = 0;
    rect[2] = p->width;
    rect[3] = p->height;

    errno = 0;
    stage = "post tiled staging";
    rc = screen_post_window(p->staging_window, p->source_buffers[source_index],
                            1, rect, 0);
    if (rc != 0) goto fail;
    stage = "flush tiled staging";
    rc = screen_flush_context(p->context, SCREEN_WAIT_IDLE);
    if (rc != 0) goto fail;

    dst = p->next_rgba++ % p->rgba_count;
    errno = 0;
    stage = "screen_read_window";
    rc = p->read_window(p->staging_window, p->rgba_buffers[dst], 0, NULL, 0);
    if (rc != 0) goto fail;

    hash = rb_sample_hash(p, dst);
    errno = 0;
    stage = "post RGBA plane 99";
    rc = screen_post_window(p->rgba_window, p->rgba_buffers[dst], 1, rect, 0);
    if (rc != 0) goto fail;
    p->frames++;

    if (!p->first_success_logged) {
        p->first_success_logged = 1;
        fprintf(stderr,
                "altscreen_render[readback]: READBACK PATH OK: Qcom 32m window -> screen_read_window -> RGBA8888 plane 99 (%dx%d hash=%08x)\n",
                p->width, p->height, (unsigned)hash);
    } else if ((p->frames % 120) == 0) {
        fprintf(stderr,
                "altscreen_render[readback]: %lu RGBA frames posted (last hash=%08x)\n",
                p->frames, (unsigned)hash);
    }
    return 0;

fail:
    p->failures++;
    if (p->failures <= 3 || (p->failures % 120) == 0) {
        fprintf(stderr,
                "altscreen_render[readback]: presentation FAILED #%lu stage='%s' rc=%d errno=%d src=%u filled=%lu\n",
                p->failures, stage, rc, errno, source_index,
                (unsigned long)filled_len);
    }
    return -1;
}

void screen_readback_present_destroy(screen_readback_present_t *p) {
    if (!p) return;
    /* Staging buffers are OMX output allocations.  The sink frees all OMX
     * headers before calling us, so destroying this window is now safe. */
    if (p->staging_window) {
        screen_destroy_window(p->staging_window);
        p->staging_window = NULL;
    }
    free(p->source_buffers);
    free(p->source_pointers);
    free(p->rgba_buffers);
    free(p->rgba_pointers);
    free(p->rgba_strides);
    if (p->rgba_surface) {
        cluster_surface_destroy(p->rgba_surface);
        p->rgba_surface = NULL;
    }
    free(p);
}
