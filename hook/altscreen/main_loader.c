/*
 * CarPlay main-screen startup loader for stock libairplay 210.81.
 *
 * This deliberately uses stock CScreenRender instead of creating a parallel
 * QNX Screen window.  The loader prepares the same displayable-59 object that
 * stock OmxVideoImpl later adopts, so the loading frame and decoded stream use
 * one window and one 12-buffer Qualcomm-32m pool.
 *
 * Stock-210 evidence (Patches/carplay_vc/libairplay.so):
 *   ScreenRegister                         0x83cfc (dio boundary)
 *   CScreenRender::create                  0x8b11c
 *   CScreenRender::render                  0x8b5c0
 *   CScreenRender::sharedBuffers           0x8b9e0
 *   CScreenRender::stop                    0x8bab0
 *   CScreenRender::config                  0x8bcc8
 *   OmxVideoImpl::configure                0x8c388
 *   COMXVideoDecoder::start                0x89670
 *   internal create/config/stop/start GOT  0xb97ac/0xb9d30/0xb9ca4/0xb9704
 *
 * The offsets are accepted only after every required exported method address
 * and text fingerprint matches the shipped 210.81 image.  Any mismatch leaves
 * the stock path untouched.
 */
#include "../framework/common.h"
#include "../framework/logging.h"
#include "../../common/carplay_loader.h"
#include "main_loader.h"

#include <limits.h>

DEFINE_LOG_MODULE(main_loader);

#define STOCK210_OFF_CREATE       0x0008b11cU
#define STOCK210_OFF_RENDER       0x0008b5c0U
#define STOCK210_OFF_SHARED       0x0008b9e0U
#define STOCK210_OFF_STOP         0x0008bab0U
#define STOCK210_OFF_CONFIG       0x0008bcc8U
#define STOCK210_OFF_OMX_START    0x00089670U

#define STOCK210_GOT_CREATE       0x000b97acU
#define STOCK210_GOT_STOP         0x000b9ca4U
#define STOCK210_GOT_CONFIG       0x000b9d30U
#define STOCK210_GOT_OMX_START    0x000b9704U

#define MAIN_WIDTH                1024U
#define MAIN_HEIGHT               480U
#define MAIN_BUFFER_COUNT         12U
#define MAIN_SCREEN_FORMAT        0x0001000cU /* Qcom tiled NV12 / 32m */
#define MAIN_DISPLAYABLE_ID       59U
#define MAIN_SCREEN_USAGE         0x00000484U
#define MAIN_Y_BYTES              0x00078000U
#define MAIN_BUFFER_BYTES         0x000b9000U

#define SYM_CREATE  "_ZN3dio13CScreenRender6createEj"
#define SYM_RENDER  "_ZN3dio13CScreenRender6renderEPh"
#define SYM_SHARED  "_ZN3dio13CScreenRender13sharedBuffersERPPh"
#define SYM_STOP    "_ZN3dio13CScreenRender4stopEv"
#define SYM_CONFIG  "_ZN3dio13CScreenRender6configERKNS_16st_screen_configE"
#define SYM_DELETE  "_ZN3dio13CScreenRenderD0Ev"
#define SYM_OMX_START "_ZN3dio16COMXVideoDecoder5startEv"

/* Layout copied by CScreenRender::config@0x8bf90 (11 x uint32_t). */
typedef struct main_screen_config {
    uint32_t x_offset;
    uint32_t y_offset;
    uint32_t width;
    uint32_t height;
    uint32_t window_width;
    uint32_t window_height;
    uint32_t output_device_tag;
    uint32_t buffer_count;
    uint32_t screen_format;
    uint32_t displayable_id;
    uint32_t usage;
} main_screen_config_t;

typedef void *(*fn_create_t)(unsigned int);
typedef int   (*fn_config_t)(void *, const main_screen_config_t *);
typedef int   (*fn_shared_t)(void *, unsigned char ***);
typedef int   (*fn_render_t)(void *, unsigned char *);
typedef void  (*fn_stop_t)(void *);
typedef void  (*fn_delete_t)(void *);
typedef int   (*fn_omx_start_t)(void *);
typedef int   (*fn_screen_register_t)(void *);

typedef enum main_loader_state {
    ML_COLD = 0,
    ML_PREPARING,
    ML_ACTIVE,
    ML_HANDOFF,
    ML_FALLBACK_CONFIG,
    ML_OMX_OWNED,
    ML_BYPASS_SESSION,
    ML_SHUTDOWN,
    ML_DISABLED
} main_loader_state_t;

