/* Standalone parser exercise: no QEMU dependency, packet payloads never logged. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#define MHT_UNIT "fuzz"
#define MHT_IMPLEMENTATION
#define QEMU_CLOCK_VIRTUAL 0
static int64_t qemu_clock_get_ns(int clock) { (void)clock; return 0; }
static uint32_t ldl_be_p(const void *p)
{
    const uint8_t *b=p;
    return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
}
#include "../common/qemu_net_trace.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static bool ready;
    if (!ready) {
        unsetenv("MHI2Q_QEMU_NET_TRACE");
        mht_packet("init", NULL, 0);
        mht_file=fopen("/dev/null","w");
        if (!mht_file) abort();
        ready=true;
    }
    mht_rows=0;
    mht_packet("fuzz", data, size);
    return 0;
}
