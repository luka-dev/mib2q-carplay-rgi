/* Host harness, part 2 (included by altscreen_card_display_fixture.h): recording
 * altav/bus leaves, waits, the phone side, SETUP/TEARDOWN requests and wiring. */
#ifndef ALTSCREEN_CARD_DISPLAY_PHONE_H
#define ALTSCREEN_CARD_DISPLAY_PHONE_H

/* ── altav / bus leaves: record every generation that reaches the transport ─ */
static uint32_t t_owner;
static uint32_t t_av_gens[64];
static unsigned t_av_gen_count;
static unsigned t_au_count;
static void t_av_seen(uint32_t gen)
{
    unsigned i;
    pthread_mutex_lock(&t_ev_lock);
    for (i = 0; i < t_av_gen_count; ++i)
        if (t_av_gens[i] == gen) break;
    if (i == t_av_gen_count && t_av_gen_count < 64) t_av_gens[t_av_gen_count++] = gen;
    pthread_mutex_unlock(&t_ev_lock);
}
static int t_av_gen_seen(uint32_t gen)
{
    unsigned i;
    int seen = 0;
    pthread_mutex_lock(&t_ev_lock);
    for (i = 0; i < t_av_gen_count; ++i) seen |= t_av_gens[i] == gen;
    pthread_mutex_unlock(&t_ev_lock);
    return seen;
}
int altav_async_start(void (*failed)(uint32_t, void *), void *context)
{ (void)failed; (void)context; return 0; }
void altav_shutdown(void) {}
int altav_claim(uint32_t gen)
{
    int won;
    t_av_seen(gen);
    pthread_mutex_lock(&t_ev_lock);
    won = gen > t_owner;
    if (won) t_owner = gen;
    pthread_mutex_unlock(&t_ev_lock);
    trace_add("claim(%u)=%d", (unsigned)gen, won);
    return won;
}
int altav_is_owner(uint32_t gen)
{
    int own;
    t_av_seen(gen);
    pthread_mutex_lock(&t_ev_lock);
    own = gen == t_owner;
    pthread_mutex_unlock(&t_ev_lock);
    return own;
}
int altav_enqueue_config(uint32_t gen, const uint8_t *config, uint32_t len)
{
    (void)config;
    t_av_seen(gen);
    trace_add("cfg(%u,%u)", (unsigned)gen, (unsigned)len);
    return 0;
}
int altav_enqueue_owned(uint32_t gen, uint8_t **data, uint32_t *capacity,
                        const alt_avcc_frame_t *info, const alt_frame_meta_t *meta)
{
    (void)data; (void)capacity; (void)meta;
    t_av_seen(gen);
    pthread_mutex_lock(&t_ev_lock);
    ++t_au_count;
    pthread_mutex_unlock(&t_ev_lock);
    trace_add("au(%u,%u)", (unsigned)gen, (unsigned)info->annexb_len);
    return 0;
}
int altav_take_failure(uint32_t gen) { t_av_seen(gen); return 0; }
int altav_keyframe_permit(uint32_t gen, unsigned kind) { (void)kind; t_av_seen(gen); return 0; }
void altav_note_input_loss(uint32_t gen) { t_av_seen(gen); trace_add("loss(%u)", (unsigned)gen); }
void altav_shutdown_if_owner(uint32_t gen) { t_av_seen(gen); trace_add("shut(%u)", (unsigned)gen); }
hook_result_t bus_on(uint16_t type, bus_handler_t handler, void *ctx)
{ (void)type; (void)handler; (void)ctx; return 0; }
hook_result_t bus_send(uint16_t type, uint8_t flags, const uint8_t *payload, uint32_t len)
{ (void)type; (void)flags; (void)payload; (void)len; return 0; }

/* ── waits ────────────────────────────────────────────────────────────────── */
static int t_count_cmds(const char *type, const char *uuid)
{
    unsigned i;
    int n = 0;
    for (i = 0; i < t_cmd_count; ++i)
        if ((!type || !strcmp(t_cmds[i].type, type)) && (!uuid || !strcmp(t_cmds[i].uuid, uuid)))
            ++n;
    return n;
}
static void t_wait_cmds(const char *type, const char *uuid, int target)
{
    struct timespec until;
    alt_deadline_after_ms(&until, 3000);
    pthread_mutex_lock(&t_ev_lock);
    while (t_count_cmds(type, uuid) < target)
        assert(pthread_cond_timedwait(&t_ev_cond, &t_ev_lock, &until) != ETIMEDOUT);
    pthread_mutex_unlock(&t_ev_lock);
}
static void t_wait_aus(unsigned target)
{
    struct timespec until;
    alt_deadline_after_ms(&until, 3000);
    pthread_mutex_lock(&t_ev_lock);
    while (t_au_count < target)
        assert(pthread_cond_timedwait(&t_ev_cond, &t_ev_lock, &until) != ETIMEDOUT);
    pthread_mutex_unlock(&t_ev_lock);
}
/* until no receiver of this transport generation is still running (0 = any) */
static void t_wait_gen_exited(uint32_t gen)
{
    int i, spins;
    for (spins = 0; spins < 3000; ++spins) {
        int running = 0;
        pthread_mutex_lock(&alt_lock);
        for (i = 0; i < ALT_MAX_SESSIONS; ++i) {
            altslot_t *s = &alt_slots[i];
            if (s->inUse && s->threadCreated && !s->threadExited &&
                (!gen || s->avGeneration == gen))
                running = 1;
        }
        pthread_mutex_unlock(&alt_lock);
        if (!running) return;
        usleep(1000);
    }
    assert(!"receiver did not exit");
}
static void t_drain_all(void)
{
    int i;
    t_wait_gen_exited(0);
    alt_gc_exited_slots();
    pthread_mutex_lock(&alt_lock);
    for (i = 0; i < ALT_MAX_SESSIONS; ++i) assert(!alt_slots[i].inUse);
    pthread_mutex_unlock(&alt_lock);
}

