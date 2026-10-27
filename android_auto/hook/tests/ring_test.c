#include "capture.h"
#include "encoded_ring.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
void probe_log(const char *fmt,...) { (void)fmt; }
int main(void) {
    char name[96]; int fd; unsigned i,epoch;
    cl_ring *r; cl_packet *p;
    unsigned char config[]={0,0,0,1,0x67,1,0,0,1,0x68,2};
    unsigned char idr[]={0,0,1,0x65,3},delta[]={0,0,1,0x41,4};
    assert(capture_init("")==0);
    snprintf(name,sizeof(name),"/sq5_cluster_encoded_%ld",(long)getpid());
    fd=shm_open(name,O_RDWR,0600); assert(fd>=0);
    r=mmap(NULL,sizeof(*r),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0); close(fd);
    assert(r!=MAP_FAILED&&r->magic==CL_MAGIC);
    capture_frame(delta,sizeof(delta)); assert(cl_pending(r)==0);
    capture_codec(config,sizeof(config)); capture_frame(idr,sizeof(idr));
    p=cl_read_slot(r); assert(p&&p->size==sizeof(config)+sizeof(idr)&&(p->flags&CL_KEYFRAME));
    assert(!memcmp(p->data,config,sizeof(config))); epoch=p->epoch; cl_release(r);
    for(i=0;i<CL_SLOTS;++i) capture_frame(delta,sizeof(delta));
    assert(cl_pending(r)==CL_SLOTS);
    capture_frame(delta,sizeof(delta)); assert(r->drops==1&&cl_pending(r)==CL_SLOTS);
    for(i=0;i<CL_SLOTS;++i) { assert(cl_read_slot(r)); cl_release(r); }
    capture_frame(delta,sizeof(delta)); assert(cl_pending(r)==0); /* references were lost */
    capture_frame(idr,sizeof(idr)); p=cl_read_slot(r);
    assert(p&&p->epoch!=epoch&&(p->flags&CL_KEYFRAME)); cl_release(r);
    capture_finish("session_end"); capture_frame(idr,sizeof(idr)); assert(!cl_pending(r));
    capture_codec(config,sizeof(config)); capture_frame(idr,sizeof(idr)); assert(cl_read_slot(r)); cl_release(r);
    /* Counter wrap does not make a full ring look empty. */
    r->write_count=r->read_count=0xfffffffeu;
    for(i=0;i<4;++i) capture_frame(delta,sizeof(delta));
    assert(cl_pending(r)==4); for(i=0;i<4;++i) { assert(cl_read_slot(r)); cl_release(r); }
    capture_join(); assert(r->stopped); munmap(r,sizeof(*r));
    assert(shm_open(name,O_RDONLY,0600)<0);
    puts("PASS: IDR gate, full-ring isolation, overflow resync, session reset, counter wrap, cleanup");
}
