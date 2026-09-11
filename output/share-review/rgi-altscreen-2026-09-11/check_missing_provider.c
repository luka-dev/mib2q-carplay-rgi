/* LOCAL TEST ONLY: missing-symbol path without a fake provider loaded. */
#include "interpose_example.h"
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    int (*submit)(void *,const uint8_t *,size_t) = NULL;
    assert(argc==2);
    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    assert(library);
    void *address=dlsym(library,"demo_video_submit");
    assert(address && sizeof(address)==sizeof(submit));
    memcpy(&submit,&address,sizeof(submit));
    for(int i=0;i<2;++i) {
        errno=0;
        assert(submit(NULL,NULL,0)==-1 && errno==ENOSYS);
    }
    assert(dlclose(library)==0);
    puts("Missing provider: deterministic ENOSYS, no recursive fallback.");
    return 0;
}
