#include "wifi_latency_runtime.h"
#include "wifi_latency_policy.h"
#include "wifi_latency_build.h"

typedef uint32_t u32;
typedef int (*mutex_fn)(void *);
struct qnx_recursive_mutex { u32 count, owner; };
static struct qnx_recursive_mutex runtime_mutex = {0, UINT32_MAX};
static _Atomic int ready;
static void *self_pin;
static void *world_scope;
/* Read-only diagnostic receipt: stage, resolver, dladdr result, name, reported
 * base, expected base, retained handle. No loader error string is retained.
 */
static volatile u32 prepare_diag[7];

static int native_lock(void) {
    return ((mutex_fn)(uintptr_t)WL_IO_MUTEX_LOCK)(&runtime_mutex);
}
static void native_unlock(void) {
    ((mutex_fn)(uintptr_t)WL_IO_MUTEX_UNLOCK)(&runtime_mutex);
}
static int expected_stack(void) {
    for (u32 n = 0; n < WL_STACK_GUARD_COUNT; ++n) {
        const volatile unsigned char *p = (const volatile unsigned char *)(uintptr_t)wl_stack_guards[n].address;
        u32 hash = 2166136261u;
        for (u32 i = 0; i < wl_stack_guards[n].length; ++i)
            hash = (hash ^ p[i]) * 16777619u;
        if (hash != wl_stack_guards[n].hash) return 0;
    }
    return *(volatile u32 *)(uintptr_t)WL_IO_HZ == 119;
}
int wl_runtime_prepare(void) {
    prepare_diag[0] = 1;
    if (!expected_stack()) return -1;
    if (native_lock()) return -1;
    prepare_diag[0] = 2;
    if (!ready) {
        struct dl_info { const char *name; void *base; const char *symbol; void *address; } info = {0};
        typedef void *(*sym_fn)(void *, const char *);
        typedef int (*addr_fn)(void *, void *);
        typedef void *(*open_fn)(const char *, int);
        /* io-pkt loads drivers with GROUP|WORLD|LAZYLOAD (0x4c00). QNX's
         * RTLD_DEFAULT lookup from that local driver scope does not expose
         * dladdr. Use the explicit world handle, as io-pkt itself does.
         */
        world_scope = ((open_fn)(uintptr_t)WL_IO_DLOPEN)(0, 0x800);
        addr_fn addr = world_scope ? (addr_fn)((sym_fn)(uintptr_t)WL_IO_DLSYM)(world_scope, "dladdr") : 0;
        prepare_diag[1] = (u32)(uintptr_t)addr;
        int addressed = addr ? addr((void *)wl_native_base, &info) : 0;
        prepare_diag[0] = 3;
        prepare_diag[2] = addressed;
        prepare_diag[3] = (u32)(uintptr_t)info.name;
        prepare_diag[4] = (u32)(uintptr_t)info.base;
        prepare_diag[5] = wl_native_base();
        if (addr && addressed && info.name
            && (uintptr_t)info.base == wl_native_base()) {
            /* Keep callback code and immutable cookie storage alive even when
             * native owner-clean removes a queued callback without invoking it.
             * QNX6.5 RTLD_NOW|RTLD_NOLOAD|RTLD_NODELETE. No dlclose is intended.
             */
            self_pin = ((open_fn)(uintptr_t)WL_IO_DLOPEN)(info.name, 0x1006);
            prepare_diag[0] = 4;
            prepare_diag[6] = (u32)(uintptr_t)self_pin;
        }
        ready = self_pin ? 1 : -1;
    }
    int result = ready == 1 ? 0 : -1;
    native_unlock();
    return result;
}
int wl_lock(void) {
    /* ready is immutable after prepare completes; acquire/release accesses
     * ensure all enrolled runtime state observes successful module pinning.
     */
    if (atomic_load_explicit(&ready, memory_order_acquire) != 1) return -1;
    return native_lock();
}
void wl_unlock(void) { native_unlock(); }
/* TX completion runs under native locks (ra_list priv+0x174c at 0x3ea7c,
 * 0x3e6b4, 0x3579c) while the stack thread can hold wl_lock and enter native
 * TX that takes them (0x3f070). Never wait on wl_lock from completion: busy
 * means another thread owns it, so take the stock path. Recursive owner wins.
 */