static struct {
    pthread_mutex_t lock;
    main_loader_state_t state;
    int initialized;
    int patches_installed;
    int shutdown;
    int worker_started;
    pthread_t worker;

    uintptr_t stock_base;
    fn_create_t real_create;
    fn_config_t real_config;
    fn_shared_t real_shared;
    fn_render_t real_render;
    fn_stop_t real_stop;
    fn_delete_t real_delete;
    fn_omx_start_t real_omx_start;

    /* One CarPlay receiver session is active in dio_manager.  Generation
     * prevents a late return from an old decoder start from opening a newer
     * session's stream-111 gate. */
    void *session;
    uint32_t session_generation;
    int main_omx_ready;

    void *render_object;
    unsigned char **buffers;
    main_screen_config_t config;
    main_screen_config_t learned_config;
    int learned_config_valid;
    unsigned frame_index;
    int first_config_logged;
} g_main = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .state = ML_COLD
};

/* Serializes early create/config with stock's internal create call.  This is
 * separate from g_main.lock because CScreenRender::config may call its own
 * stop path on error, which reaches our stop GOT wrapper. */
static pthread_mutex_t g_prepare_gate = PTHREAD_MUTEX_INITIALIZER;
/* Teardown, handoff and process shutdown may converge on the same worker. */
static pthread_mutex_t g_join_gate = PTHREAD_MUTEX_INITIALIZER;

static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static void sleep_until_ns(uint64_t deadline)
{
    for (;;) {
        uint64_t now = monotonic_ns();
        struct timespec req;
        uint64_t delta;
        if (now == 0 || now >= deadline) return;
        delta = deadline - now;
        req.tv_sec = (time_t)(delta / 1000000000ULL);
        req.tv_nsec = (long)(delta % 1000000000ULL);
        if (nanosleep(&req, NULL) == 0 || errno != EINTR) return;
    }
}

static int env_enabled(void)
{
    const char *v = getenv("CARPLAY_STARTUP_LOADER");
    return !v || strcmp(v, "0") != 0;
}

static uint32_t output_device_tag(void)
{
    const char *v = getenv("NVSS_VIDEO_OUTPUT_DEVICE");
    /* OmxVideoImpl::configure@0x8c4e8 rejects a missing variable. */
    if (!v) return 0;
    return atoi(v) ? 0x00007667U : 0x00007660U;
}

static int config_equal(const main_screen_config_t *a,
                        const main_screen_config_t *b)
{
    return a && b && memcmp(a, b, sizeof(*a)) == 0;
}

static int config_is_safe_main(const main_screen_config_t *c)
{
    return c && c->width == MAIN_WIDTH && c->height == MAIN_HEIGHT &&
           c->window_width > 0U && c->window_height > 0U &&
           c->buffer_count == MAIN_BUFFER_COUNT &&
           c->screen_format == MAIN_SCREEN_FORMAT &&
           c->displayable_id == MAIN_DISPLAYABLE_ID &&
           c->usage == MAIN_SCREEN_USAGE &&
           (c->output_device_tag == 0x00007660U ||
            c->output_device_tag == 0x00007667U);
}

static void remember_runtime_config(const main_screen_config_t *c)
{
    if (!config_is_safe_main(c)) return;
    pthread_mutex_lock(&g_main.lock);
    g_main.learned_config = *c;
    g_main.learned_config_valid = 1;
    pthread_mutex_unlock(&g_main.lock);
}

static void log_config(const char *prefix, const main_screen_config_t *c)
{
    if (!c) return;
    LOG_INFO(LOG_MODULE,
             "%s x=%u y=%u size=%ux%u window=%ux%u tag=0x%x buffers=%u format=0x%x id=%u usage=0x%x",
             prefix, c->x_offset, c->y_offset, c->width, c->height,
             c->window_width, c->window_height, c->output_device_tag,
             c->buffer_count, c->screen_format, c->displayable_id, c->usage);
}

