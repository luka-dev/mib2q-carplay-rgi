/* /info night seeding (issue #40): stock /info carries dio's startInNightMode byte,
 * which requestNightMode never updates.  Before dio sets the mode for a session the
 * cluster seeds from /info; afterwards that session's /info must not overwrite it,
 * and the stale top-level value is replaced.  A torn-down session seeds again. */
#define ENABLE_LOGGING 0
/* The product reads a literal /tmp marker; the test points it at its own file. */
static char alt_test_legacy_marker[512];
#define ALT_ADVERT_LEGACY_MARKER alt_test_legacy_marker
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
/* Keys (and values) the hook sets on the /info dictionary under test. */
static const void *g_info_dict = (void *)6;
static struct { const char *key; const void *value; } g_info_set[8];
static int g_info_count;

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
    if (d == g_info_dict && g_info_count < 8) {
        g_info_set[g_info_count].key = (const char *)k;
        g_info_set[g_info_count++].value = v;
    }
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
    if (err) *(int *)err = strcmp(k, "nightMode") ? -1 : 0;
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
    assert(build_cluster_dict(0, 0, 0) != NULL);
    assert(g_cluster_props != NULL && g_cluster_props->count == 2);
    assert(!strcmp((const char *)g_cluster_props->items[0], "Instrument Cluster"));
    assert(!strcmp((const char *)g_cluster_props->items[1], "showsInstruments"));

    /* Path 2: CFString + CFArrayAppendValue fallback */
    g_arr_used = 0;
    g_cluster_props = NULL;
    cf_arrappc = NULL;
    assert(build_cluster_dict(1, 1, 0) != NULL);
    assert(g_cluster_props != NULL && g_cluster_props->count == 2);
    assert(!strcmp((const char *)g_cluster_props->items[0], "Instrument Cluster"));
    assert(!strcmp((const char *)g_cluster_props->items[1], "showsInstruments"));

    /* Path 3: partial append failure rejects the cluster dict cleanly */
    g_arr_used = 0;
    g_cluster_props = NULL;
    g_arrappc_calls = 0;
    g_fail_arrappc_at = 1;
    cf_arrappc = t_arrappc;
    assert(build_cluster_dict(0, 0, 0) == NULL);
    assert(g_cluster_props == NULL);

    /* Path 4: the opt-in legacy advert omits "properties" and still succeeds */
    g_arr_used = 0;
    g_cluster_props = NULL;
    g_arrappc_calls = 0;
    g_fail_arrappc_at = -1;
    assert(build_cluster_dict(0, 0, 1) != NULL);
    assert(g_cluster_props == NULL && g_arrappc_calls == 0);
}

static const void *info_value(const char *key)
{
    int i;
    for (i = 0; i < g_info_count; ++i)
        if (!strcmp(g_info_set[i].key, key)) return g_info_set[i].value;
    return NULL;
}

static void publish_retained(void *s, int legacy)
{
    g_advert_session = s; g_advert_displays = (void *)4; g_advert_features = (void *)5;
    g_advert_suggest_urls = (void *)7;
    g_advert_ready = 1; g_advert_published = 1; g_advert_legacy = legacy;
    g_info_count = 0;
    alt_on_server_info(s, (void *)g_info_dict);
}

/* Disarmed /info keeps the atomic four-key contract; the armed legacy tree is
 * published under "displays" only. */
static void test_info_advert_shape(void)
{
    void *s = (void *)0x200;
    advert_clear_for_session(NULL);
    publish_retained(s, 0);
    assert(g_info_count == 4);
    assert(info_value("extendedFeatures") == (void *)5);
    assert(info_value("displays") == (void *)4);
    assert(info_value("displayPanels") == (void *)4);
    assert(info_value("altScreenSuggestUIURLs") == (void *)7);
    publish_retained(s, 1);
    assert(g_info_count == 2);
    assert(info_value("extendedFeatures") == (void *)5);
    assert(info_value("displays") == (void *)4);
    assert(!info_value("displayPanels") && !info_value("altScreenSuggestUIURLs"));
    advert_clear_for_session(NULL);
    assert(!g_advert_legacy && !g_advert_ready);
}

static void *t_copydisp(void *session, void *err)
{
    (void)session;
    *(int *)err = 0;
    return (void *)0x3000;
}
static void *t_getval(void *d, const void *k)
{
    (void)d;
    return !strcmp((const char *)k, "uuid") ? (void *)"MAIN-DISPLAY-UUID" : NULL;
}

/* End to end: the marker is read when the control SETUP builds this session's
 * tree, and /info then publishes that tree's shape.  Without the marker the
 * preflight and /info are the unchanged production contract. */
static void test_preflight_legacy_marker(void)
{
    void *s = (void *)0x300;
    const char *dir = getenv("TMPDIR");
    int fd, armed;
    setenv("ALTSCREEN_ADVERT_DISPLAYS", "1", 1);
    disp_ok = 1;
    fn_copydisp = t_copydisp;
    cf_getval = t_getval;
    cf_arrappc = t_arrappc;
    g_fail_arrappc_at = -1;
    snprintf(alt_test_legacy_marker, sizeof(alt_test_legacy_marker), "%s/alt_advert_legacy.XXXXXX",
             dir && dir[0] ? dir : "/tmp");
    fd = mkstemp(alt_test_legacy_marker);
    assert(fd >= 0);
    close(fd);
    assert(!unlink(alt_test_legacy_marker));
    for (armed = 0; armed < 2; ++armed) {
        if (armed) {
            fd = open(alt_test_legacy_marker, O_CREAT | O_WRONLY, 0600);
            assert(fd >= 0);
            close(fd);
        }
        g_arr_used = 0;
        g_cluster_props = NULL;
        assert(advert_preflight(s, (uint32_t)(10 + armed)) == 1);
        assert(g_advert_ready && g_advert_session == s && g_advert_legacy == armed);
        assert(g_advert_displays && g_advert_features && g_advert_suggest_urls);
        if (armed) {
            assert(g_cluster_props == NULL);
        } else {
            assert(g_cluster_props != NULL && g_cluster_props->count == 2);
        }
        g_info_count = 0;
        alt_on_server_info(s, (void *)g_info_dict);
        assert(g_advert_published);
        assert(info_value("displays") == g_advert_displays);
        assert(info_value("extendedFeatures") == g_advert_features);
        if (armed) {
            assert(g_info_count == 2);
            assert(!info_value("displayPanels") && !info_value("altScreenSuggestUIURLs"));
        } else {
            assert(g_info_count == 4);
            assert(info_value("displayPanels") == g_advert_displays);
            assert(info_value("altScreenSuggestUIURLs") == g_advert_suggest_urls);
        }
    }
    assert(!unlink(alt_test_legacy_marker));
    alt_test_legacy_marker[0] = '\0';
    advert_clear_for_session(NULL);
    assert(!g_advert_legacy);
}

int main(void)
{
    void *s = (void *)0x100;
    assert(!alt_url_diag_marker_path);   /* host gate: a stray /tmp file never arms it */
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
    test_info_advert_shape();
    test_preflight_legacy_marker();
    puts("altscreen_night_test: /info seeds until dio sets the mode, stale /info kept out + cluster properties"
         " + opt-in legacy displays-only advert PASS");
    return 0;
}
