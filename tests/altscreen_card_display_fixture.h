/* Host harness for tests/altscreen_card_display_test.c: a thread-safe fake
 * CFLite with a mutation trace, the stock AirPlay primitives the self-contained
 * stream-111 receiver uses (identity AES, real loopback listeners), recording
 * altav/bus leaves, and a phone that connects to a response dataPort.
 * Included after the hook, so every hook static is in scope. */
#ifndef ALTSCREEN_CARD_DISPLAY_FIXTURE_H
#define ALTSCREEN_CARD_DISPLAY_FIXTURE_H
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/stat.h>

/* ── fake CF ──────────────────────────────────────────────────────────────── */
#define TCF_MAGIC 0x7CF0BEEFu
#define TCF_MAX 48
enum { TCF_STR = 1, TCF_ARR, TCF_DICT, TCF_NUM, TCF_DATA, TCF_BOOL };
typedef struct tcf {
    uint32_t magic;
    int kind, refs;
    unsigned id;
    char text[160];
    int64_t num;
    unsigned dlen;
    int n;
    struct tcf *keys[TCF_MAX], *vals[TCF_MAX];
} tcf_t;

static pthread_mutex_t t_cf_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned t_next_id, t_live;
static const char *t_fail_str;   /* cf_str/cf_arrappc of this exact text fails */

static tcf_t *tcf_of(const void *p)
{
    const tcf_t *t = (const tcf_t *)p;
    return (t && t->magic == TCF_MAGIC) ? (tcf_t *)t : NULL;
}
static tcf_t *tcf_new_locked(int kind)
{
    tcf_t *t = (tcf_t *)calloc(1, sizeof(*t));
    assert(t);
    t->magic = TCF_MAGIC;
    t->kind = kind;
    t->refs = 1;
    t->id = ++t_next_id;
    ++t_live;
    return t;
}
static void tcf_rel_locked(tcf_t *t)
{
    int i;
    if (!t) return;
    assert(t->refs > 0);
    if (--t->refs) return;
    for (i = 0; i < t->n; ++i) {
        tcf_rel_locked(t->keys[i]);
        tcf_rel_locked(t->vals[i]);
    }
    t->magic = 0;
    --t_live;
    free(t);
}
static unsigned t_live_count(void)
{
    unsigned n;
    pthread_mutex_lock(&t_cf_lock);
    n = t_live;
    pthread_mutex_unlock(&t_cf_lock);
    return n;
}

/* ── trace ────────────────────────────────────────────────────────────────── */
static pthread_mutex_t t_ev_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t t_ev_cond = PTHREAD_COND_INITIALIZER;
static char t_trace[262144];
static size_t t_trace_len;
static int t_trace_on;

