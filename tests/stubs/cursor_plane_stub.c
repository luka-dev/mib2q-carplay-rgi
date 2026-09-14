#include "../../cursor_render/cursor_plane.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int opened;
int cursor_plane_open(unsigned w, unsigned h) {
    const char *mode = getenv("CURSOR_TEST_PLANE"); (void)w; (void)h;
    if (mode && !strcmp(mode, "hang")) for (;;) pause();
    if (mode && !strcmp(mode, "fail")) return -1;
    if (!opened) { puts("OPEN"); fflush(stdout); }
    opened = 1; return 0;
}
int cursor_plane_managed(void) {
    const char *mode = getenv("CURSOR_TEST_PLANE");
    return !mode || strcmp(mode, "adopting");
}
int cursor_plane_draw(const cursor_frame *f) {
    printf("DRAW %u %u\n", f->x, f->alpha); fflush(stdout); return 0;
}
void cursor_plane_close(void) {
    if (opened) { puts("CLOSE"); fflush(stdout); }
    opened = 0;
}