static int wl_trylock(void) {
    if (atomic_load_explicit(&ready, memory_order_acquire) != 1) return -1;
    return ((mutex_fn)(uintptr_t)WL_IO_MUTEX_TRYLOCK)(&runtime_mutex);
}
u32 wl_native_ticks(void) { return *(volatile u32 *)(uintptr_t)WL_IO_TICKS; }
u32 wl_runtime_probe(u32 *out) {
    if (out) {
        out[0] = (u32)wl_native_base();
        out[1] = sizeof(runtime_mutex);
        out[2] = sizeof(struct wl_tx_wake);
        out[3] = runtime_mutex.owner;
        out[4] = (u32)ready;
    }
    return 0x574c5031;
}

#define TX_CONTEXTS 128u
#define TX_COOKIES 256u
struct tx_context {
    u32 allocated, live, generation, fallback;
    void *owner, *ifnet;
    struct wl_tx_wake wake;
};
struct tx_cookie {
    u32 allocated;
    struct tx_context *context;
    struct wl_tx_ticket ticket;
    void *function, *argument, *owner;
};
static struct tx_context contexts[TX_CONTEXTS];
static struct tx_cookie cookies[TX_COOKIES];
static struct tx_cookie *cookie_hint;
static u32 next_generation;
static u32 exhausted_contexts, exhausted_cookies, notifications, suppressed, retired;

