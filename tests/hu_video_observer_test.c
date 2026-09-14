#include "../hook/hu_startup/main_video.c"
#include <assert.h>

static uint8_t received[12];
static int config_result, render_result, config_race, render_race;
static int setup_110 = 110, setup_audio = 103;
static int array_token;
static void *array_item;
static int fake_config(void *self, const void *arg) {
    (void)self; (void)arg;
    if (config_race) { pthread_mutex_lock(&lock); reset_locked(); pthread_mutex_unlock(&lock); }
    return config_result;
}
static int fake_render(void *self, unsigned char *pixels) {
    (void)self; (void)pixels;
    if (render_race) { pthread_mutex_lock(&lock); reset_locked(); pthread_mutex_unlock(&lock); }
    return render_result;
}
static void *fake_string(void *a, const char *b, unsigned c) { (void)a; (void)c; return (void *)b; }
static void *fake_get(void *dict, const void *key) { (void)key; array_item = dict; return &array_token; }
static long fake_count(void *a) { (void)a; return 1; }
static void *fake_item(void *a, long i) { (void)i; (void)a; return array_item; }
static int64_t fake_int(void *a, void *b, int *c) { (void)b; (void)c; return *(int *)a; }
static void fake_release(void *a) { (void)a; }
static unsigned long fake_type(void *p) { return p == &array_token ? 2 : 1; }
static unsigned long fake_dict_type(void) { return 1; }
static unsigned long fake_array_type(void) { return 2; }
hook_result_t bus_on(uint16_t t, bus_handler_t h, void *p) { (void)t; (void)h; (void)p; return HOOK_OK; }
void bus_off(uint16_t t) { (void)t; }
hook_result_t bus_send(uint16_t t, uint8_t f, const uint8_t *p, uint32_t n) {
    assert(t == EVT_HU_VIDEO_STATUS && f == BUS_FLAG_BINARY && n == 12);
    memcpy(received, p, n); return HOOK_OK;
}
static void check_status(int expected) {
    uint8_t nonce[4] = {1, 2, 3, 4};
    query(CMD_HU_VIDEO_STATUS, BUS_FLAG_BINARY, nonce, 4, NULL);
    assert(memcmp(received, nonce, 4) == 0 && received[11] == expected);
}
int main(void) {
    void *session_a = &config_result, *session_b = &render_result, *renderer = &received;
    hook_setup_ctx_t s = {0};
    pthread_once(&resolve_once, resolve);
    real_config = fake_config; real_render = fake_render;
    cf_string = fake_string; cf_get = fake_get; cf_count = fake_count;
    cf_item = fake_item; cf_int = fake_int; cf_release = fake_release;
    cf_type = fake_type; cf_dict_type = fake_dict_type; cf_array_type = fake_array_type;
    s.session = session_a; s.request = &setup_110; setup(&s); check_status(1);
    assert(_ZN3dio13CScreenRender6renderEPh(renderer, NULL) == 0); check_status(1);
    config_result = -7;
    assert(_ZN3dio13CScreenRender6configERKNS_16st_screen_configE(renderer, NULL) == -7); check_status(1);
    config_result = 0; _ZN3dio13CScreenRender6configERKNS_16st_screen_configE(renderer, NULL);
    render_result = -9; assert(_ZN3dio13CScreenRender6renderEPh(renderer, NULL) == -9); check_status(1);
    render_result = 0; _ZN3dio13CScreenRender6renderEPh(renderer, NULL); check_status(2);
    s.request = &setup_audio; setup(&s); check_status(2); // unrelated audio setup
    teardown(session_b, NULL, 0); check_status(2); // old session cannot clear new
    teardown(session_a, &setup_audio, 0); check_status(2);
    s.request = &setup_110; setup(&s); check_status(1);
    _ZN3dio13CScreenRender6renderEPh(renderer, NULL); check_status(1); // old renderer after new SETUP
    config_race = 1; _ZN3dio13CScreenRender6configERKNS_16st_screen_configE(renderer, NULL);
    config_race = 0; _ZN3dio13CScreenRender6renderEPh(renderer, NULL); check_status(1);
    _ZN3dio13CScreenRender6configERKNS_16st_screen_configE(renderer, NULL);
    render_race = 1; _ZN3dio13CScreenRender6renderEPh(renderer, NULL); render_race = 0; check_status(1);
    _ZN3dio13CScreenRender6configERKNS_16st_screen_configE(renderer, NULL);
    _ZN3dio13CScreenRender6renderEPh(renderer, NULL); check_status(2);
    state(NULL, HOOK_EVENT_DISCONNECT, NULL); check_status(0);
    _ZN3dio13CScreenRender6renderEPh(renderer, NULL); check_status(0);
    s.session = session_a; s.request = &setup_110; s.sequence = 20; setup(&s);
    hook_setup_ctx_t old = s;
    s.sequence = 21; setup(&s); old.result = -1; setup_done(&old); check_status(1);
    s.result = -1; setup_done(&s); check_status(0);
    puts("hu_video_observer_test: stock returns, setup filtering, first post, old renderer/config races, disconnect PASS");
}
