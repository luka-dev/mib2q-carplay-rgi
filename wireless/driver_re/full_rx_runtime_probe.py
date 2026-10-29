"""Full appended ELF RX execution with real DT_REL, PHDRs and root prepare.

Native io-pkt callout instructions execute. OS mutex, dynamic loader, allocation
and packet delivery boundaries are explicit models; physical RF is absent.
"""
from pathlib import Path
import os
import hashlib, io, json, struct
from elftools.elf.elffile import ELFFile
from unicorn import UC_PROT_ALL
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_PC,UC_ARM_REG_SP,UC_ARM_REG_LR
from .reorder_machine import ReorderMachine, DriverImage, SAVED_REGISTERS
from .callout_machine import EXECUTED as IO_EXECUTED, CAPTURED_CALL
from .probe_latency_candidates import TICK_US


# ponytail: env override so a rebuild never overwrites the deployed artifact's folder
FOLDER=Path(os.environ.get('WIFI_LATENCY_DIR','build/wifi-latency-full-20261022'))

class FullRuntimeMachine(ReorderMachine):
    adapter, handle, timer=0x12000000,0x12009000,0x12008000
    priv, table, slots, peer=0x12010000,0x12020000,0x12030000,0x12031000
    stub_lock,stub_unlock,stub_free=0x13000000,0x13000004,0x13000008
    stop,stack=0x13000100,0x14000000
    io_lock,io_unlock,dladdr=0x13000200,0x13000204,0x13000208
    tls_pointer,tls,ops,threads,thread,context=0x16000000,0x16001000,0x16002000,0x16003000,0x16004000,0x16005000

    def __init__(self,image_path=None,io_pkt_path=None,payload_elf_path=None,*,base=0,start=4090,window=32,pin=True):
        folder=FOLDER
        image_path=Path(image_path or folder/'devnp-mrvl_wlan-sdiorm.so')
        io_pkt_path=Path(io_pkt_path or 'build/wifi-latency-re-20261022/io-pkt-v6-hc')
        payload_elf_path=Path(payload_elf_path or folder/'latency_payload.elf')
        self.base=base;self.full_symbols={};self.io_symbols={};self.pin=pin
        self.native_mutex_depth=0;self.io_lock_depth=0;self.external_hooks={}
        self.pause_softclock=False;self.captured_context=None;self.prepared=False
        self.driver_data=image_path.read_bytes();self.driver_sha256=hashlib.sha256(self.driver_data).hexdigest()
        stock=DriverImage(Path('build/wifi-latency-re-20261022/devnp-mrvl_wlan-sdiorm.so').read_bytes())
        super().__init__(stock,start=start,window=window)
        self._map(self.stack+0x10000,0x10000)
        # Full ET_DYN program headers, including the appended text and RW state.
        full=ELFFile(io.BytesIO(self.driver_data));assert full['e_type']=='ET_DYN'
        for seg in full.iter_segments():
            if seg['p_type']=='PT_LOAD':
                self._map(base+int(seg['p_vaddr']),int(seg['p_memsz']))
                self.cpu.mem_write(base+int(seg['p_vaddr']),seg.data())
                tail=int(seg['p_memsz'])-len(seg.data())
                if tail:self.cpu.mem_write(base+int(seg['p_vaddr'])+len(seg.data()),bytes(tail))
        # io-pkt ET_EXEC remains fixed at its original addresses.
        stack_data=io_pkt_path.read_bytes();stackelf=ELFFile(io.BytesIO(stack_data))
        self.io_symbols={s.name:int(s['st_value'])for s in stackelf.get_section_by_name('.symtab').iter_symbols()if s.name}
        for seg in stackelf.iter_segments():
            if seg['p_type']=='PT_LOAD':
                self._map(int(seg['p_vaddr']),int(seg['p_memsz']));self.cpu.mem_write(int(seg['p_vaddr']),seg.data())
        self._map(self.tls_pointer,0x10000)
        payload=ELFFile(io.BytesIO(payload_elf_path.read_bytes()))
        self.full_symbols={s.name:int(s['st_value'])for s in payload.get_section_by_name('.symtab').iter_symbols()
                           if s.name and s['st_shndx']!='SHN_UNDEF' and s['st_info']['type']=='STT_FUNC'}
        for s in payload.get_section_by_name('.symtab').iter_symbols():
            if s.name and s['st_shndx']!='SHN_UNDEF':self.full_symbols.setdefault(s.name,int(s['st_value']))
        # Parse the active PT_DYNAMIC table, not stale SHT_REL metadata.
        dynamic=next(s for s in full.iter_segments()if s['p_type']=='PT_DYNAMIC')
        tags={t.entry.d_tag:int(t.entry.d_val)for t in dynamic.iter_tags()}
        assert tags['DT_RELENT']==8
        symtab=full.get_section_by_name('.dynsym')
        self.relocation_rows=[]
        tables=[(tags['DT_REL'],tags['DT_RELSZ'])]
        if tags.get('DT_PLTRELSZ'):tables.append((tags['DT_JMPREL'],tags['DT_PLTRELSZ']))
        entries=[struct.unpack('<II',self.cpu.mem_read(base+va+off,8))for va,n in tables for off in range(0,n,8)]
        for addr,info in entries:
            kind=info&255;index=info>>8
            sym=symtab.get_symbol(index)
            addend=self.get(base+addr)
            if kind==23:value=base+addend
            else:
                if sym['st_shndx']=='SHN_UNDEF':
                    if sym.name in ('callout_init','callout_msec','callout_stop'):value=self.io_symbols[sym.name]
                    else:value=base+stock.symbols[sym.name][0]if sym.name in stock.symbols else 0
                else:value=base+int(sym['st_value'])
                if kind==2:value+=addend
                elif kind not in(21,22):raise AssertionError(('unsupported relocation',kind))
            self.put(base+addr,value)
            self.relocation_rows.append((addr,kind,value&0xffffffff))
        self.allowed=[range(base+r.start,base+r.stop)for r in self.allowed]
        for seg in full.iter_segments():
            if seg['p_type']=='PT_LOAD'and seg['p_flags']&1:
                self.allowed.append(range(base+int(seg['p_vaddr']),base+int(seg['p_vaddr'])+int(seg['p_memsz'])))
        self.allowed += [range(self.io_symbols[n],self.io_symbols[n]+next(int(s['st_size'])for s in stackelf.get_section_by_name('.symtab').iter_symbols()if s.name==n))for n in IO_EXECUTED]
        self.initial_code=bytes(self.cpu.mem_read(0x8ee4,len(self.initial_code)))
        self.full_text=[(base+int(s['p_vaddr']),bytes(self.cpu.mem_read(base+int(s['p_vaddr']),int(s['p_filesz']))))for s in full.iter_segments()if s['p_type']=='PT_LOAD'and s['p_flags']&1]
        # Root prepare hashes the original stack text; these data pointers are
        # the same OS/TLS boundary values used by the native callout harness.
        self.put(0x23b26c,self.tls_pointer);self.put(self.tls_pointer,self.tls);self.put(self.tls+24,2)
        self.put(0x24a144,self.threads);self.put(self.threads,self.thread)
        self.put(0x24a088,self.ops);self.put(self.ops+12,self.io_lock);self.put(self.ops+16,self.io_unlock)
        self.put(self.io_symbols['hz'],119);self.put(self.io_symbols['hardclock_ticks'],0)
        self.put(self.priv+1533*4,0xaabbccdd)
        self.cpu.mem_write(self.priv+6096,struct.pack('<H',window))
        self.cpu.mem_write(self.adapter+0x13ac,bytes(2))
        self.call('io:callout_startup');self.call('io:callout_init',self.timer+20)
        self.cpu.mem_write(0x12034000,b'/fixture/devnp-mrvl_wlan-sdiorm.so\0')
        self.cpu.mem_write(0x12032000,bytes(64))
        self.prepared=self.call('wl_runtime_prepare')==0
        assert self.prepared==pin
        self.enroll_existing()

    def _map(self,address,length):
        for p in range(address&~4095,(address+length+4095)&~4095,4096):
            if not any(a<=p<=b for a,b,_ in self.cpu.mem_regions()):self.cpu.mem_map(p,4096)
    def address(self,name):
        if name.startswith('io:'):return self.io_symbols[name[3:]]
        if name in self.full_symbols:return self.base+self.full_symbols[name]
        return self.base+super().address(name)
    def packet(self,seq):
        p=0x15000000+256*len(self.ownership);self.ownership[p]={'sequence':seq,'state':'pending'};return p
    def enroll_existing(self):
        data=bytearray(14);data[1:7]=bytes(self.cpu.mem_read(self.peer,6));data[7]=9
        data[8:10]=struct.pack('<H',2|self.window<<6);data[12:14]=struct.pack('<H',self.get(self.table+20))
        self.cpu.mem_write(0x12033000,bytes(data))
        assert self.call('wlan_cmd_11n_addba_rspgen',self.priv,0x12032000,0x12033000)==0
    def _hook(self,uc,address,size,user):
        a=[uc.reg_read(r)for r in(UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
        if address in self.external_hooks:self.external_hooks[address](self,a);return
        if address==0x118400 and getattr(self,'mutex_busy',False):self._return(16) # EBUSY: another thread owns wl_lock
        elif address in(0x11867c,0x118400):
            self.native_mutex_depth+=1;self.put(a[0],self.get(a[0])+1);self.put(a[0]+4,7);self._return(0)
        elif address==0x118c1c:
            assert self.native_mutex_depth>0;self.native_mutex_depth-=1;count=self.get(a[0])-1;self.put(a[0],count)
            if not count:self.put(a[0]+4,0xffffffff)
            self._return(0)
        elif address==0x118628:
            assert a[0]==0xcafe0001;assert bytes(self.cpu.mem_read(a[1],7))==b'dladdr\0';self._return(self.dladdr)
        elif address==self.dladdr:
            assert a[0]==self.address('wl_native_base')
            for i,v in enumerate((0x12034000,self.base,0,0)):self.put(a[1]+4*i,v)
            self._return(1)
        elif address==0x118d54:
            if a[0]==0:
                assert a[1]==0x800;self._return(0xcafe0001)
            else:
                assert a[0]==0x12034000 and a[1]==0x1006;self._return(0xcafe0000 if self.pin else 0)
        elif address==self.io_symbols.get('pthread_mutex_init'):self._return(0)
        elif address==self.io_lock:
            assert not self.io_lock_depth;self.io_lock_depth=1;self._return()
        elif address==self.io_unlock:
            assert self.io_lock_depth==1;self.io_lock_depth=0;self._return()
        elif address==CAPTURED_CALL and self.pause_softclock:
            assert not self.io_lock_depth and not self.native_mutex_depth
            self.pause_softclock=False;uc.emu_stop()
        elif any(self.address(n)<=address<self.address(n)+12 for n in ('callout_init','callout_msec','callout_stop')):
            pass # Real driver PLT executes with DT_JMPREL-bound native target.
        else:super()._hook(uc,address,size,user)
    def call(self,name,*args):
        result=super().call(name,*args)
        assert self.native_mutex_depth==self.io_lock_depth==0
        assert all(bytes(self.cpu.mem_read(a,len(b)))==b for a,b in self.full_text)if hasattr(self,'full_text')else True
        return result
    def _softclock(self,capture=False):
        if not capture:return self.call('io:softclock',self.context)
        sp=self.stack+0x1fff0
        for i,r in enumerate((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)):self.cpu.reg_write(r,self.context if i==0 else 0)
        saved={r:0xc1000000+i*256 for i,r in enumerate(SAVED_REGISTERS)}
        for r,v in saved.items():self.cpu.reg_write(r,v)
        self.cpu.reg_write(UC_ARM_REG_SP,sp);self.cpu.reg_write(UC_ARM_REG_LR,self.stop);self.pause_softclock=True
        self.cpu.emu_start(self.address('io:softclock'),self.stop,count=100000)
        if self.cpu.reg_read(UC_ARM_REG_PC)==CAPTURED_CALL:
            self.captured_context=(self.cpu.context_save(),sp,saved);return 'captured'
        assert self.cpu.reg_read(UC_ARM_REG_PC)==self.stop
    def advance(self,until_ms,*,capture=False):
        assert until_ms>=self.now_ms
        target=round(until_ms*1000)//TICK_US
        while self.get(self.io_symbols['hardclock_ticks'])<target:
            tick=self.get(self.io_symbols['hardclock_ticks'])+1;self.put(self.io_symbols['hardclock_ticks'],tick);self.now_ms=tick*TICK_US/1000
            self.call('io:callout_hardclock')
            if self._softclock(capture)=='captured':self.now_ms=until_ms;return 'captured'
        self.now_ms=until_ms
    def resume_captured(self):
        context,sp,saved=self.captured_context;self.captured_context=None;self.cpu.context_restore(context)
        self.cpu.emu_start(CAPTURED_CALL,self.stop,count=100000)
        assert self.cpu.reg_read(UC_ARM_REG_PC)==self.stop and self.cpu.reg_read(UC_ARM_REG_SP)==sp
        assert all(self.cpu.reg_read(r)==v for r,v in saved.items())
        assert self.native_mutex_depth==self.io_lock_depth==self.lock_depth==0
    def delete(self):
        super().delete()
    def getstats(self):
        self.call('wl_rx_runtime_stats',0x12035000)
        names=['enroll','fallback','pool_exhausted','stale_callback','callbacks','arm','cancel_queued','cancel_captured','forced_flush','invalid']
        return dict(zip(names,[self.get(0x12035000+i*4)for i in range(len(names))]))


def main():
    folder=FOLDER;rows=[]
    initial_sha=hashlib.sha256((folder/'devnp-mrvl_wlan-sdiorm.so').read_bytes()).hexdigest()
    for base in(0,0x2000000,0x4100000):
        m=FullRuntimeMachine(base=base)
        assert m.call('wl_native_base')==base
        assert m.getstats()['enroll']==1
        for i in range(64):m.advance(i);m.receive((4090+i)%4096);m.snapshot()
        assert len(m.delivered)==64 and m.getstats()['arm']==0
        m.delete();assert m.call('wl_rx_runtime_quiescent')==1
        rows.append(dict(base=hex(base),case='clean64_real_hooks',relocations=len(m.relocation_rows),stats=m.getstats()))
        m=FullRuntimeMachine(base=base);m.receive(4090);m.advance(40);m.receive(4092);m.advance(60)
        assert m.delivered[-1]['at_ms']==50.418
        m.delete();rows.append(dict(base=hex(base),case='inherited_deadline',stats=m.getstats()))
        m=FullRuntimeMachine(base=base);m.receive(4091);assert m.advance(51,capture=True)=='captured';m.delete()
        assert m.call('wl_rx_runtime_quiescent')==0
        m.resume_captured();assert m.call('wl_rx_runtime_quiescent')==1
        rows.append(dict(base=hex(base),case='captured_delete_real_softclock',stats=m.getstats()))
    for base in(0x2000000,):
        m=FullRuntimeMachine(base=base,pin=False)
        m.receive(4092);m.advance(60)
        assert len(m.delivered)==1 and m.delivered[0]['at_ms']==50.418
        m.delete();rows.append(dict(base=hex(base),case='prepare_selfpin_failure_stock_delivery'))
        m=FullRuntimeMachine(base=base);m.receive(4092)
        for tick in range(1,8):
            m.put(m.io_symbols['hardclock_ticks'],tick);m.now_ms=tick*TICK_US/1000
            m.call('io:callout_hardclock')
        m.receive(4093)
        assert len(m.delivered)==2 and not m.snapshot()['pending']
        m._softclock();assert len(m.delivered)==2
        m.delete();rows.append(dict(base=hex(base),case='overdue_rx_real_hardclock_softclock_deferred',stats=m.getstats()))
        m=FullRuntimeMachine(base=base)
        original={p:bytes(m.cpu.mem_read(p,n))for p,n in[(m.table,0x1000),(m.timer,0x1000),(m.slots,0x1000),(m.priv,0x2000)]}
        m.receive(4091);m.advance(51,capture=True);m.delete()
        for p,data in original.items():m.cpu.mem_protect(p,len(data),UC_PROT_ALL);m.cpu.mem_write(p,data)
        m.alive=m.timer_alive=True;m.enroll_existing();m.receive(4091)
        m.resume_captured();assert m.getstats()['stale_callback']==1
        m.advance(120);assert len(m.delivered)==2
        m.delete();rows.append(dict(base=hex(base),case='captured_native_table_address_reuse',stats=m.getstats()))
    assert m.driver_sha256==initial_sha==hashlib.sha256((folder/'devnp-mrvl_wlan-sdiorm.so').read_bytes()).hexdigest()
    out=folder/'full_rx_runtime_results.json';out.write_text(json.dumps({'status':'pass','binary_sha256':m.driver_sha256,'cases':rows},indent=2)+'\n');print(out)

if __name__=='__main__':main()
