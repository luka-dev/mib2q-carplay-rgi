#include "wireless_hook.h"
#include "pairing.h"
#include "stock210_abi.h"
#include "media.h"
#include "advertisement.h"
#include <sys/socket.h>

DEFINE_LOG_MODULE(WIRELESS);
#define CONNECTIONS 8
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static cp_pair_store *store;
static int enabled;
static void *http_server, *http_queue;
static int admission_closed, advertisement_failed;
static void (*queue_retain)(void *), (*queue_release)(void *);
static void (*queue_sync)(void *,void *,void (*)(void *));
static struct connection {
    void *http;
    int fd, commit_pending;
    cp_pair_connection *pair;
} connections[CONNECTIONS];

static struct connection *find_connection(void *http, int fd)
{
    size_t i;
    for (i = 0; i < CONNECTIONS; ++i)
        if (connections[i].http && (http ? connections[i].http == http : connections[i].fd == fd))
            return &connections[i];
    return NULL;
}

static void wireless_init(void)
{
    const char *mode = getenv("MHI2Q_DIO_OWNER_MODE");
    const char *flag = getenv("MHI2Q_CP_WIRELESS_PAIRING");
    const char *directory = getenv("MHI2Q_CP_PAIRING_DIR");
    int err;
    if (!mode || strcmp(mode, "wireless") || !flag || strcmp(flag, "1")) return;
    if (!directory) directory = "/mnt/persist/mhi2q-carplay/pairing";
    if (!strcmp(directory, "/mnt/persist/mhi2q-carplay/pairing")) {
        if (mkdir("/mnt/persist/mhi2q-carplay", 0700) != 0 && errno != EEXIST) {
            LOG_ERROR(LOG_MODULE, "pairing parent unavailable errno=%d", errno);
            return;
        }
    }
    pthread_mutex_lock(&lock);
    err = cp_pair_store_open(directory, &store);
    enabled = err == 0;
    pthread_mutex_unlock(&lock);
    if (err) LOG_ERROR(LOG_MODULE, "pairing store unavailable status=%d%s", err, ap_status(err));
    else LOG_WARN(LOG_MODULE, "stock210 pairing store ready; receiver readiness awaits guarded HTTP/media");
}

void cp_wireless_http_created(void *server)
{
    pthread_mutex_lock(&lock);
    http_server=server;
    pthread_mutex_unlock(&lock);
}

void cp_wireless_http_queue(void *server,void *queue)
{
    void *previous=NULL;
    pthread_mutex_lock(&lock);
    if(server==http_server && queue) {
        queue_retain=(void (*)(void *))dlsym(RTLD_NEXT,"dispatch_retain");
        queue_release=(void (*)(void *))dlsym(RTLD_NEXT,"dispatch_release");
        queue_sync=(void (*)(void *,void *,void (*)(void *)))dlsym(RTLD_NEXT,"dispatch_sync_f");
        if(queue_retain && queue_release && queue_sync) {
            queue_retain(queue);previous=http_queue;http_queue=queue;
        }
    }
    pthread_mutex_unlock(&lock);
    if(previous)queue_release(previous);
}

void cp_wireless_advertisement_failed(void)
{ pthread_mutex_lock(&lock);advertisement_failed=1;pthread_mutex_unlock(&lock); }

HOOK_EXPORT int carplay_wireless_receiver_ready(void)
{
    int ready;
    pthread_mutex_lock(&lock);
    ready=enabled && store && http_queue && !admission_closed && !advertisement_failed;
    pthread_mutex_unlock(&lock);
    return ready && cp_media_available() && cp_advertisement_available();
}