static void trace_add(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void trace_add(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    size_t n;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pthread_mutex_lock(&t_ev_lock);
    if (t_trace_on) {
        n = strlen(line);
        assert(t_trace_len + n + 2 < sizeof(t_trace));
        memcpy(t_trace + t_trace_len, line, n);
        t_trace_len += n;
        t_trace[t_trace_len++] = '\n';
        t_trace[t_trace_len] = 0;
    }
    pthread_cond_broadcast(&t_ev_cond);
    pthread_mutex_unlock(&t_ev_lock);
}
static void trace_reset(int on)
{
    pthread_mutex_lock(&t_ev_lock);
    t_trace_len = 0;
    t_trace[0] = 0;
    t_trace_on = on;
    pthread_mutex_unlock(&t_ev_lock);
}
static void tcf_desc(const tcf_t *v, const char *key, char *out, size_t n)
{
    if (key && !strcmp(key, "dataPort")) { snprintf(out, n, "<port>"); return; }
    if (!v) { snprintf(out, n, "NULL"); return; }
    switch (v->kind) {
    case TCF_STR:  snprintf(out, n, "\"%s\"", v->text); break;
    case TCF_NUM:  snprintf(out, n, "%lld", (long long)v->num); break;
    case TCF_BOOL: snprintf(out, n, "b%lld", (long long)v->num); break;
    case TCF_DATA: snprintf(out, n, "<data %u>", v->dlen); break;
    case TCF_DICT: snprintf(out, n, "D%u", v->id); break;
    default:       snprintf(out, n, "A%u", v->id); break;
    }
}

/* ── CF entry points wired into the hook's pointers ──────────────────────── */
static void *t_str(void *a, const char *s, unsigned enc)
{
    tcf_t *t;
    (void)a; (void)enc;
    if (!s || (t_fail_str && !strcmp(t_fail_str, s))) return NULL;
    pthread_mutex_lock(&t_cf_lock);
    t = tcf_new_locked(TCF_STR);
    snprintf(t->text, sizeof(t->text), "%s", s);
    pthread_mutex_unlock(&t_cf_lock);
    return t;
}
static void *t_ret(void *p)
{
    tcf_t *t = tcf_of(p);
    if (!t) return p;                 /* the stock session buffer */
    pthread_mutex_lock(&t_cf_lock);
    assert(t->refs > 0);
    ++t->refs;
    pthread_mutex_unlock(&t_cf_lock);
    return p;
}
static void t_rel(void *p)
{
    tcf_t *t = tcf_of(p);
    if (!t) return;
    pthread_mutex_lock(&t_cf_lock);
    tcf_rel_locked(t);
    pthread_mutex_unlock(&t_cf_lock);
}
static void *t_dictm(void *a, long cap, const void *kcb, const void *vcb)
{
    tcf_t *t;
    (void)a; (void)cap; (void)kcb; (void)vcb;
    pthread_mutex_lock(&t_cf_lock);
    t = tcf_new_locked(TCF_DICT);
    pthread_mutex_unlock(&t_cf_lock);
    return t;
}
static void *t_arrm(void *a, long cap, const void *cb)
{
    tcf_t *t;
    (void)a; (void)cap; (void)cb;
    pthread_mutex_lock(&t_cf_lock);
    t = tcf_new_locked(TCF_ARR);
    pthread_mutex_unlock(&t_cf_lock);
    return t;
}
/* untraced: the test's own requests and the stock main display */
static void tcf_set_raw(tcf_t *d, const char *key, tcf_t *v)
{
    int i;
    pthread_mutex_lock(&t_cf_lock);
    assert(d && d->kind == TCF_DICT && v);
    ++v->refs;
    for (i = 0; i < d->n; ++i)
        if (!strcmp(d->keys[i]->text, key)) {
            tcf_rel_locked(d->vals[i]);
            d->vals[i] = v;
            pthread_mutex_unlock(&t_cf_lock);
            return;
        }
    assert(d->n < TCF_MAX);
    d->keys[d->n] = tcf_new_locked(TCF_STR);
    snprintf(d->keys[d->n]->text, sizeof(d->keys[d->n]->text), "%s", key);
    d->vals[d->n++] = v;
    pthread_mutex_unlock(&t_cf_lock);
}
static void tcf_app_raw(tcf_t *a, tcf_t *v)
{
    pthread_mutex_lock(&t_cf_lock);
    assert(a && a->kind == TCF_ARR && v && a->n < TCF_MAX);
    ++v->refs;
    a->vals[a->n++] = v;
    pthread_mutex_unlock(&t_cf_lock);
}
static tcf_t *tcf_num(int kind, int64_t v)
{
    tcf_t *t;
    pthread_mutex_lock(&t_cf_lock);
    t = tcf_new_locked(kind);
    t->num = v;
    pthread_mutex_unlock(&t_cf_lock);
    return t;
}
static void tcf_set_i64(tcf_t *d, const char *key, int64_t v)
{
    tcf_t *n = tcf_num(TCF_NUM, v);
    tcf_set_raw(d, key, n);
    t_rel(n);
}
static void tcf_set_s(tcf_t *d, const char *key, const char *s)
{
    tcf_t *v = (tcf_t *)t_str(NULL, s, 0);
    tcf_set_raw(d, key, v);
    t_rel(v);
}
static void tcf_set_obj(tcf_t *d, const char *key, tcf_t *v)
{
    tcf_set_raw(d, key, v);
    t_rel(v);
}

static void t_dictset(void *d, const void *k, const void *v)
{
    tcf_t *dd = tcf_of(d), *kk = tcf_of(k), *vv = tcf_of(v);
    char desc[200];
    int i;
    assert(dd && dd->kind == TCF_DICT && kk && kk->kind == TCF_STR && vv);
    tcf_desc(vv, kk->text, desc, sizeof(desc));
    trace_add("D%u.%s=%s", dd->id, kk->text, desc);
    pthread_mutex_lock(&t_cf_lock);
    ++kk->refs;
    ++vv->refs;
    for (i = 0; i < dd->n; ++i)
        if (!strcmp(dd->keys[i]->text, kk->text)) {
            tcf_rel_locked(dd->keys[i]);
            tcf_rel_locked(dd->vals[i]);
            dd->keys[i] = kk;
            dd->vals[i] = vv;
            pthread_mutex_unlock(&t_cf_lock);
            return;
        }
    assert(dd->n < TCF_MAX);
    dd->keys[dd->n] = kk;
    dd->vals[dd->n++] = vv;
    pthread_mutex_unlock(&t_cf_lock);
}
static int t_dictseti(void *d, const void *k, int64_t v)
{
    tcf_t *n = tcf_num(TCF_NUM, v);
    t_dictset(d, k, n);
    t_rel(n);
    return 0;
}
static tcf_t *tcf_get(const void *d, const char *key)
{
    tcf_t *dd = tcf_of(d), *r = NULL;
    int i;
    if (!dd || dd->kind != TCF_DICT || !key) return NULL;
    pthread_mutex_lock(&t_cf_lock);
    for (i = 0; i < dd->n; ++i)
        if (!strcmp(dd->keys[i]->text, key)) { r = dd->vals[i]; break; }
    pthread_mutex_unlock(&t_cf_lock);
    return r;
}
static void *t_getval(void *d, const void *k)
{
    tcf_t *kk = tcf_of(k);
    return kk ? tcf_get(d, kk->text) : NULL;
}
static int64_t t_geti64(void *d, const void *k, void *err)
{
    tcf_t *kk = tcf_of(k), *v = kk ? tcf_get(d, kk->text) : NULL;
    if (v && (v->kind == TCF_NUM || v->kind == TCF_BOOL)) {
        if (err) *(int *)err = 0;
        return v->num;
    }
    if (err) *(int *)err = -6727;
    return 0;
}
static int64_t t_geti64_value(void *v, void *err)
{
    tcf_t *t = tcf_of(v);
    int ok = t && (t->kind == TCF_NUM || t->kind == TCF_BOOL);
    if (err) *(int *)err = ok ? 0 : -6756;
    return ok ? t->num : 0;
}
static int t_arrappc(void *a, const char *s, int len)
{
    tcf_t *aa = tcf_of(a), *v;
    (void)len;
    assert(aa && aa->kind == TCF_ARR);
    v = (tcf_t *)t_str(NULL, s, 0);
    if (!v) return -1;
    trace_add("A%u+=\"%s\"", aa->id, s);
    tcf_app_raw(aa, v);
    t_rel(v);
    return 0;
}
static void t_arrapp(void *a, const void *v)
{
    tcf_t *aa = tcf_of(a), *vv = tcf_of(v);
    char desc[200];
    assert(aa && aa->kind == TCF_ARR && vv);
    tcf_desc(vv, NULL, desc, sizeof(desc));
    trace_add("A%u+=%s", aa->id, desc);
    tcf_app_raw(aa, vv);
}
static int t_arrappi64(void *a, int64_t v)
{
    tcf_t *n = tcf_num(TCF_NUM, v);
    t_arrapp(a, n);
    t_rel(n);
    return 0;
}
static long t_arrcount(void *a)
{
    tcf_t *aa = tcf_of(a);
    return (aa && aa->kind == TCF_ARR) ? aa->n : 0;
}
static void *t_arridx(void *a, long i)
{
    tcf_t *aa = tcf_of(a);
    assert(aa && aa->kind == TCF_ARR && i >= 0 && i < aa->n);
    return aa->vals[i];
}
static unsigned long t_typeof(const void *v)
{
    tcf_t *t = tcf_of(v);
    return t ? (unsigned long)t->kind : 0;
}
static unsigned long t_type_str(void)  { return TCF_STR; }
static unsigned long t_type_arr(void)  { return TCF_ARR; }
static unsigned long t_type_dict(void) { return TCF_DICT; }
static int t_getcstr(const void *v, char *buf, long max, uint32_t enc)
{
    tcf_t *t = tcf_of(v);
    (void)enc;
    if (!t || t->kind != TCF_STR || !buf || max <= 0 || (long)strlen(t->text) >= max) return 0;
    snprintf(buf, (size_t)max, "%s", t->text);
    return 1;
}
static void *t_copydisp(void *arg, void *err)
{
    tcf_t *d;
    (void)arg;
    *(int *)err = 0;
    d = (tcf_t *)t_dictm(NULL, 0, NULL, NULL);
    tcf_set_s(d, "uuid", "MAIN-UUID");
    tcf_set_i64(d, "widthPixels", 800);
    tcf_set_i64(d, "heightPixels", 480);
    tcf_set_i64(d, "widthPhysical", 188);
    tcf_set_i64(d, "heightPhysical", 88);
    return d;
}

/* ── stock AirPlay primitives ─────────────────────────────────────────────── */
static int t_fail_sockopen;   /* the next N ServerSocketOpen calls fail */
static void t_derive(const uint8_t *key, int len, uint64_t conn, uint8_t *k, uint8_t *iv)
{
    (void)key; (void)len;
    memset(k, (int)(conn & 0xff), 16);
    memset(iv, (int)((conn >> 8) & 0xff), 16);
    trace_add("derive(%llu)", (unsigned long long)conn);
}
static int t_aes_init(void *ctx, const uint8_t *k, const uint8_t *iv)
{ (void)ctx; (void)k; (void)iv; return 0; }
static int t_aes_update(void *ctx, const uint8_t *src, uint32_t len, uint8_t *dst)
{ (void)ctx; memmove(dst, src, len); return 0; }
static int t_aes_final(void *ctx) { (void)ctx; return 0; }
static int t_sockopen(int family, int type, int proto, int pref, int *port, int rcv, int *sock)
{
    struct sockaddr_in a;
    socklen_t len = sizeof(a);
    int fd, one = 1;
    (void)family; (void)type; (void)proto; (void)pref; (void)rcv;
    if (t_fail_sockopen > 0) { --t_fail_sockopen; return -6753; }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(fd, (struct sockaddr *)&a, sizeof(a)) == 0);
    assert(listen(fd, 4) == 0);
    assert(getsockname(fd, (struct sockaddr *)&a, &len) == 0);
    *port = ntohs(a.sin_port);
    *sock = fd;
    return 0;
}
static int t_accept(int ls, int timeout, int *fd, void *addr)
{
    struct pollfd p;
    (void)timeout; (void)addr;
    p.fd = ls; p.events = POLLIN; p.revents = 0;
    if (poll(&p, 1, 20) <= 0 || !(p.revents & POLLIN)) {
        if (p.revents & (POLLNVAL | POLLERR | POLLHUP)) usleep(2000);
        return -6722;
    }
    *fd = accept(ls, NULL, NULL);
    return *fd >= 0 ? 0 : -6722;
}

