/* Copyright (c) 2026, original project author. */
#ifndef SHARE_OUTPUT_BACKEND_H
#define SHARE_OUTPUT_BACKEND_H
#include "OMX_Core.h"
#include "OMX_Component.h"

typedef struct output_backend output_backend_t;
typedef struct {
    const char *component_name;
    int width, height;
    OMX_U32 input_port, output_port;
} output_config;

/* Implement these functions for your own platform. No implementation is
 * included. No component name, vendor extension, pixel layout or allocator
 * is selected by the core. The component name must outlive the sink.
 */
int output_backend_get_config(output_config *config);
int output_backend_configure_decoder(OMX_HANDLETYPE decoder, const output_config *config);
output_backend_t *output_backend_create(const output_config *config,
                                        unsigned int count, unsigned int minimum_size);
void *output_backend_pointer(output_backend_t *output, unsigned int index);
unsigned int output_backend_capacity(output_backend_t *output, unsigned int index);

/* Called on the component's callback thread. The buffer remains owned by this
 * call until it returns; consume/copy/synchronize it before returning because
 * the core then recycles it. Return 0 on success, nonzero to latch a sink error.
 * Respect filled length/offset and the output format selected by your adapter.
 */
int output_backend_present(output_backend_t *output, unsigned int index,
                           const OMX_BUFFERHEADERTYPE *header);
/* Called after component buffer headers and the component are released. */
void output_backend_destroy(output_backend_t *output);
#endif
