/* /info night seeding (issue #40): stock /info carries dio's startInNightMode byte,
 * which requestNightMode never updates.  Before dio sets the mode for a session the
 * cluster seeds from /info; afterwards that session's /info must not overwrite it,
 * and the stale top-level value is replaced.  A torn-down session seeds again. */
#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_hook.c"
#include <assert.h>

static int64_t info_night;      /* stock /info "nightMode" */
static int64_t written = -1;    /* what the hook wrote to /info "nightMode" */

typedef struct {
    int count;
    const void *items[8];
} mock_arr_t;

static mock_arr_t g_arrs[16];
static int g_arr_used;
static const mock_arr_t *g_cluster_props;
static int g_fail_arrappc_at = -1;
static int g_arrappc_calls;

static void *t_str(void *a, const char *s, unsigned enc) { (void)a; (void)enc; return (void *)s; }
static void t_rel(void *p) { (void)p; }
static void *t_dictm(void *a, long cap, const void *kcb, const void *vcb)
{
    static int dict_seq;
    (void)a; (void)cap; (void)kcb; (void)vcb;
    return (void *)(uintptr_t)(0x1000 + (++dict_seq));
}
static void *t_arrm(void *a, long cap, const void *cb)
{
    (void)a; (void)cap; (void)cb;
    assert(g_arr_used < (int)(sizeof(g_arrs) / sizeof(g_arrs[0])));
    mock_arr_t *arr = &g_arrs[g_arr_used++];
    arr->count = 0;
    return arr;
}
static int t_arrappc(void *a, const char *s, int len)
{
    (void)len;
    if (g_arrappc_calls++ == g_fail_arrappc_at) return -1;
    mock_arr_t *arr = (mock_arr_t *)a;
    assert(arr->count < 8);
    arr->items[arr->count++] = s;
    return 0;
}
static void t_arrapp(void *a, const void *v)
{
    mock_arr_t *arr = (mock_arr_t *)a;
    assert(arr->count < 8);
    arr->items[arr->count++] = v;
}
static int t_arrappi64(void *a, int64_t v)
{
    t_arrapp(a, (const void *)(intptr_t)v);
    return 0;
}
static void t_set(void *d, const void *k, const void *v)
{
    if ((uintptr_t)d == 0x1001 || (const char *)k != NULL) {
        if (k && !strcmp((const char *)k, "properties"))
            g_cluster_props = (const mock_arr_t *)v;
    }
}
static int t_seti(void *d, const void *k, int64_t v)
{
    (void)d;
    if (!strcmp(k, "nightMode")) written = v;
    return 0;
}
static int64_t t_geti(void *d, const void *k, void *err)
{
    (void)d;
    *(int *)err = strcmp(k, "nightMode") ? -1 : 0;
    return info_night;
}

static void test_cluster_dict_properties(void)
{
    cf_dictm = t_dictm;
    cf_arrm = t_arrm;
    cf_arrapp = t_arrapp;
    cf_arrappi64 = t_arrappi64;

    /* Path 1: CFArrayAppendCString present */
    g_arr_used = 0;
    g_cluster_props = NULL;
    g_arrappc_calls = 0;
    g_fail_arrappc_at = -1;
    cf_arrappc = t_arrappc;
    assert(build_cluster_dict(0, 0) != NULL);
    assert(g_cluster_props != NULL && g_cluster_props->count == 2);
    assert(!strcmp((const char *)g_cluster_props->items[0], "Instrument Cluster"));
    assert(!strcmp((const char *)g_cluster_props->items[1], "showsInstruments"));

    /* Path 2: CFString + CFArrayAppendValue fallback */
    g_arr_used = 0;
    g_cluster_props = NULL;
    cf_arrappc = NULL;
    assert(build_cluster_dict(1, 1) != NULL);
    assert(g_cluster_props != NULL && g_cluster_props->count == 2);
    assert(!strcmp((const char *)g_cluster_props->items[0], "Instrument Cluster"));
    assert(!strcmp((const char *)g_cluster_props->items[1], "showsInstruments"));

    /* Path 3: partial append failure rejects the cluster dict cleanly */
    g_arr_used = 0;
    g_cluster_props = NULL;
    g_arrappc_calls = 0;
    g_fail_arrappc_at = 1;
    cf_arrappc = t_arrappc;
    assert(build_cluster_dict(0, 0) == NULL);
    assert(g_cluster_props == NULL);
}

int main(void)
{
    void *s = (void *)0x100;
    setenv("ALTSCREEN_HANDLE_ALT", "1", 1);
    unsetenv("MHI2Q_DIO_OWNER_MODE");
    cf_ok = alt_syms_ok = 1;
    cf_str = t_str; cf_rel = t_rel; cf_dictset = t_set;
    cf_dictseti = (cf_dictseti_t)t_seti; cf_geti64 = (fn_cfdict_geti64_t)t_geti;
    /* advert ready, already published (no display mutation) */
    g_advert_session = s; g_advert_displays = (void *)4; g_advert_features = (void *)5;
    g_advert_suggest_urls = (void *)7;
    g_advert_ready = 1; g_advert_published = 1;

    info_night = 1;                              /* no dio call yet: seed from /info */
    alt_on_server_info(s, (void *)6);
    assert(g_alt_night_mode == 1 && written == -1);

    g_alt_night_mode = 0; g_alt_night_dio_session = s;   /* what SetNightMode(s, day) records */
    alt_on_server_info(s, (void *)6);           /* stale /info still says night */
    assert(g_alt_night_mode == 0 && written == 0);

    advert_clear_for_session(s);                 /* teardown: the next session seeds again */
    assert(g_alt_night_dio_session == NULL);
    g_advert_session = s; g_advert_displays = (void *)4; g_advert_features = (void *)5;
    g_advert_suggest_urls = (void *)7;
    g_advert_ready = 1; g_advert_published = 1;
    alt_on_server_info(s, (void *)6);
    assert(g_alt_night_mode == 1);
    test_cluster_dict_properties();
    puts("altscreen_night_test: /info seeds until dio sets the mode, stale /info kept out + cluster properties PASS");
    return 0;
}
