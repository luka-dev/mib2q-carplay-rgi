/* Exercise the real waiting-loader -> direct-video ownership handoff.
 * GL/Screen hardware calls are replaced; no renderer state machine is mocked. */
#include <assert.h>
#include "../altscreen_render/screen_egl_present.c"

static screen_egl_present_t presenter;
static volatile unsigned loader_posts, video_posts;
static unsigned active_program, draws, finishes, refreshes;
static int fail_post;

EGLBoolean eglMakeCurrent(EGLDisplay d,EGLSurface a,EGLSurface b,EGLContext c)
{ (void)d;(void)a;(void)b;(void)c; return 1; }
EGLBoolean eglWaitNative(EGLint engine) { (void)engine; return 1; }
EGLint eglGetError(void) { return 0; }
EGLBoolean eglSwapBuffers(EGLDisplay d,EGLSurface s) {
    (void)d;(void)s;
    if (active_program == 11) __sync_add_and_fetch(&loader_posts, 1);
    else {
        assert(active_program == 22 && !presenter.loader_thread_started);
        assert(presenter.loader_exited && finishes >= video_posts + 1);
        if (fail_post) return 0;
        __sync_add_and_fetch(&video_posts, 1);
    }
    return 1; /* hidden-window case: swap doesn't pace the loader */
}
GLenum glGetError(void) { return GL_NO_ERROR; }
void glUseProgram(GLuint p) { active_program = p; }
void glFinish(void) { ++finishes; }
void glViewport(GLint x,GLint y,GLsizei w,GLsizei h)
{ assert(x == 0 && y == 0 && w == 1440 && h == 542); }
void glDisable(GLenum c) { (void)c; }
void glUniform1i(GLint l,GLint v) { (void)l;(void)v; }
void glUniform1f(GLint l,GLfloat v) { (void)l;(void)v; }
void glUniform2f(GLint l,GLfloat x,GLfloat y) { (void)l;(void)x;(void)y; }
void glUniform4fv(GLint l,GLsizei count,const GLfloat *v)
{ (void)l;assert(count==1 && v); }
void glClearColor(GLfloat r,GLfloat g,GLfloat b,GLfloat a)
{ assert(r==0 && g==0 && b==0 && a==1); }
void glClear(GLbitfield mask)
{ assert(mask==GL_COLOR_BUFFER_BIT && active_program==11); }
void glActiveTexture(GLenum t) { (void)t; }
void glBindTexture(GLenum t,GLuint id) { (void)t;(void)id; }
void glEnableVertexAttribArray(GLuint i) { (void)i; }
void glDisableVertexAttribArray(GLuint i) { (void)i; }
void glVertexAttribPointer(GLuint i,GLint n,GLenum t,GLboolean b,GLsizei s,const void *p)
{ (void)i;(void)n;(void)t;(void)b;(void)s;assert(p); }
void glDrawArrays(GLenum mode,GLint first,GLsizei count)
{
    assert(first==0);
    if(active_program==11) assert(mode==GL_TRIANGLES && count==12);
    else assert(mode==GL_TRIANGLE_STRIP && count==4);
    ++draws;
}
static void refresh(GLenum target,GLeglImageOES image) {
    assert(target == GL_TEXTURE_EXTERNAL_OES && image == (void *)3); ++refreshes;
}
static void wait_for_loader(unsigned count) {
    for (int i = 0; i < 100 && loader_posts < count; ++i) {
        struct timespec pause = {0,10000000L}; nanosleep(&pause,NULL);
    }
    assert(loader_posts >= count);
}
int main(void) {
    GLuint texture = 77;
    EGLImageKHR image = (void *)3;
    unsigned stopped_posts, first_draws;
    alarm(5);
    memset(&presenter,0,sizeof(presenter));
    presenter.width=1440; presenter.height=542;
    presenter.program=22; presenter.loader_program=11;
    presenter.logo_texture=7; presenter.sig_texture=8;
    presenter.source_count=1; presenter.textures=&texture; presenter.images=&image;
    presenter.image_target_texture=refresh;
    pthread_mutex_init(&presenter.gl_lock,NULL); presenter.gl_lock_initialized=1;
    assert(screen_egl_present_start_loader(&presenter)==0);
    assert(screen_egl_present_start_loader(&presenter)==0); /* idempotent */
    wait_for_loader(2);
    assert(screen_egl_present_frame(&presenter,0,1024)==0);
    stopped_posts=loader_posts; first_draws=draws;
    for(int i=0;i<10;i++) assert(screen_egl_present_frame(&presenter,0,1024)==0);
    assert(loader_posts==stopped_posts && video_posts==11);
    assert(draws==first_draws+10 && refreshes==11 && finishes==11);
    fail_post=1;
    assert(screen_egl_present_frame(&presenter,0,1024)<0);
    assert(loader_posts==stopped_posts && video_posts==11);
    fail_post=0;
    assert(screen_egl_present_start_loader(&presenter)==0);
    wait_for_loader(stopped_posts+1);
    assert(screen_egl_present_frame(&presenter,0,1024)==0);
    assert(!presenter.loader_thread_started && presenter.loader_exited);
    pthread_mutex_destroy(&presenter.gl_lock);
    puts("altscreen_present_test: loader stops/joins, one-pass video, no loader redraw, UV/lifetime barrier, reconnect PASS");
    return 0;
}
