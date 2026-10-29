/* Ordered loopback delivery independent of TCP connection lifetime. Input
 * retries replay the last result; output remains pending until acknowledged. */
#ifndef CP_SEMANTIC_DELIVERY_H
#define CP_SEMANTIC_DELIVERY_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct cp_semantic_delivery {
    uint32_t input_sequence,output_sequence;
    size_t input_size,output_size;
    uint8_t input[8192],output[512];
    int input_result;
};
static inline uint32_t cp_semantic_u32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static inline int cp_semantic_input(struct cp_semantic_delivery *s,const uint8_t *p,size_t n,
                                    int (*receive)(unsigned,const uint8_t *,size_t)) {
    if(n<6||n>sizeof(s->input)||p[0]>2||!receive)return -1;
    uint32_t seq=cp_semantic_u32(p+1);if(!seq)return -1;
    if(seq==s->input_sequence)return n==s->input_size&&!memcmp(p,s->input,n)?s->input_result:-1;
    /* An explicit BEGIN can resynchronize after an exhausted TCP retry.
     * Other packets never skip an unknown input, especially artwork data. */
    if(seq<=s->input_sequence||(seq!=s->input_sequence+1&&p[0]!=0))return -1;
    int result=receive(p[0],p+5,n-5);
    if(result) {s->input_sequence=seq;s->input_size=n;memcpy(s->input,p,n);s->input_result=result;}
    return result;
}
static inline int cp_semantic_output(struct cp_semantic_delivery *s,uint32_t acknowledged,
                                     int (*next)(uint8_t *,size_t)) {
    if(!next||acknowledged>s->output_sequence)return -1;
    if(acknowledged==s->output_sequence)s->output_size=0;
    else if(!s->output_size)return -1;
    if(!s->output_size) {
        if(s->output_sequence==UINT32_MAX)return -1;
        int size=next(s->output,sizeof(s->output));
        if(size<0||size>(int)sizeof(s->output))return -1;
        if(size){++s->output_sequence;s->output_size=(size_t)size;}
    }
    return (int)s->output_size;
}
#endif
