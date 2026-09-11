/* Compile the REAL interposers/seams against fake native boundaries. This
 * tests hook control flow, not QNX ABI/driver behavior or codec fidelity. */
#define ENABLE_LOGGING 0
#define ENABLE_STATE_TRACE 0
#define SPA_103_TEARDOWN_WAIT_MS 20
#include <dlfcn.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
static void *test_dlsym(void *, const char *);
static void *test_realloc(void *, size_t);
static int test_mutex_lock(pthread_mutex_t *);
static int test_cond_wait(pthread_cond_t *, pthread_mutex_t *);
#define dlsym test_dlsym
#define realloc test_realloc
#define pthread_mutex_lock test_mutex_lock
#define pthread_cond_wait test_cond_wait
#include "../hook/audio/audio_hook.c"
#include "../hook/framework/airplay_seams.c"
#undef dlsym
#undef realloc
#undef pthread_mutex_lock
#undef pthread_cond_wait

static int allocation_fail, params_failures, readback_bad, nonblock_fail;
static int native_writes, params_calls, cancelled_write, short_error, probe_during_params;
static snd_pcm_t media_device = { 1 }, phone_device = { 2 };
static snd_pcm_channel_setup_t device_setup;
static int16_t source_pcm[1536];
static int poll_pipe[2];
static unsigned char *image_base;
static int request_object, response_object, streams_object, desc_object, key_object;
static int request_refs, response_refs, socket_opens, stock_teardowns;
static pthread_mutex_t fault_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fault_cond = PTHREAD_COND_INITIALIZER;
static int delay_at, reached_delay, release_delay;
static int setup_result;
static pthread_mutex_t *generation_pause_mutex;
static int generation_watch_read_wait;
static int native_flushes, native_prepares;

static void fault_pause(int stage)
{
    pthread_mutex_lock(&fault_lock);
    if (delay_at == stage) {
        reached_delay = 1;
        pthread_cond_broadcast(&fault_cond);
        while (!release_delay) pthread_cond_wait(&fault_cond, &fault_lock);
    }
    pthread_mutex_unlock(&fault_lock);
}
/* Observe exact production wait boundaries; no sleeps or scheduler guesses. */
static int test_mutex_lock(pthread_mutex_t *mutex)
{
    if (mutex == generation_pause_mutex) fault_pause(4);
    return pthread_mutex_lock(mutex);
}
static int test_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    if (generation_watch_read_wait && cond == &spa103_render.readDone) {
        pthread_mutex_lock(&fault_lock);
        reached_delay = 1;
        pthread_cond_broadcast(&fault_cond);
        pthread_mutex_unlock(&fault_lock);
    }
    return pthread_cond_wait(cond, mutex);
}
/* Real setup runs through render_start and pauses at its external codec loader. */
const spa_apac_codec_api_t *spa_apac_runtime_load(const char **path)
{
    if (path) *path = "test: no decoder";
    fault_pause(2);
    return NULL;
}

