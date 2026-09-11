/* LOCAL REVIEW ONLY: synthetic checks, not part of the shared selection. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "avcc.h"
#include "frame_io.h"
#include "process_scope.h"

static void check_avcc(void) {
    const uint8_t expected[] = {0,0,0,1,0x65,0xa1,0xa2,0,0,0,1,0x41,0xb1};
    for (uint8_t ls = 1; ls <= 4; ++ls) {
        uint8_t input[16] = {0}, output[32];
        uint32_t n = 0;
        n += ls; input[n-1] = 3;
        memcpy(input+n, expected+4, 3); n += 3;
        n += ls; input[n-1] = 2;
        memcpy(input+n, expected+11, 2); n += 2;
        memset(output, 0xcc, sizeof(output));
        assert(avcc_to_annexb(input,n,ls,NULL,0) == sizeof(expected));
        assert(avcc_to_annexb(input,n,ls,output,sizeof(output)) == sizeof(expected));
        assert(memcmp(output,expected,sizeof(expected)) == 0);
        assert(output[sizeof(expected)] == 0xcc);
        memset(output, 0xcc, sizeof(output));
        assert(avcc_to_annexb(input,n,ls,output,6) == 0);
        assert(output[0] == 0xcc);
        assert(avcc_to_annexb(input,n,ls,output,7) == 7);
        assert(output[7] == 0xcc);
        assert(avcc_to_annexb(input,n-1,ls,NULL,0) == 7);
        input[ls-1] = 0;
        assert(avcc_to_annexb(input,n,ls,NULL,0) == 0);
    }
    const uint8_t fallback[] = {0,0,0,1,0x65};
    assert(avcc_to_annexb(fallback,5,0,NULL,0)==5);
    assert(avcc_to_annexb(fallback,5,5,NULL,0)==5);
    assert(avcc_to_annexb(fallback,3,4,NULL,0)==0);
}

static int packet_reader(const uint8_t *bytes, size_t size) {
    int fd[2];
    assert(pipe(fd)==0);
    assert(write(fd[1],bytes,size)==(ssize_t)size);
    close(fd[1]);
    return fd[0];
}

static void check_frames(void) {
    const uint8_t stream[] = {0,0,0,0, 0,0,0,3, 0x11,0x22,0x33};
    uint8_t *buf=NULL;
    uint32_t cap=0, len=999;
    int fd=packet_reader(stream,sizeof(stream));
    assert(frame_read(fd,&buf,&cap,&len,3)==0 && len==0);
    assert(frame_read(fd,&buf,&cap,&len,3)==0 && len==3 && cap>=3);
    assert(memcmp(buf,stream+8,3)==0);
    assert(frame_read(fd,&buf,&cap,&len,3)==-1);
    close(fd); free(buf); buf=NULL; cap=0;
    fd=packet_reader(stream+4,sizeof(stream)-4);
    assert(frame_read(fd,&buf,&cap,&len,2)==-2);
    assert(buf==NULL && cap==0);
    close(fd);
    fd=packet_reader(stream+4,sizeof(stream)-5);
    assert(frame_read(fd,&buf,&cap,&len,3)==-1);
    close(fd); free(buf);
    uint8_t short_header[2]={0,0};
    buf=NULL; cap=0;
    fd=packet_reader(short_header,sizeof(short_header));
    assert(frame_read(fd,&buf,&cap,&len,3)==-1);
    close(fd); free(buf);
}

static void check_process_scope(void) {
    assert(hook_process_name_matches("/example/bin/example_host","example_host")==HOOK_PROCESS_MATCH);
    assert(hook_process_name_matches("example_host","example_host")==HOOK_PROCESS_MATCH);
    assert(hook_process_name_matches("/example_host/bin/other","example_host")==HOOK_PROCESS_OTHER);
    assert(hook_process_name_matches("example_host_helper","example_host")==HOOK_PROCESS_OTHER);
    assert(hook_process_name_matches("", "example_host")==HOOK_PROCESS_UNKNOWN);
    assert(hook_process_name_matches(NULL, "example_host")==HOOK_PROCESS_UNKNOWN);
    assert(hook_process_name_matches("/example/bin/", "example_host")==HOOK_PROCESS_UNKNOWN);
    assert(hook_process_name_matches("example_host", "")==HOOK_PROCESS_UNKNOWN);
    assert(hook_process_name_matches("example_host", "bad/name")==HOOK_PROCESS_UNKNOWN);
}

int main(void) {
    check_avcc();
    check_frames();
    check_process_scope();
    return 0;
}
