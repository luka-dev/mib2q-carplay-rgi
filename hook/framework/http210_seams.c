/* Stock MU1316 210.81 HTTP boundary. Layout and call sites are verified in
 * docs/wireless-carplay/STOCK210_HTTP_PAIRING_ABI.md. No 280 HTTP structs are used here. */
#include "hook_framework.h"
#include "http210_seams.h"
#include "../wireless/stock210_abi.h"
#include "../wireless/wireless_hook.h"

static int (*stock_open)(void *);
static void (*stock_close)(void *);
static int (*stock_request)(void *, void *);
static int (*request_fallback)(void *, void *);

static int connection_open(void *connection)
{
    size_t i;
    int err = stock_open ? stock_open(connection) : 0;
    if (err) return err;
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_open && (err = m->http.on_open(connection))) return err;
    }
    return 0;
}

static void connection_close(void *connection)
{
    size_t i = hook_framework_module_count();
    while (i--) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_close) m->http.on_close(connection);
    }
    if (stock_close) stock_close(connection);
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_close_final) m->http.on_close_final(connection);
    }
}

static int connection_request(void *connection, void *message)
{
    size_t i;
    int result;
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_request && m->http.on_request(connection, message, &result)) return result;
    }
#ifdef MHI2Q_HTTP210_LAB
    /* Explicit QEMU-only peripheral: software MFi and a RECORD observer.
     * Run behind pairing/admission, without replacing the guarded callback.
     * Shipping builds contain neither lookup nor callback. */
    typedef int (*lab_request_fn)(void *,void *,int *);
    typedef void (*lab_done_fn)(void *,void *,int);
    lab_request_fn lab_request=(lab_request_fn)dlsym(RTLD_DEFAULT,"mhi2q_lab_http_request");
    if(lab_request && lab_request(connection,message,&result))return result;
#endif
    int (*fallback)(void *,void *)=__sync_val_compare_and_swap(&request_fallback,NULL,NULL);
    result=fallback ? fallback(connection,message) : stock_request(connection, message);
#ifdef MHI2Q_HTTP210_LAB
    lab_done_fn lab_done=(lab_done_fn)dlsym(RTLD_DEFAULT,"mhi2q_lab_http_done");
    if(lab_done)lab_done(connection,message,result);
#endif
    return result;
}

int hook_http210_set_request_fallback(void *router, void *expected_stock,
                                     int (*fallback)(void *, void *))
{
    if (router != (void *)connection_request || !stock_request ||
        expected_stock != (void *)stock_request || !fallback ||
        fallback == connection_request || fallback == stock_request) return 0;
    int (*previous)(void *,void *)=__sync_val_compare_and_swap(&request_fallback,NULL,fallback);
    return !previous || previous==fallback;
}

HOOK_EXPORT int HTTPServerCreate(void **out_server, const void *delegate)
{
    typedef int (*create_fn)(void **, const void *);
    create_fn real = (create_fn)dlsym(RTLD_NEXT, "HTTPServerCreate");
    const uintptr_t *slots = delegate;
    uintptr_t copied[11];
    const char *mode = getenv("MHI2Q_DIO_OWNER_MODE");
    const char *enable = getenv("MHI2Q_CP_WIRELESS_PAIRING");
    if (!real) return kBadReferenceErr;
    if (!mode || strcmp(mode, "wireless") || !enable || strcmp(enable, "1") ||
        !slots || !cp210_matches((void *)real, (void *)slots[9])) return real(out_server, delegate);
    if (hook_framework_init() != HOOK_OK) return kBadReferenceErr;
    /* Module registration may have been denied by the build's VIN gate. */
    size_t i;
    int wanted = 0;
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_open) wanted = 1;
    }
    if (!wanted) return real(out_server, delegate);
    if (stock_request && stock_request != (void *)slots[9]) return kBadReferenceErr;
    memcpy(copied, delegate, sizeof(copied));
    stock_open = (int (*)(void *))slots[6];
    stock_close = (void (*)(void *))slots[7];
    stock_request = (int (*)(void *, void *))slots[9];
    copied[6] = (uintptr_t)connection_open;
    copied[7] = (uintptr_t)connection_close;
    copied[9] = (uintptr_t)connection_request;
    int result=real(out_server, copied);
    if(!result && out_server && *out_server)cp_wireless_http_created(*out_server);
    return result;
}

HOOK_EXPORT void HTTPServerSetDispatchQueue(void *server,void *queue)
{
    typedef void (*fn)(void *,void *);
    fn real=(fn)dlsym(RTLD_NEXT,"HTTPServerSetDispatchQueue");
    if(real) { real(server,queue);cp_wireless_http_queue(server,queue); }
}

HOOK_EXPORT int HTTPMessageReadMessage(int fd, void *message)
{
    typedef int (*read_fn)(int, void *);
    read_fn real;
    size_t i;
    int result;
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_read && m->http.on_read(fd, message, &result)) return result;
    }
    real = (read_fn)dlsym(RTLD_NEXT, "HTTPMessageReadMessage");
    return real ? real(fd, message) : kBadReferenceErr;
}

HOOK_EXPORT int SocketWriteData(int fd, struct iovec **vectors, int *count)
{
    typedef int (*write_fn)(int, struct iovec **, int *);
    write_fn real;
    size_t i;
    int result;
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_write && m->http.on_write(fd, vectors, count, &result)) return result;
    }
    real = (write_fn)dlsym(RTLD_NEXT, "SocketWriteData");
    result = real ? real(fd, vectors, count) : kBadReferenceErr;
    for (i = 0; i < hook_framework_module_count(); ++i) {
        const hook_module_def_t *m = hook_framework_module_at(i);
        if (m && m->http.on_write_done) result = m->http.on_write_done(fd, result);
    }
    return result;
}