static int stock210_resolve(void)
{
    uintptr_t base;
    uint32_t *text;

    g_main.real_create = (fn_create_t)dlsym(RTLD_NEXT, SYM_CREATE);
    g_main.real_config = (fn_config_t)dlsym(RTLD_NEXT, SYM_CONFIG);
    g_main.real_shared = (fn_shared_t)dlsym(RTLD_NEXT, SYM_SHARED);
    g_main.real_render = (fn_render_t)dlsym(RTLD_NEXT, SYM_RENDER);
    g_main.real_stop = (fn_stop_t)dlsym(RTLD_NEXT, SYM_STOP);
    g_main.real_delete = (fn_delete_t)dlsym(RTLD_NEXT, SYM_DELETE);
    g_main.real_omx_start = (fn_omx_start_t)dlsym(RTLD_NEXT, SYM_OMX_START);
    if (!g_main.real_create || !g_main.real_config || !g_main.real_shared ||
        !g_main.real_render || !g_main.real_stop || !g_main.real_delete ||
        !g_main.real_omx_start) {
        LOG_WARN(LOG_MODULE, "loader[main]: stock render/OMX symbols unavailable; disabled");
        return -1;
    }

    base = (uintptr_t)g_main.real_create - STOCK210_OFF_CREATE;
    if ((uintptr_t)g_main.real_config != base + STOCK210_OFF_CONFIG ||
        (uintptr_t)g_main.real_shared != base + STOCK210_OFF_SHARED ||
        (uintptr_t)g_main.real_render != base + STOCK210_OFF_RENDER ||
        (uintptr_t)g_main.real_stop != base + STOCK210_OFF_STOP ||
        (uintptr_t)g_main.real_omx_start != base + STOCK210_OFF_OMX_START) {
        LOG_ERROR(LOG_MODULE, "loader[main]: libairplay layout is not stock 210.81; no patch");
        return -1;
    }

    /* Original ARM words from shipped libairplay.so. */
    text = (uint32_t *)base;
    if (text[STOCK210_OFF_CREATE / 4U] != 0xe59f302cU ||
        text[STOCK210_OFF_CONFIG / 4U] != 0xe92d47f0U ||
        text[STOCK210_OFF_SHARED / 4U] != 0xe5900040U ||
        text[STOCK210_OFF_RENDER / 4U] != 0xe92d45f0U ||
        text[STOCK210_OFF_STOP / 4U] != 0xe92d4070U ||
        text[STOCK210_OFF_OMX_START / 4U] != 0xe92d4ff0U) {
        LOG_ERROR(LOG_MODULE, "loader[main]: stock-210 text fingerprint mismatch; no patch");
        return -1;
    }
    g_main.stock_base = base;
    return 0;
}

static void *main_create_wrapper(unsigned int output_device);
static int main_config_wrapper(void *object, const main_screen_config_t *config);
static void main_stop_wrapper(void *object);
static int main_omx_start_wrapper(void *decoder);

static int install_internal_patches(void)
{
    volatile uintptr_t *create_slot;
    volatile uintptr_t *config_slot;
    volatile uintptr_t *stop_slot;
    volatile uintptr_t *omx_start_slot;

    if (!g_main.stock_base) return -1;
    create_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_CREATE);
    config_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_CONFIG);
    stop_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_STOP);
    omx_start_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_OMX_START);

    /* SET_32 relocations resolve these slots directly to the exported methods.
     * Refuse to overwrite another patch/hook even when the text fingerprint is
     * correct: composing two unknown ownership schemes is not safe. */
    if (*create_slot != (uintptr_t)g_main.real_create ||
        *config_slot != (uintptr_t)g_main.real_config ||
        *stop_slot != (uintptr_t)g_main.real_stop ||
        *omx_start_slot != (uintptr_t)g_main.real_omx_start) {
        LOG_ERROR(LOG_MODULE,
                  "loader[main]: stock GOT already modified; preserving existing path");
        return -1;
    }

    /* Stock 210 has no RELRO; .got is writable.  Publish stop/config first and
     * create LAST as the commit point.  Once stock can receive our prepared
     * object, every later lifecycle call is guaranteed to reach the wrappers. */
    *omx_start_slot = (uintptr_t)main_omx_start_wrapper;
    *stop_slot = (uintptr_t)main_stop_wrapper;
    *config_slot = (uintptr_t)main_config_wrapper;
    __sync_synchronize();
    *create_slot = (uintptr_t)main_create_wrapper;
    __sync_synchronize();

    if (*create_slot != (uintptr_t)main_create_wrapper ||
        *config_slot != (uintptr_t)main_config_wrapper ||
        *stop_slot != (uintptr_t)main_stop_wrapper ||
        *omx_start_slot != (uintptr_t)main_omx_start_wrapper) {
        LOG_ERROR(LOG_MODULE, "loader[main]: stock GOT write verification failed");
        /* This is a four-slot transaction. patches_installed is deliberately
         * still false here, so restore every slot which accepted our value
         * before returning; never leave a partial lifecycle interpose. */
        if (*create_slot == (uintptr_t)main_create_wrapper)
            *create_slot = (uintptr_t)g_main.real_create;
        if (*config_slot == (uintptr_t)main_config_wrapper)
            *config_slot = (uintptr_t)g_main.real_config;
        if (*stop_slot == (uintptr_t)main_stop_wrapper)
            *stop_slot = (uintptr_t)g_main.real_stop;
        if (*omx_start_slot == (uintptr_t)main_omx_start_wrapper)
            *omx_start_slot = (uintptr_t)g_main.real_omx_start;
        __sync_synchronize();
        return -1;
    }
    pthread_mutex_lock(&g_main.lock);
    g_main.patches_installed = 1;
    pthread_mutex_unlock(&g_main.lock);
    LOG_INFO(LOG_MODULE,
             "loader[main]: stock210 GOT installed create=+0x%x config=+0x%x stop=+0x%x omxStart=+0x%x",
             STOCK210_GOT_CREATE, STOCK210_GOT_CONFIG, STOCK210_GOT_STOP,
             STOCK210_GOT_OMX_START);
    return 0;
}