/* commands shipped by the worker (one ordered list; the worker is single) */
typedef struct { char type[32], uuid[64], url[128], extra[96]; } t_cmd_t;
static t_cmd_t t_cmds[256];
static unsigned t_cmd_count;
static int t_sendcmd(void *session, void *cmd, void *completion, void *ctx)
{
    tcf_t *c = tcf_of(cmd), *p, *v;
    t_cmd_t rec;
    int i;
    (void)session;
    memset(&rec, 0, sizeof(rec));
    assert(c);
    v = tcf_get(c, "type");
    if (v) snprintf(rec.type, sizeof(rec.type), "%s", v->text);
    p = tcf_get(c, "params");
    if (p) {
        pthread_mutex_lock(&t_cf_lock);
        for (i = 0; i < p->n; ++i) {
            const char *k = p->keys[i]->text;
            tcf_t *pv = p->vals[i];
            if (!strcmp(k, "uuid")) snprintf(rec.uuid, sizeof(rec.uuid), "%s", pv->text);
            else if (!strcmp(k, "url")) snprintf(rec.url, sizeof(rec.url), "%s", pv->text);
            else if (pv->kind == TCF_NUM) {
                size_t w = strlen(rec.extra);
                snprintf(rec.extra + w, sizeof(rec.extra) - w, "%s%s=%lld", w ? "," : "", k,
                         (long long)pv->num);
            }
        }
        pthread_mutex_unlock(&t_cf_lock);
    }
    pthread_mutex_lock(&t_ev_lock);
    assert(t_cmd_count < sizeof(t_cmds) / sizeof(t_cmds[0]));
    t_cmds[t_cmd_count++] = rec;
    pthread_cond_broadcast(&t_ev_cond);
    pthread_mutex_unlock(&t_ev_lock);
    if (completion) ((void (*)(int, void *, void *))completion)(0, NULL, ctx);
    return 0;
}

#include "altscreen_card_display_phone.h"
#endif