/* ── phone side ───────────────────────────────────────────────────────────── */
static const uint8_t t_avcc[] = {1, 0x64, 0, 0x1f, 0xff, 0xe1, 0, 4, 0x67, 0x64, 0, 0x1f,
                                 1, 0, 2, 0x68, 0xce};
static const uint8_t t_idr[] = {0, 0, 0, 5, 0x65, 0x88, 0x84, 0, 0x10};
static const uint8_t t_pau[] = {0, 0, 0, 3, 0x41, 0x9a, 0x02};
static int t_phone_connect(int port)
{
    struct sockaddr_in a;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)port);
    assert(connect(fd, (struct sockaddr *)&a, sizeof(a)) == 0);
    return fd;
}
static void t_phone_send(int fd, uint8_t opcode, const uint8_t *body, uint32_t len)
{
    uint8_t hdr[ALT_HDR_SIZE];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = (uint8_t)len; hdr[1] = (uint8_t)(len >> 8);
    hdr[2] = (uint8_t)(len >> 16); hdr[3] = (uint8_t)(len >> 24);
    hdr[4] = opcode;
    assert(write(fd, hdr, sizeof(hdr)) == (ssize_t)sizeof(hdr));
    if (len) assert(write(fd, body, len) == (ssize_t)len);
}

/* ── requests, SETUP, /info ───────────────────────────────────────────────── */
typedef struct { int type; int64_t conn; const char *uuid; } t_stream_t;
static tcf_t *t_req_control(void)
{
    tcf_t *d = (tcf_t *)t_dictm(NULL, 0, NULL, NULL), *a = (tcf_t *)t_arrm(NULL, 0, NULL);
    tcf_t *s = (tcf_t *)t_str(NULL, "altScreen", 0);
    tcf_app_raw(a, s);
    t_rel(s);
    tcf_set_obj(d, "features", a);
    return d;
}
static tcf_t *t_req_streams(int n, const t_stream_t *s)
{
    tcf_t *d = (tcf_t *)t_dictm(NULL, 0, NULL, NULL), *a = (tcf_t *)t_arrm(NULL, 0, NULL);
    int i;
    for (i = 0; i < n; ++i) {
        tcf_t *e = (tcf_t *)t_dictm(NULL, 0, NULL, NULL);
        tcf_set_i64(e, "type", s[i].type);
        if (s[i].conn) tcf_set_i64(e, "streamConnectionID", s[i].conn);
        if (s[i].uuid) tcf_set_s(e, "uuid", s[i].uuid);
        tcf_app_raw(a, e);
        t_rel(e);
    }
    tcf_set_obj(d, "streams", a);
    return d;
}
/* SETUP through the module seam; returns the result, *resp keeps the response */
static int t_setup(void *sess, tcf_t *req, tcf_t **resp, uint32_t seq)
{
    hook_setup_ctx_t c;
    tcf_t *r = (tcf_t *)t_dictm(NULL, 0, NULL, NULL);
    memset(&c, 0, sizeof(c));
    c.session = sess;
    c.request = req;
    c.response = r;
    c.sequence = seq;
    alt_on_setup_response(&c);
    if (resp) *resp = r; else t_rel(r);
    return c.result;
}
static int t_resp_port(tcf_t *resp, int index)
{
    tcf_t *a = tcf_get(resp, "streams"), *e, *p;
    assert(a && a->kind == TCF_ARR && index < a->n);
    e = a->vals[index];
    assert(tcf_get(e, "type") && tcf_get(e, "type")->num == 111);
    p = tcf_get(e, "dataPort");
    assert(p && p->num > 0);
    return (int)p->num;
}
static int t_resp_count(tcf_t *resp)
{
    tcf_t *a = tcf_get(resp, "streams");
    return a ? a->n : 0;
}
static void t_info(void *sess, int night)
{
    tcf_t *info = (tcf_t *)t_dictm(NULL, 0, NULL, NULL);
    tcf_set_i64(info, "nightMode", night);
    alt_on_server_info(sess, info);
    t_rel(info);
}

static void t_wire_hook(void)
{
    signal(SIGPIPE, SIG_IGN);
    setenv("ALTSCREEN_HANDLE_ALT", "1", 1);
    setenv("ALTSCREEN_ADVERT_DISPLAYS", "1", 1);
    unsetenv("MHI2Q_DIO_OWNER_MODE");
    cf_str = t_str; cf_rel = t_rel; cf_ret = t_ret;
    cf_dictm = t_dictm; cf_dictset = t_dictset; cf_dictseti = t_dictseti;
    cf_geti64 = t_geti64; cf_geti64_value = t_geti64_value; cf_getval = t_getval;
    cf_arrm = t_arrm; cf_arrappc = t_arrappc; cf_arrapp = t_arrapp; cf_arrappi64 = t_arrappi64;
    cf_arrcount = t_arrcount; cf_arridx = t_arridx;
    cf_typeof = t_typeof; cf_strtype = t_type_str; cf_arrtype = t_type_arr;
    cf_dicttype = t_type_dict; cf_getcstr = t_getcstr; fn_copydisp = t_copydisp;
    arr_cb = dict_keycb = dict_valcb = (void *)1;
    derive_aes = t_derive;
    aes_ctr_init = t_aes_init; aes_ctr_update = t_aes_update; aes_ctr_final = t_aes_final;
    srv_sockopen = t_sockopen; sock_setqos = NULL; sock_accept = t_accept;
    sess_sendcmd = t_sendcmd;
    cf_ok = disp_ok = alt_syms_ok = 1;
}
#endif