struct receiver_operation { void *server; unsigned operation; int result; };
static void receiver_on_queue(void *opaque)
{
    struct receiver_operation *op=opaque;
    unsigned i;
    /* Operations 1=idle stop, 2=resume admission, 3=wired drain. Every SETUP
     * and connection finalizer runs on this same retained stock HTTP queue. */
    pthread_mutex_lock(&lock);
    if(op->operation==2) {
        admission_closed=0;op->result=0;pthread_mutex_unlock(&lock);return;
    }
    if(op->operation==1)for(i=0;i<CONNECTIONS;++i) {
        void *context=connections[i].http?cp210_ptr(connections[i].http,8):NULL;
        if(context && cp210_ptr(context,244)) {
            op->result=1;pthread_mutex_unlock(&lock);return;
        }
    }
    admission_closed=1;
    pthread_mutex_unlock(&lock);
    if(op->operation==3) {
        void (*stop)(void)=(void (*)(void))dlsym(RTLD_NEXT,"AirTunesServer_StopAllConnections");
        if(stop){stop();op->result=0;}
    } else {
        void *(*string)(void *,const char *,unsigned)=(void *(*)(void *,const char *,unsigned))dlsym(RTLD_NEXT,"CFStringCreateWithCString");
        void (*release)(void *)=(void (*)(void *))dlsym(RTLD_NEXT,"CFRelease");
        int (*control)(void *,unsigned,void *,void *,void *,void *)=(int (*)(void *,unsigned,void *,void *,void *,void *))dlsym(RTLD_NEXT,"AirPlayReceiverServerControl");
        void *command=string?string(NULL,"stopServer",0x08000100):NULL;
        if(command && release && control)op->result=control(op->server,1,command,NULL,NULL,NULL);
        if(command && release)release(command);
    }
}

/* The bridge supplies its retained stock server. No 280 server offsets/API. */
HOOK_EXPORT int carplay_wireless_receiver_control(void *server,unsigned operation)
{
    struct receiver_operation op={server,operation,-1};
    void **actual=(void **)dlsym(RTLD_NEXT,"gAirPlayReceiverServer");
    void *queue=NULL;
    if(!actual || *actual!=server || !server || operation<1 || operation>3)return -1;
    pthread_mutex_lock(&lock);
    if(http_queue && queue_sync && queue_retain && queue_release) {
        queue=http_queue;queue_retain(queue);
    }
    pthread_mutex_unlock(&lock);
    if(!queue)return -1;
    queue_sync(queue,&op,receiver_on_queue);
    queue_release(queue);
    return op.result;
}

/* Same process C seam for receiver_bridge. ID and EdPK are deliberately
 * separate outputs. This is identity availability, not full receiver readiness. */
HOOK_EXPORT int carplay_wireless_copy_pairing_identity(char *id, size_t capacity, uint8_t pk[32])
{
    char value[CP_PAIR_ID_MAX + 1];
    int err;
    pthread_mutex_lock(&lock);
    err = enabled && store ? cp_pair_identity(store, value, pk) : ENOENT;
    if (!err && (!id || capacity <= strlen(value))) err = ENOSPC;
    if (!err) strcpy(id, value);
    pthread_mutex_unlock(&lock);
    return err;
}

static int wireless_open(void *http)
{
    size_t i;
    int err = kNoMemoryErr;
    pthread_mutex_lock(&lock);
    if (!enabled) { pthread_mutex_unlock(&lock); return kBadReferenceErr; }
    for (i = 0; i < CONNECTIONS; ++i) if (!connections[i].http) {
        err = cp_pair_connection_create(store, &connections[i].pair);
        if (!err) {
            connections[i].http = http;
            connections[i].fd = (int)cp210_get(http, CP210_CNX_FD);
            /* Pairing/control bodies have a bounded allocation before dispatch. */
            cp210_set(cp210_ptr(http, CP210_CNX_REQUEST), CP210_MSG_BODY_LIMIT, CP_PAIR_BODY_MAX);
        }
        break;
    }
    pthread_mutex_unlock(&lock);
    return err;
}

static void wireless_close(void *http)
{
    size_t i;
    int active = 0;
    pthread_mutex_lock(&lock);
    struct connection *c = find_connection(http, -1);
    if (c) {
        cp_pair_connection_free(c->pair);
        memset(c, 0, sizeof(*c));
    }
    for (i = 0; i < CONNECTIONS; ++i) active |= connections[i].http != NULL;
    if (!active && !enabled) { cp_pair_store_close(store); store = NULL; }
    pthread_mutex_unlock(&lock);
}

