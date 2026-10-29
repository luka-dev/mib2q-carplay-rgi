"""Execute the complete driver TX runtime and pinned io-pkt callback queue.

Only firmware attach/detach, allocation, OS pulses and final TX work are modeled.
The actual payload C, ARM conditional gate and native callback queue execute.
"""
from pathlib import Path
import json
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_PC,UC_ARM_REG_LR,UC_ARM_REG_SP,UC_ARM_REG_CPSR
from .full_rx_runtime_probe import FullRuntimeMachine,FOLDER
from .reorder_machine import ReorderMachine

class TXMachine(FullRuntimeMachine):
    head,tail=0x24a42c,0x24a430
    softc,process,pulse,errno=0x17000000,0x17010000,0x17011000,0x17012000
    def __init__(self,**kw):
        super().__init__(**kw)
        self.head=type(self).head
        self._map(self.softc,0x40000);self.targets=[];self.nodes=set();self.nextnode=self.softc+0x20000
        self.fail_malloc=False;self.fail_pulse=False;self.capture_next=False;self.saved_dispatch=None
        self.external_hooks[self.address('wl_orig_attach')]=lambda m,a:m._native_lifecycle(a,True)
        self.external_hooks[self.address('wl_orig_detach')]=lambda m,a:m._native_lifecycle(a,False)
        self.external_hooks[self.io_symbols['malloc']]=lambda m,a:m._malloc(a)
        self.external_hooks[self.io_symbols['free']]=lambda m,a:m._free(a)
        self.external_hooks[self.io_symbols['__get_errno_ptr']]=lambda m,a:m._return(m.errno)
        self.external_hooks[self.io_symbols['MsgSendPulse']]=lambda m,a:m._return(0xffffffff if m.fail_pulse else 0)
        self.external_hooks[self.base+0x197a0]=lambda m,a:m._target(a)
        self.external_hooks[0x1d5ca0]=lambda m,a:m._capture()
        for lo,hi in ((0x1d56a8,0x1d5a80),(0x1d5a80,0x1d5d0c)):
            self.allowed.append(range(lo,hi))
        self.put(self.head,0);self.put(self.tail,self.head)
        self.put(0x24a0dc,self.process);self.put(self.process+116,0);self.put(self.process+168,self.pulse)
        self.cpu.mem_write(self.pulse+4,b'\x06');self.put(self.pulse+8,0)
        self.attach()
    def _native_lifecycle(self,a,attach):
        assert not self.native_mutex_depth # no lock across native worker waits
        self._return(0)
    def _malloc(self,a):
        assert a[0]==20
        if self.fail_malloc:self.fail_malloc=False;self._return(0);return
        n=self.nextnode;self.nextnode+=32;self.nodes.add(n);self._return(n)
    def _free(self,a):
        assert a[0] in self.nodes;self.nodes.remove(a[0]);self._return()
    def _target(self,a):
        assert self.native_mutex_depth==1
        assert a[0]==self.softc+128
        self.targets.append(a[0]);self._return()
    def _capture(self):
        if self.capture_next:self.capture_next=False;self.cpu.emu_stop()
    def attach(self):assert self.call('wl_tx_runtime_attach',0,self.softc,0)==0
    def detach(self):assert self.call('wl_tx_runtime_detach',self.softc)==0
    def request(self):
        self.full_symbols['tx_enqueue_site']=0xf1a8
        self.external_hooks[self.base+0xf1ac]=lambda m,a:m.cpu.reg_write(UC_ARM_REG_PC,m.stop)
        try:return self.call('tx_enqueue_site',self.base+0x197a0,self.softc+128,self.softc)
        finally:del self.external_hooks[self.base+0xf1ac]
    def dispatch(self,capture=False):
        # FullRuntimeMachine.call provides callee-save and SP assertions.
        if not capture:return self.call('io:tx_dispatch')
        self.capture_next=True
        self.cpu.reg_write(UC_ARM_REG_SP,self.stack+0x1fff0);self.cpu.reg_write(UC_ARM_REG_LR,self.stop)
        self.cpu.emu_start(0x1d5a80,self.stop,count=100000)
        assert self.cpu.reg_read(UC_ARM_REG_PC)==0x1d5ca0 and not self.native_mutex_depth
        self.saved_dispatch=self.cpu.context_save()
    def resume(self):
        self.cpu.context_restore(self.saved_dispatch);self.saved_dispatch=None
        self.cpu.emu_start(0x1d5ca0,self.stop,count=100000)
        assert self.cpu.reg_read(UC_ARM_REG_PC)==self.stop and not self.native_mutex_depth
    def gate(self,managed=True):
        # Execute the *patched conditional BNE at the original gate*, rather
        # than calling its C helper. Stop at the two original continuations.
        hit=[]
        for off in (0xf18c,0xf1ac):
            self.external_hooks[self.base+off]=lambda m,a,off=off:(hit.append(off),m.cpu.emu_stop())
        self.cpu.reg_write(UC_ARM_REG_SP,self.stack+0xfff0)
        self.cpu.reg_write(UC_ARM_REG_R4,self.handle);self.cpu.reg_write(UC_ARM_REG_R5,self.softc+128 if managed else self.softc+256)
        self.put(self.priv+24,self.softc);self.cpu.reg_write(UC_ARM_REG_R6,self.priv)
        self.cpu.reg_write(UC_ARM_REG_CPSR,self.cpu.reg_read(UC_ARM_REG_CPSR)&~(1<<30)) # NE
        self.cpu.emu_start(self.base+0xf188,self.stop,count=100000)
        for off in (0xf18c,0xf1ac):del self.external_hooks[self.base+off]
        assert not self.native_mutex_depth and len(hit)==1
        return hit[0]

    def completion_fixture(self, occupied, stopped=True):
        self.put(self.priv+200,self.softc+128)
        self.put(self.priv+192,self.base+0x197a0)
        self.put(self.priv+196,self.softc+128)
        self.put(self.priv+24,self.softc)
        self.put(self.handle+344,occupied)
        self.put(self.handle+152,occupied.bit_count())
        self.put(self.softc+128+536,0x440 if stopped else 0x40)
        self.external_hooks[self.address('moal_print')]=lambda m,a:m._return()
        self.external_hooks[self.address('woal_bss_index_to_priv')]=lambda m,a:m._return(m.priv)
        self.external_hooks[self.address('atomic_sub')]=lambda m,a:(m.put(a[0],m.get(a[0])-a[1]),m._return())
        self.external_hooks[self.address('atomic_clr')]=lambda m,a:(m.put(a[0],m.get(a[0])&~a[1]),m._return())

    def complete(self, slot):
        descriptor=self.softc+0x30000
        self.put(descriptor+28,2)
        self.cpu.mem_write(descriptor+32,b'\0')
        self.cpu.mem_write(descriptor+56,bytes([slot]))
        self.cpu.mem_write(descriptor+26,(64).to_bytes(2,'little'))
        self.call('moal_send_packet_complete',self.handle,descriptor,0)
        assert self.get(self.handle+152)==self.get(self.handle+344).bit_count()

