#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/mman.h>
#include "pair_setup.h"

static unsigned char owner[1024],handle[512],priv[8192],descriptor[64];
static unsigned char process[512],pulse[32];
static unsigned callbacks,pulses,fail_alloc,fail_pulse;
static void *target_arg;
static void target(void *arg){if(arg!=target_arg)abort();++callbacks;}
static void *test_malloc(size_t n){if(fail_alloc){--fail_alloc;return 0;}return malloc(n);}
static int test_pulse(int coid,int priority,int code,int value){
 (void)coid;(void)priority;(void)code;(void)value;++pulses;
 return fail_pulse?-1:0;
}
static void put(void *p,unsigned offset,uintptr_t value){*(uint32_t*)((char*)p+offset)=value;}
static unsigned nodes(void){unsigned n=0;uint32_t *p=*(uint32_t**)0x24a42c;
 while(p){if(++n>2048)abort();p=(uint32_t*)p[3];}return n;}
static void drain(void){unsigned n=0;while(nodes()){if(++n>2048)abort();((void(*)(void))0x1d5a80)();}}
static void fixture(unsigned bitmap,int stopped){
 put(handle,344,bitmap);put(handle,152,__builtin_popcount(bitmap));
 put(owner,128+536,stopped?0x440:0x40);
}
int main(void){
 setbuf(stdout,0);puts("PAIR DRIVER BEGIN");
 if(!dlopen("/proc/boot/stubs.so",RTLD_NOW|RTLD_GLOBAL))return 1;
 void *h=dlopen("/proc/boot/driver.so",0x4c00);if(!h){puts(dlerror());return 2;}
 void (*complete)(void*,void*,int)=dlsym(h,"moal_send_packet_complete");
 void *lib=dlopen("libc.so.3",RTLD_NOW|RTLD_NOLOAD);
 int (*addr)(void*,Dl_info*)=dlsym(lib,"dladdr");Dl_info d={0};
 if(!complete||!addr||!addr((void*)complete,&d))return 3;
 uintptr_t base=(uintptr_t)d.dli_fbase;
 if(native_setup())return 4;
 set_native_import("malloc",test_malloc);set_native_import("MsgSendPulse",test_pulse);
 *(uint32_t*)0x24a42c=0;*(uint32_t*)0x24a430=0x24a42c;
 *(void**)0x24a0dc=process;put(process,116,0);put(process,168,(uintptr_t)pulse);
 pulse[4]=6;put(pulse,8,0);
 if(pthread_mutex_init((pthread_mutex_t*)0x2393b4,0))return 5;
 target_arg=owner+128;
 put(handle,4,(uintptr_t)priv);put(priv,24,(uintptr_t)owner);
 put(priv,192,(uintptr_t)target);put(priv,196,(uintptr_t)target_arg);put(priv,200,(uintptr_t)target_arg);
 put(descriptor,28,2);*(uint16_t*)(descriptor+26)=64;
 int (*request)(void*,void*,void*)=0;
 if(PATCHED){
  if(((int(*)(void))(base+PREPARE))())return 6;
  uint32_t *ctx=(uint32_t*)(base+CONTEXTS);
  ctx[0]=ctx[1]=ctx[2]=1;ctx[4]=(uintptr_t)owner;ctx[5]=(uintptr_t)target_arg;
  ctx[6]=0x40000000;ctx[7]=1;
  *(uint32_t*)(base+NEXT_GENERATION)=1;
  request=(void*)(base+REQUEST);
 }
 fixture(0xffffffff,1);unsigned first=0;
 for(unsigned i=0;i<32;i++){
  descriptor[56]=i;complete(handle,descriptor,0);
  if(nodes()&&!first)first=i+1;
 }
 unsigned queued=nodes(),wake_pulses=pulses;
 printf("PAIR DRIVER native_state bitmap=%08x pending=%u retired=%u flags=%08x\n",*(uint32_t*)(handle+344),*(uint32_t*)(handle+152),descriptor[56],*(uint32_t*)(owner+128+536));
 drain();unsigned executed=callbacks;
 printf("PAIR DRIVER workload=completion32 patched=%d first=%u queued=%u pulses=%u callbacks=%u remaining=%u\n",PATCHED,first,queued,wake_pulses,executed,nodes());
 if(first!=(PATCHED?1:31)||queued!=(PATCHED?1:2)||executed!=queued||nodes())return 7;
 fixture(0xffffffff,0);
 for(unsigned i=0;i<32;i++){descriptor[56]=i;complete(handle,descriptor,0);}
 if(nodes())return 8;
 puts("PAIR DRIVER workload=not_stopped queued=0");
 fail_pulse=1;
 if(PATCHED)request(target,target_arg,owner);
 else ((int(*)(void*,void*,void*))NATIVE_ENQUEUE)(target,target_arg,owner);
 if(nodes())return 12;
 fail_pulse=0;unsigned pulse_before=callbacks;
 fixture(0xffffffff,1);
 for(unsigned i=0;i<32;i++){descriptor[56]=i;complete(handle,descriptor,0);}
 unsigned pulse_recovered=nodes();drain();
 printf("PAIR DRIVER workload=after_pulse_error patched=%d queued=%u callbacks=%u remaining=%u\n",PATCHED,pulse_recovered,callbacks-pulse_before,nodes());
 if(pulse_recovered!=(PATCHED?1:2)||callbacks-pulse_before!=pulse_recovered||nodes())return 13;
 /* Identical transport-error workload: 260 failed direct enqueue requests.
  * Completion-gate semantics are measured separately above and below.
  */
 for(unsigned i=0;i<260;i++){
  fail_alloc=1;
  if(PATCHED)request(target,target_arg,owner);
  else ((int(*)(void*,void*,void*))NATIVE_ENQUEUE)(target,target_arg,owner);
  if(nodes())return 9;
 }
 fail_alloc=0;unsigned before=callbacks;
 fixture(0xffffffff,1);
 for(unsigned i=0;i<32;i++){descriptor[56]=i;complete(handle,descriptor,0);}
 unsigned recovered=nodes();drain();
 printf("PAIR DRIVER workload=after260errors patched=%d queued=%u callbacks=%u remaining=%u\n",PATCHED,recovered,callbacks-before,nodes());
 if(recovered!=2||callbacks-before!=2||nodes())return 10;
 fixture(3,1);
 for(unsigned i=0;i<2;i++){descriptor[56]=i;complete(handle,descriptor,0);}
 unsigned clean_before=nodes();
 ((void(*)(void*))NATIVE_CLEAN)(owner);
 if(nodes())return 11;
 printf("PAIR DRIVER workload=owner_clean patched=%d queued=%u remaining=%u\n",PATCHED,clean_before,nodes());
 puts("PAIR DRIVER PASS");return 0;
}
