/* A black opaque surface or nonzero row padding must not count as UI. */
#include <assert.h>
#include <stdint.h>
#include "screen_render_probe.c"

int main(void)
{
    unsigned char pixels[64*32];
    size_t count;
    memset(pixels, 0, sizeof(pixels));
    for (unsigned y = 0; y < 2; ++y) {
        pixels[y*12+3] = pixels[y*12+7] = 255;
        for (unsigned x = 8; x < 12; ++x) pixels[y*12+x] = 255;
    }
    assert(!count_rgb(pixels, 2, 2, 12, 24, &count) && count == 0);
    pixels[0] = 42;
    pixels[12+4+2] = 17;
    assert(!count_rgb(pixels, 2, 2, 12, 24, &count) && count == 2);
    assert(count_rgb(pixels, 2, 2, 12, 23, &count));
    assert(count_rgb(pixels, 2, 2, 7, sizeof(pixels), &count));
    assert(count_rgb(pixels, 801, 2, 4096, sizeof(pixels), &count));
    assert(count_rgb(pixels, 1, 543, 4, sizeof(pixels), &count));
    assert(count_rgb(pixels, 0, 2, 0, 0, &count));
    assert(count_rgb(0, 2, 2, 12, 24, &count));
    assert(count_rgb(pixels, 2, 2, 12, 24, 0));
    uint32_t rng = 8030;
    for (unsigned i = 0; i < 10000; ++i) {
        unsigned width = i%8+1, height = i%32+1, row = width*4 + (i%5)*4;
        size_t expected = 0;
        for (unsigned y = 0; y < height; ++y) {
            for (unsigned x = 0; x < row; ++x) {
                rng = rng*1664525+1013904223;
                pixels[y*row+x] = (rng >> 28) == 0 ? 42 : 0;
            }
            for (unsigned x = 0; x < width; ++x) {
                unsigned sum = pixels[y*row+x*4]+pixels[y*row+x*4+1]+pixels[y*row+x*4+2];
                expected += sum != 0;
            }
        }
        assert(!count_rgb(pixels, width, height, row, row*height, &count));
        assert(count == expected);
    }
    printf("native screen RGB/alpha/padding boundaries and 10000 cases passed\n");
    return 0;
}