static void *test_realloc(void *p, size_t n)
{
    return allocation_fail ? NULL : realloc(p, n);
}
static int native_params(snd_pcm_t *handle, snd_pcm_channel_params_t *params)
{
    assert(handle == &media_device);
    ++params_calls;
    if (probe_during_params) {
        int writes = native_writes;
        assert(snd_pcm_plugin_write(handle, source_pcm, sizeof(source_pcm)) == -EBUSY);
        assert(native_writes == writes);
    }
    if (params_failures) { --params_failures; return -EIO; }
    if (!params) return -EINVAL;
    device_setup.channel = params->channel;
    device_setup.mode = params->mode;
    device_setup.format = params->format;
    device_setup.buf.block = params->buf.block;
    device_setup.buf.block.frag_size = 768 * params->format.voices * 2;
    params->buf.block.frag_size = device_setup.buf.block.frag_size;
    return 0;
}
static int native_setup(snd_pcm_t *handle, snd_pcm_channel_setup_t *setup)
{
    (void)handle;
    if (!setup) return -EINVAL;
    *setup = device_setup;
    if (readback_bad) setup->format.voices = 7;
    return 0;
}
static int native_info(snd_pcm_t *h, snd_pcm_channel_info_t *info)
{
    (void)h; info->max_voices = 6; return 0;
}
static int native_nonblock(snd_pcm_t *h, int on)
{
    (void)h; assert(on == 1); return nonblock_fail ? -EIO : 0;
}
static int native_fd(snd_pcm_t *h, int ch)
{
    (void)h; (void)ch; return poll_pipe[1];
}
static int native_status(snd_pcm_t *h, snd_pcm_channel_status_t *status)
{
    (void)h; status->status = SND_PCM_STATUS_RUNNING;
    errno = ERANGE; /* exercise errno survival through post-write diagnostics */
    return 0;
}
static int native_flush(snd_pcm_t *h, int channel)
{ assert(h == &media_device && channel == SND_PCM_CHANNEL_PLAYBACK); ++native_flushes; return 0; }
static int native_prepare(snd_pcm_t *h, int channel)
{ assert(h == &media_device && channel == SND_PCM_CHANNEL_PLAYBACK); ++native_prepares; return 0; }
static ssize_t native_write(snd_pcm_t *h, const void *data, size_t bytes)
{
    snd_pcm_channel_setup_t actual;
    ++native_writes;
    if (h == &phone_device) return (ssize_t)bytes;
    assert(h == &media_device);
    /* Mirror real libasound's PLT re-entry; no deadlock, no stereo translation. */
    memset(&actual, 0, sizeof(actual));
    assert(snd_pcm_plugin_setup(h, &actual) == 0);
    assert(actual.format.voices == device_setup.format.voices);
    if (spa_pcm_sink.expandedParams) assert(data != source_pcm);
    if (cancelled_write) {
        pthread_mutex_lock(&spa_pcm_sink.lock);
        spa_pcm_sink.transport.stopping = 1;
        pthread_mutex_unlock(&spa_pcm_sink.lock);
        return 24;
    }
    if (short_error) { errno = EIO; return 24; }
    return (ssize_t)bytes;
}
static snd_pcm_channel_params_t stereo_params(void)
{
    snd_pcm_channel_params_t p;
    memset(&p, 0, sizeof(p));
    p.channel = SND_PCM_CHANNEL_PLAYBACK;
    p.mode = SND_PCM_MODE_BLOCK;
    p.format.format = SND_PCM_SFMT_S16_LE;
    p.format.rate = 48000; p.format.voices = 2;
    p.buf.block.frag_size = sizeof(source_pcm);
    return p;
}
static void configure_ok(void)
{
    snd_pcm_channel_params_t p = stereo_params();
    assert(snd_pcm_plugin_params(&media_device, &p) == 0);
    assert(spa_pcm_sink.paramsState == SPA_PCM_PARAMS_READY);
}
static void qsa_transactions(void)
{
    snd_pcm_channel_params_t p = stereo_params();
    snd_pcm_channel_setup_t setup;
    int calls;
    spa_pcm_sink.handle = &media_device;
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == -EIO);
    allocation_fail = 1;
    assert(snd_pcm_plugin_params(&media_device, &p) == -ENOMEM);
    assert(params_calls == 0 && spa_pcm_sink.paramsState == SPA_PCM_PARAMS_FAILED);
    allocation_fail = 0;
    probe_during_params = 1;
    configure_ok();
    assert(spa_pcm_sink.expandedParams && device_setup.format.voices == 6);
    calls = params_calls;
    spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_OPEN;
    configure_ok();
    assert(params_calls == calls); /* live reclaim still skips destructive params */
    spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_NONE;
    params_failures = 2; p = stereo_params(); calls = native_writes;
    assert(snd_pcm_plugin_params(&media_device, &p) == -EIO);
    assert(device_setup.format.voices == 6); /* both failures leave old device geometry */
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == -EIO);
    assert(native_writes == calls);
    spa_pcm_sink.detached = 1;
    {
        size_t pending = 123;
        double rate = 1;
        int measured = 1;
        assert(!spa_pcm_sink_detached_pending(&pending) && pending == 0);
        assert(!spa_pcm_sink_detached_feedback_rate(&rate, &measured));
        assert(rate == 0 && measured == 0);
        spa_pcm_sink_write_one_detached_period();
        assert(native_writes == calls);
    }
    spa_pcm_sink.detached = 0;
    assert(snd_pcm_plugin_write(&phone_device, source_pcm, sizeof(source_pcm)) == sizeof(source_pcm));
    params_failures = 1; p = stereo_params();
    assert(snd_pcm_plugin_params(&media_device, &p) == 0); /* verified stock rollback */
    assert(!spa_pcm_sink.expandedParams && device_setup.format.voices == 2);
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == sizeof(source_pcm));
    readback_bad = 1; p = stereo_params();
    assert(snd_pcm_plugin_params(&media_device, &p) == -EIO);
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == -EIO);
    readback_bad = 0; nonblock_fail = 1; p = stereo_params();
    assert(snd_pcm_plugin_params(&media_device, &p) == -EIO);
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == -EIO);
    nonblock_fail = 0;
    calls = params_calls;
    spa_pcm_sink.reclaimStage = SPA_PCM_RECLAIM_OPEN;
    configure_ok();
    assert(params_calls > calls); /* failed state cannot skip fresh params via reclaim */
    readback_bad = 1; memset(&setup, 0, sizeof(setup));
    assert(snd_pcm_plugin_setup(&media_device, &setup) != 0);
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == -EIO);
    readback_bad = 0; configure_ok();
    memset(&setup, 0, sizeof(setup));
    assert(snd_pcm_plugin_setup(&media_device, &setup) == 0);
    assert(setup.format.voices == 2); /* external stock caller still sees stereo */
    cancelled_write = 1; errno = 0;
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == 8);
    assert(errno == ECANCELED); /* two accepted PCM6 frames -> two stereo frames */
    cancelled_write = 0; spa_pcm_sink.transport.stopping = 0;
    short_error = 1;
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == 8);
    assert(errno == EIO);
    short_error = 0;
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == sizeof(source_pcm));
    assert(errno == 0);
    /* Real buffered peek/write/commit must drop ONLY the accepted two frames. */
    spa103_render.ring = calloc(spa103_ring_frames() * 6, sizeof(int16_t));
    assert(spa103_render.ring);
    spa103_render.state = SPA_103_RENDER_RUNNING;
    spa103_render.outputChannels = 6;
    spa103_render.fill = 768; spa103_render.head = 0; spa103_render.primed = 1;
    spa_apap_lifecycle_init(&spa103_render.lifecycle, 0);
    spa_apap_lifecycle_set_rate(&spa103_render.lifecycle, 1);
    cancelled_write = 1;
    assert(snd_pcm_plugin_write(&media_device, source_pcm, sizeof(source_pcm)) == 8);
    assert(errno == ECANCELED && spa103_render.fill == 766 && spa103_render.head == 2);
    assert(spa103_render.reservedReadFrames == 0);
    cancelled_write = 0; spa_pcm_sink.transport.stopping = 0;
    free(spa103_render.ring); spa103_render.ring = NULL;
    spa103_render.state = SPA_103_RENDER_IDLE;
    spa103_render.fill = 0;
    spa_apap_lifecycle_dispose(&spa103_render.lifecycle);
    spa_pcm_sink.handle = NULL;
    puts("audio_interposer_test: QSA transactions/rollback/partial-error PASS");
}

