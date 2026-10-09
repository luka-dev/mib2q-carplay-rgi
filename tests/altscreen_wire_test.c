#include <assert.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "../altscreen_render/frame_io.h"
#include "../common/alt_timing_stats.h"

static void check(const uint8_t *bytes, size_t n, int expected) {
    int fd[2];
    uint8_t *data = NULL;
    uint32_t cap = 0, len = 0;
    alt_frame_meta_t meta;
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    if (n)
        assert(write(fd[1], bytes, n) == (ssize_t)n);
    shutdown(fd[1], SHUT_WR);
    assert(av_read_frame_ex(fd[0], &data, &cap, &len, &meta) == expected);
    free(data);
    close(fd[0]);
    close(fd[1]);
}
int main(void) {
    uint8_t record[ALT_WIRE_HEADER_SIZE + 6], broken[sizeof(record)];
    alt_frame_meta_t in = {0}, out;
    uint32_t len;
    alt_timing_stat_t stats = {0};
    in.version = 2;
    in.flags = ALT_WIRE_IDR;
    in.generation = 19;
    in.sequence = UINT64_C(0x123456789abcdef0);
    in.sender_pts = UINT64_MAX;
    in.received_us = 12345;
    alt_wire_encode(record, 6, &in);
    memcpy(record + ALT_WIRE_HEADER_SIZE, "abcdef", 6);
    assert(!alt_wire_decode(record, ALT_WIRE_HEADER_SIZE, &len, &out));
    assert(len == 6 && out.sequence == in.sequence && out.sender_pts == in.sender_pts &&
           out.generation == 19);
    check(record, sizeof(record), 0);
    for (size_t n = 0; n < sizeof(record); ++n)
        check(record, n, -1);
    memcpy(broken, record, sizeof(record));
    broken[5] = 3;
    check(broken, sizeof(broken), -2);
    memcpy(broken, record, sizeof(record));
    broken[7] = 0x80;
    check(broken, sizeof(broken), -2);
    memcpy(broken, record, sizeof(record));
    alt_wire_put32(broken + 8, ALT_WIRE_MAX_PAYLOAD + 1);
    check(broken, sizeof(broken), -2);
    memcpy(broken, record, sizeof(record));
    alt_wire_put32(broken + 12, 0);
    check(broken, sizeof(broken), -2);
    memcpy(broken, record, sizeof(record));
    alt_wire_put64(broken + 16, 0);
    check(broken, sizeof(broken), -2);
    in.flags = ALT_WIRE_CONFIG;
    in.sequence = 0;
    alt_wire_encode(record, 6, &in);
    check(record, sizeof(record), 0);
    alt_wire_encode(record, 4097, &in);
    check(record, sizeof(record), -2);
    {
        uint8_t legacy[] = {0, 0, 0, 2, 1, 2};
        check(legacy, sizeof(legacy), 0);
    }
    {
        uint8_t keepalive[] = {0, 0, 0, 0};
        check(keepalive, sizeof(keepalive), 0);
    }
    {
        int fd[2];uint8_t ack[ALT_WIRE_ACK_SIZE],*data=NULL;uint32_t cap=0,size;alt_frame_meta_t meta;
        assert(!socketpair(AF_UNIX,SOCK_STREAM,0,fd));
        alt_wire_put32(ack,ALT_WIRE_ACK);alt_wire_put32(ack+4,7);alt_wire_put32(ack+8,1);
        assert(write(fd[1],ack,sizeof(ack))==sizeof(ack));
        assert(!av_read_frame_mode(fd[0],&data,&cap,&size,&meta,0));
        assert(!size && meta.flags==ALT_WIRE_INTERNAL_ACK && meta.generation==7);
        in.flags=ALT_WIRE_IDR;in.sequence=10;alt_wire_encode(record,6,&in);
        assert(write(fd[1],record,sizeof(record))==sizeof(record));
        assert(!av_read_frame_mode(fd[0],&data,&cap,&size,&meta,2));
        assert(size==6 && meta.sequence==10);free(data);close(fd[0]);close(fd[1]);
        check(ack,sizeof(ack),0);
        for(size_t n=0;n<sizeof(ack);++n)check(ack,n,-1);
        alt_wire_put32(ack+4,0);check(ack,sizeof(ack),-2);
        alt_wire_put32(ack+4,7);alt_wire_put32(ack+8,2);check(ack,sizeof(ack),-2);
    }
    for (unsigned i = 0; i < 95; ++i)
        alt_timing_add(&stats, 1000);
    for (unsigned i = 0; i < 5; ++i)
        alt_timing_add(&stats, 50000);
    assert(alt_timing_percentile(&stats, 95) == 1024 && alt_timing_percentile(&stats, 99) == 50000);
    assert(stats.over_33ms == 5 && !stats.over_66ms);
    puts("altscreen_wire_test: legacy/v2, metadata, truncation, malformed headers and timing tails "
         "PASS");
    return 0;
}
