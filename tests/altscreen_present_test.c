/* Exercise the real waiting-loader -> direct-video ownership handoff.
 * GL/Screen hardware calls are replaced; no renderer state machine is mocked. */
#include <assert.h>
#include <stdint.h>
#include <time.h>
static int test_clock_gettime(clockid_t clock_id, struct timespec *now);
#define clock_gettime test_clock_gettime
#include "../altscreen_render/screen_egl_present.c"
#undef clock_gettime

static screen_egl_present_t presenter;
static volatile unsigned loader_posts, video_posts;
static unsigned active_program, draws, finishes, refreshes;
static int fail_post, fail_interval, native_interval;
static unsigned recovery_intervals, recovery_destroyed;
static uint64_t fake_now_ns, finish_delay_ns, swap_delay_ns;
static float first_loader_time = -1.0f;

static int test_clock_gettime(clockid_t clock_id, struct timespec *now) {
    if (!fake_now_ns) return clock_gettime(clock_id, now);
    assert(clock_id == CLOCK_MONOTONIC);
    now->tv_sec = (time_t)(fake_now_ns / 1000000000ULL);
    now->tv_nsec = (long)(fake_now_ns % 1000000000ULL);
    return 0;
}

int cluster_surface_lost(cluster_surface_t *p) { (void)p; return 1; }
int cluster_surface_recreate(cluster_surface_t *p) { (void)p; native_interval = 1; return 0; }
screen_window_t cluster_surface_window(cluster_surface_t *p) { (void)p; return (screen_window_t)2; }
int screen_set_window_property_iv(screen_window_t w, int property, const int *v)
{ (void)w; assert(property == SCREEN_PROPERTY_SWAP_INTERVAL && *v == 2); native_interval = *v; return 0; }
int screen_get_window_property_iv(screen_window_t w, int property, int *v)
{ (void)w; assert(property == SCREEN_PROPERTY_SWAP_INTERVAL); *v = native_interval; return 0; }
EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, EGLNativeWindowType w, const EGLint *a)
{ (void)d;(void)c;(void)w;(void)a; assert(native_interval == 2); return (EGLSurface)3; }
EGLBoolean eglDestroySurface(EGLDisplay d, EGLSurface s)
{ (void)d;assert(s); ++recovery_destroyed; return 1; }
EGLBoolean eglSwapInterval(EGLDisplay d, EGLint interval)
{ (void)d; assert(interval == 2); ++recovery_intervals; return !fail_interval; }
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
        if (fake_now_ns) fake_now_ns += swap_delay_ns;
        __sync_add_and_fetch(&video_posts, 1);
    }
    return 1; /* hidden-window case: swap doesn't pace the loader */
}
GLenum glGetError(void) { return GL_NO_ERROR; }
void glUseProgram(GLuint p) { active_program = p; }
void glFinish(void) {
    ++finishes;
    if (fake_now_ns) fake_now_ns += finish_delay_ns;
}
void glViewport(GLint x,GLint y,GLsizei w,GLsizei h)
{ assert(x == 0 && y == 0 && w == 1440 && h == 542); }
void glDisable(GLenum c) { (void)c; }
void glUniform1i(GLint l,GLint v) { (void)l;(void)v; }
void glUniform1f(GLint l,GLfloat v) {
    if(active_program==11 && l==7 && first_loader_time<0.0f)first_loader_time=v;
}
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
    presenter.loader_u_itime=7;
    presenter.logo_texture=7; presenter.sig_texture=8;
    presenter.source_count=1; presenter.textures=&texture; presenter.images=&image;
    presenter.image_target_texture=refresh;
    pthread_mutex_init(&presenter.gl_lock,NULL); presenter.gl_lock_initialized=1;
    assert(screen_egl_present_start_loader(&presenter)==0);
    assert(loader_posts>=1 && first_loader_time>=2.6f);
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

    /* A cheap swap does not establish a cheap callback: OMX cannot reuse the
     * sampled source until glFinish has completed. Use a fake clock after the
     * real loader join to reproduce the timing blind spot deterministically,
     * without pretending to benchmark the QNX GPU on a host. */
    presenter.timing_enabled = 1;
    fake_now_ns = 1000000000ULL;
    finish_delay_ns = 50000000ULL;
    swap_delay_ns = 300000ULL;
    presenter.pace_last_entry_ns = presenter.pace_arrival_sum_ns = 0;
    presenter.pace_arrival_max_ns = presenter.pace_swap_sum_ns = 0;
    presenter.pace_swap_max_ns = presenter.pace_samples = presenter.pace_swap_samples = 0;
    presenter.pace_work_samples = presenter.pace_work_sum_ns = presenter.pace_work_max_ns = 0;
    for (int i = 0; i < 4; ++i) {
        uint64_t start = fake_now_ns;
        assert(screen_egl_present_frame(&presenter,0,1024)==0);
        assert(fake_now_ns - start == finish_delay_ns + swap_delay_ns);
    }
    assert(presenter.pace_work_samples == 4 && presenter.pace_work_sum_ns == 4 * 50300000ULL);
    assert(presenter.pace_work_max_ns == 50300000ULL && presenter.pace_swap_samples == 4);
    assert(presenter.pace_samples == 3);
    assert(presenter.pace_arrival_sum_ns == 3 * 50300000ULL);
    assert(presenter.pace_swap_sum_ns == 4 * 300000ULL);
    assert(presenter.pace_swap_max_ns == 300000ULL);
    assert(presenter.stages[6].samples == 4 && presenter.stages[6].max_us == 50000);
    assert(presenter.stages[7].samples == 4 && presenter.stages[7].max_us == 300);
    assert(presenter.stages[9].samples == 4 && presenter.stages[9].max_us == 50300);
    assert(presenter.stages[9].over_33ms == 4 && !presenter.stages[9].over_66ms);
    screen_egl_present_reset_timing(&presenter);
    assert(!presenter.stages[9].samples && presenter.timing_startup_pending);
    assert(!presenter.pace_work_samples && !presenter.pace_last_entry_ns);
    puts("altscreen_present_test: injected glFinish=50 ms, swap=0.3 ms -> full present=50.3 ms (~19.88 fps), not a hardware measurement");
    fake_now_ns = finish_delay_ns = swap_delay_ns = 0;
    presenter.rgba_surface = (cluster_surface_t *)1;
    presenter.egl_display = (EGLDisplay)2; presenter.egl_context = (EGLContext)3;
    presenter.egl_surface = (EGLSurface)4;
    assert(screen_egl_present_recover_output(&presenter) == 0 && recovery_intervals == 1);
    fail_interval = 1;
    assert(screen_egl_present_recover_output(&presenter) < 0);
    assert(presenter.egl_surface == EGL_NO_SURFACE && recovery_destroyed == 3);
    fail_interval = 0;
    assert(screen_egl_present_recover_output(&presenter) == 0 && recovery_intervals == 3);
    assert(presenter.images == &image && presenter.textures == &texture); /* no OMX source replacement */
    pthread_mutex_destroy(&presenter.gl_lock);
    puts("altscreen_present_test: loader stops/joins, one-pass video, no loader redraw, UV/lifetime barrier, reconnect PASS");
    return 0;
}
