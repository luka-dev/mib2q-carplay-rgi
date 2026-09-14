/* Read-only observation of stock libairplay's main-video renderer.
 * No window/buffer ownership, visibility changes, GOT writes or frame painting. */
#include "../framework/hook_framework.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t resolve_once = PTHREAD_ONCE_INIT;
static void *owner, *air_session;
static uint32_t epoch, setup_sequence;
static int posted;
static int (*real_config)(void *, const void *);
static int (*real_render)(void *, unsigned char *);
static void *(*cf_string)(void *, const char *, unsigned);
static void *(*cf_get)(void *, const void *);
static long (*cf_count)(void *);
static void *(*cf_item)(void *, long);
static int64_t (*cf_int)(void *, void *, int *);
static void (*cf_release)(void *);
static unsigned long (*cf_type)(void *), (*cf_dict_type)(void), (*cf_array_type)(void);

static void resolve(void) {
    *(void **)(&real_config) = dlsym(RTLD_NEXT, "_ZN3dio13CScreenRender6configERKNS_16st_screen_configE");
    *(void **)(&real_render) = dlsym(RTLD_NEXT, "_ZN3dio13CScreenRender6renderEPh");
    *(void **)(&cf_string) = dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
    *(void **)(&cf_get) = dlsym(RTLD_DEFAULT, "CFDictionaryGetValue");
    *(void **)(&cf_count) = dlsym(RTLD_DEFAULT, "CFArrayGetCount");
    *(void **)(&cf_item) = dlsym(RTLD_DEFAULT, "CFArrayGetValueAtIndex");
    *(void **)(&cf_int) = dlsym(RTLD_DEFAULT, "CFDictionaryGetInt64");
    *(void **)(&cf_release) = dlsym(RTLD_DEFAULT, "CFRelease");
    *(void **)(&cf_type) = dlsym(RTLD_DEFAULT, "CFGetTypeID");
    *(void **)(&cf_dict_type) = dlsym(RTLD_DEFAULT, "CFDictionaryGetTypeID");
    *(void **)(&cf_array_type) = dlsym(RTLD_DEFAULT, "CFArrayGetTypeID");
}
static int has_main(void *request) {
    void *ks, *kt, *arr; long i, count; int found = 0;
    pthread_once(&resolve_once, resolve);
    if (!request || !cf_string || !cf_get || !cf_count || !cf_item || !cf_int || !cf_release
            || !cf_type || !cf_dict_type || !cf_array_type || cf_type(request) != cf_dict_type()) return 0;
    ks = cf_string(NULL, "streams", 0x08000100); kt = cf_string(NULL, "type", 0x08000100);
    if (ks && kt && (arr = cf_get(request, ks)) != NULL && cf_type(arr) == cf_array_type()) {
        count = cf_count(arr);
        for (i = 0; i < count && i < 64; i++) {
            void *item = cf_item(arr, i);
            if (item && cf_type(item) == cf_dict_type() && cf_int(item, kt, NULL) == 110) { found = 1; break; }
        }
    }
    if (ks) cf_release(ks);
    if (kt) cf_release(kt);
    return found;
}
static void reset_locked(void) { if (++epoch == 0) ++epoch; owner = NULL; posted = 0; }
static void setup(hook_setup_ctx_t *s) {
    if (!has_main(s->request)) return;
    pthread_mutex_lock(&lock); reset_locked(); air_session = s->session;
    setup_sequence = s->sequence; pthread_mutex_unlock(&lock);
}
static void setup_done(hook_setup_ctx_t *s) {
    if (s->result == 0) return;
    pthread_mutex_lock(&lock);
    if (s->sequence == setup_sequence && s->session == air_session) {
        reset_locked(); air_session = NULL;
    }
    pthread_mutex_unlock(&lock);
}
static void teardown(void *session, void *request, int reason) {
    (void)reason;
    if (request && !has_main(request)) return;
    pthread_mutex_lock(&lock);
    if (session == air_session) { reset_locked(); air_session = NULL; }
    pthread_mutex_unlock(&lock);
}
static void state(hook_context_t *ctx, int event, void *data) {
    (void)ctx; (void)data;
    if (event == HOOK_EVENT_IDENTIFY_START || event == HOOK_EVENT_DISCONNECT || event == HOOK_EVENT_SHUTDOWN) {
        pthread_mutex_lock(&lock); reset_locked(); air_session = NULL; pthread_mutex_unlock(&lock);
    }
}

/* MU1316 libairplay 210.81: config @0x8bcc8, render @0x8b5c0.
 * render returns zero only after its screen_post_window succeeds (@0x8b690).
 * The configuration boundary associates the renderer with the current setup;
 * an old in-flight render/config cannot mark a newer attempt as presented. */
HOOK_EXPORT int _ZN3dio13CScreenRender6configERKNS_16st_screen_configE(void *self, const void *config) {
    uint32_t generation; int result;
    pthread_once(&resolve_once, resolve);
    if (!real_config) return -1;
    pthread_mutex_lock(&lock); reset_locked(); generation = epoch; pthread_mutex_unlock(&lock);
    result = real_config(self, config);
    pthread_mutex_lock(&lock);
    if (generation == epoch && air_session && result == 0) owner = self;
    pthread_mutex_unlock(&lock);
    return result;
}
HOOK_EXPORT int _ZN3dio13CScreenRender6renderEPh(void *self, unsigned char *pixels) {
    uint32_t generation; int observe, result;
    pthread_once(&resolve_once, resolve);
    if (!real_render) return -1;
    pthread_mutex_lock(&lock); generation = epoch; observe = owner == self && !posted; pthread_mutex_unlock(&lock);
    result = real_render(self, pixels);
    if (observe && result == 0) {
        pthread_mutex_lock(&lock);
        if (generation == epoch && owner == self) posted = 1;
        pthread_mutex_unlock(&lock);
    }
    return result;
}
static void put32(uint8_t *p, uint32_t x) { p[0]=x>>24; p[1]=x>>16; p[2]=x>>8; p[3]=x; }
static void query(uint16_t type, uint8_t flags, const uint8_t *p, uint32_t len, void *ctx) {
    uint8_t reply[12]; (void)type; (void)ctx;
    if (!p || len != 4 || flags != BUS_FLAG_BINARY) return;
    memcpy(reply, p, 4); // Echo the Java request nonce; no sticky replay across connections.
    pthread_mutex_lock(&lock);
    put32(reply + 4, epoch); put32(reply + 8, posted ? 2 : air_session ? 1 : 0);
    pthread_mutex_unlock(&lock);
    bus_send(EVT_HU_VIDEO_STATUS, BUS_FLAG_BINARY, reply, sizeof(reply));
}
static void init(void) { bus_on(CMD_HU_VIDEO_STATUS, query, NULL); }
static void shutdown(void) { bus_off(CMD_HU_VIDEO_STATUS); }
const hook_module_def_t hu_startup_module_def = {
    .name = "hu-startup-observer", .priority = HOOK_PRIORITY_NORMAL,
    .on_init = init, .on_shutdown = shutdown, .on_state = state,
    .airplay = { .on_setup_request = setup, .on_setup_response_final = setup_done, .on_teardown = teardown }
};
