#include "semantic_delivery.h"
#include <assert.h>
#include <stdio.h>
static unsigned receives,outputs;
static int accept(unsigned kind,const uint8_t *p,size_t n) {
    assert(kind<=2&&p&&n);++receives;return 1;
}
static int next(uint8_t *p,size_t n) {
    assert(n>=6);++outputs;
    if(outputs>2)return 0;
    memcpy(p,"@@\0\6\122\0",6);return 6;
}
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i));}
int main(void) {
    struct cp_semantic_delivery s={0};uint8_t request[16]={0,1,0,0,0,0x40};
    assert(cp_semantic_input(&s,request,6,accept)==1&&receives==1);
    /* ACK lost after native execution: retry never appends artwork twice. */
    assert(cp_semantic_input(&s,request,6,accept)==1&&receives==1);
    request[5]^=1;assert(cp_semantic_input(&s,request,6,accept)==-1&&receives==1);request[5]^=1;
    request[0]=2;put(request+1,3);assert(cp_semantic_input(&s,request,6,accept)==-1&&receives==1);
    request[0]=0;assert(cp_semantic_input(&s,request,6,accept)==1&&receives==2);
    put(request+1,2);assert(cp_semantic_input(&s,request,6,accept)==-1);
    assert(cp_semantic_output(&s,0,next)==6&&outputs==1&&s.output_sequence==1);
    assert(cp_semantic_output(&s,0,next)==6&&outputs==1); /* response lost */
    assert(cp_semantic_output(&s,1,next)==6&&outputs==2&&s.output_sequence==2);
    assert(cp_semantic_output(&s,3,next)==-1&&outputs==2);
    assert(!cp_semantic_output(&s,2,next)&&outputs==3);
    assert(cp_semantic_output(&s,1,next)==-1);
    s.output_sequence=UINT32_MAX;assert(cp_semantic_output(&s,UINT32_MAX,next)==-1);
    unsigned random=17;
    for(unsigned i=0;i<20000;++i) {
        uint8_t mutated[16];memcpy(mutated,request,sizeof(mutated));
        random=random*1664525+1013904223;mutated[random%sizeof(mutated)]^=(uint8_t)(1+(random>>24));
        (void)cp_semantic_input(&s,mutated,i%17,accept);
        assert(s.input_size<=sizeof(s.input)&&s.output_size<=sizeof(s.output));
    }
    puts("semantic delivery: input exactly-once retry/conflict, explicit resync, output ACK/replay, saturation and 20000 mutations PASS");
}
