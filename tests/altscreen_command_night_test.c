/* /info night seeding (issue #40): stock /info carries dio's startInNightMode byte,
 * which requestNightMode never updates.  Before dio sets the mode for a session the
 * cluster seeds from /info; afterwards that session's /info must not overwrite it,
 * and the stale top-level value is replaced.  A torn-down session seeds again. */
#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_hook.c"
#include <assert.h>

static int64_t info_night;      /* stock /info "nightMode" */
static int64_t written = -1;    /* what the hook wrote to /info "nightMode" */
static void *t_str(void *a, const char *s, unsigned enc) { (void)a; (void)enc; return (void *)s; }
static void t_rel(void *p) { (void)p; }
static void t_set(void *d, const void *k, const void *v) { (void)d; (void)k; (void)v; }
static int t_seti(void *d, const void *k, int64_t v)
{
    (void)d;
    if (!strcmp(k, "nightMode")) written = v;
    return 0;
}
static int64_t t_geti(const void *d, const void *k, int *err)
{
    (void)d;
    *err = strcmp(k, "nightMode") ? -1 : 0;
    return info_night;
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
    g_advert_ready = 1; g_advert_published = 1;
    alt_on_server_info(s, (void *)6);
    assert(g_alt_night_mode == 1);
    puts("altscreen_night_test: /info seeds until dio sets the mode, stale /info kept out PASS");
    return 0;
}
