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
    puts("AVC memory: IDR reset, configuration, channels, bounds and 10000 fuzz inputs OK");
    return 0;
}