static int token(void *message, size_t ptr_offset, size_t length_offset, const char *value)
{
    size_t n = cp210_get(message, length_offset);
    return n == strlen(value) && !memcmp(cp210_ptr(message, ptr_offset), value, n);
}

static int respond(void *http, void *request, int status, const void *body, size_t length)
{
    typedef int (*init_fn)(void *, const char *, int, const char *);
    typedef int (*body_fn)(void *, const char *, const void *, size_t);
    typedef int (*field_fn)(void *, const char *, const char *, ...);
    typedef int (*scan_fn)(const void *, size_t, const char *, const char *, ...);
    typedef int (*send_fn)(void *);
    init_fn init = (init_fn)dlsym(RTLD_NEXT, "HTTPHeader_InitResponse");
    body_fn set_body = (body_fn)dlsym(RTLD_NEXT, "HTTPMessageSetBody");
    field_fn field = (field_fn)dlsym(RTLD_NEXT, "HTTPHeader_SetField");
    scan_fn scan = (scan_fn)dlsym(RTLD_NEXT, "HTTPScanFHeaderValue");
    send_fn send_response = (send_fn)dlsym(RTLD_NEXT, "HTTPConnectionSendResponse");
    void *response = cp210_ptr(http, CP210_CNX_RESPONSE);
    uint8_t *header = (uint8_t *)response + CP210_MSG_HEADER;
    unsigned int seq;
    int err;
    if (!init || !set_body || !field || !scan || !send_response) return kBadReferenceErr;
    err = init(header, "HTTP/1.1", status, NULL);
    if (!err) err = set_body(response, "application/pairing+tlv8", body, length);
    if (!err) err = field(header, "Server", "%s", "AirTunes/210.81");
    if (!err && scan((uint8_t *)request + CP210_MSG_HEADER,
                     cp210_get(request, CP210_MSG_HEADER_LENGTH), "CSeq", "%u", &seq) == 1)
        err = field(header, "CSeq", "%u", seq);
    return err ? err : send_response(http);
}

static int wireless_request(void *http, void *message, int *result)
{
    typedef int (*scan_fn)(const void *, size_t, const char *, const char *, ...);
    uint8_t *output = NULL;
    size_t length = 0;
    int verify, setup, done = 0, type = 0, err, status;
    pthread_mutex_lock(&lock);
    struct connection *c = find_connection(http, -1);
    if (!c) { pthread_mutex_unlock(&lock); return 0; }
    verify = token(message, CP210_MSG_PATH_PTR, CP210_MSG_PATH_LENGTH, "/pair-verify");
    setup = token(message, CP210_MSG_PATH_PTR, CP210_MSG_PATH_LENGTH, "/pair-setup");
    if (!verify && !setup) {
        if (token(message, CP210_MSG_METHOD_PTR, CP210_MSG_METHOD_LENGTH, "SETUP")) {
            if(admission_closed) {
                *result=respond(http,message,503,NULL,0);
                pthread_mutex_unlock(&lock);return 1;
            }
            if (!cp_pair_is_encrypted(c->pair)) {
                *result = respond(http, message, 403, NULL, 0);
                pthread_mutex_unlock(&lock);
                return 1;
            }
            int media_err = cp_media_enabled() ? cp_media_normalize_setup(message) : kUnsupportedErr;
            if (!media_err) { pthread_mutex_unlock(&lock); return 0; }
            *result = respond(http, message, 461, NULL, 0);
            pthread_mutex_unlock(&lock);
            return 1;
        }
        pthread_mutex_unlock(&lock);
        return 0;
    }
    scan_fn scan = (scan_fn)dlsym(RTLD_NEXT, "HTTPScanFHeaderValue");
    if (!token(message, CP210_MSG_METHOD_PTR, CP210_MSG_METHOD_LENGTH, "POST")) status = 405;
    else if (!scan || scan((uint8_t *)message + CP210_MSG_HEADER,
                           cp210_get(message, CP210_MSG_HEADER_LENGTH), "X-Apple-HKP", "%d", &type) != 1) status = 403;
    else if (c->commit_pending || cp_pair_is_encrypted(c->pair) ||
             cp210_get(message, CP210_MSG_EXTRA_LENGTH)) status = 400;
    else {
        err = cp_pair_exchange(c->pair, verify, cp210_ptr(message, CP210_MSG_BODY_PTR),
                               cp210_get(message, CP210_MSG_BODY_OFFSET), &output, &length, &done);
        if (err) LOG_WARN(LOG_MODULE, "pairing exchange failed verify=%d status=%d%s", verify, err, ap_status(err));
        status = err ? 400 : 200;
        if (!err && verify && done) c->commit_pending = 1;
    }
    *result = respond(http, message, status, output, length);
    if (*result) c->commit_pending = 0;
    free(output);
    pthread_mutex_unlock(&lock);
    return 1;
}

