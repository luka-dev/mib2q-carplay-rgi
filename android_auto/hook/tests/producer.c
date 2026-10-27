#include "capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <libavcodec/avcodec.h>
void probe_log(const char *fmt,...) { (void)fmt; }
int main(int argc,char **argv) {
    AVCodecParserContext *p=av_parser_init(AV_CODEC_ID_H264);
    AVCodecContext *c=avcodec_alloc_context3(NULL);
    unsigned char buf[4096+AV_INPUT_BUFFER_PADDING_SIZE]={0}; FILE *in;
    int n; unsigned frames=0;
    if(argc!=2||!p||!c||!(in=fopen(argv[1],"rb"))||capture_init("test")) return 2;
    printf("%ld\n",(long)getpid()); fflush(stdout); sleep(1);
    while((n=fread(buf,1,4096,in))>0) {
        unsigned char *at=buf;
        while(n>0) {
            unsigned char *data; int size;
            int used=av_parser_parse2(p,c,&data,&size,at,n,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
            if(used<0) return 3;
            at+=used; n-=used;
            if(size) { capture_frame(data,size); ++frames; usleep(33000); }
        }
    }
    fprintf(stderr,"producer frames=%u; awaiting simulated GAL exit\n",frames);
    for(;;) pause();
}