static void uninstall_internal_patches(void)
{
    volatile uintptr_t *create_slot;
    volatile uintptr_t *config_slot;
    volatile uintptr_t *stop_slot;
    volatile uintptr_t *omx_start_slot;

    int installed;

    pthread_mutex_lock(&g_main.lock);
    installed = g_main.patches_installed;
    pthread_mutex_unlock(&g_main.lock);
    if (!installed || !g_main.stock_base) return;
    create_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_CREATE);
    config_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_CONFIG);
    stop_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_STOP);
    omx_start_slot = (volatile uintptr_t *)(g_main.stock_base + STOCK210_GOT_OMX_START);

    /* Disable new adoption first.  A stock object which is already alive can
     * then finish through the original config/stop methods even if this preload
     * is dlclosed or its destructor order differs at process shutdown. */
    if (*create_slot == (uintptr_t)main_create_wrapper)
        *create_slot = (uintptr_t)g_main.real_create;
    __sync_synchronize();
    if (*config_slot == (uintptr_t)main_config_wrapper)
        *config_slot = (uintptr_t)g_main.real_config;
    if (*stop_slot == (uintptr_t)main_stop_wrapper)
        *stop_slot = (uintptr_t)g_main.real_stop;
    if (*omx_start_slot == (uintptr_t)main_omx_start_wrapper)
        *omx_start_slot = (uintptr_t)g_main.real_omx_start;
    __sync_synchronize();
    pthread_mutex_lock(&g_main.lock);
    g_main.patches_installed = 0;
    pthread_mutex_unlock(&g_main.lock);
}

static void destroy_prepared(void *object)
{
    if (!object) return;
    g_main.real_stop(object);
    g_main.real_delete(object);
}

static int prepare_object(void **out_object, unsigned char ***out_buffers)
{
    main_screen_config_t c;
    void *object;
    unsigned char **buffers = NULL;
    int rc;

    memset(&c, 0, sizeof(c));
    c.width = MAIN_WIDTH;
    c.height = MAIN_HEIGHT;
    c.window_width = MAIN_WIDTH;
    c.window_height = MAIN_HEIGHT;
    c.output_device_tag = output_device_tag();
    c.buffer_count = MAIN_BUFFER_COUNT;
    c.screen_format = MAIN_SCREEN_FORMAT;
    c.displayable_id = MAIN_DISPLAYABLE_ID;
    c.usage = MAIN_SCREEN_USAGE;
    pthread_mutex_lock(&g_main.lock);
    if (g_main.learned_config_valid) c = g_main.learned_config;
    pthread_mutex_unlock(&g_main.lock);
    if (!c.output_device_tag) {
        LOG_WARN(LOG_MODULE,
                 "loader[main]: NVSS_VIDEO_OUTPUT_DEVICE missing; preserving stock path");
        return -1;
    }

    object = g_main.real_create(4U); /* OmxVideoImpl::initialize@0x8c698 */
    if (!object) return -1;
    rc = g_main.real_config(object, &c);
    if (rc != 0) {
        LOG_ERROR(LOG_MODULE, "loader[main]: early stock config failed rc=%d", rc);
        g_main.real_delete(object); /* config failure calls stop internally */
        return -1;
    }
    rc = g_main.real_shared(object, &buffers);
    if (rc != 0 || !buffers) {
        LOG_ERROR(LOG_MODULE, "loader[main]: sharedBuffers failed rc=%d buffers=%p",
                  rc, (void *)buffers);
        destroy_prepared(object);
        return -1;
    }
    for (unsigned i = 0; i < MAIN_BUFFER_COUNT; ++i) {
        if (!buffers[i]) {
            LOG_ERROR(LOG_MODULE, "loader[main]: buffer[%u] is NULL", i);
            destroy_prepared(object);
            return -1;
        }
    }

    pthread_mutex_lock(&g_main.lock);
    g_main.config = c;
    pthread_mutex_unlock(&g_main.lock);
    *out_object = object;
    *out_buffers = buffers;
    return 0;
}

