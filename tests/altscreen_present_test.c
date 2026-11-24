/* Exercise the real waiting-loader -> direct-video ownership handoff.
 * GL/Screen hardware calls are replaced; no renderer state machine is mocked. */
#include <assert.h>
#include <stdint.h>
#include <time.h>
static int test_clock_gettime(clockid_t clock_id, struct timespec *now);
#define clock_gettime test_clock_gettime
#include "../altscreen_render/screen_egl_present.c"
#undef clock_gettime
#include <fcntl.h>

static screen_egl_present_t presenter;
static volatile unsigned loader_posts, video_posts;
static unsigned active_program, draws, finishes, refreshes, framebuffer, cache_draws;
static int fail_post, fail_interval, native_interval;
static unsigned recovery_intervals, recovery_destroyed;
static uint64_t fake_now_ns, finish_delay_ns, swap_delay_ns;
static float first_loader_time = -1.0f;
/* Luma probe fakes: rows are read after this frame's glFinish, before its swap. */
static int frame_finished;
static unsigned readpixel_calls, readpixel_fail, pending_gl_errors;
static int readpixel_y[3];
static uint8_t readpixel_rgba[3][4];
static int luma_armed_flag = -1;
static unsigned luma_publishes, luma_tokens;
static alt_luma_t luma_published;
static uint64_t luma_published_ms;
static int luma_published_token;

static int test_clock_gettime(clockid_t clock_id, struct timespec *now) {
    if (!fake_now_ns || clock_id != CLOCK_MONOTONIC) return clock_gettime(clock_id, now);
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
        assert((active_program == 22 || active_program == 33) && !presenter.loader_thread_started);
        assert(presenter.loader_exited && finishes >= video_posts + 1);
        frame_finished = 0;
        if (fail_post) return 0;
        if (fake_now_ns) fake_now_ns += swap_delay_ns;
        __sync_add_and_fetch(&video_posts, 1);
    }
    return 1; /* hidden-window case: swap doesn't pace the loader */
}
GLenum glGetError(void) {
    if (pending_gl_errors) { --pending_gl_errors; return 0x0502; /* GL_INVALID_OPERATION */ }
    return GL_NO_ERROR;
}
void glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void *pixels) {
    unsigned row = readpixel_calls % 3u, i;
    assert(x == 0 && w == 1440 && h == 1 && format == GL_RGBA && type == GL_UNSIGNED_BYTE && pixels);
    assert(frame_finished && (active_program == 22 || active_program == 33));
    readpixel_y[row] = y;
    for (i = 0; i < (unsigned)w; ++i) memcpy((uint8_t *)pixels + i * 4u, readpixel_rgba[row], 4);
    ++readpixel_calls;
    if (readpixel_fail) pending_gl_errors = 2;
}
int altctl_luma_arm(int armed) {
    luma_armed_flag = armed;
    return armed ? (int)((++luma_tokens << 1) | 1u) : 0;
}
void altctl_luma_publish(const alt_luma_t *l, uint64_t ms, int token) {
    ++luma_publishes; luma_published = *l; luma_published_ms = ms; luma_published_token = token;
}
void glBindFramebuffer(GLenum target, GLuint id) { assert(target == GL_FRAMEBUFFER); framebuffer=id; }
void glUseProgram(GLuint p) { active_program = p; }
void glFinish(void) {
    ++finishes;
    frame_finished = 1;
    if (fake_now_ns) fake_now_ns += finish_delay_ns;
}
void glViewport(GLint x,GLint y,GLsizei w,GLsizei h)
{ assert(x == 0 && y == 0 && ((w == 1440 && h == 542) || (w == PRISM_CACHE_WIDTH && h == PRISM_CACHE_HEIGHT))); }
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
    if (framebuffer) ++cache_draws;
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
static void fill_rgba(uint8_t *px, unsigned pixels, unsigned split, const uint8_t a[4],
                      const uint8_t b[4]) {
    for (unsigned i = 0; i < pixels; ++i) memcpy(px + i * 4u, i < split ? a : b, 4);
}
static void set_rows(const uint8_t a[4], const uint8_t b[4], const uint8_t c[4]) {
    memcpy(readpixel_rgba[0], a, 4); memcpy(readpixel_rgba[1], b, 4); memcpy(readpixel_rgba[2], c, 4);
}

