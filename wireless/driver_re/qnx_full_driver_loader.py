"""Load the complete patched driver in a disposable QNX6.5 QEMU guest.

Unresolved io-pkt dependencies are generated stubs; actual libc, dlopen, REL
relocations, static recursive mutex and module pinning execute in QNX. Does not
start a radio or contact the head unit. Run as a module from the repository root.
"""
def main():
    from pathlib import Path
    import re, shutil,json, argparse, subprocess, hashlib
    from wireless.graft.qemu_loader import docker,BASE,SDP,AUDI,FIRMWARE
    from elftools.elf.elffile import ELFFile
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver',type=Path,default=Path('build/wifi-latency-full-20261022/devnp-mrvl_wlan-sdiorm.so'))
    parser.add_argument('--out-dir',type=Path,default=Path('build/wifi-latency-full-20261022/qnx-loader'))
    parser.add_argument('--expect-deadlock',action='store_true',help='negative control: require crossed-lock markers then SIGALRM, never PASS')
    args=parser.parse_args()
    w=args.out_dir.resolve(); w.mkdir(parents=True,exist_ok=True)
    ex=w/'extracted';ex.mkdir(exist_ok=True)
    if not (ex/'proc/boot/devc-serdebug').exists():
     listing=docker(w,'dumpifs','-v',f'/src/{BASE.relative_to(AUDI)}.bin',capture_output=True).stdout
     for line in listing.splitlines():
      m=re.match(r'^\s+[0-9a-f]+\s+[0-9a-f]+\s+([^\s]+)',line)
      if m and not m[1].startswith(('Startup-','Image-','Root-','*.')) and not m[1].endswith('.*'):(ex/m[1]).parent.mkdir(parents=True,exist_ok=True)
     docker(w,'sh','-c','cd /work/extracted && dumpifs -x /src/'+str(BASE.relative_to(AUDI))+'.bin',capture_output=True)
    for f in ('startup-virt','libstartup.a'):shutil.copyfile(SDP/'tests/qemu'/f,w/f)
    libc=FIRMWARE/'armle/lib/libc.so.3'
    l=ELFFile(libc.open('rb'));exports={s.name for s in l.get_section_by_name('.dynsym').iter_symbols()if s['st_shndx']!='SHN_UNDEF'}
    p=args.driver;e=ELFFile(p.open('rb'))
    undef=[s.name for s in e.get_section_by_name('.dynsym').iter_symbols()if s.name and s['st_shndx']=='SHN_UNDEF' and s.name not in exports]
    (w/'stubs.S').write_text('.syntax unified\n.arm\n.text\n'+''.join(f'.global {n}\n.type {n},%function\n{n}: mov r0,#0\nbx lr\n.balign 8\n.space 2048\n'for n in undef))
    func=next(s for s in e.get_section_by_name('.dynsym').iter_symbols()if s.name and s['st_shndx']!='SHN_UNDEF' and s['st_info']['type']=='STT_FUNC')
    meta=json.loads(p.with_suffix('.so.json').read_text()); off=meta['payload_symbols']['wl_runtime_probe']; dataoff=meta['payload_symbols']['runtime_mutex']
    if hashlib.sha256(p.read_bytes()).hexdigest()!=meta['sha256']:raise SystemExit('driver/manifest hash mismatch')
    # Execute the real prepare function with exact native guard bytes/PLT.
    # Only the fixed io-pkt import GOT slots and hz are supplied by the fixture;
    # dladdr/dlsym/dlopen/mutex operations themselves execute in QNX libc.
    stack=ELFFile(Path('build/wifi-driver-re-20261018/io-pkt-v6-hc').open('rb'))
    loads=[s for s in stack.iter_segments()if s['p_type']=='PT_LOAD']
    def native_bytes(va,n):
        s=next(s for s in loads if s['p_vaddr']<=va and va+n<=s['p_vaddr']+s['p_filesz'])
        return s.data()[va-s['p_vaddr']:va-s['p_vaddr']+n]
    from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
    cs=Cs(CS_ARCH_ARM,CS_MODE_ARM);cs.detail=True
    imports={0x11867c:'pthread_mutex_lock',0x118c1c:'pthread_mutex_unlock',0x118400:'pthread_mutex_trylock',0x118628:'dlsym',0x118d54:'dlopen'}
    slots={}
    for va,name in imports.items():
        ins=list(cs.disasm(native_bytes(va,12),va));assert len(ins)==3
        assert ins[0].mnemonic==ins[1].mnemonic=='add' and ins[2].mnemonic=='ldr'
        slots[name]=va+8+ins[0].operands[2].imm+ins[1].operands[2].imm+ins[2].operands[1].mem.disp
    pages=set(v&~4095 for v in slots.values())|{0x24a000}
    for va,n,_hash in meta['stack_guards']:
        pages.update(range(va&~4095,((va+n+4095)&~4095),4096))
    stack_syms={s.name:int(s['st_value'])for s in stack.get_section_by_name('.symtab').iter_symbols()if s.name}
    setup=['#include <sys/mman.h>','static int native_setup(void){']
    for page in sorted(pages):
        setup.append('if(mmap((void*)'+hex(page)+',4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANON|MAP_FIXED,-1,0)==MAP_FAILED)return -1;')
    for i,(va,n,_hash)in enumerate(meta['stack_guards']):
        setup.append('static const unsigned char bytes'+str(i)+'[]={'+','.join(str(v)for v in native_bytes(va,n))+'};')
        setup.append('memcpy((void*)'+hex(va)+',bytes'+str(i)+','+str(n)+');')
    for name,slot in slots.items():setup.append('*(void**)'+hex(slot)+'=dlsym(RTLD_DEFAULT,"'+name+'");')
    setup+=['*(volatile uint32_t*)0x24a6d8=119;return 0;}']
    (w/'native_setup.h').write_text('\n'.join(setup)+'\n')
    (w/'probe.c').write_text('''#define _GNU_SOURCE
    #include <stdio.h>
    #include <stdint.h>
    #include <dlfcn.h>
    #include <pthread.h>
    #include <string.h>
    #include <errno.h>
    #include <unistd.h>
    #include "native_setup.h"
    static pthread_mutex_t peer_lock=PTHREAD_MUTEX_INITIALIZER;
    static pthread_mutex_t sync_lock=PTHREAD_MUTEX_INITIALIZER;
    static pthread_cond_t sync_cond=PTHREAD_COND_INITIALIZER;
    static int entered,go,busy_result,gate_result,mode,enqueue_calls;
    static void *seen_fn,*seen_arg,*seen_owner;
    static pthread_mutex_t *payload_mutex;
    static unsigned (*payload_gate)(void*,void*,void*);
    static int (*payload_request)(void*,void*,void*);
    static int capture_enqueue(void *fn,void *arg,void *owner) {
     seen_fn=fn;seen_arg=arg;seen_owner=owner;++enqueue_calls;return 0;
    }
    static void *contention_worker(void *unused) {
     (void)unused;
     pthread_mutex_lock(&peer_lock);
     printf("WORKER PEER LOCKED mode=%d\\n",mode);
     pthread_mutex_lock(&sync_lock);entered=1;pthread_cond_signal(&sync_cond);
     while(!go)pthread_cond_wait(&sync_cond,&sync_lock);
     pthread_mutex_unlock(&sync_lock);
     busy_result=pthread_mutex_trylock(payload_mutex);
     if(!busy_result)pthread_mutex_unlock(payload_mutex);
     printf("WORKER BEFORE PAYLOAD mode=%d\\n",mode);
     if(mode==0)gate_result=payload_gate(0,(void*)0x345680,(void*)0x345600);
     else gate_result=payload_request((void*)0x456700,(void*)0x345680,(void*)0x345600);
     printf("WORKER AFTER PAYLOAD mode=%d result=%d\\n",mode,gate_result);
     pthread_mutex_unlock(&peer_lock);return 0;
    }
    int main(void) {
     setbuf(stdout,0);puts("BEGIN");void *s=dlopen("/proc/boot/stubs.so",RTLD_NOW|RTLD_GLOBAL); if(!s){printf("STUB FAIL %s\\n",dlerror());return 1;}
     puts("STUB LOADED");void *h=dlopen("/proc/boot/driver.so",0x4c00);if(!h){printf("LOAD FAIL %s\\n",dlerror());return 2;}
     puts("DRIVER LOADED");
     void *libc_handle=dlopen("libc.so.3",RTLD_NOW|RTLD_NOLOAD);
     int (*probe_dladdr)(void*,Dl_info*)=libc_handle?(int(*)(void*,Dl_info*))dlsym(libc_handle,"dladdr"):0;
     Dl_info d; void *f=dlsym(h,"'''+func.name+'''"); if(!f||!probe_dladdr||!probe_dladdr(f,&d)){puts("ADDR FAIL");return 3;}
     uintptr_t base=(uintptr_t)f-'''+str(int(func['st_value']))+''';
     uint32_t out[5]={0};uint32_t magic=((uint32_t(*)(uint32_t*))(base+'''+str(off)+'''))(out);
     printf("LOADED base=%p dladdr=%p magic=%08x probe_base=%08x mutex=%u tx=%u ready=%u\\n",(void*)base,d.dli_fbase,magic,out[0],out[1],out[2],out[4]);
     Dl_info payload_info={0};int payload_addr=probe_dladdr((void*)(base+'''+str(meta['payload_symbols']['wl_native_base'])+'''),&payload_info);
     printf("PAYLOAD DLADDR ok=%d base=%p name=%s\\n",payload_addr,payload_info.dli_fbase,payload_info.dli_fname?payload_info.dli_fname:"NULL");
     void *actual_fn=*(void**)(base+0x80008);Dl_info got_info={0};int got_addr=probe_dladdr(actual_fn,&got_info);
     printf("GOT DLADDR ptr=%p expected=%p ok=%d base=%p\\n",actual_fn,(void*)(base+'''+str(meta['payload_symbols']['wl_native_base'])+'''),got_addr,got_info.dli_fbase);
     if(actual_fn!=(void*)(base+'''+str(meta['payload_symbols']['wl_native_base'])+'''))return 6;
     if(native_setup()){puts("NATIVE SETUP FAIL");return 7;}
     int prep=((int(*)(void))(base+'''+str(meta['payload_symbols']['wl_runtime_prepare'])+'''))();
     ((uint32_t(*)(uint32_t*))(base+'''+str(off)+'''))(out);
     printf("ACTUAL PREPARE result=%d ready=%d\\n",prep,(int)out[4]);
     uint32_t *pd=(uint32_t*)(base+'''+str(meta['payload_symbols'].get('prepare_diag',0))+''');
     printf("PREPARE DIAG %u resolver=%08x addr_result=%u name=%08x base=%08x expected=%08x pin=%08x\\n",pd[0],pd[1],pd[2],pd[3],pd[4],pd[5],pd[6]);
     if(prep||out[4]!=1)return 8;
     pthread_mutex_t *m=(pthread_mutex_t*)(base+'''+str(dataoff)+''');
     payload_mutex=m;
     payload_gate=(unsigned(*)(void*,void*,void*))(base+'''+str(meta['payload_symbols']['wl_tx_runtime_gate'])+''');
     payload_request=(int(*)(void*,void*,void*))(base+'''+str(meta['payload_symbols']['wl_tx_runtime_request'])+''');
     uint32_t *ctx=(uint32_t*)(base+'''+str(meta['payload_symbols']['contexts'])+''');
     ctx[0]=1;ctx[1]=1;ctx[2]=1;ctx[4]=0x345600;ctx[5]=0x345680;
     ctx[6]=0x40000000;ctx[7]=1;
     *(uint32_t*)(base+'''+str(meta['payload_symbols']['next_generation'])+''')=1;
     /* Queue endpoint is a capture stub, installed after native guards pass. */
     uint32_t *queue=(uint32_t*)'''+hex(stack_syms['stk_context_callback_2'])+''';
     queue[0]=0xe51ff004;queue[1]=(uint32_t)capture_enqueue;
     alarm(4);
     if(pthread_mutex_lock(m))return 9;
     int recursive_try=pthread_mutex_trylock(m);
     if(recursive_try)return 10;
     if(payload_gate(0,(void*)0x345680,(void*)0x345600)!=1)return 11;
     if(payload_request((void*)0x456700,(void*)0x345680,(void*)0x345600))return 14;
     if(enqueue_calls!=1||seen_fn==(void*)0x456700||seen_arg==(void*)0x345680||seen_owner!=(void*)0x345600)return 15;
     puts("RECURSIVE PAYLOAD GATE AND REQUEST PASS");
     pthread_mutex_unlock(m);
     for(mode=0;mode<2;++mode) {
     if(mode&&pthread_mutex_lock(m))return 16;
     entered=go=0;enqueue_calls=0;seen_fn=seen_arg=seen_owner=0;
     printf("MAIN PAYLOAD LOCKED mode=%d\\n",mode);
     pthread_t worker;
     if(pthread_create(&worker,0,contention_worker,0))return 12;
     pthread_mutex_lock(&sync_lock);
     while(!entered)pthread_cond_wait(&sync_cond,&sync_lock);
     printf("MAIN BEFORE GO mode=%d\\n",mode);
     go=1;pthread_cond_signal(&sync_cond);pthread_mutex_unlock(&sync_lock);
     /* Both locks are owned before crossing. A blocking gate recreates ABBA
      * and the alarm terminates the fixture rather than reporting success.
      */
     pthread_mutex_lock(&peer_lock);pthread_mutex_unlock(&peer_lock);
     pthread_mutex_unlock(m);pthread_join(worker,0);alarm(0);
     if(busy_result!=EBUSY||gate_result!=0)return 13;
     if(mode&&(enqueue_calls!=1||seen_fn!=(void*)0x456700||seen_arg!=(void*)0x345680||seen_owner!=(void*)0x345600))return 17;
     printf("CROSSED LOCK PASS mode=%d\\n",mode);
     }
     printf("CONTENTION PASS busy=%d gate=%d recursive_try=%d\\n",busy_result,gate_result,recursive_try);
     int tl=pthread_mutex_trylock(m),tu=pthread_mutex_unlock(m),a=pthread_mutex_lock(m),b=pthread_mutex_lock(m),c=pthread_mutex_unlock(m),v=pthread_mutex_unlock(m);
     printf("MUTEX size=%u align=%u operations=%d,%d,%d,%d count=%08x owner=%08x\\n",(unsigned)sizeof(*m),(unsigned)__alignof__(*m),a,b,c,v,((uint32_t*)m)[0],((uint32_t*)m)[1]);
     void *pin=dlopen(d.dli_fname,RTLD_NOW|RTLD_NOLOAD|RTLD_NODELETE);
     printf("SELF PIN %s %s\\n",pin?"PASS":"FAIL",pin?d.dli_fname:dlerror());
     if(!pin||magic!=0x574c5031||out[0]!=base||out[1]!=sizeof(*m)||tl||tu||a||b||c||v)return 4;
     dlclose(h);memset(out,0,sizeof(out));magic=((uint32_t(*)(uint32_t*))(base+'''+str(off)+'''))(out);
     printf("AFTER CLOSE magic=%08x base=%08x\\n",magic,out[0]);
     if(magic!=0x574c5031||out[0]!=base)return 5;
     puts("WIFI FULL ELF QNX PASS");fflush(stdout);return 0;
    }
    ''')
    docker(w,'arm-unknown-nto-qnx6.5.0eabi-gcc','-shared','-fPIC','-o','/work/stubs.so','/work/stubs.S',capture_output=True)
    docker(w,'arm-unknown-nto-qnx6.5.0eabi-gcc','-O2','-Wall','-Wextra','-Werror','-o','/work/probe','/work/probe.c',capture_output=True)
    shutil.copyfile(p,w/'driver.so')
    base=BASE.with_suffix('.build').read_text().split('[image=')[0].replace('/root/bsp-qemu-virt-src/src/hardware/startup/lib/arm/a.le.v7/libstartup.a','/work/libstartup.a').replace('/usr/qnx650','/opt/qnx650')
    source=base+'''[image=0x40200000]
    [virtual=armle-v7,raw] .bootstrap = {
     /work/startup-virt -S
     PATH=/proc/boot
     LD_LIBRARY_PATH=/proc/boot
     /work/extracted/proc/boot/procnto-smp
    }
    [+script] .script = {
     procmgr_symlink ../../proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
     /proc/boot/devc-serdebug -F -S
     waitfor /dev/ser1 4
     reopen /dev/ser1
     /proc/boot/probe
     display_msg "WIFI LOADER END"
    }
    [type=link] /tmp=/dev/shmem
    /proc/boot/devc-serdebug=/work/extracted/proc/boot/devc-serdebug
    /proc/boot/libc.so.3=/src/'''+str(libc.relative_to(AUDI))+'''
    /proc/boot/probe=/work/probe
    /proc/boot/stubs.so=/work/stubs.so
    /proc/boot/driver.so=/work/driver.so
    '''
    (w/'loader.build').write_text(source);docker(w,'mkifs','loader.build','loader.bin',capture_output=True)
    from wireless.graft.qemu_loader import QEMU
    command=[str(QEMU),'-M','virt','-m','2048','-cpu','cortex-a15','-device',f'loader,file={w}/loader.bin,addr=0x40200000,force-raw=on,cpu-num=0','-display','none','-serial','stdio','-monitor','none']
    try:
     result=subprocess.run(command,capture_output=True,timeout=8)
     log=result.stdout+result.stderr
    except subprocess.TimeoutExpired as exc:
     log=(exc.stdout or b'')+(exc.stderr or b'')
    (w/'loader.log').write_bytes(log)
    output=log.decode(errors='replace')
    if hashlib.sha256(p.read_bytes()).hexdigest()!=meta['sha256']:raise SystemExit('driver changed during QNX check')
    if args.expect_deadlock:
     required=('RECURSIVE PAYLOAD GATE AND REQUEST PASS','MAIN PAYLOAD LOCKED mode=0','WORKER PEER LOCKED mode=0','MAIN BEFORE GO mode=0','WORKER BEFORE PAYLOAD mode=0','SIGALRM')
     if not all(s in output for s in required) or any(s in output for s in ('WORKER AFTER PAYLOAD mode=0','CONTENTION PASS','WIFI FULL ELF QNX PASS','SIGSEGV')):raise SystemExit(f'deadlock control not localized: {w}/loader.log')
     receipt=dict(status='pass',binary_sha256=meta['sha256'],test='negative control: exact crossed-lock markers then SIGALRM',expected_failure='ABBA deadlock in original blocking TX gate')
     (w/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
     print(f'QNX localized deadlock control PASS: {w}/loader.log');return
    if 'WIFI FULL ELF QNX PASS' not in output or 'CONTENTION PASS' not in output or 'CROSSED LOCK PASS mode=1' not in output or 'SIGSEGV' in output:raise SystemExit(f'QNX loader check failed: {w}/loader.log')
    receipt=dict(status='pass',binary_sha256=meta['sha256'],test='real QNX6.5 GROUP|WORLD|LAZYLOAD dlopen/REL/actual prepare ready=1/recursive mutex/self pin/close',prepare_ready=1,driver_load_flags='0x4c00',limits='io-pkt guard pages/PLT/GOT/hz supplied by fixture and unresolved dependencies stubbed; actual libc loader/symbol lookup/mutex execute; no physical radio, attach, RF, or HU scheduling test')
    receipt['contention']='two real QNX threads: crossed payload/peer locks, actual TX gate and request busy fallback, original fn/arg/owner capture, recursive-owner gate/request'
    (w/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'QNX full ELF PASS: {w}/loader.log')

if __name__=='__main__':main()
