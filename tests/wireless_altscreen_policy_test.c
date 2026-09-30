#define ENABLE_LOGGING 0
#include "../hook/altscreen/altscreen_hook.c"
#include <assert.h>
static int stock_display_calls, mutations;
static void *no_display(void *s,void *err) { (void)s;(void)err;++stock_display_calls;return NULL; }
static void *test_string(void *a,const char *s,unsigned enc) { (void)a;(void)enc;return (void *)s; }
static void test_release(void *p) { (void)p; }
static void *test_get(void *d,const void *k) { (void)d;return !strcmp(k,"features")?(void *)1:NULL; }
static void test_set(void *d,const void *k,const void *v) { (void)d;(void)k;(void)v;++mutations; }
int run_altscreen_owner_tests(void) {
    setenv("ALTSCREEN_HANDLE_ALT","1",1);setenv("ALTSCREEN_ADVERT_DISPLAYS","1",1);
    cf_ok=disp_ok=alt_syms_ok=1;fn_copydisp=no_display;
    cf_str=test_string;cf_rel=test_release;cf_getval=test_get;cf_dictset=test_set;
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    hook_setup_ctx_t setup={0};setup.session=(void *)1;setup.request=(void *)2;
    setup.response=(void *)3;setup.sequence=1;
    alt_on_setup_response(&setup);
    assert(setup.result==0 && !stock_display_calls && !mutations && !g_advert_ready);
    // Even a cached wired contract must not escape into wireless /info.
    g_advert_session=setup.session;g_advert_displays=(void *)4;g_advert_features=(void *)5;g_advert_ready=1;
    alt_on_server_info(setup.session,(void *)6);
    assert(!mutations && !stock_display_calls);
    assert(!advert_preflight(setup.session,2));assert(!g_advert_ready);
    // Wired and ordinary builds still enter stock display preflight with the
    // same wrapper flags. Missing graphics is the fixture's only rejection.
    for(int wired=0;wired<2;++wired) {
        if(wired)setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
        else unsetenv("MHI2Q_DIO_OWNER_MODE");
        setup.sequence++;alt_on_setup_response(&setup);
        assert(stock_display_calls==wired+1 && setup.result==0);
    }
    puts("altScreen policy: paired negotiation/info suppressed, cached advert blocked, wired preflight retained PASS");
    return 0;
}