static void *fake_string(void *allocator, const char *text, uint32_t encoding)
{ (void)allocator; (void)encoding; return (void *)text; }
static const void *fake_retain(const void *p)
{
    if (p == &request_object) ++request_refs;
    if (p == &response_object) ++response_refs;
    return p;
}
static void fake_release(const void *p)
{
    if (p == &request_object) --request_refs;
    if (p == &response_object) --response_refs;
}
static void *fake_dict_get(const void *dict, const void *key)
{
    const char *name = key;
    if (dict == &request_object && !strcmp(name, "streams")) return &streams_object;
    if (dict == &desc_object) {
        if (!strcmp(name, "audioType")) return "media";
        if (!strcmp(name, "shk")) return &key_object;
        if (!strcmp(name, "type") || !strcmp(name, "audioFormatIndex") ||
            !strcmp(name, "spf") || !strcmp(name, "isMedia") ||
            !strcmp(name, "packetFormatAPAP") || !strcmp(name, "audioFormat"))
            return &desc_object; /* presence check; numeric value comes from GetInt64 */
    }
    return NULL;
}
static int64_t fake_dict_int(const void *dict, const void *key, int32_t *error)
{
    const char *name = key;
    *error = 0;
    if (dict == &desc_object) {
        if (!strcmp(name, "type")) return 103;
        if (!strcmp(name, "audioFormatIndex")) return 65;
        if (!strcmp(name, "spf")) return 1024;
        if (!strcmp(name, "isMedia") || !strcmp(name, "packetFormatAPAP")) return 1;
        if (!strcmp(name, "audioFormat")) return 0;
    }
    *error = -1;
    return 0;
}
static long fake_array_count(const void *p) { return p == &streams_object ? 1 : 0; }
static void *fake_array_index(const void *p, long i)
{ return p == &streams_object && i == 0 ? &desc_object : NULL; }
static int fake_string_copy(const void *p, char *out, size_t size)
{ snprintf(out, size, "%s", (const char *)p); return 1; }
static void *fake_typed_get(const void *dict, const void *key, uint32_t type, int32_t *error)
{ (void)type; *error = 0; return fake_dict_get(dict, key); }
static uint32_t fake_type_id(void) { return 1; }
static long fake_data_length(const void *p) { assert(p == &key_object); return 32; }
static const uint8_t *fake_data_bytes(const void *p)
{ static const uint8_t key[32] = { 0 }; assert(p == &key_object); return key; }
static void unused_native(void) { assert(!"unexpected native boundary"); }
static int native_socket_open(int family, int type, int protocol, int preferred,
                               int *port, int flags, int *sock)
{
    (void)type; (void)protocol; (void)preferred; (void)port; (void)flags; (void)sock;
    assert(family == AF_INET);
    ++socket_opens;
    return -EIO; /* stop before the private ARM dispatcher (not executed on host) */
}
static int native_airplay_setup(void *session, void *request, void **response, int extra)
{
    (void)session; (void)extra;
    assert(request == &request_object);
    fault_pause(1);
    *response = &response_object;
    return 0;
}
static int native_airplay_teardown(void *session, void *request, int reason, int extra)
{
    (void)request; (void)reason; (void)extra;
    ++stock_teardowns;
    free(session); /* deliberately poison the former private fields for ASan */
    return 0;
}
static void *setup_thread(void *session)
{
    void *response = NULL;
    setup_result = AirPlayReceiverSessionSetup(session, &request_object, &response, 0);
    return NULL;
}
static void setup_lifetime(void)
{
    int stage, iteration;
    cfa_ready = 1;
    cfa_str = fake_string; cfa_retain = fake_retain; cfa_rel = fake_release;
    cfa_dictget = fake_dict_get; cfa_dictgeti = fake_dict_int;
    cfa_arrcount = fake_array_count; cfa_arridx = fake_array_index;
    cfa_strgetc = fake_string_copy;
    for (stage = 1; stage <= 2; ++stage) {
        for (iteration = 0; iteration < 32; ++iteration) {
            unsigned char *session = calloc(1, 128);
            pthread_t worker;
            int opens = socket_opens;
            assert(session);
            session[85] = AF_INET;
            delay_at = stage; reached_delay = release_delay = 0;
            assert(!pthread_create(&worker, NULL, setup_thread, session));
            pthread_mutex_lock(&fault_lock);
            while (!reached_delay) pthread_cond_wait(&fault_cond, &fault_lock);
            pthread_mutex_unlock(&fault_lock);
            assert(AirPlayReceiverSessionTearDown(session, NULL, 0, 0) == 0);
            pthread_mutex_lock(&fault_lock);
            release_delay = 1;
            pthread_cond_broadcast(&fault_cond);
            pthread_mutex_unlock(&fault_lock);
            assert(!pthread_join(worker, NULL));
            if (!setup_result || socket_opens != opens)
                fprintf(stderr, "race stage=%d iteration=%d result=%d sockets=%d/%d\n",
                        stage, iteration, setup_result, socket_opens, opens);
            assert(setup_result != 0 && socket_opens == opens);
            assert(!spa103_receiver.session && !spa103_receiver.settingUp && !spa103_receiver.joining);
            assert(request_refs == 0 && response_refs == 0);
        }
    }
    /* New SETUP is accepted for processing after the timed-out one retires. */
    {
        unsigned char *session = calloc(1, 128);
        void *response = NULL;
        int opens = socket_opens;
        session[85] = AF_INET; delay_at = 0;
        assert(AirPlayReceiverSessionSetup(session, &request_object, &response, 0) == -EIO);
        assert(socket_opens == opens + 1);
        assert(!spa103_receiver.session && !spa103_receiver.settingUp);
        assert(AirPlayReceiverSessionTearDown(session, NULL, 0, 0) == 0);
    }
    /* A late old fail path cannot stop a newer same-session generation. */
    spa103_receiver.session = &request_object;
    spa103_receiver.generation = 100;
    spa103_receiver.settingUp = 1;
    assert(!spa103_stop_receiver(&request_object, 0, 99));
    assert(spa103_receiver.session == &request_object && !spa103_receiver.stop);
    spa103_receiver.session = NULL; spa103_receiver.settingUp = 0;
    assert(stock_teardowns == 65);
    puts("audio_interposer_test: real AirPlay SETUP/teardown seams, 64 races + restart PASS");
}

