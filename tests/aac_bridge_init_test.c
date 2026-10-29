/* Concurrent codec clients must see one complete resolver/configuration. */
#include <dlfcn.h>
#include <pthread.h>
static void *aac_test_dlsym(void *handle, const char *name);
#define dlsym aac_test_dlsym
#include "../hook/audio/aac_bridge.c"
#undef dlsym

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t start_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
static int started, resolve_lookups, config_creates, directories, other_init;
static int config;
static pthread_once_t other_once = PTHREAD_ONCE_INIT;

static int32_t codec_create(void **out) { (void)out; return 0; }
static int32_t codec_init(void *a, void *b, void *c, const void *d, const void *e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; return 0; }
static int32_t codec_write(void *a, const void *b, uint32_t c, int32_t d, int64_t e, int32_t f)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return 0; }
static int32_t codec_simple(void *a) { (void)a; return 0; }
static void initialize_other(void) { ++other_init; }
static void *configuration_worker(void *unused)
{
    (void)unused;
    assert(!pthread_once(&other_once, initialize_other));
    return NULL;
}
static int32_t config_create(void **out)
{
    pthread_t thread;
    ++config_creates;
    /* Native Cinemo waits for a worker that can initialize unrelated libc state. */
    assert(!pthread_create(&thread, NULL, configuration_worker, NULL));
    assert(!pthread_join(thread, NULL));
    *out = &config;
    return 0;
}
static int32_t config_directory(void *object, const char *path)
{
    assert(object == &config && !strcmp(path, "/mnt/app/armle/usr/lib/cinemo"));
    ++directories;
    return 0;
}
static void *aac_test_dlsym(void *handle, const char *name)
{
    struct timespec pause = {0, 1000000};
    (void)handle;
    if (!strcmp(name, "CinemoCreateConfig")) return (void *)config_create;
    if (!strcmp(name, "ICinemoConfig_SetPluginDirectory")) return (void *)config_directory;
    __sync_fetch_and_add(&resolve_lookups, 1);
    nanosleep(&pause, NULL); /* Expose partially published resolver state. */
    if (!strcmp(name, "CinemoCreateAudioCodec")) return (void *)codec_create;
    if (!strcmp(name, "ICinemoAudioCodec_Initialize")) return (void *)codec_init;
    if (!strcmp(name, "ICinemoAudioCodec_Write")) return (void *)codec_write;
    if (!strcmp(name, "ICinemoAudioCodec_Drain") ||
        !strcmp(name, "ICinemoAudioCodec_Flush") ||
        !strcmp(name, "ICinemoAudioCodec_Release")) return (void *)codec_simple;
    assert(0 && "unexpected codec symbol");
    return NULL;
}
static void *client(void *unused)
{
    unsigned i;
    const spa_apac_codec_api_t *api;
    (void)unused;
    pthread_mutex_lock(&start_lock);
    while (!started) pthread_cond_wait(&start_cond, &start_lock);
    pthread_mutex_unlock(&start_lock);
    for (i = 0; i < 100; ++i) {
        assert(aac_resolve());
        assert(g_cinemo.create == codec_create && g_cinemo.initialize == codec_init);
        assert(g_cinemo.write == codec_write && g_cinemo.release == codec_simple);
        api = spa_aac_codec_api();
        assert(api->structSize == sizeof(*api) && api->create && api->decodeS16 &&
               api->reset && api->destroy && api->getInfo);
        aac_register_plugin_directory();
    }
    return NULL;
}
int main(void)
{
    pthread_t clients[8];
    unsigned i;
    alarm(10);
    for (i = 0; i < 8; ++i) assert(!pthread_create(&clients[i], NULL, client, NULL));
    pthread_mutex_lock(&start_lock);
    started = 1;
    pthread_cond_broadcast(&start_cond);
    pthread_mutex_unlock(&start_lock);
    for (i = 0; i < 8; ++i) assert(!pthread_join(clients[i], NULL));
    assert(resolve_lookups == 6 && config_creates == 1 && directories == 1 && other_init == 1);
    puts("aac_bridge_init: concurrent complete resolver, single plugin config, independent worker init PASS");
    return 0;
}
