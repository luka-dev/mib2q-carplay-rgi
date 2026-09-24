/* Persistent GL program-binary cache (GL_OES_get_program_binary).
 *
 * The Adreno 320 driver compiles GLSL through libllvm-qcom at every launch; the
 * first renderer start after each boot paid that for every program.  The first
 * successful link now saves the driver's binary to the qnx6 persist partition
 * (GLPC_DIR, one directory per renderer under /mnt/persist/var/app/)
 * and later launches load it instead of compiling.  No offline compiler exists
 * for this driver, so the cache fills itself on the unit.
 *
 * Key = FNV-1a 64 of tag + both sources + GL_RENDERER + GL_VERSION, so a new
 * shader, new attribute bindings (put them in the tag) or a firmware driver
 * update all miss.  Any load failure (missing extension, corrupt or foreign
 * binary, link status 0) deletes the file and the caller compiles as before.
 *
 * Usage, on a fresh program object with attributes already bound:
 *     if (!glpc_load(prog, tag, vs, fs)) { compile, attach, link; glpc_store(...); }
 */
#ifndef GL_PROGRAM_CACHE_H
#define GL_PROGRAM_CACHE_H

#ifdef PLATFORM_MACOS
/* Desktop GL preview build: no program-binary cache, always compile. */
static inline int glpc_load(GLuint p, const char *t, const char *v, const char *f)
{ (void)p; (void)t; (void)v; (void)f; return 0; }
static inline void glpc_store(GLuint p, const char *t, const char *v, const char *f)
{ (void)p; (void)t; (void)v; (void)f; }
#else

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GLPC_DIR
#error "define GLPC_DIR (one persist directory per renderer) before including gl_program_cache.h"
#endif
#define GLPC_MAGIC 0x43504c47u          /* "GLPC" */
#define GLPC_MAX_BINARY (512 * 1024)
#define GLPC_PROGRAM_BINARY_LENGTH 0x8741 /* GL_PROGRAM_BINARY_LENGTH_OES */
#ifndef GL_APIENTRY
#define GL_APIENTRY
#endif

typedef void (GL_APIENTRY *glpc_get_fn)(GLuint, GLsizei, GLsizei *, GLenum *, void *);
typedef void (GL_APIENTRY *glpc_put_fn)(GLuint, GLenum, const void *, GLint);

typedef struct { uint32_t magic, format, length, reserved; uint64_t key; } glpc_header_t;

static int glpc_procs(glpc_get_fn *get, glpc_put_fn *put) {
    static int state = -1;
    static glpc_get_fn g;
    static glpc_put_fn p;
    if (state < 0) {
        const char *ext = (const char *)glGetString(GL_EXTENSIONS);
        g = (glpc_get_fn)eglGetProcAddress("glGetProgramBinaryOES");
        p = (glpc_put_fn)eglGetProcAddress("glProgramBinaryOES");
        state = ext && strstr(ext, "GL_OES_get_program_binary") && g && p;
    }
    *get = g;
    *put = p;
    return state;
}

static uint64_t glpc_mix(uint64_t h, const char *s) {
    if (s) for (; *s; ++s) { h ^= (unsigned char)*s; h *= 1099511628211ULL; }
    h ^= 0xff; h *= 1099511628211ULL;   /* field separator */
    return h;
}

static uint64_t glpc_key(const char *tag, const char *vs, const char *fs) {
    uint64_t h = 1469598103934665603ULL;
    h = glpc_mix(h, tag);
    h = glpc_mix(h, vs);
    h = glpc_mix(h, fs);
    h = glpc_mix(h, (const char *)glGetString(GL_RENDERER));
    h = glpc_mix(h, (const char *)glGetString(GL_VERSION));
    return h;
}

static void glpc_path(char *out, size_t n, uint64_t key) {
    snprintf(out, n, "%s/%08x%08x.bin", GLPC_DIR,
             (unsigned)(key >> 32), (unsigned)key);
}

static int glpc_read_all(int fd, void *buf, size_t n) {
    char *p = (char *)buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        p += r; n -= (size_t)r;
    }
    return 0;
}

/* 1 = program is linked from the cache; 0 = caller must compile. */
static int glpc_load(GLuint prog, const char *tag, const char *vs, const char *fs) {
    glpc_get_fn get; glpc_put_fn put;
    glpc_header_t hdr;
    char path[160];
    void *data;
    GLint ok = 0;
    int fd;
    uint64_t key;
    if (!prog || !glpc_procs(&get, &put)) return 0;
    key = glpc_key(tag, vs, fs);
    glpc_path(path, sizeof(path), key);
    fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    if (glpc_read_all(fd, &hdr, sizeof(hdr)) != 0 || hdr.magic != GLPC_MAGIC ||
        hdr.key != key || !hdr.length || hdr.length > GLPC_MAX_BINARY) {
        close(fd); unlink(path); return 0;
    }
    data = malloc(hdr.length);
    if (!data || glpc_read_all(fd, data, hdr.length) != 0) {
        free(data); close(fd); unlink(path); return 0;
    }
    close(fd);
    while (glGetError() != GL_NO_ERROR) { }
    put(prog, (GLenum)hdr.format, data, (GLint)hdr.length);
    free(data);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (glGetError() != GL_NO_ERROR || !ok) { unlink(path); return 0; }
    return 1;
}

/* Best effort: a failed save only means the next launch compiles again. */
static void glpc_store(GLuint prog, const char *tag, const char *vs, const char *fs) {
    glpc_get_fn get; glpc_put_fn put;
    glpc_header_t hdr;
    char path[160], tmp[176];
    GLint len = 0, ok = 0;
    GLsizei got = 0;
    GLenum format = 0;
    void *data;
    int fd, bad;
    if (!prog || !glpc_procs(&get, &put)) return;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glGetProgramiv(prog, GLPC_PROGRAM_BINARY_LENGTH, &len);
    if (!ok || len <= 0 || len > GLPC_MAX_BINARY) return;
    data = malloc((size_t)len);
    if (!data) return;
    get(prog, len, &got, &format, data);
    if (got <= 0 || got > len) { free(data); return; }
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = GLPC_MAGIC; hdr.format = format; hdr.length = (uint32_t)got;
    hdr.key = glpc_key(tag, vs, fs);
    glpc_path(path, sizeof(path), hdr.key);
    snprintf(tmp, sizeof(tmp), "%s.%d", path, (int)getpid());
    mkdir(GLPC_DIR, 0755);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { free(data); return; }
    bad = write(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr) ||
          write(fd, data, (size_t)got) != (ssize_t)got;
    bad |= fsync(fd) != 0;
    bad |= close(fd) != 0;
    free(data);
    if (bad || rename(tmp, path) != 0) unlink(tmp);
}

#endif /* PLATFORM_MACOS */
#endif
