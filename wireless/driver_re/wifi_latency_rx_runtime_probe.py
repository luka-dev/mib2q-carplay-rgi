"""Execute actual ARM RX runtime C/glue with pinned driver instructions.

Only PI mutex, native tick source, table allocation and callout OS boundaries
are modeled. Does not claim QNX-loader or concurrent scheduler acceptance.
"""
from pathlib import Path
import io, json, subprocess
from elftools.elf.elffile import ELFFile
from unicorn import UC_PROT_ALL, UC_PROT_NONE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3
from .reorder_machine import ReorderMachine,DriverImage,compile_candidate,compile_window_candidate,compile_harden_candidate
from .probe_latency_candidates import TICK_US
from .callout_machine import CalloutMachine


def compile_runtime(out):
    out.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).parent
    sources=['wifi_latency_rx_runtime.c','wifi_latency_rx_runtime.S','wifi_latency_policy.c','rx_timer_churn_policy.S']
    objs=[]
    for name in sources:
        target=out/(name.replace('.','_')+'.o')
        subprocess.run(['clang','--target=armv7-none-eabi','-march=armv7-a','-marm','-mfloat-abi=soft',
                        '-O2','-fPIC','-ffreestanding','-fno-builtin','-fno-stack-protector',
                        '-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fno-vectorize',
                        '-fno-slp-vectorize','-Wall','-Wextra','-Werror','-c',str(root/name),'-o',str(target)],
                       check=True,capture_output=True)
        objs.append(target)
    stub=out/'runtime_test_stubs.S'
    stub.write_text('''.syntax unified
.arm
.text
.global wl_lock,wl_unlock,wl_native_ticks,wl_native_base,wl_orig_rx,wl_orig_delete,wl_orig_flush,wl_orig_create
wl_lock: bx lr
wl_unlock: bx lr
wl_native_ticks: bx lr
wl_native_base: bx lr
wl_orig_rx: ldr ip,=0x473a8
bx ip
wl_orig_delete: ldr ip,=0x46e8c
bx ip
wl_orig_flush: ldr ip,=0x472fc
bx ip
wl_orig_create: bx lr
''')
    obj=out/'runtime_test_stubs.o';subprocess.run(['clang','--target=armv7-none-eabi','-c',str(stub),'-o',str(obj)],check=True,capture_output=True);objs.append(obj)
    script=out/'runtime.ld';script.write_text('SECTIONS { . = 0x800000; .text : { *(.text*) *(.rodata*) } . = ALIGN(16); .data : { *(.data*) *(.got*) } .bss : { *(.bss*) } /DISCARD/ : { *(.ARM.exidx*) *(.comment*) *(.note*) } }\n')
    target=out/'rx_runtime.elf'
    subprocess.run(['ld.lld','-T',str(script),'--no-undefined','--entry=wl_rx_runtime_receive',*(str(x)for x in objs),'-o',str(target)],check=True,capture_output=True)
    return target.read_bytes()