static void fill_breathing_frame(unsigned char *buffer, uint64_t elapsed_ms)
{
    unsigned pulse = carplay_loader_breathing_level(elapsed_ms);
    unsigned y = 24U + (pulse * 72U) / 255U;
    if (!buffer) return;
    /* Constant planes are independent of Qualcomm macro-tile ordering. */
    memset(buffer, (int)y, MAIN_Y_BYTES);
    memset(buffer + MAIN_Y_BYTES, 128, MAIN_BUFFER_BYTES - MAIN_Y_BYTES);
}

static void *loader_worker(void *unused)
{
    void *object = NULL;
    unsigned char **buffers = NULL;
    uint64_t started;
    uint64_t next;
    (void)unused;

    pthread_mutex_lock(&g_prepare_gate);
    pthread_mutex_lock(&g_main.lock);
    if (g_main.shutdown || g_main.state != ML_PREPARING) {
        pthread_mutex_unlock(&g_main.lock);
        pthread_mutex_unlock(&g_prepare_gate);
        return NULL;
    }
    pthread_mutex_unlock(&g_main.lock);

    if (prepare_object(&object, &buffers) != 0) {
        pthread_mutex_lock(&g_main.lock);
        if (g_main.state == ML_PREPARING) g_main.state = ML_DISABLED;
        pthread_mutex_unlock(&g_main.lock);
        pthread_mutex_unlock(&g_prepare_gate);
        return NULL;
    }

    pthread_mutex_lock(&g_main.lock);
    if (g_main.shutdown || g_main.state == ML_BYPASS_SESSION ||
        g_main.state == ML_SHUTDOWN) {
        pthread_mutex_unlock(&g_main.lock);
        destroy_prepared(object);
        pthread_mutex_unlock(&g_prepare_gate);
        return NULL;
    }
    g_main.render_object = object;
    g_main.buffers = buffers;
    g_main.frame_index = 0;
    g_main.state = ML_ACTIVE;
    pthread_mutex_unlock(&g_main.lock);
    pthread_mutex_unlock(&g_prepare_gate);

    LOG_INFO(LOG_MODULE,
             "loader[main]: tiled window 59 ready, 1024x480 buffers=12 @30fps object=%p",
             object);
    started = monotonic_ns();
    next = started;
    for (;;) {
        unsigned frame;
        unsigned char *buffer;
        int rc;

        pthread_mutex_lock(&g_main.lock);
        if (g_main.shutdown || g_main.state != ML_ACTIVE) {
            pthread_mutex_unlock(&g_main.lock);
            break;
        }
        frame = g_main.frame_index++;
        buffer = g_main.buffers[frame % MAIN_BUFFER_COUNT];
        pthread_mutex_unlock(&g_main.lock);

        fill_breathing_frame(buffer,
                             (next >= started) ?
                                 (next - started) / 1000000ULL : 0U);
        rc = g_main.real_render(object, buffer);
        if (frame == 0U) {
            LOG_INFO(LOG_MODULE,
                     "loader[main]: first Qcom32m breathing frame posted rc=%d", rc);
        } else if (rc != 0 && frame < 4U) {
            LOG_WARN(LOG_MODULE, "loader[main]: post failed frame=%u rc=%d", frame, rc);
        }

        next += (uint64_t)CARPLAY_LOADER_FRAME_INTERVAL_NS;
        if (monotonic_ns() > next +
            (uint64_t)CARPLAY_LOADER_FRAME_INTERVAL_NS) {
            next = monotonic_ns(); /* drop missed animation deadlines */
        }
        sleep_until_ns(next);
    }
    return NULL;
}

static int start_worker_locked(void)
{
    int rc;
    if (g_main.shutdown || g_main.worker_started ||
        g_main.state == ML_OMX_OWNED || g_main.state == ML_HANDOFF ||
        g_main.state == ML_FALLBACK_CONFIG) return 0;
    g_main.state = ML_PREPARING;
    rc = pthread_create(&g_main.worker, NULL, loader_worker, NULL);
    if (rc == 0) g_main.worker_started = 1;
    else {
        g_main.state = ML_DISABLED;
        LOG_ERROR(LOG_MODULE, "loader[main]: worker create failed rc=%d", rc);
    }
    return rc;
}

