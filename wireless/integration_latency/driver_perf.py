"""Native ARM instruction/event profiling; modeled OS endpoints, not HU cycles."""
from pathlib import Path
import argparse,hashlib,json,math
from elftools.elf.elffile import ELFFile
from unicorn import UC_HOOK_CODE
from wireless.driver_re.full_tx_runtime_probe import TXMachine
from wireless.driver_re.full_rx_runtime_probe import FullRuntimeMachine
from wireless.driver_re.reorder_machine import DriverImage,ReorderMachine,compile_candidate,compile_window_candidate,compile_harden_candidate
ROOT=Path(__file__).resolve().parents[2]
BASELINE=ROOT/'build/wifi-driver-finished-20261023'

class Counter:
    def __init__(self,m):
        self.n=0;self.calls={};self.on=False;self.m=m
        self.targets={0x118400:'trylock',0x11867c:'lock',0x118c1c:'unlock',0x1185d4:'malloc',0x118cc4:'free',0x118a48:'pulse'}
        self.targets.update({m.stub_lock:'modeled_reorder_lock',m.stub_unlock:'modeled_reorder_unlock'})
        m.cpu.hook_add(UC_HOOK_CODE,self.hook)
    def hook(self,cpu,address,size,user):
        if not self.on:return
        self.n+=1
        if address in self.targets:
            name=self.targets[address];self.calls[name]=self.calls.get(name,0)+1
    def measure(self,fn):
        self.n=0;self.calls={};self.on=True
        try:fn()
        finally:self.on=False
        return {'instructions':self.n,'native_calls':dict(self.calls)}

def summarize(rows):
    values=sorted(r['instructions']for r in rows)
    p=lambda q:values[max(0,min(len(values)-1,math.ceil(len(values)*q)-1))]
    return {'operations':len(rows),'instruction_total':sum(values),'instruction_p50':p(.5),'instruction_p95':p(.95),'instruction_p99':p(.99),
        'native_calls':{k:sum(r['native_calls'].get(k,0)for r in rows)for k in set().union(*(r['native_calls']for r in rows))},'samples':rows}

def machine(folder):
    m=TXMachine(image_path=folder/'devnp-mrvl_wlan-sdiorm.so',payload_elf_path=folder/'latency_payload.elf',base=0x2000000)
    m.io_symbols['tx_dispatch']=0x1d5a80
    return m

def tx_profile(folder):
    results={}
    for occupancy in (1,8,128):
        m=machine(folder);c=Counter(m)
        # Valid allocated prefix matches real permanent context allocation.
        contexts=m.address('contexts');m.put(m.address('next_generation'),occupancy)
        for i in range(1,occupancy):
            at=contexts+i*32;m.put(at,1);m.put(at+4,1);m.put(at+8,i+1)
            m.put(at+16,m.softc+0x4000+i*128);m.put(at+20,m.softc+0x4080+i*128)
        results[f'gate_oldest_context_of_{occupancy}']=summarize([c.measure(m.gate)for _ in range(16)])
        results[f'coalesced_request_contexts_{occupancy}']=None
        m.request()
        results[f'coalesced_request_contexts_{occupancy}']=summarize([c.measure(m.request)for _ in range(16)])
    for mode in ('full','stock_tx_ablation'):
        m=machine(folder);c=Counter(m)
        if mode=='stock_tx_ablation':
            stock=(ROOT/'build/wifi-driver-re-20261018/devnp-mrvl_wlan-sdiorm.so').read_bytes()
            for off in (0xf188,0xf1a8):m.cpu.mem_write(m.base+off,stock[off:off+4])
            # Full fixture normally reaches fixed native enqueue through the
            # payload. Restoring the stock BL also requires its real GOT bind.
            elf=ELFFile((folder/'devnp-mrvl_wlan-sdiorm.so').open('rb'))
            rel=elf.get_section_by_name('.rel.plt');dyn=elf.get_section(rel['sh_link'])
            for r in rel.iter_relocations():
                if dyn.get_symbol(r['r_info_sym']).name=='stk_context_callback_2':m.put(m.base+r['r_offset'],m.io_symbols['stk_context_callback_2'])
            m.full_text=[(a,bytes(m.cpu.mem_read(a,len(b))))for a,b in m.full_text]
            m.external_hooks[m.base+0x197a0]=lambda x,a:(x.targets.append(a[0]),x._return())
        completed=[];dispatched=[];queued=[]
        for _ in range(4):
            m.completion_fixture(0xffffffff)
            for slot in range(32):completed.append(c.measure(lambda slot=slot:m.complete(slot)))
            queued.append(len(m.nodes))
            while m.nodes:dispatched.append(c.measure(m.dispatch))
        results[mode+'_completion32x4']=summarize(completed)
        results[mode+'_callback_dispatch']=summarize(dispatched)
        results[mode+'_queued_per_burst']=queued
    m=machine(folder);c=Counter(m);m.mutex_busy=True
    results['busy_gate']=summarize([c.measure(m.gate)for _ in range(16)])
    results['busy_request']=summarize([c.measure(m.request)for _ in range(16)])
    for retained in (0,32,128,255,256):
        m=machine(folder);c=Counter(m)
        # Real failed submissions establish both quarantine occupancy and any
        # allocator hint; no synthetic hint state favors the optimized binary.
        for _ in range(retained):m.fail_malloc=True;m.request()
        results[f'first_request_retained_cookies_{retained}']=summarize([c.measure(m.request)])
    return results