static int successor_session, control_params, dynamic_queries;
static int control_switch_generation, requested_rate, control_result;
static uint32_t control_generation;
static enum spa103_control_method control_method;
static unsigned char *control_request;
static spa_apap_flush_range_t test_flush_range;

static void wait_fault(void)
{
    pthread_mutex_lock(&fault_lock);
    while (!reached_delay) pthread_cond_wait(&fault_cond, &fault_lock);
    pthread_mutex_unlock(&fault_lock);
}
static void release_fault(void)
{
    pthread_mutex_lock(&fault_lock);
    release_delay = 1;
    pthread_cond_broadcast(&fault_cond);
    pthread_mutex_unlock(&fault_lock);
}
static void *pause_after_ticket(void *allocator, const char *name, uint32_t encoding)
{
    /* Request diagnostics query first; the second follows the private snapshot. */
    if (!strcmp(name, "supportsDynamicStreamID") && ++dynamic_queries == 2)
        fault_pause(3);
    return fake_string(allocator, name, encoding);
}
static void set_generation_fixture(uint32_t generation, uint32_t sequence,
                                    void *session, int playing)
{
    pthread_mutex_lock(&spa103_lock);
    spa103_receiver.session = session;
    spa103_receiver.generation = generation;
    spa103_receiver.sequence = sequence;
    spa103_receiver.stop = spa103_receiver.failed = spa103_receiver.settingUp = 0;
    spa103_receiver.joining = 0;
    spa103_receiver.rate = playing;
    pthread_mutex_unlock(&spa103_lock);
    pthread_mutex_lock(&spa103_render.lock);
    spa103_render.generation = generation;
    spa_apap_lifecycle_set_rate(&spa103_render.lifecycle, playing);
    pthread_mutex_unlock(&spa103_render.lock);
}
static void stale_setup_generations(void)
{
    int same_pointer, iteration;
    for (same_pointer = 0; same_pointer <= 1; ++same_pointer) {
        for (iteration = 0; iteration < 16; ++iteration) {
            unsigned char *old = calloc(1, 128);
            void *next = same_pointer ? old : (void *)&successor_session;
            pthread_t worker;
            uint32_t generation, sequence;
            assert(old); old[85] = AF_INET;
            cfa_str = pause_after_ticket; dynamic_queries = 0;
            delay_at = 3; reached_delay = release_delay = 0;
            assert(!pthread_create(&worker, NULL, setup_thread, old));
            wait_fault();
            pthread_mutex_lock(&spa103_lock);
            spa103_setup_session = next;
            sequence = ++spa103_setup_sequence;
            generation = spa103_receiver.generation + 1;
            pthread_mutex_unlock(&spa103_lock);
            /* Successor-commit fixture: no native ARM dispatcher is executed. */
            set_generation_fixture(generation, sequence, next, 1);
            release_fault();
            assert(!pthread_join(worker, NULL));
            assert(setup_result == -ECANCELED);
            assert(spa103_receiver.session == next && !spa103_receiver.stop);
            assert(spa103_receiver.generation == generation);
            assert(spa103_render.generation == generation);
            assert(spa_apap_lifecycle_playing(&spa103_render.lifecycle));
            assert(request_refs == 0 && response_refs == 0);
            spa103_receiver.session = NULL; spa103_setup_valid = 0;
            free(old);
        }
    }
    cfa_str = fake_string; delay_at = 0;
}
static void *absent_cleanup_thread(void *unused)
{
    (void)unused;
    control_result = spa103_stop_receiver_checked(0, 1, 0, &request_object, 100);
    return NULL;
}
static void absent_cleanup_generation(void)
{
    pthread_t worker;
    spa103_receiver.session = NULL;
    spa103_setup_session = &request_object; spa103_setup_sequence = 100;
    spa103_setup_valid = 1;
    generation_pause_mutex = &spa_pcm_sink.ioLock;
    delay_at = 4; reached_delay = release_delay = 0;
    assert(!pthread_create(&worker, NULL, absent_cleanup_thread, NULL));
    wait_fault();
    pthread_mutex_lock(&spa103_lock);
    assert(spa103_receiver.joining);
    spa103_setup_session = &successor_session; spa103_setup_sequence = 101;
    pthread_mutex_unlock(&spa103_lock);
    /* No receiver exists, but its global QSA cleanup still owns the join gate. */
    assert(spa103_stop_receiver_checked(0, 1, 0, &successor_session, 101) == -EBUSY);
    release_fault(); assert(!pthread_join(worker, NULL));
    assert(!control_result && !spa103_receiver.joining);
    generation_pause_mutex = NULL; delay_at = 0;
    assert(!spa103_stop_receiver_checked(0, 1, 0, &successor_session, 101));
    spa103_setup_valid = 0;
}
static void *generation_plist(const void *bytes, size_t size, uint32_t flags,
                              uint32_t type, int32_t *error)
{
    (void)bytes; (void)size; (void)flags; (void)type;
    if (control_switch_generation)
        /* Keep the same sequence deliberately: generation is independently checked. */
        set_generation_fixture(control_generation + 1, 500, &successor_session, !requested_rate);
    *error = 0;
    return &control_params;
}
static int64_t generation_dict_int(const void *dict, const void *key, int32_t *error)
{
    if (dict == &control_params) {
        if (!strcmp((const char *)key, "rate")) { *error = 0; return requested_rate; }
        if (!strcmp((const char *)key, "flushUntilSeq")) { *error = 0; return 100; }
    }
    return fake_dict_int(dict, key, error);
}
static void *generation_dict_get(const void *dict, const void *key)
{
    if (dict == &control_params && (!strcmp((const char *)key, "rate") ||
                                    !strcmp((const char *)key, "flushUntilSeq")))
        return &control_params;
    return fake_dict_get(dict, key);
}
static int generation_status(void *connection, int status)
{ (void)connection; assert(status == 200); return 0; }
static int generation_send(void *connection) { (void)connection; return 0; }
static void *generation_control_thread(void *unused)
{
    (void)unused;
    control_result = spa103_process_buffered_control(NULL, control_request,
                                                    control_method, "test-control");
    return NULL;
}
static void *generation_flush_thread(void *unused)
{
    (void)unused;
    control_result = spa103_render_flush(control_generation, &test_flush_range);
    return NULL;
}
static void control_generation_tests(void)
{
    const spa103_private_profile_t *profile = spa103_private_profile();
    unsigned char *allocation = calloc(1, 0x2210);
    const unsigned char body = 0;
    const void *body_pointer = &body;
    uint32_t body_size = 1;
    pthread_t worker;
    int method, pause, flushes, prepares;
    assert(allocation && profile);
    /* Align the pointer at its ARM field offset for the host's wider ABI. */
    control_request = allocation +
        ((sizeof(void *) - profile->httpBodyPointerOffset % sizeof(void *)) % sizeof(void *));
    memcpy(control_request + profile->httpBodyPointerOffset, &body_pointer, sizeof(body_pointer));
    memcpy(control_request + profile->httpBodyLengthOffset, &body_size, sizeof(body_size));
    spa103_cf_create_plist_bytes = generation_plist;
    cfa_dictgeti = generation_dict_int; cfa_dictget = generation_dict_get;
    spa103_http_init_response = generation_status; spa103_http_send_response = generation_send;

    /* All four full handlers must reject a generation replaced during parsing. */
    control_switch_generation = 1;
    for (method = SPA103_CONTROL_GET_ANCHOR; method <= SPA103_CONTROL_FLUSH; ++method) {
        for (pause = 0; pause <= 1; ++pause) {
            requested_rate = pause; control_generation = 600 + method * 2 + pause;
            set_generation_fixture(control_generation, 500, &request_object, !pause);
            spa103_render.primed = 1;
            control_method = (enum spa103_control_method)method;
            generation_control_thread(NULL);
            assert(control_result == 0 && spa103_receiver.rate == !pause);
            assert(spa_apap_lifecycle_playing(&spa103_render.lifecycle) == !pause);
            assert(spa103_render.primed == 1); /* stale FLUSH cannot de-prime successor */
        }
    }
    control_switch_generation = 0;
    /* Current commands still work, including pause and resume. */
    control_generation = 700; control_method = SPA103_CONTROL_SET_RATE_ANCHOR;
    set_generation_fixture(700, 500, &request_object, 1);
    for (requested_rate = 0; requested_rate <= 1; ++requested_rate) {
        generation_control_thread(NULL);
        assert(!control_result && spa103_receiver.rate == requested_rate);
        assert(spa_apap_lifecycle_playing(&spa103_render.lifecycle) == requested_rate);
    }

    /* Replacement AFTER receiver check, while rate waits for renderer lock. */
    requested_rate = 0; generation_pause_mutex = &spa103_render.lock;
    delay_at = 4; reached_delay = release_delay = 0;
    assert(!pthread_create(&worker, NULL, generation_control_thread, NULL));
    wait_fault();
    set_generation_fixture(701, 501, &successor_session, 1);
    release_fault(); assert(!pthread_join(worker, NULL));
    assert(!control_result && spa103_receiver.rate == 1);
    assert(spa_apap_lifecycle_playing(&spa103_render.lifecycle));
    generation_pause_mutex = NULL; delay_at = 0;

    memset(&test_flush_range, 0, sizeof(test_flush_range));
    test_flush_range.have_until_sequence = 1; test_flush_range.until_sequence = 100;
    /* FLUSH must revalidate after cond_wait released render.lock. */
    control_generation = 800;
    set_generation_fixture(800, 800, &request_object, 1);
    spa103_render.reservedReadFrames = 1;
    generation_watch_read_wait = 1; reached_delay = 0;
    assert(!pthread_create(&worker, NULL, generation_flush_thread, NULL));
    wait_fault();
    set_generation_fixture(801, 801, &successor_session, 1);
    pthread_mutex_lock(&spa103_render.lock);
    spa103_render.primed = 1;
    /* Leave a successor reservation active: stale FLUSH must not wait for it. */
    pthread_cond_broadcast(&spa103_render.readDone);
    pthread_mutex_unlock(&spa103_render.lock);
    assert(!pthread_join(worker, NULL));
    assert(control_result == 0 && spa103_render.primed == 1);
    generation_watch_read_wait = 0; spa103_render.reservedReadFrames = 0;

    /* Replacement after software pruning but before native QSA flush. */
    spa_pcm_sink.handle = &media_device; configure_ok();
    flushes = native_flushes; prepares = native_prepares;
    control_generation = 900;
    set_generation_fixture(900, 900, &request_object, 1);
    generation_pause_mutex = &spa_pcm_sink.ioLock;
    delay_at = 4; reached_delay = release_delay = 0;
    assert(!pthread_create(&worker, NULL, generation_flush_thread, NULL));
    wait_fault();
    set_generation_fixture(901, 901, &successor_session, 1);
    spa103_render.primed = 1;
    release_fault(); assert(!pthread_join(worker, NULL));
    generation_pause_mutex = NULL; delay_at = 0;
    assert(control_result == 0 && spa103_render.primed == 1);
    assert(native_flushes == flushes && native_prepares == prepares);
    assert(spa103_render_flush(901, &test_flush_range) == 1);
    assert(native_flushes == flushes + 1 && native_prepares == prepares + 1);
    assert(spa103_render.primed == 0);
    spa_pcm_sink.handle = NULL; spa103_receiver.session = NULL;
    spa103_render_stop();
    assert(spa103_render.generation == 0 && !spa103_render_set_rate(901, 1));
    free(allocation);
    puts("audio_interposer_test: stale SETUP (32), controls, PCM-wait and QSA-wait generations PASS");
}