static void start_worker(void)
{
    pthread_mutex_lock(&g_main.lock);
    (void)start_worker_locked();
    pthread_mutex_unlock(&g_main.lock);
}

static void join_worker(void)
{
    pthread_t thread;
    int join = 0;
    pthread_mutex_lock(&g_join_gate);
    pthread_mutex_lock(&g_main.lock);
    if (g_main.worker_started) {
        thread = g_main.worker;
        join = 1;
    }
    pthread_mutex_unlock(&g_main.lock);
    /* Never turn a still-running worker into an apparently-free slot.  Self
     * join is not expected in the normal state machine, but clearing the flag
     * there could start a second producer before the first one returns. */
    if (join && pthread_equal(pthread_self(), thread)) {
        pthread_mutex_unlock(&g_join_gate);
        return;
    }
    if (join) pthread_join(thread, NULL);
    pthread_mutex_lock(&g_main.lock);
    if (join) g_main.worker_started = 0;
    pthread_mutex_unlock(&g_main.lock);
    pthread_mutex_unlock(&g_join_gate);
}

static void *main_create_wrapper(unsigned int output_device)
{
    void *object = NULL;
    int adopt = 0;

    if (output_device != 4U) return g_main.real_create(output_device);

    /* If early configuration is in flight, wait for it and adopt the result.
     * If stock reaches this gate before the worker, mark BYPASS so the worker
     * never creates a competing displayable-59 window for this session. */
    pthread_mutex_lock(&g_prepare_gate);
    pthread_mutex_lock(&g_main.lock);
    if (!g_main.shutdown && output_device == 4U && g_main.state == ML_ACTIVE &&
        g_main.render_object) {
        object = g_main.render_object;
        g_main.state = ML_HANDOFF;
        adopt = 1;
    } else if (!g_main.shutdown && output_device == 4U &&
               g_main.state == ML_PREPARING) {
        /* Phone beat early preparation.  Preserve this session completely;
         * worker destroys any half-prepared object before it becomes visible. */
        g_main.state = ML_BYPASS_SESSION;
    }
    pthread_mutex_unlock(&g_main.lock);
    pthread_mutex_unlock(&g_prepare_gate);

    if (!adopt) return g_main.real_create(output_device);
    join_worker(); /* ownership barrier: no loader writer/poster remains */
    LOG_INFO(LOG_MODULE, "loader[main]: prepared object claimed by stock OMX object=%p", object);
    return object;
}

static int main_config_wrapper(void *object, const main_screen_config_t *config)
{
    int is_adopted;
    int exact;
    int rc;

    pthread_mutex_lock(&g_main.lock);
    is_adopted = (object && object == g_main.render_object &&
                  g_main.state == ML_HANDOFF);
    exact = is_adopted && config_equal(config, &g_main.config);
    if (!g_main.first_config_logged) {
        g_main.first_config_logged = 1;
        log_config("loader[main]: first runtime config", config);
    }
    pthread_mutex_unlock(&g_main.lock);

    if (!is_adopted) {
        rc = g_main.real_config(object, config);
        if (rc == 0) remember_runtime_config(config);
        return rc;
    }
    if (exact) {
        pthread_mutex_lock(&g_main.lock);
        g_main.state = ML_OMX_OWNED;
        pthread_mutex_unlock(&g_main.lock);
        LOG_INFO(LOG_MODULE,
                 "loader[main]: handoff OK, same CScreenRender adopted by stock OMX object=%p",
                 object);
        return 0; /* window and all twelve shared buffers already configured */
    }

    /* Safe fallback: the object itself is valid and already owned by stock.
     * Stop destroys/zeros only its Screen resources; config then recreates the
     * normal stock pool from the actual request.  Seamlessness is sacrificed,
     * but main CarPlay remains stock-correct. */
    log_config("loader[main]: adoption mismatch expected", &g_main.config);
    log_config("loader[main]: adoption mismatch actual", config);
    pthread_mutex_lock(&g_main.lock);
    g_main.state = ML_FALLBACK_CONFIG;
    pthread_mutex_unlock(&g_main.lock);
    g_main.real_stop(object);
    rc = g_main.real_config(object, config);
    if (rc == 0) remember_runtime_config(config);
    pthread_mutex_lock(&g_main.lock);
    g_main.state = ML_OMX_OWNED;
    pthread_mutex_unlock(&g_main.lock);
    LOG_WARN(LOG_MODULE,
             "loader[main]: config mismatch; recreated stock window rc=%d object=%p",
             rc, object);
    return rc;
}