def rx_profile(folder,out):
    image=DriverImage((ROOT/'build/wifi-driver-re-20261018/devnp-mrvl_wlan-sdiorm.so').read_bytes())
    fragments=dict(candidate=compile_candidate(out)[0],window_candidate=compile_window_candidate(out)[0],harden_candidate=compile_harden_candidate(out)[0])
    results={}
    for mode in ('stock','rx_fragments','full'):
        if mode=='full':m=FullRuntimeMachine(image_path=folder/'devnp-mrvl_wlan-sdiorm.so',payload_elf_path=folder/'latency_payload.elf',base=0x2000000,window=64,start=0)
        else:m=ReorderMachine(image,start=0,window=64,**(fragments if mode=='rx_fragments'else {}))
        c=Counter(m)
        rows=[c.measure(lambda i=i:m.receive(i))for i in range(64)]
        results[mode+'_clean64']=summarize(rows)
        results[mode+'_events']={'delivered':len(m.delivered),'events':len(m.events)}
        if mode=='full':results[mode+'_runtime_stats']=m.getstats()
        if mode=='full':m=FullRuntimeMachine(image_path=folder/'devnp-mrvl_wlan-sdiorm.so',payload_elf_path=folder/'latency_payload.elf',base=0x2000000,window=64,start=0)
        else:m=ReorderMachine(image,start=0,window=64,**(fragments if mode=='rx_fragments'else {}))
        c=Counter(m);loss=[]
        for i in range(1,4):
            m.advance(i);loss.append(c.measure(lambda i=i:m.receive(i)))
        expiry=c.measure(lambda:m.advance(70))
        assert len(m.delivered)==3
        results[mode+'_hole3_receive']=summarize(loss)
        results[mode+'_hole3_expiry']=summarize([expiry])
        results[mode+'_hole3_events']={'delivered':len(m.delivered),'native_events':len(m.events),'native_timer_arms':sum(e['event']=='arm'for e in m.events)}
        if mode=='full':results[mode+'_hole3_runtime_stats']=m.getstats()
    return results

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--candidate',type=Path,default=BASELINE)
    ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    before=hashlib.sha256((a.candidate/'devnp-mrvl_wlan-sdiorm.so').read_bytes()).hexdigest()
    result={'status':'pass','binary_sha256':before,'tx':tx_profile(a.candidate),'rx':rx_profile(a.candidate,a.out),
      'method':'Actual ARM instructions counted with Unicorn hook after initialization. Sixteen repeated hot calls and four identical32completion bursts. Percentiles describe deterministic instruction-cost distribution, not time or latency.',
      'limits':['OS mutex/allocator/pulse/finalTX endpoints modeled; no QNX/libc mutex internals counted.','Stock TX ablation restores native completion gate/enqueue instructions in a disposable full-image fixture; RX payload irrelevant on that path.','RX stock/fragments use native timer endpoint models; full runtime executes native callout integration, so total-cost scope differs.','No HU CPU, cycles, RF or end-to-end latency inference.']}
    assert before==hashlib.sha256((a.candidate/'devnp-mrvl_wlan-sdiorm.so').read_bytes()).hexdigest()
    result['harness_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    (a.out/'profile.json').write_text(json.dumps(result,indent=2)+'\n');print(a.out/'profile.json')
if __name__=='__main__':main()