static struct tx_context *find_context(void *owner, void *ifnet) {
    /* Allocations form a permanent prefix: one new generation per allocated
     * slot, never freed/reordered. Skip the never-allocated suffix but retain
     * newest-first search, including retired generations at a reused owner.
     * Callers own runtime_mutex; no cache or unlocked native pointer exists.
     */
    u32 count = next_generation < TX_CONTEXTS ? next_generation : TX_CONTEXTS;
    for (u32 i = count; i; --i) {
        struct tx_context *c = &contexts[i-1];
        if (c->allocated && c->owner == owner && c->ifnet == ifnet) return c;
    }
    return 0;
}
static struct tx_cookie *new_cookie(void) {
    struct tx_cookie *c = cookie_hint;
    /* runtime_mutex serializes both this hint and allocated flags. The hint
     * never grants ownership: even the hint's allocated flag is checked.
     * Quarantined/captured cookies remain allocated and cannot be reused.
     */
    if (c && c < cookies + TX_COOKIES && !c->allocated) {
        c->allocated = 1;
        cookie_hint = c + 1;
        return c;
    }
    /* A stale/one-past hint only loses this shortcut. Retain the original
     * bounded scan for arbitrary holes and full-pool exhaustion.
     */
    for (u32 i = 0; i < TX_COOKIES; ++i) {
        if (!cookies[i].allocated) {
            cookies[i].allocated = 1;
            cookie_hint = &cookies[i] + 1;
            return &cookies[i];
        }
    }
    ++exhausted_cookies;
    return 0;
}
static int native_enqueue(void *fn, void *arg, void *owner) {
    typedef int (*enqueue_fn)(void *, void *, void *);
    return ((enqueue_fn)(uintptr_t)WL_IO_CONTEXT_CALLBACK)(fn, arg, owner);
}
static void callback(void *opaque);
static void submit(struct tx_context *c, struct wl_tx_ticket t,
                   void *fn, void *arg, void *owner) {
    struct tx_cookie *cookie = new_cookie();
    if (!cookie) {
        wl_tx_submit_failed(&c->wake, t);
        c->fallback = 1;
        /* Failed/owner-cleaned submissions can retain immutable cookies.
         * Exhaustion must preserve event-driven wakeups, not just the tick
         * retry. Retain stock completion hysteresis for future requests too.
         */
        native_enqueue(fn, arg, owner);
        return;
    }
    cookie->context = c;
    cookie->ticket = t;
    cookie->function = fn; cookie->argument = arg; cookie->owner = owner;
    if (native_enqueue((void *)callback, cookie, owner) == -1) {
        /* The pinned stack failure return removes/frees the queued node.
         * Queued execution is serialized by the same global runtime mutex;
         * a captured callback, if any, will still own this immutable cookie.
         * Conservatively retain the cookie/context on an ambiguous failure.
         * Reset QUEUED so later requests can wake again: the next request
         * takes a new serial, so a late callback with this ticket discards.
         */
        wl_tx_submit_failed(&c->wake, t);
        ++suppressed;
        return;
    }
    ++notifications;
}
static void callback(void *opaque) {
    uintptr_t p = (uintptr_t)opaque, lo = (uintptr_t)cookies;
    if (p < lo || p >= lo + sizeof(cookies) || (p-lo) % sizeof(cookies[0])) return;
    if (wl_lock()) return;
    struct tx_cookie *cookie = opaque;
    if (!cookie->allocated) { wl_unlock(); return; }
    struct tx_context *c = cookie->context;
    struct wl_tx_ticket t = cookie->ticket, next;
    void *fn = cookie->function, *arg = cookie->argument, *owner = cookie->owner;
    int action = wl_tx_begin(&c->wake, t);
    if (action == WL_TX_RUN && c->live) {
        ((void (*)(void *))fn)(arg);
        if (wl_tx_finish(&c->wake, t, &next) == WL_TX_SUBMIT)
            submit(c, next, fn, arg, owner);
    }
    cookie->allocated = 0;
    cookie_hint = cookie;
    wl_unlock();
}
int wl_tx_runtime_attach(void *parent, void *softc, void *args) {
    int prepared = wl_runtime_prepare();
    /* Native attach waits for card-worker commands; do not hold our RX/TX
     * mutex across it. Until enrollment, the TX gate retains stock hysteresis.
     */
    int result = wl_orig_attach(parent, softc, args);
    if (result || prepared || wl_lock()) return result;
    struct tx_context *c = 0;
    if (next_generation != UINT32_MAX) {
        for (u32 i = 0; i < TX_CONTEXTS; ++i)
            if (!contexts[i].allocated) { c = &contexts[i]; break; }
    }
    if (c) {
        c->allocated = 1; c->live = 1; c->generation = ++next_generation;
        c->owner = softc; c->ifnet = (unsigned char *)softc + 128;
        wl_tx_init(&c->wake);
        if (wl_tx_bind(&c->wake, c->generation) != WL_OK) c->live = 0;
    } else {
        ++exhausted_contexts;
        /* The same native owner address can be attached again after our
         * permanent generation pool is full. Old captured cookies still need
         * their retired context, but the new unmanaged attachment must use
         * native enqueue rather than the detach-gap suppression below.
         */
        c = find_context(softc, (unsigned char *)softc+128);
        if (c && !c->live) c->fallback = 1;
    }
    wl_unlock();
    return result;
}
int wl_tx_runtime_detach(void *softc) {
    if (!wl_lock()) {
        struct tx_context *c = find_context(softc, (unsigned char *)softc+128);
        if (c && c->live) {
            c->live = 0;
            wl_tx_retire(&c->wake, c->generation);
            ++retired;
        }
        if (c) c->fallback = 0;
        /* Captured/queued cookies reference permanent module state only.
         * A callback already running held this mutex across its target call;
         * all later callbacks discard without dereferencing the native target.
         */
        wl_unlock();
    }
    return wl_orig_detach(softc);
}
u32 wl_tx_runtime_gate(void *handle, void *ifnet, void *owner) {
    (void)handle;
    if (wl_trylock()) return 0;
    struct tx_context *c = find_context(owner, ifnet);
    if (c && c->live && wl_tx_serial_exhausted(&c->wake)) c->fallback = 1;
    u32 managed = c && c->live && !c->fallback;
    wl_unlock();
    return managed;
}
int wl_tx_runtime_request(void *fn, void *arg, void *owner) {
    if (wl_trylock()) return native_enqueue(fn, arg, owner);
    struct tx_context *c = find_context(owner, arg);
    if (c && c->live && wl_tx_serial_exhausted(&c->wake)) c->fallback = 1;
    if (!c || c->fallback) { wl_unlock(); return native_enqueue(fn, arg, owner); }
    if (c->live) {
        struct wl_tx_ticket t;
        if (wl_tx_request(&c->wake, c->generation, 1, 1, &t) == WL_TX_SUBMIT)
            submit(c, t, fn, arg, owner);
        else ++suppressed;
    }
    wl_unlock();
    return 0;
}
