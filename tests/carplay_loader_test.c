#include "../common/carplay_loader.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_WIDTH 96
#define TEST_HEIGHT 48
#define TEST_PADDING 16
#define TEST_STRIDE (TEST_WIDTH * 4 + TEST_PADDING)

static uint32_t checksum(const uint8_t *bytes, size_t length) {
    uint32_t value = 2166136261U;
    size_t i;
    for (i = 0; i < length; ++i) {
        value ^= bytes[i];
        value *= 16777619U;
    }
    return value;
}

static int write_ppm(const char *path, const uint8_t *rgba) {
    FILE *file = fopen(path, "wb");
    int y;

    if (!file) return -1;
    if (fprintf(file, "P6\n%d %d\n255\n", TEST_WIDTH, TEST_HEIGHT) < 0) {
        fclose(file);
        return -1;
    }
    for (y = 0; y < TEST_HEIGHT; ++y) {
        const uint8_t *row = rgba + y * TEST_STRIDE;
        int x;
        for (x = 0; x < TEST_WIDTH; ++x) {
            if (fwrite(row + x * 4, 1U, 3U, file) != 3U) {
                fclose(file);
                return -1;
            }
        }
    }
    return fclose(file) == 0 ? 0 : -1;
}

int main(int argc, char **argv) {
    uint8_t first[TEST_HEIGHT * TEST_STRIDE];
    uint8_t second[TEST_HEIGHT * TEST_STRIDE];
    uint32_t first_sum;
    uint32_t second_sum;
    int y;

    memset(first, 0xa5, sizeof(first));
    memset(second, 0xa5, sizeof(second));

    if (carplay_loader_render_rgba8888(first, TEST_WIDTH, TEST_HEIGHT,
                                       TEST_STRIDE, 0U) != 0 ||
        carplay_loader_render_rgba8888(second, TEST_WIDTH, TEST_HEIGHT,
                                       TEST_STRIDE, 450U) != 0) {
        fprintf(stderr, "carplay_loader_test: render failed\n");
        return 1;
    }

    first_sum = checksum(first, sizeof(first));
    second_sum = checksum(second, sizeof(second));
    if (first_sum == second_sum) {
        fprintf(stderr, "carplay_loader_test: animation did not change\n");
        return 1;
    }

    for (y = 0; y < TEST_HEIGHT; ++y) {
        int x;
        const uint8_t *row = first + y * TEST_STRIDE;
        for (x = 0; x < TEST_WIDTH; ++x) {
            if (row[x * 4 + 3] != 255U) {
                fprintf(stderr, "carplay_loader_test: non-opaque pixel at %d,%d\n", x, y);
                return 1;
            }
        }
        for (x = TEST_WIDTH * 4; x < TEST_STRIDE; ++x) {
            if (row[x] != 0xa5U) {
                fprintf(stderr, "carplay_loader_test: stride padding overwritten\n");
                return 1;
            }
        }
    }

    if (carplay_loader_render_rgba8888(NULL, TEST_WIDTH, TEST_HEIGHT,
                                       TEST_STRIDE, 0U) == 0 ||
        carplay_loader_render_rgba8888(first, TEST_WIDTH, TEST_HEIGHT,
                                       TEST_WIDTH * 4U - 1U, 0U) == 0) {
        fprintf(stderr, "carplay_loader_test: invalid input accepted\n");
        return 1;
    }
    if (carplay_loader_breathing_level(0U) != 0U ||
        carplay_loader_breathing_level(600U) != 255U ||
        carplay_loader_breathing_level(1200U) != 0U) {
        fprintf(stderr, "carplay_loader_test: breathing phase mismatch\n");
        return 1;
    }

    if (argc == 2) {
        char path0[512];
        char path450[512];
        int n0 = snprintf(path0, sizeof(path0), "%s_0000.ppm", argv[1]);
        int n450 = snprintf(path450, sizeof(path450), "%s_0450.ppm", argv[1]);
        if (n0 < 0 || (size_t)n0 >= sizeof(path0) ||
            n450 < 0 || (size_t)n450 >= sizeof(path450) ||
            write_ppm(path0, first) != 0 || write_ppm(path450, second) != 0) {
            fprintf(stderr, "carplay_loader_test: preview write failed\n");
            return 1;
        }
        printf("carplay_loader_test: previews %s %s\n", path0, path450);
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [preview-path-prefix]\n", argv[0]);
        return 2;
    }

    printf("carplay_loader_test: OK frame0=%08x frame450=%08x\n",
           first_sum, second_sum);
    return 0;
}