/* Pure luma reduction, classifier, sampling cadence and snapshot hand-off. */
static void luma_pure_tests(void) {
    static uint8_t px[1440u * 3u * 4u];
    static const uint8_t black[4] = {0, 0, 0, 255}, white[4] = {255, 255, 255, 0};
    static const uint8_t red[4] = {255, 0, 0, 255}, green[4] = {0, 255, 0, 255};
    static const uint8_t blue[4] = {0, 0, 255, 255};
    const unsigned n = 1440u * 3u;
    alt_luma_t l, dim, got;
    alt_luma_sampler_t s;
    alt_luma_shared_t shared;
    uint32_t stamp = 0, token = 0;

    assert(alt_luma_row_y(542, 0) == 135 && alt_luma_row_y(542, 1) == 271 &&
           alt_luma_row_y(542, 2) == 406);
    fill_rgba(px, n, n, black, black);
    alt_luma_reduce_rgba(px, n, &l);
    assert(l.min == 0 && l.mean == 0 && l.max == 0 && alt_luma_is_dark(&l));
    fill_rgba(px, n, n, white, white);                /* alpha is ignored */
    alt_luma_reduce_rgba(px, n, &l);
    assert(l.min == 255 && l.mean == 255 && l.max == 255 && !alt_luma_is_dark(&l));
    fill_rgba(px, n, n / 2u, black, white);           /* mean 127.5 floors to 127 */
    alt_luma_reduce_rgba(px, n, &l);
    assert(l.min == 0 && l.mean == 127 && l.max == 255 && !alt_luma_is_dark(&l));
    fill_rgba(px, 1, 1, red, red); alt_luma_reduce_rgba(px, 1, &l); assert(l.mean == 76);
    fill_rgba(px, 1, 1, green, green); alt_luma_reduce_rgba(px, 1, &l); assert(l.mean == 149);
    fill_rgba(px, 1, 1, blue, blue); alt_luma_reduce_rgba(px, 1, &l); assert(l.mean == 28);
    alt_luma_reduce_rgba(NULL, 0, &l);
    assert(!l.min && !l.mean && !l.max);
    dim.min = 0; dim.mean = 5; dim.max = 23; assert(alt_luma_is_dark(&dim));
    dim.mean = 6; assert(!alt_luma_is_dark(&dim));    /* either threshold ends "dark" */
    dim.mean = 5; dim.max = 24; assert(!alt_luma_is_dark(&dim));

    memset(&s, 0, sizeof(s));
    assert(!alt_luma_sample_due(&s, 1000000000ULL, 1));     /* PRISM flight picture */
    assert(alt_luma_sample_due(&s, 1100000000ULL, 0));      /* first after the flight */
    fill_rgba(px, n, n, black, black); alt_luma_reduce_rgba(px, n, &l);
    assert(alt_luma_note_sample(&s, &l, 1100000000ULL));    /* first sample logs */
    assert(!alt_luma_sample_due(&s, 1600000000ULL, 0));
    assert(!alt_luma_sample_due(&s, 2000000000ULL, 0));     /* 0.9 s: not yet */
    assert(alt_luma_sample_due(&s, 2100000000ULL, 0));      /* 1 s cadence */
    assert(!alt_luma_note_sample(&s, &l, 2100000000ULL));   /* same state, < 10 s */
    assert(!alt_luma_sample_due(&s, 2500000000ULL, 0));
    assert(alt_luma_sample_due(&s, 4000000000ULL, 0));      /* first after a 1.5 s gap */
    fill_rgba(px, n, n, white, white); alt_luma_reduce_rgba(px, n, &got);
    assert(alt_luma_note_sample(&s, &got, 4000000000ULL));  /* dark -> not dark edge logs */
    assert(!alt_luma_note_sample(&s, &got, 5000000000ULL));
    assert(!alt_luma_note_sample(&s, &got, 13900000000ULL));
    assert(alt_luma_note_sample(&s, &got, 14000000000ULL)); /* 10 s steady cadence */
    assert(alt_luma_note_sample(&s, &l, 14100000000ULL));   /* not dark -> dark edge logs */
    s.failed = 1;
    assert(!alt_luma_sample_due(&s, 20000000000ULL, 0));

    memset(&shared, 0, sizeof(shared));
    assert(!alt_luma_shared_read(&shared, &got, &stamp, &token));   /* nothing published yet */
    l.min = 1; l.mean = 2; l.max = 3;
    alt_luma_shared_publish(&shared, &l, 77u, 3u);
    assert(alt_luma_shared_read(&shared, &got, &stamp, &token));
    assert(got.min == 1 && got.mean == 2 && got.max == 3 && stamp == 77u && token == 3u &&
           shared.seq == 2u);
    l.min = 300; l.mean = 255; l.max = 999;                  /* packing clamps, never bleeds */
    alt_luma_shared_publish(&shared, &l, 0xffffffffu, 0x7fffffffu);
    assert(alt_luma_shared_read(&shared, &got, &stamp, &token));
    assert(got.min == 255 && got.mean == 255 && got.max == 255 && stamp == 0xffffffffu &&
           token == 0x7fffffffu);
    shared.seq = 5u;                                         /* writer mid-update: give up */
    assert(!alt_luma_shared_read(&shared, &got, &stamp, &token));
}