static int wireless_read(int fd, void *message, int *result)
{
    int handled = 0;
    pthread_mutex_lock(&lock);
    struct connection *c = find_connection(NULL, fd);
    if (c && cp_pair_is_encrypted(c->pair)) {
        *result = cp210_read_message(c->pair, message);
        handled = 1;
    }
    pthread_mutex_unlock(&lock);
    return handled ? 1 : cp_media_event_read(fd, message, result);
}

static int wireless_write(int fd, struct iovec **vectors, int *count, int *result)
{
    int handled = 0;
    pthread_mutex_lock(&lock);
    struct connection *c = find_connection(NULL, fd);
    if (c && cp_pair_is_encrypted(c->pair)) {
        *result = cp_pair_writev(c->pair, vectors, count);
        handled = 1;
    }
    pthread_mutex_unlock(&lock);
    return handled ? 1 : cp_media_event_write(fd, vectors, count, result);
}

static int wireless_write_done(int fd, int result)
{
    pthread_mutex_lock(&lock);
    struct connection *c = find_connection(NULL, fd);
    if (c && c->commit_pending && result == 0) {
        c->commit_pending = 0;
        result = cp_pair_commit(c->pair, fd);
    }
    pthread_mutex_unlock(&lock);
    return result;
}

static void wireless_shutdown(void)
{
    size_t i;
    int active = 0;
    pthread_mutex_lock(&lock);
    enabled = 0;
    for (i = 0; i < CONNECTIONS; ++i) if (connections[i].http) {
        shutdown(connections[i].fd, SHUT_RDWR);
        active = 1;
    }
    /* Leave live contexts until the stock finalizer relinquishes each fd. */
    if (!active) { cp_pair_store_close(store); store = NULL; }
    pthread_mutex_unlock(&lock);
}

static void wireless_setup(hook_setup_ctx_t *setup)
{
    unsigned i;
    if (setup->result || !cp_dio_owner_is_wireless() || !cp_media_enabled()) return;
    pthread_mutex_lock(&lock);
    /* Never normalize a paired SETUP and then silently run legacy media if
     * the guarded stock context cannot be associated with its verifier. */
    setup->result = kStateErr;
    for (i = 0; i < CONNECTIONS; ++i) {
        struct connection *c = &connections[i];
        void *context = c->http ? cp210_ptr(c->http, 8) : NULL;
        if (context && cp210_ptr(context, 244) == setup->session && cp_pair_is_encrypted(c->pair)) {
            setup->result = cp_media_prepare(c->http, setup->session, setup->request, c->pair);
            break;
        }
    }
    pthread_mutex_unlock(&lock);
}

const hook_module_def_t wireless_module_def = {
    .name = "wireless", .priority = HOOK_PRIORITY_FIRST,
    .on_init = wireless_init, .on_shutdown = wireless_shutdown,
    .airplay = { .on_setup_request = wireless_setup,
                 .on_server_info_final = cp_advertisement_info,
                 .on_setup_response = cp_media_setup_response,
                 .on_teardown_final = cp_media_teardown },
    .http = { .on_open = wireless_open, .on_close = wireless_close,
              .on_close_final = cp_media_forget,
              .on_request = wireless_request, .on_read = wireless_read,
              .on_write = wireless_write, .on_write_done = wireless_write_done }
};
