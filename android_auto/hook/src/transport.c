/* SPDX-License-Identifier: GPL-3.0-or-later
 * Replaces the capture-only worker API in the separately built live hook.
 * No decoder, file writer, Screen calls or waits on the consumer in GAL. */
#include "capture.h"
#include "encoded_ring.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
static cl_ring *ring;
static char object_name[96];
static unsigned char codec[65536];
static unsigned codec_size, codec_types, epoch=1, need_idr=1;
static pthread_mutex_t producer=PTHREAD_MUTEX_INITIALIZER;
unsigned capture_nals(const void *data, size_t size) {
    const unsigned char *p=data; size_t i; unsigned types=0;
    if (!p) return 0;
    for(i=0;i+3<size;++i) if(!p[i]&&!p[i+1]&&p[i+2]==1) types|=1u<<(p[i+3]&31);
    return types;
}
int capture_init(const char *unused) {
    int fd; (void)unused;
    snprintf(object_name,sizeof(object_name),"/sq5_cluster_encoded_%ld",(long)getpid());
    fd=shm_open(object_name,O_RDWR|O_CREAT|O_EXCL,0600);
    if(fd<0) return -1;
    if(ftruncate(fd,sizeof(cl_ring))) { close(fd); shm_unlink(object_name); return -1; }
    ring=mmap(NULL,sizeof(cl_ring),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0); close(fd);
    if(ring==MAP_FAILED) { ring=NULL; shm_unlink(object_name); return -1; }
    /* Fresh O_EXCL object is zero-filled. Publish magic last. */
    ring->version=CL_VERSION; ring->owner=getpid(); ring->bytes=sizeof(cl_ring);
    __sync_synchronize(); ring->magic=CL_MAGIC;
    probe_log("live.transport object=%s bytes=%lu",object_name,(unsigned long)sizeof(cl_ring));
    return 0;
}
void capture_codec(const void *data,size_t size) {
    if(!ring||!data||!size) return;
    pthread_mutex_lock(&producer);
    if(size<=sizeof(codec)) {
        /* Codec messages normally contain both SPS and PPS. Retain split config
         * messages, resetting when another SPS announces a new configuration. */
        unsigned types=capture_nals(data,size);
        if(types&(1u<<7)) { codec_size=codec_types=0; ++epoch; need_idr=1; }
        if(size<=sizeof(codec)-codec_size) {
            memcpy(codec+codec_size,data,size); codec_size+=(unsigned)size; codec_types|=types;
        } else { codec_size=codec_types=0; need_idr=1; }
    }
    pthread_mutex_unlock(&producer);
}
void capture_frame(const void *data,size_t size) {
    cl_packet *p; unsigned types,key,extra;
    if(!ring||!data||!size) return;
    pthread_mutex_lock(&producer);
    types=capture_nals(data,size); key=!!(types&(1u<<5));
    if(!key && need_idr) goto out;
    extra=key?codec_size:0;
    if(key && ((types|codec_types)&0x180)!=0x180) goto out;
    if(size>CL_PACKET_MAX-extra || !(p=cl_write_slot(ring))) {
        ++ring->drops; if(!need_idr) ++epoch; need_idr=1; goto out;
    }
    if(extra) memcpy(p->data,codec,extra);
    memcpy(p->data+extra,data,size);
    p->size=extra+(unsigned)size; p->epoch=epoch; p->flags=key?CL_KEYFRAME:0;
    cl_publish(ring); need_idr=0;
out: pthread_mutex_unlock(&producer);
}
void capture_finish(const char *reason) {
    pthread_mutex_lock(&producer);
    ++epoch; need_idr=1; codec_size=codec_types=0;
    pthread_mutex_unlock(&producer);
    probe_log("live.transport reset reason=%s",reason);
}
void capture_join(void) {
    if(ring) { ring->stopped=1; __sync_synchronize(); munmap(ring,sizeof(cl_ring)); ring=NULL; }
    if(object_name[0]) shm_unlink(object_name);
}
