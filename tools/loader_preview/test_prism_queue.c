/* Exercise source lifetime, concurrent submissions, failures and stalled input
 * against the same rendezvous used by the QNX compositor. GL pixels are covered
 * separately by test_prism_gpu; this test has no Screen/EGL dependencies. */
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../common/prism.h"
#include "../../altscreen_render/prism_queue.h"

typedef struct {
    prism_queue_t queue;
    int source[64], copied[64], cached, posts, single;
    handoff_t motion;
} fixture;
typedef struct { fixture *f; int begin,end; } producer_args;
static void *producer(void *opaque) {
    producer_args *a=opaque;
    for(int i=a->begin;i<a->end;i++) {
        a->f->source[i]=1000+i;
        int result=prism_queue_submit(&a->f->queue,i,123,750);
        assert(result==(i==31?-1:0));
        assert(a->f->copied[i]==1000+i);
        /* Recycling is permitted only after the consumer finished reading. */
        a->f->source[i]=-1;
    }
    return NULL;
}
static void *compositor(void *opaque) {
    fixture *f=opaque;
    int count=f->single?1:64;
    for(int ticks=0;f->posts<count || (f->single && f->motion.position!=1);ticks++) {
        unsigned int index; uint64_t arrival; int reset;
        if(prism_queue_take(&f->queue,&index,&arrival,&reset)) {
            assert(arrival==123);
            f->cached=f->source[index]; f->copied[index]=f->cached;
            f->motion.target=1;
            f->posts++;
            prism_queue_complete(&f->queue,index==31?-1:0);
        }
        if(reset) f->motion.target=0;
        handoff_step(&f->motion,1.0/30);
        struct timespec yield={0,1000000L}; nanosleep(&yield,NULL);
        assert(ticks<3000);
    }
    return NULL;
}
static void exercise(int single) {
    fixture f;memset(&f,0,sizeof(f));f.single=single;
    assert(!prism_queue_init(&f.queue));
    producer_args a={&f,0,single?1:32},b={&f,32,64};
    pthread_t render,pa,pb;
    assert(!pthread_create(&render,NULL,compositor,&f));
    assert(!pthread_create(&pa,NULL,producer,&a));
    if(!single) assert(!pthread_create(&pb,NULL,producer,&b));
    pthread_join(pa,NULL);if(!single) pthread_join(pb,NULL);pthread_join(render,NULL);
    if(single) {
        assert(f.motion.position==1 && f.cached==1000 && f.source[0]==-1);
        prism_queue_loader(&f.queue);
        unsigned int index;uint64_t arrival;int reset;
        assert(!prism_queue_take(&f.queue,&index,&arrival,&reset) && reset);
        f.motion.target=0;handoff_step(&f.motion,4);
        assert(f.motion.position==0 && f.cached==1000);
    } else assert(f.posts==64);
    prism_queue_destroy(&f.queue);
}
int main(void) {
    exercise(1);exercise(0);
    prism_queue_t stalled;memset(&stalled,0,sizeof(stalled));assert(!prism_queue_init(&stalled));
    assert(!prism_queue_idle(&stalled)); /* no redraw for a static settled stream */
    prism_queue_loader(&stalled);assert(prism_queue_idle(&stalled));
    unsigned int index;uint64_t arrival;int reset;
    assert(!prism_queue_take(&stalled,&index,&arrival,&reset) && reset);
    assert(prism_queue_submit(&stalled,7,123,10)==-2);
    assert(stalled.state==1); /* timeout never claims the source was released */
    assert(prism_queue_idle(&stalled));
    prism_queue_destroy(&stalled);
    puts("PRISM queue: one-frame transition, owned return, concurrent callbacks, post failure and bounded timeout passed.");
}
