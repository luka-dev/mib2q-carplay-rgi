/* Real framework router and fallback registration. Native image matching
 * and HTTPServerCreate are boundary models, not a QNX ABI assertion. */
#define ENABLE_LOGGING 0
#include <assert.h>
#include <stdlib.h>
#include <dlfcn.h>
static void *test_lookup(void *,const char *);
#define dlsym test_lookup
#include "../hook/framework/http210_seams.c"
#undef dlsym

static uintptr_t captured[11];
static int server, stock_calls, buffered_calls, module_calls, fingerprint=1;
#ifdef MHI2Q_HTTP210_LAB
static int lab_pre,lab_post;
static int lab_request(void *c,void *m,int *r) {
    assert(c==&server);++lab_pre;if(m==(void *)2){*r=711;return 1;}return 0;
}
static void lab_done(void *c,void *m,int r) {
    assert(c==&server);assert(m!=(void *)1 && m!=(void *)2);assert(r==103||r==210);++lab_post;
}
#endif
static int native_request(void *connection,void *message) {
    assert(connection==&server);(void)message;++stock_calls;return 210;
}
static int buffered_request(void *connection,void *message) {
    ++buffered_calls;
    if(message==(void *)103) return 103;
    return native_request(connection,message);
}
static int foreign_request(void *connection,void *message) {
    (void)connection;(void)message;assert(!"foreign callback executed");return -1;
}
static int module_open(void *connection) {(void)connection;return 0;}
static int module_request(void *connection,void *message,int *result) {
    (void)connection;++module_calls;
    if(message==(void *)1) {*result=403;return 1;}
    return 0;
}
static const hook_module_def_t module={.name="pairing-model",.http={.on_open=module_open,.on_request=module_request}};
hook_result_t hook_framework_init(void) {return HOOK_OK;}
size_t hook_framework_module_count(void) {return 1;}
const hook_module_def_t *hook_framework_module_at(size_t i) {return i?NULL:&module;}
int cp210_matches(void *create,void *request) {(void)create;return fingerprint && request==(void *)native_request;}
void cp_wireless_http_created(void *p) {assert(p==&server);}
void cp_wireless_http_queue(void *p,void *q) {(void)p;(void)q;}
static int native_create(void **out,const void *delegate) {
    memcpy(captured,delegate,sizeof(captured));*out=&server;return 0;
}
static void *test_lookup(void *scope,const char *name) {
#ifdef MHI2Q_HTTP210_LAB
    if(scope==RTLD_DEFAULT) {
        if(!strcmp(name,"mhi2q_lab_http_request"))return (void *)lab_request;
        if(!strcmp(name,"mhi2q_lab_http_done"))return (void *)lab_done;
        assert(!"unexpected lab symbol");
    }
#endif
    assert(scope==RTLD_NEXT);
    return !strcmp(name,"HTTPServerCreate") ? (void *)native_create : NULL;
}
int main(void) {
    uintptr_t slots[11]={0};void *out=NULL;
    slots[9]=(uintptr_t)native_request;
    setenv("MHI2Q_DIO_OWNER_MODE","wired",1);
    setenv("MHI2Q_CP_WIRELESS_PAIRING","1",1);
    assert(!HTTPServerCreate(&out,slots) && captured[9]==slots[9]);
    assert(!hook_http210_set_request_fallback((void *)captured[9],(void *)native_request,buffered_request));
    setenv("MHI2Q_DIO_OWNER_MODE","wireless",1);
    fingerprint=0;
    assert(!HTTPServerCreate(&out,slots) && captured[9]==slots[9]);
    fingerprint=1;
    assert(!HTTPServerCreate(&out,slots) && captured[9]!=slots[9]);
    int (*router)(void *,void *)=(void *)captured[9];
    assert(router(&server,NULL)==210 && stock_calls==1);
    assert(!hook_http210_set_request_fallback((void *)foreign_request,(void *)native_request,buffered_request));
    assert(!hook_http210_set_request_fallback((void *)router,(void *)foreign_request,buffered_request));
    assert(hook_http210_set_request_fallback((void *)router,(void *)native_request,buffered_request));
    assert(hook_http210_set_request_fallback((void *)router,(void *)native_request,buffered_request));
    assert(!hook_http210_set_request_fallback((void *)router,(void *)native_request,foreign_request));
    assert(router(&server,(void *)1)==403 && buffered_calls==0 && stock_calls==1);
    assert(router(&server,(void *)103)==103 && buffered_calls==1 && stock_calls==1);
    assert(router(&server,NULL)==210 && buffered_calls==2 && stock_calls==2 && module_calls==4);
    assert(!HTTPServerCreate(&out,slots) && captured[9]==(uintptr_t)router);
    assert(router(&server,NULL)==210 && buffered_calls==3 && stock_calls==3);
#ifdef MHI2Q_HTTP210_LAB
    assert(lab_pre==4 && lab_post==4);
    assert(router(&server,(void *)2)==711 && lab_pre==5 && lab_post==4);
    assert(router(&server,(void *)1)==403 && lab_pre==5 && lab_post==4);
#endif
    puts("HTTP210 composition: wired/pin/foreign/conflict/idempotence/module-first/buffered/stock/recreate PASS");
}