/* Framework dependencies are fake native boundaries, not alternative seams. */
hook_result_t hook_framework_init(void) { return HOOK_OK; }
size_t hook_framework_module_count(void) { return 1; }
const hook_module_def_t *hook_framework_module_at(size_t n) { return n ? NULL : &audio_module_def; }
hook_result_t hook_inject_message(uint16_t id, const uint8_t *p, size_t n)
{ (void)id; (void)p; (void)n; return HOOK_ERR_NOT_FOUND; }

static void *test_dlsym(void *scope, const char *name)
{
    (void)scope;
#define SYMBOL(n, f) if (!strcmp(name, n)) return (void *)(uintptr_t)(f)
    SYMBOL("snd_pcm_plugin_params", native_params);
    SYMBOL("snd_pcm_plugin_setup", native_setup);
    SYMBOL("snd_pcm_plugin_write", native_write);
    SYMBOL("snd_pcm_plugin_status", native_status);
    SYMBOL("snd_pcm_plugin_flush", native_flush);
    SYMBOL("snd_pcm_plugin_prepare", native_prepare);
    SYMBOL("snd_pcm_channel_info", native_info);
    SYMBOL("snd_pcm_nonblock_mode", native_nonblock);
    SYMBOL("snd_pcm_file_descriptor", native_fd);
    SYMBOL("AirPlayReceiverSessionSetup", native_airplay_setup);
    SYMBOL("AirPlayReceiverSessionTearDown", native_airplay_teardown);
    SYMBOL("ServerSocketOpen", native_socket_open);
    SYMBOL("CFDictionaryGetTypeID", fake_type_id);
    SYMBOL("CFDataGetTypeID", fake_type_id);
    SYMBOL("CFDictionaryGetTypedValue", fake_typed_get);
    SYMBOL("CFDataGetLength", fake_data_length);
    SYMBOL("CFDataGetBytePtr", fake_data_bytes);
    SYMBOL("SocketAccept", unused_native);
    SYMBOL("CFCreateWithPlistBytes", unused_native);
    SYMBOL("HTTPConnectionInitResponse", unused_native);
    SYMBOL("HTTPConnectionSendResponse", unused_native);
    SYMBOL("UpTicks", unused_native);
    SYMBOL("MillisecondsToUpTicks", unused_native);
    SYMBOL("AirTunesClock_GetSynchronizedTimeNearUpTicks", unused_native);
    if (!strcmp(name, "AirTunesServer_EnsureCreated")) return image_base + 0x2e9ac;
#undef SYMBOL
    return NULL;
}
int main(void)
{
    assert(!setenv("AUDIO_SPATIAL", "1", 1));
    assert(!setenv("AUDIO_STEREO_UPMIX", "off", 1));
    assert(!pipe(poll_pipe));
    assert(!posix_memalign((void **)&image_base, 4096, 0xc0000));
    memset(image_base, 0, 0xc0000);
    memcpy(image_base + 0x2e9ac, spa103_private_profiles[0].ensureCreatedPrologue, 12);
    memcpy(image_base + 0x2f040, spa103_private_profiles[0].setPlistResponsePrologue, 12);
    qsa_transactions();
    setup_lifetime();
    stale_setup_generations();
    absent_cleanup_generation();
    control_generation_tests();
    free(spa_pcm_sink.scratch);
    spa_pcm_sink.scratch = NULL; spa_pcm_sink.scratchBytes = 0;
    free(image_base);
    close(poll_pipe[0]); close(poll_pipe[1]);
    return 0;
}