def main():
    rows=[]
    for base in (0,0x2000000,0x4100000):
        m=TXMachine(base=base);m.io_symbols['tx_dispatch']=0x1d5a80
        assert m.gate()==0xf18c and m.gate(False)==0xf1ac
        for _ in range(100):assert m.request()==0
        assert len(m.nodes)==1 and m.get(m.get(m.head)+8)==m.softc
        m.dispatch();assert len(m.targets)==1 and not m.nodes and not m.get(m.head)
        m.request();m.dispatch();assert len(m.targets)==2 and not m.nodes
        rows.append(dict(base=hex(base),case='first_free_gate_and_100_requests_one_native_callback'))
        m=TXMachine(base=base);m.io_symbols['tx_dispatch']=0x1d5a80
        m.request();m.dispatch(capture=True);m.detach();m.call('io:stk_context_callback_2_clean',m.softc)
        assert m.nodes;m.resume();assert not m.targets and not m.nodes
        m.attach();m.request();m.dispatch();assert len(m.targets)==1
        rows.append(dict(base=hex(base),case='captured_detach_rebind_same_owner_old_cookie_discarded'))
        m=TXMachine(base=base);m.io_symbols['tx_dispatch']=0x1d5a80
        m.request();m.detach();m.call('io:stk_context_callback_2_clean',m.softc)
        assert not m.nodes and not m.targets;m.attach();m.request();m.dispatch();assert len(m.targets)==1
        rows.append(dict(base=hex(base),case='owner_cleanup_and_new_attach_generation'))
        for fail in ('fail_malloc','fail_pulse'):
            m=TXMachine(base=base);m.io_symbols['tx_dispatch']=0x1d5a80;setattr(m,fail,True);m.request();assert not m.nodes
            setattr(m,fail,False);m.request();assert len(m.nodes)==1 # failure resets QUEUED; next request wakes again
            m.dispatch();assert len(m.targets)==1
            rows.append(dict(base=hex(base),case=fail+'_then_next_request_wakes'))
        # ABBA guard: completion runs under native ra_list lock; if another thread
        # owns wl_lock, gate and request must not wait but take the stock path.
        m=TXMachine(base=base);m.mutex_busy=True
        assert m.gate()==0xf1ac and m.request()==0 and len(m.nodes)==1
        assert m.get(m.get(m.head))==m.base+0x197a0 # original stk callback, no cookie
        m.mutex_busy=False;m.request();assert len(m.nodes)==2 # managed wake still usable
        rows.append(dict(base=hex(base),case='busy_wl_lock_gate_and_request_take_stock_path'))
    m=TXMachine();m.io_symbols['tx_dispatch']=0x1d5a80
    # Fill all permanent attach contexts without spending time in 128 identical
    # prepare hashes. This writes only the fixture pool's allocation status;
    # the actual attach/exhaustion/fallback implementation still executes.
    start=m.address('contexts');stride=32
    for i in range(1,128):m.put(start+i*stride,1)
    m.put(m.address('next_generation'),128)
    m.detach();m.attach();assert m.gate()==0xf1ac
    m.request();assert len(m.nodes)==1
    node=m.get(m.head);assert m.get(node)==m.base+0x197a0
    m.detach();m.call('io:stk_context_callback_2_clean',m.softc);assert not m.nodes
    rows.append(dict(base='0x0',case='exhausted_attach_pool_same_owner_uses_original_callback'))
    m=TXMachine();m.io_symbols['tx_dispatch']=0x1d5a80
    start=m.address('contexts');latest=start+127*32
    # Sparse fixture with declared high-water: newest matching generation at
    # the end must beat an older live context sharing the native owner.
    for off,value in ((0,1),(4,1),(8,128),(12,1),(16,m.softc),(20,m.softc+128),(24,0x40000000),(28,128)):
        m.put(latest+off,value)
    m.put(m.address('next_generation'),128)
    assert m.gate()==0xf1ac
    m.request();assert len(m.nodes)==1
    assert m.get(m.get(m.head))==m.base+0x197a0
    m.call('io:stk_context_callback_2_clean',m.softc);m.detach()
    assert m.get(start+4)==1 and m.get(latest+4)==0
    m.request();assert not m.nodes # retired newest masks older live owner
    rows.append(dict(base='0x0',case='sparse_declared_prefix_newest_owner_and_retirement'))
    m=TXMachine();m.io_symbols['tx_dispatch']=0x1d5a80
    m.detach();m.attach()
    assert m.get(m.address('next_generation'))==2
    assert m.get(m.address('contexts')+32+4)==1
    m.request();m.dispatch();assert len(m.targets)==1
    rows.append(dict(base='0x0',case='real_attach_highwater_rebind_uses_new_generation'))
    m=TXMachine();m.io_symbols['tx_dispatch']=0x1d5a80
    m.request();oldcookie=m.get(m.get(m.head)+4)
    target=m.base+0x197a0
    m.external_hooks[target]=lambda x,a:x.cpu.emu_stop()
    m.cpu.reg_write(UC_ARM_REG_SP,m.stack+0x1fff0);m.cpu.reg_write(UC_ARM_REG_LR,m.stop)
    m.cpu.emu_start(0x1d5a80,m.stop,count=100000)
    assert m.cpu.reg_read(UC_ARM_REG_PC)==target and m.native_mutex_depth==1
    saved=m.cpu.context_save()
    # A second emulated thread entry is unnecessary: this is a recursive owner
    # request while fn(arg) is still RUNNING. Use a disjoint stack and preserve
    # the paused callback registers; the real policy/request code executes.
    for _ in range(2):
        ReorderMachine.call(m,'wl_tx_runtime_request',target,m.softc+128,m.softc)
        assert m.native_mutex_depth==1 and len(m.nodes)==1
    m.cpu.context_restore(saved);m.external_hooks[target]=lambda x,a:x._target(a)
    m.cpu.emu_start(target,m.stop,count=100000)
    assert m.cpu.reg_read(UC_ARM_REG_PC)==m.stop and m.native_mutex_depth==0
    assert len(m.nodes)==1
    newcookie=m.get(m.get(m.head)+4)
    assert newcookie!=oldcookie and m.get(oldcookie)==0 and m.get(newcookie)==1
    m.dispatch();assert len(m.targets)==2 and not m.nodes
    m.request();m.dispatch();assert len(m.targets)==3
    rows.append(dict(base='0x0',case='recursive_followup_allocates_before_old_cookie_release'))
    m=TXMachine();m.io_symbols['tx_dispatch']=0x1d5a80
    pool=m.address('cookies')
    for free,hint in ((17,10),(3,256)):
        for i in range(256):m.put(pool+i*28,1)
        m.put(pool+free*28,0)
        if 'cookie_hint' in m.full_symbols:m.put(m.address('cookie_hint'),pool+hint*28)
        m.request();assert len(m.nodes)==1
        assert m.get(m.get(m.head)+4)==pool+free*28
        m.dispatch();assert not m.nodes and m.get(pool+free*28)==0
    rows.append(dict(base='0x0',case='occupied_and_one_past_cookie_hint_find_arbitrary_free_hole'))
    # Ambiguous failures intentionally quarantine immutable cookies. Once all
    # 256 are retained, requests must enqueue the native callback directly.
    for fail in ('fail_malloc','fail_pulse'):
        m=TXMachine();m.io_symbols['tx_dispatch']=0x1d5a80
        for _ in range(260):
            setattr(m,fail,True);m.request();assert not m.nodes
        setattr(m,fail,False)
        for _ in range(3):
            m.request();assert len(m.nodes)==1
            assert m.get(m.get(m.head))==m.base+0x197a0
            # Native callbacks run outside the managed wrapper mutex.
            m.external_hooks[m.base+0x197a0]=lambda x,a:(x.targets.append(a[0]),x._return())
            m.dispatch();assert not m.nodes
        assert len(m.targets)==3
        assert m.gate()==0xf1ac
        # Native bitmap/flags gate only requests at one or zero descriptors.
        m.completion_fixture(0xffffffff)
        counts=[]
        for slot in range(32):
            m.complete(slot);counts.append(len(m.nodes))
        assert counts==[0]*30+[1,2],counts
        m.dispatch();m.dispatch();assert len(m.targets)==5 and not m.nodes
        m.completion_fixture(0xffffffff,stopped=False)
        for slot in range(32):m.complete(slot)
        assert not m.nodes
        rows.append(dict(base='0x0',case=fail+'_cookie_exhaustion_preserves_native_wakes'))
    for base in (0,0x2000000,0x4100000):
        m=TXMachine(base=base);m.io_symbols['tx_dispatch']=0x1d5a80
        wake=m.address('contexts')+24
        m.put(wake,0x4ffffffe)
        m.request();assert len(m.nodes)==1
        assert m.gate()==0xf18c # queued max ticket is still managed
        m.dispatch();assert len(m.targets)==1
        assert m.gate()==0xf1ac
        m.completion_fixture(0xff)
        for slot in range(8):m.complete(slot)
        assert len(m.nodes)==2
        m.external_hooks[m.base+0x197a0]=lambda x,a:(x.targets.append(a[0]),x._return())
        m.dispatch();m.dispatch();assert len(m.targets)==3
        rows.append(dict(base=hex(base),case='last_serial_drains_then_stock_gate_without_wrap'))
    p=FOLDER/'full_tx_runtime_results.json'
    p.write_text(json.dumps(dict(status='pass',binary_sha256=m.driver_sha256,cases=rows),indent=2)+'\n');print(p)
if __name__=='__main__':main()