/* The real stream edge and presenter path, with a fake GL readback. */
static void luma_probe_tests(void) {
    static const uint8_t black[4] = {0, 0, 0, 255}, white[4] = {255, 255, 255, 255};
    static const uint8_t gray[4] = {128, 128, 128, 255};
    char marker[] = "/tmp/altscreen-luma-marker.XXXXXX";
    unsigned long failures = presenter.failures;
    unsigned posts, calls;
    int fd, request;

    /* Host default: no marker path at all, so a stray /tmp file cannot arm a
     * test. Every earlier frame ran disarmed: no readback, no buffer. */
    assert(ep_luma_marker == NULL);
    assert(readpixel_calls == 0 && luma_publishes == 0 && luma_armed_flag == 0);
    assert(!presenter.luma_request && !presenter.luma_rows);
    fd = mkstemp(marker);
    assert(fd >= 0);
    close(fd);
    unlink(marker);
    ep_luma_marker = marker;
    screen_egl_present_set_stream_pending(&presenter, 0);
    screen_egl_present_set_stream_pending(&presenter, 1);
    assert(!presenter.luma_request && !presenter.luma_rows && luma_armed_flag == 0);

    fd = open(marker, O_CREAT | O_WRONLY, 0600);
    assert(fd >= 0);
    close(fd);
    screen_egl_present_set_stream_pending(&presenter, 0);
    screen_egl_present_set_stream_pending(&presenter, 1);
    assert((presenter.luma_request & 1) && presenter.luma_rows && luma_armed_flag == 1);
    assert(presenter.luma_request == (int)((luma_tokens << 1) | 1u));   /* altctl's token */
    request = presenter.luma_request;
    screen_egl_present_set_stream_pending(&presenter, 1);   /* source attach: same stream */
    assert(presenter.luma_request == request);

    set_rows(black, white, gray);
    fake_now_ns = 10000000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(readpixel_calls == 3 && readpixel_y[0] == 135 && readpixel_y[1] == 271 &&
           readpixel_y[2] == 406);
    assert(luma_publishes == 1 && luma_published.min == 0 && luma_published.mean == 127 &&
           luma_published.max == 255 && luma_published_ms == 10000u &&
           luma_published_token == request);
    fake_now_ns = 10500000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(readpixel_calls == 3);                            /* at most one sample a second */
    fake_now_ns = 11000000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(readpixel_calls == 6 && luma_publishes == 2);

    /* PRISM flight pictures are never sampled, even when the second is due. */
    presenter.transition_program = 33;
    presenter.transition_active = 1;
    presenter.transition_started_ns = 12000000000ULL;
    fake_now_ns = 12200000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(presenter.transition_active && readpixel_calls == 6);
    fake_now_ns = 12800000000ULL;                             /* flight completes this call */
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(!presenter.transition_active && readpixel_calls == 9 && luma_publishes == 3);
    presenter.transition_program = 0;

    /* A readback GL error is drained and logged: the frame still swaps and is
     * not a presentation failure; the probe stops for this stream only. */
    readpixel_fail = 1;
    posts = video_posts;
    fake_now_ns = 14000000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    readpixel_fail = 0;
    assert(video_posts == posts + 1 && presenter.failures == failures);
    assert(!pending_gl_errors && presenter.luma.failed && luma_publishes == 3);
    calls = readpixel_calls;
    fake_now_ns = 16000000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(readpixel_calls == calls);
    screen_egl_present_set_stream_pending(&presenter, 0);
    screen_egl_present_set_stream_pending(&presenter, 1);
    assert(presenter.luma_request != request && (presenter.luma_request & 1));
    set_rows(black, black, black);
    fake_now_ns = 17000000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(readpixel_calls == calls + 3 && luma_publishes == 4);
    assert(!luma_published.min && !luma_published.mean && !luma_published.max);
    assert(luma_published_token == presenter.luma_request && luma_published_token != request);

    /* Marker gone: the next stream disarms and stops reading back. */
    unlink(marker);
    screen_egl_present_set_stream_pending(&presenter, 0);
    screen_egl_present_set_stream_pending(&presenter, 1);
    assert(!presenter.luma_request && luma_armed_flag == 0);
    fake_now_ns = 19000000000ULL;
    assert(screen_egl_present_frame(&presenter, 0, 1024) == 0);
    assert(readpixel_calls == calls + 3 && luma_publishes == 4);
    assert(presenter.failures == failures);
    ep_luma_marker = NULL;
    fake_now_ns = 0;
    free(presenter.luma_rows);
    presenter.luma_rows = NULL;
}

