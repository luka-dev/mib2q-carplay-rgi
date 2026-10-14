/* Opt-in lab instrumentation; compile only into the owned diagnostic QEMU. */
void mht_packet(const char *stage, const uint8_t *buf, size_t size);

#ifdef MHT_IMPLEMENTATION
#include <time.h>
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

static FILE *mht_file;
static unsigned mht_rows;

static uint64_t mht_monotonic_ns(void)
{
#ifdef __APPLE__
    mach_timebase_info_data_t base;
    mach_timebase_info(&base);
    return (uint64_t)((__uint128_t)mach_absolute_time() * base.numer / base.denom);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000 + now.tv_nsec;
#endif
}

static void mht_close(void)
{
    if (mht_file) {
        fprintf(mht_file, "{\"event\":\"end\",\"unit\":\"" MHT_UNIT
                "\",\"rows\":%u,\"truncated\":%s}\n", mht_rows,
                mht_rows >= 131072 ? "true" : "false");
        fclose(mht_file);
    }
}

void mht_packet(const char *stage, const uint8_t *buf, size_t size)
{
    static bool initialized;
    unsigned iplen, total, tcphead, offset, payload, addrlen, src, dst;
    char source[33], destination[33];
    uint32_t seq, ack;
    uint64_t stamp;
    if (!initialized) {
        const char *path = getenv("MHI2Q_QEMU_NET_TRACE");
        initialized = true;
        /* The host creates the owned private file; never create another path. */
        if (path) {
            int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC | O_NOFOLLOW);
            if (fd >= 0) mht_file = fdopen(fd, "a");
            if (fd >= 0 && !mht_file) close(fd);
            if (mht_file) {
                setvbuf(mht_file, NULL, _IOFBF, 65536);
                fprintf(mht_file, "{\"event\":\"begin\",\"unit\":\"" MHT_UNIT "\",\"schema\":1}\n");
                atexit(mht_close);
            }
        }
    }
    if (!mht_file || mht_rows >= 131072 || size < 54) return;
    if (buf[12] == 8 && buf[13] == 0 && buf[14] >> 4 == 4 && buf[23] == 6) {
        iplen = (buf[14] & 15) * 4;
        total = ((unsigned)buf[16] << 8) | buf[17];
        /* No fragments, malformed lengths, or payload logging. */
        if (iplen < 20 || (buf[20] & 0x3f) || buf[21]) return;
        src = 26; dst = 30; addrlen = 4;
    } else if (buf[12] == 0x86 && buf[13] == 0xdd && buf[14] >> 4 == 6 && size >= 74 && buf[20] == 6) {
        /* Extension-header/fragment traffic is deliberately outside this probe. */
        iplen = 40;
        total = 40 + (((unsigned)buf[18] << 8) | buf[19]);
        src = 22; dst = 38; addrlen = 16;
    } else return;
    if (total < iplen + 20 || size < 14 + total) return;
    offset = 14 + iplen;
    tcphead = (buf[offset + 12] >> 4) * 4;
    if (tcphead < 20 || total < iplen + tcphead) return;
    payload = total - iplen - tcphead;
    seq = ldl_be_p(buf + offset + 4);
    ack = ldl_be_p(buf + offset + 8);
    for (unsigned i = 0; i < addrlen; i++) {
        snprintf(source + 2*i, 3, "%02x", buf[src+i]);
        snprintf(destination + 2*i, 3, "%02x", buf[dst+i]);
    }
    stamp = mht_monotonic_ns();
    fprintf(mht_file, "{\"event\":\"packet\",\"stage\":\"%s\",\"mono_ns\":%" PRIu64
            ",\"virtual_ns\":%" PRId64 ",\"flow\":[\"%s\",\"%s\",%u,%u],"
            "\"seq\":%u,\"ack\":%u,\"flags\":%u,\"payload_bytes\":%u}\n",
            stage, stamp, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL),
            source, destination,
            ((unsigned)buf[offset] << 8) | buf[offset + 1],
            ((unsigned)buf[offset + 2] << 8) | buf[offset + 3], seq, ack,
            buf[offset + 13], payload);
    mht_rows++;
}
#endif
