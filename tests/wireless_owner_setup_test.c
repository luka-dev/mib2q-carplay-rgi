#define ENABLE_LOGGING 0
#include "../hook/wireless/stock210_abi.h"
/* Only translate the ARM32 context pointers for this 64-bit host fixture. */
static int http_object, context_object, session_object;
static void *fixture_pointer(const void *p,size_t offset) {
    if(p==&http_object && offset==8)return &context_object;
    if(p==&context_object && offset==244)return &session_object;
    return NULL;
}
#define cp210_ptr fixture_pointer
#include "../hook/wireless/wireless_hook.c"
#undef cp210_ptr
#include <assert.h>
int run_media_contract_tests(void);
void *test_media_request(void);
void test_media_reset(void);
void test_media_encrypted(int);
int run_altscreen_owner_tests(void);
static int store_calls;
int cp_pair_store_open(const char *d,cp_pair_store **out) { (void)d;(void)out;++store_calls;return -1; }

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"altscreen"))return run_altscreen_owner_tests();
    run_media_contract_tests();
    if(argc==2 && !strcmp(argv[1],"media"))return 0;
    const char *modes[]={NULL,"wired","wireless"};
    for(unsigned mode=0;mode<3;++mode)for(int media=0;media<2;++media)
    for(int initialized=0;initialized<2;++initialized)for(int verified=0;verified<2;++verified) {
        if(modes[mode])setenv("MHI2Q_DIO_OWNER_MODE",modes[mode],1);
        else unsetenv("MHI2Q_DIO_OWNER_MODE");
        setenv("MHI2Q_CP_WIRELESS_MEDIA",media?"1":"0",1);
        test_media_reset();test_media_encrypted(verified);
        memset(connections,0,sizeof(connections));enabled=initialized;
        if(initialized){connections[0].http=&http_object;connections[0].pair=(void *)3;}
        hook_setup_ctx_t setup={0};setup.session=&session_object;setup.request=test_media_request();
        wireless_setup(&setup);
        int expected=(mode==2 && media && !(initialized && verified))?-6709:0;
        assert(setup.result==expected);
        setup.result=-123;wireless_setup(&setup);assert(setup.result==-123);
    }
    // Initializing wired/unclaimed owners never opens a wireless pairing store.
    setenv("MHI2Q_CP_WIRELESS_PAIRING","1",1);
    for(int wired=0;wired<2;++wired) {
        if(wired)setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
        else unsetenv("MHI2Q_DIO_OWNER_MODE");
        enabled=0;wireless_init();assert(!enabled && !store_calls);
    }
    // Wireless initialization failure must still reject a flagged SETUP.
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    setenv("MHI2Q_CP_PAIRING_DIR","/unused-test-store",1);
    setenv("MHI2Q_CP_WIRELESS_MEDIA","1",1);
    memset(connections,0,sizeof(connections));wireless_init();
    assert(!enabled && store_calls==1);
    hook_setup_ctx_t failed={0};failed.session=&session_object;
    wireless_setup(&failed);assert(failed.result==-6709);
    puts("wireless owner SETUP: mode/flags/init/verification matrix and init failure PASS");
    return 0;
}
