#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <assert.h>
#define MHI2Q_CAPTURE_INPUT_QEMU 1
#define MHI2Q_CAPTURE_VIDEO_MEMORY_QEMU 1
#include "qnx_input_probe.h"

static pthread_mutex_t blocked_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t blocked_cond = PTHREAD_COND_INITIALIZER;
static int blocked_started, unblock, releases;
static int blocked_force(void *session, void *completion, void *context)
{
    (void)session; (void)completion; (void)context;
    pthread_mutex_lock(&blocked_lock);
    blocked_started = 1; pthread_cond_signal(&blocked_cond);
    while (!unblock) pthread_cond_wait(&blocked_cond, &blocked_lock);
    pthread_mutex_unlock(&blocked_lock);
    return 0;
}
static void fake_release(const void *session) { (void)session; releases++; }

int main(void)
{
    const uint8_t cfg[] = {0,0,0,1,0x67,1,0,0,0,1,0x68,2};
    const uint8_t idr[] = {0,0,0,1,0x65,3};
    const uint8_t delta[] = {0,0,1,0x41,4};
    uint8_t fuzz[256];
    unsigned seed = 32503, iteration, i;
    (void)lab_touch_register;
    (void)lab_avc_set_session;
    lab_avc_store("native-video-qemu", delta, sizeof(delta));
    assert(lab_avc[0].length == 0);
    lab_avc_store("native-video-qemu", cfg, sizeof(cfg));
    lab_avc_store("native-video-qemu", idr, sizeof(idr));
    lab_avc_store("native-video-qemu", delta, sizeof(delta));
    assert(lab_avc[0].length == sizeof(cfg)+sizeof(idr)+sizeof(delta));
    assert(!memcmp(lab_avc[0].bytes, cfg, sizeof(cfg)));
    lab_avc_store("native-video-qemu", idr, sizeof(idr));
    assert(lab_avc[0].length == sizeof(cfg)+sizeof(idr));
    lab_avc_store("native-cluster-qemu", cfg, sizeof(cfg));
    lab_avc_store("native-cluster-qemu", idr, sizeof(idr));
    assert(lab_avc[1].length == lab_avc[0].length);
    lab_avc_store("native-video-qemu", NULL, 10);
    lab_avc_store("native-video-qemu", idr, UINT32_MAX);
    assert(lab_avc[0].length == sizeof(cfg)+sizeof(idr));
    lab_avc[0].length = LAB_AVC_LIMIT-2;
    lab_avc_store("native-video-qemu", delta, sizeof(delta));
    assert(!lab_avc[0].collecting && lab_avc[0].length == LAB_AVC_LIMIT-2);
    {
        int fd[2]; uint32_t length;
        assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
        lab_avc_snapshot(fd[0], 0, 0);
        assert(recv(fd[1], &length, sizeof(length), MSG_WAITALL) == sizeof(length));
        assert(ntohl(length) == 0); /* Capped GOP must not be returned as current. */
        lab_avc_snapshot(fd[0], 1, 1);
        assert(recv(fd[1], &length, sizeof(length), MSG_WAITALL) == sizeof(length));
        assert(ntohl(length) == 0); /* No authenticated session: never reuse old bytes. */
        close(fd[0]); close(fd[1]);
    }
    {
        uint64_t previous = lab_avc[0].generation;
        lab_avc_store("native-video-qemu", idr, sizeof(idr));
        assert(lab_avc[0].generation == previous+1);
    }
    assert(lab_avc[0].collecting && lab_avc[0].length == sizeof(cfg)+sizeof(idr));
    assert(lab_avc[0].timestamp_count == lab_avc[0].pictures);
    for (iteration = 0; iteration < 10000; iteration++) {
        unsigned length = iteration % sizeof(fuzz);
        for (i = 0; i < length; i++) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            fuzz[i] = (uint8_t)seed;
        }
        lab_avc_store(iteration & 1 ? "native-cluster-qemu" : "native-video-qemu", fuzz, length);
        assert(lab_avc[0].length <= LAB_AVC_LIMIT && lab_avc[1].length <= LAB_AVC_LIMIT);
    }
    {
        int fd[2]; uint32_t count; uint8_t stamp[8];
        assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
        /* Small transfer checks real wire framing, after fuzz exhausted capacity. */
        lab_avc[0].timestamp_count = 1; lab_avc[0].timestamps[0] = 0x0102030405060708ULL;
        lab_avc_timing(fd[0],0);
        assert(recv(fd[1],&count,sizeof(count),MSG_WAITALL) == sizeof(count));
        assert(ntohl(count) == 1);
        assert(recv(fd[1],stamp,sizeof(stamp),MSG_WAITALL) == sizeof(stamp));
        for (i=0;i<8;i++) assert(stamp[i] == i+1);
        close(fd[0]); close(fd[1]);
    }
    {
        struct lab_force_context *context = malloc(sizeof(*context));
        pthread_t worker;
        int fd[2]; uint8_t clocks[56]; uint64_t generation;
        assert(context);
        context->session = (void *)1; context->channel = 0;
        context->force = blocked_force; context->release = fake_release;
        lab_force_active = 1;
        assert(!pthread_create(&worker,NULL,lab_avc_force_worker,context));
        pthread_mutex_lock(&blocked_lock);
        while (!blocked_started) pthread_cond_wait(&blocked_cond,&blocked_lock);
        pthread_mutex_unlock(&blocked_lock);
        assert(!socketpair(AF_UNIX,SOCK_STREAM,0,fd));
        lab_clock_snapshot(fd[0]);
        assert(recv(fd[1],clocks,sizeof(clocks),MSG_WAITALL) == sizeof(clocks));
        assert(lab_avc_refresh(0,&generation) != 0); /* No second in-flight job. */
        pthread_mutex_lock(&blocked_lock);
        unblock = 1; pthread_cond_signal(&blocked_cond);
        pthread_mutex_unlock(&blocked_lock);
        assert(!pthread_join(worker,NULL));
        assert(!lab_force_active && !lab_force_result && releases == 1);
        close(fd[0]); close(fd[1]);
    }
    puts("AVC memory: IDR reset, configuration, channels, bounds and 10000 fuzz inputs OK");
    return 0;
}
