/* Program-binary cache: save after a link, hit on the next launch, recompile
 * on a corrupt file, a foreign binary or a changed source. GL is stubbed. */
#define GLPC_DIR "/tmp/glpc_test_cache"
#include <assert.h>
#include "gl_program_cache.h"

static const char BIN[] = "Z400-binary-bytes";
static int link_ok = 1, driver_accepts = 1, loaded = 0;

const unsigned char *glGetString(GLenum n) {
    return (const unsigned char *)(n == GL_EXTENSIONS ? "GL_OES_get_program_binary" : "Adreno 320");
}
GLenum glGetError(void) { return GL_NO_ERROR; }
void glGetProgramiv(GLuint p, GLenum n, GLint *v) {
    (void)p;
    if (n == GL_LINK_STATUS) *v = link_ok;
    else if (n == GLPC_PROGRAM_BINARY_LENGTH) *v = (GLint)sizeof(BIN);
}
static void GL_APIENTRY get_bin(GLuint p, GLsizei n, GLsizei *len, GLenum *fmt, void *out) {
    (void)p; (void)n; memcpy(out, BIN, sizeof(BIN)); *len = sizeof(BIN); *fmt = 0x8fc4;
}
static void GL_APIENTRY put_bin(GLuint p, GLenum fmt, const void *d, GLint len) {
    (void)p;
    link_ok = driver_accepts && fmt == 0x8fc4 && len == (GLint)sizeof(BIN) && !memcmp(d, BIN, sizeof(BIN));
    loaded++;
}
__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *n) {
    if (!strcmp(n, "glGetProgramBinaryOES")) return (__eglMustCastToProperFunctionPointerType)get_bin;
    if (!strcmp(n, "glProgramBinaryOES")) return (__eglMustCastToProperFunctionPointerType)put_bin;
    return NULL;
}

int main(void) {
    char path[160];
    FILE *f;
    system("rm -rf " GLPC_DIR);
    assert(!glpc_load(1, "t", "vs", "fs"));            /* empty cache: compile */
    link_ok = 1;
    glpc_store(1, "t", "vs", "fs");
    glpc_path(path, sizeof(path), glpc_key("t", "vs", "fs"));
    assert(access(path, F_OK) == 0);
    assert(glpc_load(1, "t", "vs", "fs") && loaded == 1);  /* hit */
    assert(!glpc_load(1, "t", "vs", "fs2"));           /* other source: miss */
    assert(!glpc_load(1, "t2", "vs", "fs"));           /* other bindings: miss */
    driver_accepts = 0;                                /* driver update rejects it */
    assert(!glpc_load(1, "t", "vs", "fs") && access(path, F_OK) != 0);
    driver_accepts = 1; link_ok = 1;
    glpc_store(1, "t", "vs", "fs");
    f = fopen(path, "r+b"); assert(f); fputs("junk", f); fclose(f);  /* corrupt header */
    assert(!glpc_load(1, "t", "vs", "fs") && access(path, F_OK) != 0);
    link_ok = 0;
    glpc_store(1, "t", "vs", "fs");                    /* failed link is never saved */
    assert(access(path, F_OK) != 0);
    system("rm -rf " GLPC_DIR);
    puts("gl_program_cache: store/hit/miss on source+bindings/driver reject/corrupt/no-save-on-fail PASS");
    return 0;
}
