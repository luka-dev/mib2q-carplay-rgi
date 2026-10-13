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
    lab_avc_store("native-video-qemu", idr, sizeof(idr));
    assert(lab_avc[0].collecting && lab_avc[0].length == sizeof(cfg)+sizeof(idr));
    for (iteration = 0; iteration < 10000; iteration++) {
        unsigned length = iteration % sizeof(fuzz);
        for (i = 0; i < length; i++) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            fuzz[i] = (uint8_t)seed;
        }
        lab_avc_store(iteration & 1 ? "native-cluster-qemu" : "native-video-qemu", fuzz, length);
        assert(lab_avc[0].length <= LAB_AVC_LIMIT && lab_avc[1].length <= LAB_AVC_LIMIT);
    }
    puts("AVC memory: IDR reset, configuration, channels, bounds and 10000 fuzz inputs OK");
    return 0;
}