class RuntimeMachine(ReorderMachine):
    def __init__(self,image,blob,prepared=True,**kw):
        self.runtime_symbols={};self.os_due={};self.captured=[];self.runtime_locks=0;self.lock_fail=not prepared
        super().__init__(image,**kw)
        self.cpu.mem_map(0x7f0000,0x40000);self.cpu.mem_map(0x1af000,0x1000)
        elf=ELFFile(io.BytesIO(blob))
        self.runtime_symbols={s.name:int(s['st_value'])for s in elf.get_section_by_name('.symtab').iter_symbols()if s.name and s['st_shndx']!='SHN_UNDEF' and s['st_info']['type']=='STT_FUNC'}
        for s in elf.get_section_by_name('.symtab').iter_symbols():
            if s.name and s['st_shndx']!='SHN_UNDEF':self.runtime_symbols.setdefault(s.name,int(s['st_value']))
        for seg in elf.iter_segments():
            if seg['p_type']=='PT_LOAD':self.cpu.mem_write(int(seg['p_vaddr']),seg.data())
        text=elf.get_section_by_name('.text');self.allowed.append(range(int(text['sh_addr']),int(text['sh_addr'])+len(text.data())))
        target=self.runtime_symbols['wl_rx_runtime_tail_trampoline'];disp=(target-0x47748-8)//4
        self.cpu.mem_write(0x47748,(0xea000000|(disp&0xffffff)).to_bytes(4,'little'))
        self.initial_code=bytes(self.cpu.mem_read(0x8ee4,len(self.initial_code)))
        self.put(self.priv+1533*4,0xaabbccdd)
        event=bytearray(16);event[1:7]=bytes(self.cpu.mem_read(self.peer,6));self.cpu.mem_write(0x234000,bytes(event))
        self.call('wl_rx_runtime_create',self.priv,0,0x234000)

    def address(self,name):return self.runtime_symbols.get(name) or super().address(name)
    def tick(self):return round(self.now_ms*1000)//TICK_US
    def _hook(self,uc,address,size,user):
        a=[uc.reg_read(r)for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
        if address==self.runtime_symbols.get('wl_lock'):
            if self.lock_fail:self._return(1)
            else:self.runtime_locks+=1;self._return(0)
        elif address==self.runtime_symbols.get('wl_unlock'):
            self.runtime_locks-=1;assert self.runtime_locks>=0;self._return()
        elif address==self.runtime_symbols.get('wl_native_ticks'):self._return(self.tick())
        elif address==self.runtime_symbols.get('wl_native_base'):self._return(0)
        elif address==self.runtime_symbols.get('wl_orig_create'):self._return(0)
        elif address==0x1af650:
            self.cpu.mem_write(a[0],bytes(24));self._return()
        elif address==0x1af6b8:
            assert 0<a[1]<=6
            self.put(a[0]+8,a[2]);self.put(a[0]+12,a[3]);self.put(a[0]+16,self.tick()+a[1]);self.put(a[0]+20,2)
            self.os_due[a[0]]=(self.tick()+a[1],a[2],a[3]);self.events.append({'event':'runtime_arm','at_ms':self.now_ms});self._return()
        elif address==0x1af95c:
            if self.get(a[0]+20)&2:self.os_due.pop(a[0],None)
            self.put(a[0]+20,self.get(a[0]+20)&~6);self._return()
        else:super()._hook(uc,address,size,user)

    def receive(self,sequence):
        p=self.packet(sequence)
        result=self.call('wl_rx_runtime_receive',self.priv,sequence,0,self.peer,0,p)
        if result:
            assert self.ownership[p]['state']=='pending'
            self.ownership[p]['state']='rejected';self.rejected.append(sequence)
        assert self.runtime_locks==0
        return result
    def advance(self,until_ms,*,capture=False):
        while self.os_due:
            co,(tick,fn,arg)=min(self.os_due.items(),key=lambda kv:kv[1][0])
            due=tick*TICK_US/1000
            if due>until_ms:break
            self.now_ms=due;del self.os_due[co];self.put(co+20,12)
            if capture:self.captured.append((fn,arg));self.now_ms=until_ms;return
            self.call_callback(fn,arg)
        super().advance(until_ms)
    def call_callback(self,fn,arg):
        name=next(n for n,a in self.runtime_symbols.items()if a==fn)
        self.call(name,arg);assert self.runtime_locks==0
    def resume_captured(self):
        pending,self.captured=self.captured,[]
        for fn,arg in pending:self.call_callback(fn,arg)
    def delete(self):
        self.call('wl_rx_runtime_delete',self.priv,self.table);self.alive=False
        assert not self.os_due and not self.timer_alive and self.runtime_locks==0
    def getstats(self):
        self.call('wl_rx_runtime_stats',0x235000)
        names=['enroll','fallback','pool_exhausted','stale_callback','callbacks','arm','cancel_queued','cancel_captured','forced_flush','invalid']
        return dict(zip(names,[self.get(0x235000+i*4)for i in range(len(names))]))



def callout_contract():
    data=Path('build/wifi-latency-re-20261022/io-pkt-v6-hc').read_bytes()
    rows=[]
    for captured in (False,True):
        c=CalloutMachine(data,hz=119);c.schedule(1,50)
        if captured:assert c.advance(6,pause=True)=='captured'
        before=c.get(c.entry+20);c.cancel();after=c.get(c.entry+20)
        assert bool(after&8)==captured
        if captured:c.resume();assert len(c.calls)==1
        else:c.advance(10);assert not c.calls
        rows.append(dict(captured=captured,flags_before=before,flags_after_stop=after))
    c=CalloutMachine(data,hz=119)
    c.callback=lambda _arg:c.cpu.mem_protect(c.entry,0x1000,UC_PROT_NONE)
    c.schedule(1,50);c.advance(6)
    assert len(c.calls)==1
    return dict(status='pass',cancel_cases=rows,
                callback_epilogue_after_callout_memory_revoked='pass')


def main():
    out=Path('build/wifi-rx-timer-churn-20261022/native-runtime');blob=compile_runtime(out)
    image=DriverImage(Path('build/wifi-latency-re-20261022/devnp-mrvl_wlan-sdiorm.so').read_bytes())
    fixes=dict(candidate=compile_candidate(out)[0],window_candidate=compile_window_candidate(out)[0],harden_candidate=compile_harden_candidate(out)[0])
    def machine():return RuntimeMachine(image,blob,start=4090,window=32,**fixes)
    rows=[]
    contract=callout_contract()
    m=machine()
    for i in range(192):m.advance(i);m.receive((4090+i)%4096);m.snapshot()
    assert len(m.delivered)==192 and not m.os_due and not any(e['event']=='arm'for e in m.events)
    rows.append(dict(case='clean_192',stats=m.getstats()));m.delete();assert m.call('wl_rx_runtime_quiescent')==1
    m=machine();m.receive(4090);m.advance(40);m.receive(4092);m.advance(60)
    assert m.delivered[-1]['at_ms']==50.418
    rows.append(dict(case='inherited_target',stats=m.getstats()));m.delete()
    m=machine();m.receive(4092);m.now_ms=60;m.receive(4093)
    assert len(m.delivered)==2 and not m.os_due
    rows.append(dict(case='overdue_rx_flush',stats=m.getstats()));m.delete()
    m=machine();m.receive(4092);m.advance(51,capture=True);m.delete()
    assert m.call('wl_rx_runtime_quiescent')==0
    m.resume_captured();assert m.call('wl_rx_runtime_quiescent')==1
    rows.append(dict(case='captured_then_native_delete_memory_revoked',stats=m.getstats()))
    m=machine();m.receive(4091);m.advance(51,capture=True);m.receive(4090)
    assert not m.os_due and len(m.delivered)==2
    m.receive(4093);assert m.os_due
    before=list(m.os_due.items());m.resume_captured();assert list(m.os_due.items())==before
    m.advance(120);assert len(m.delivered)==3
    rows.append(dict(case='captured_cancel_then_rearm_same_table',stats=m.getstats()));m.delete()
    m=machine()
    original={p:bytes(m.cpu.mem_read(p,n)) for p,n in
              [(m.table,0x1000),(m.timer,0x1000),(m.slots,0x1000),(m.priv,0x2000)]}
    m.receive(4091);m.advance(51,capture=True);m.delete()
    for p,data in original.items():
        m.cpu.mem_protect(p,len(data),UC_PROT_ALL);m.cpu.mem_write(p,data)
    m.alive=m.timer_alive=True
    m.call('wl_rx_runtime_create',m.priv,0,0x234000)
    m.receive(4091);before=list(m.os_due.items());assert before
    m.resume_captured();assert list(m.os_due.items())==before
    m.advance(120);assert len(m.delivered)==2
    rows.append(dict(case='captured_delete_allocate_same_native_table_address',stats=m.getstats()))
    m.delete();assert m.call('wl_rx_runtime_quiescent')==1
    m=RuntimeMachine(image,blob,prepared=False,start=4090,window=32,**fixes)
    m.receive(4092);assert m.due_ms==50 and not m.os_due
    m.now_ms=50;m.due_ms=None;m.call('wl_rx_runtime_flush',m.table+32)
    assert len(m.delivered)==1
    m.delete()
    rows.append(dict(case='preparation_failed_uniform_stock_fallback',status='pass'))
    m=machine()
    for i in range(129):
        start=m.get(m.table+20);m.receive((start+1)%4096)
        if i==128:break
        next_due=min(v[0]for v in m.os_due.values())*TICK_US/1000
        m.advance(next_due,capture=True);m.receive(start)
    st=m.getstats();assert st['pool_exhausted']==1 and st['fallback']==1
    assert m.due_ms is not None and not m.os_due
    m.resume_captured();assert m.getstats()['stale_callback']==128
    m.due_ms=None;m.call('wl_rx_runtime_flush',m.table+32)
    assert not m.snapshot()['pending']
    rows.append(dict(case='128_retained_cookie_exhaustion_stock_fallback',stats=m.getstats()))
    m.delete();assert m.call('wl_rx_runtime_quiescent')==1
    target=out/'rx_runtime_results.json';target.write_text(json.dumps({'status':'pass; native ARM runtime and driver; OS boundaries modeled','native_callout_contract':contract,'cases':rows},indent=2)+'\n');print(target)

if __name__=='__main__':main()
