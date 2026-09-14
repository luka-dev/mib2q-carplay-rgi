#include "prism.h"
#include <limits.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_THREAD_LOCALS
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../hook/coverart/stb_image.h"

unsigned char *prism_decode_png(const unsigned char *bytes,size_t size,int *w,int *h) {
    int channels;
    if(!bytes || !size || size>INT_MAX || !w || !h) return NULL;
    return stbi_load_from_memory(bytes,(int)size,w,h,&channels,4);
}
void prism_free_image(unsigned char *image) { stbi_image_free(image); }