static void main_stop_wrapper(void *object)
{
    int was_owned = 0;
    int was_bypass = 0;
    int stops_main = 0;

    /* Close the stream-111 ordering gate before the stock object releases its
     * OMX/Screen resources.  The receiver observes the generation change and
     * waits for the next successful COMXVideoDecoder::start (or its bounded
     * fail-safe) before reconnecting the alt renderer. */
    pthread_mutex_lock(&g_main.lock);
    stops_main = ((object && object == g_main.render_object &&
                   (g_main.state == ML_OMX_OWNED ||
                    g_main.state == ML_HANDOFF)) ||
                  g_main.state == ML_BYPASS_SESSION);
    pthread_mutex_unlock(&g_main.lock);
    if (stops_main) main_loader_main_stream_stopped(NULL);

    g_main.real_stop(object);

    pthread_mutex_lock(&g_main.lock);
    if (object && object == g_main.render_object &&
        (g_main.state == ML_OMX_OWNED || g_main.state == ML_HANDOFF)) {
        g_main.render_object = NULL;
        g_main.buffers = NULL;
        was_owned = 1;
    } else if (g_main.state == ML_BYPASS_SESSION) {
        was_bypass = 1;
        was_owned = 1;
    }
    if (was_owned && !was_bypass && !g_main.shutdown) {
        g_main.state = ML_COLD;
    }
    pthread_mutex_unlock(&g_main.lock);

    if (was_owned && !g_main.shutdown) {
        LOG_INFO(LOG_MODULE, "loader[main]: stock teardown complete; rearming early window");
        /* BYPASS_SESSION may leave a completed preparation worker waiting to
         * be reaped.  Keep the BYPASS state until join finishes so that worker
         * cannot publish a new ACTIVE object while this thread waits. */
        join_worker();
        pthread_mutex_lock(&g_main.lock);
        if (!g_main.shutdown) g_main.state = ML_COLD;
        pthread_mutex_unlock(&g_main.lock);
        start_worker();
    }
}

/* Stock OmxVideoImpl::configure@0x8c550 has the only code xref to the PLT/GOT
 * entry at +0xb9704.  COMXVideoDecoder::start@0x89670 is synchronous: it
 * performs Loaded->Idle->Executing and queues every output buffer before a
 * zero return.  That return is therefore the exact, timer-free point at which
 * the main (type-110) decoder owns its Qualcomm resources. */
static int main_omx_start_wrapper(void *decoder)
{
    uint32_t generation;
    void *session;
    int published = 0;
    int rc;

    pthread_mutex_lock(&g_main.lock);
    generation = g_main.session_generation;
    session = g_main.session;
    pthread_mutex_unlock(&g_main.lock);

    rc = g_main.real_omx_start(decoder);

    pthread_mutex_lock(&g_main.lock);
    if (rc == 0 && session && g_main.session == session &&
        g_main.session_generation == generation) {
        g_main.main_omx_ready = 1;
        published = 1;
    }
    pthread_mutex_unlock(&g_main.lock);

    if (published) {
        LOG_INFO(LOG_MODULE,
                 "OMX ORDER: stock 110 decoder READY session=%p generation=%u",
                 session, (unsigned)generation);
    } else if (rc == 0 && session) {
        LOG_WARN(LOG_MODULE,
                 "OMX ORDER: ignoring stale stock 110 READY session=%p generation=%u",
                 session, (unsigned)generation);
    } else if (rc != 0) {
        LOG_ERROR(LOG_MODULE,
                  "OMX ORDER: stock 110 decoder start FAILED rc=%d session=%p generation=%u; 111 remains gated",
                  rc, session, (unsigned)generation);
    }
    return rc;
}

int main_loader_session_begin(void *session)
{
    int is_new = 0;
    pthread_mutex_lock(&g_main.lock);
    if (g_main.session != session) {
        ++g_main.session_generation;
        if (g_main.session_generation == 0) ++g_main.session_generation;
        g_main.session = session;
        g_main.main_omx_ready = 0;
        is_new = 1;
    }
    pthread_mutex_unlock(&g_main.lock);
    if (is_new) {
        LOG_INFO(LOG_MODULE,
                 "OMX ORDER: new CarPlay session=%p; 111 gated pending stock 110",
                 session);
    }
    return is_new;
}

