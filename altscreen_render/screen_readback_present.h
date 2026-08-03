#ifndef ALTSCREEN_SCREEN_READBACK_PRESENT_H
#define ALTSCREEN_SCREEN_READBACK_PRESENT_H

typedef struct screen_readback_present screen_readback_present_t;

/*
 * Allocate decoder-native Qualcomm 32m Screen WINDOW buffers plus a separate
 * managed RGBA8888 cluster window.  The returned pointers belong to the tiled
 * window buffers and are the only pointers that may be passed to OMX_UseBuffer.
 */
screen_readback_present_t *screen_readback_present_create(int width, int height,
                                                           unsigned int source_count);
void *screen_readback_present_source_pointer(screen_readback_present_t *p,
                                              unsigned int index);

/* Blocking post/readback/post sequence.  The caller must not recycle the OMX
 * source buffer until this returns.  0 means Screen completed the RGBA readback
 * and accepted the plane-99 post; -1 is a logged presentation failure. */
int screen_readback_present_frame(screen_readback_present_t *p,
                                  unsigned int source_index,
                                  unsigned int filled_len);

void screen_readback_present_destroy(screen_readback_present_t *p);

#endif /* ALTSCREEN_SCREEN_READBACK_PRESENT_H */