int main(void) {
    GLuint texture = 77;
    EGLImageKHR image = (void *)3;
    unsigned stopped_posts, first_draws;
    alarm(5);
    luma_pure_tests();
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

    /* Flight uses one cached-art draw, then one external-video draw per tick.
     * Real time controls completion: it does not need another phone frame. */
    presenter.transition_program=33;
    presenter.transition_fbo=44;
    assert(screen_egl_present_start_loader(&presenter)==0);
    screen_egl_present_stop_loader(&presenter);
    fake_now_ns=1000000000ULL;
    unsigned cache_before=cache_draws, video_before=video_posts;
    for (int i=0;i<8;i++) {
        fake_now_ns=1000000000ULL+(uint64_t)i*100000000ULL;
        assert(screen_egl_present_frame(&presenter,0,1024)==0);
        assert(screen_egl_present_transition_active(&presenter)==(i<7));
        assert(cache_draws==cache_before+1);
        assert(active_program==(i<7?33u:22u));
    }
    assert(video_posts==video_before+8);
    for(int i=0;i<10;i++) assert(screen_egl_present_frame(&presenter,0,1024)==0);
    assert(cache_draws==cache_before+1 && active_program==22);
    fake_now_ns=0;
    assert(screen_egl_present_start_loader(&presenter)==0);
    assert(!presenter.transition_active && presenter.transition_pending);
    screen_egl_present_stop_loader(&presenter);
    presenter.transition_program=0;

    /* The one-second minimum is armed once per stream. Repeated pending
     * notifications/source attachment cannot turn it into a sliding timer. */
    fake_now_ns=1000000000ULL;
    presenter.loader_thread_started=1; /* clock-only fixture, no live thread */
    presenter.loader_stream_pending=0;
    screen_egl_present_set_stream_pending(&presenter,1);
    assert(screen_egl_present_loader_hold_remaining_ms(&presenter)==1000);
    fake_now_ns=1750000000ULL;
    screen_egl_present_set_stream_pending(&presenter,1);
    assert(screen_egl_present_loader_hold_remaining_ms(&presenter)==250);
    fake_now_ns=2100000000ULL;
    screen_egl_present_set_stream_pending(&presenter,1);
    assert(screen_egl_present_loader_hold_remaining_ms(&presenter)==0);
    screen_egl_present_set_stream_pending(&presenter,0);
    screen_egl_present_set_stream_pending(&presenter,1);
    assert(screen_egl_present_loader_hold_remaining_ms(&presenter)==1000);
    presenter.loader_thread_started=0;
    presenter.loader_stream_pending=0;
    fake_now_ns=0;

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
    luma_probe_tests();
    puts("altscreen_present_test: luma probe disarmed by default, armed per stream, 1 s cadence, flight skip, readback errors never fail a frame PASS");
    pthread_mutex_destroy(&presenter.gl_lock);
    puts("altscreen_present_test: loader stops/joins, one-pass video, no loader redraw, UV/lifetime barrier, reconnect PASS");
    return 0;
}