void main_loader_session_end(void *session)
{
    pthread_mutex_lock(&g_main.lock);
    if (!session || g_main.session == session) {
        ++g_main.session_generation;
        if (g_main.session_generation == 0) ++g_main.session_generation;
        g_main.session = NULL;
        g_main.main_omx_ready = 0;
    }
    pthread_mutex_unlock(&g_main.lock);
}

void main_loader_main_stream_stopped(void *session)
{
    void *active_session = NULL;
    uint32_t generation = 0;

    pthread_mutex_lock(&g_main.lock);
    if (g_main.session && (!session || g_main.session == session)) {
        ++g_main.session_generation;
        if (g_main.session_generation == 0) ++g_main.session_generation;
        g_main.main_omx_ready = 0;
        active_session = g_main.session;
        generation = g_main.session_generation;
    }
    pthread_mutex_unlock(&g_main.lock);

    if (active_session) {
        LOG_INFO(LOG_MODULE,
                 "OMX ORDER: stock 110 stopped session=%p; rearmed generation=%u",
                 active_session, (unsigned)generation);
    }
}

int main_loader_main_omx_ready(void *session)
{
    int ready;
    pthread_mutex_lock(&g_main.lock);
    ready = session && g_main.session == session && g_main.main_omx_ready;
    pthread_mutex_unlock(&g_main.lock);
    return ready;
}

int main_loader_main_omx_gate_available(void)
{
    int available;
    pthread_mutex_lock(&g_main.lock);
    available = g_main.patches_installed && !g_main.shutdown;
    pthread_mutex_unlock(&g_main.lock);
    return available;
}

unsigned int main_loader_session_generation(void *session)
{
    uint32_t generation = 0;
    pthread_mutex_lock(&g_main.lock);
    if (session && g_main.session == session)
        generation = g_main.session_generation;
    pthread_mutex_unlock(&g_main.lock);
    return (unsigned int)generation;
}

static void main_loader_after_screen_register(void)
{
    pthread_mutex_lock(&g_main.lock);
    if (g_main.initialized || g_main.shutdown) {
        pthread_mutex_unlock(&g_main.lock);
        return;
    }
    g_main.initialized = 1;
    pthread_mutex_unlock(&g_main.lock);

    if (!hook_process_is_dio_manager() || !env_enabled()) {
        pthread_mutex_lock(&g_main.lock);
        g_main.state = ML_DISABLED;
        pthread_mutex_unlock(&g_main.lock);
        return;
    }
    if (stock210_resolve() != 0) {
        pthread_mutex_lock(&g_main.lock);
        g_main.state = ML_DISABLED;
        pthread_mutex_unlock(&g_main.lock);
        return;
    }
    /* Close the tiny patch->pthread_create race: a stock create that arrives
     * immediately after the GOT write sees PREPARING and claims BYPASS. */
    pthread_mutex_lock(&g_main.lock);
    g_main.state = ML_PREPARING;
    pthread_mutex_unlock(&g_main.lock);
    if (install_internal_patches() != 0) {
        pthread_mutex_lock(&g_main.lock);
        g_main.state = ML_DISABLED;
        pthread_mutex_unlock(&g_main.lock);
        return;
    }
    start_worker();
}

/* Verified boundary: dio_manager MU1316 calls ScreenRegister at 0x158e38. */
int ScreenRegister(void *screen)
{
    static fn_screen_register_t real;
    int rc;
    if (!real) real = (fn_screen_register_t)dlsym(RTLD_NEXT, "ScreenRegister");
    if (!real) return -1;
    rc = real(screen);
    if (rc == 0) main_loader_after_screen_register();
    return rc;
}

void main_loader_shutdown(void)
{
    void *object = NULL;
    int destroy = 0;

    pthread_mutex_lock(&g_main.lock);
    if (g_main.shutdown) {
        pthread_mutex_unlock(&g_main.lock);
        return;
    }
    g_main.shutdown = 1;
    if (g_main.state == ML_ACTIVE || g_main.state == ML_PREPARING) {
        g_main.state = ML_SHUTDOWN;
    }
    pthread_mutex_unlock(&g_main.lock);
    join_worker();
    uninstall_internal_patches();

    pthread_mutex_lock(&g_main.lock);
    if (g_main.render_object && g_main.state != ML_OMX_OWNED &&
        g_main.state != ML_HANDOFF && g_main.state != ML_FALLBACK_CONFIG) {
        object = g_main.render_object;
        g_main.render_object = NULL;
        g_main.buffers = NULL;
        destroy = 1;
    }
    g_main.state = ML_SHUTDOWN;
    pthread_mutex_unlock(&g_main.lock);
    if (destroy) destroy_prepared(object);
}
