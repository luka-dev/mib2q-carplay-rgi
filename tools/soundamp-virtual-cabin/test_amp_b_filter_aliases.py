"""Independent decoded constructor/reset oracle; no audio/SHARC CPU claims."""
from pathlib import Path
import re
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_amp_b_filter_aliases import LOADER, IMAGE, DOMAINS, audit, layout, cascade_domain, retained_cascade_domains
from inspect_amp_visa import load_shortwords, decode


class Oracle:
    """Integer subset, actual decoded control flow and old parallel operands.

    Only allocator/context calls are stand-ins; nested12FC86/12FC78 execute
    independently. Calls bridge the documented stack ABI, not DSP call timing.
    No expected descriptor layout or section/channel loop bounds consulted.
    """
    def __init__(self, code, start=101182):
        self.code = code; self.cursor = start; self.memory = {}; self.allocations = []; self.reads = []; self.process_calls = []; self.stores = []

    def run(self, entry, r4, r8=0, r12=0, extra=0, depth=0, initial=None, stop_pc=None):
        regs = {f'{k}{i}': 0 for k in ('R','I','M') for i in range(16)}
        frame = 400000-depth*1000
        regs.update(R4=r4, R8=r8, R12=r12, I6=frame, I7=frame-2,
                    M5=0, M6=1, M7=-1, M14=1)
        if initial:regs.update(initial)
        self.memory.update({frame-1: 0, frame+1: extra})
        pc=entry; flag=0; pending=None; loop=None; sz=False
        for _ in range(20000):
            if pc==stop_pc:return regs['R0']
            n, text, target = decode(self.code, pc)
            if text.startswith('JUMP (M14,I12)'): return regs['R0']
            if text.startswith('CJUMP '):
                if target == 0x12B683: regs['R0'] = 76543
                elif target == 0x132D56:
                    assert regs['R12'] == 0x2000
                    words = (regs['R4']*regs['R8']+1)&~1
                    regs['R0']=self.cursor; self.allocations.append((self.cursor,self.cursor+words))
                    self.memory.update({p:0 for p in range(self.cursor,self.cursor+words)}); self.cursor+=words
                elif target == 0x12FC86:
                    regs['R0']=self.run(target,regs['R4'],depth=depth+1)
                elif target == 0x12FC78:
                    self.run(target,regs['R4'],depth=depth+1)  # R0 unchanged by actual reset routine
                elif target == 0x12DAA5:
                    self.process_calls.append(regs['R4']);regs['R0']=0  # capture only, no filter audio execution
                elif target == 0x12D7DC:
                    self.memory.update({regs['R8']+i:regs['R4'] for i in range(regs['R12'])})
                elif target in (0x12D9A9,0x12DA2F):
                    pass  # address-domain oracle: audio bodies only update caller scratch histories
                else: raise AssertionError(f'unexpected call {target:x}')
                pc+=n
                for _ in range(2): pc+=decode(self.code,pc)[0]
                continue
            old=regs.copy(); oldflag=flag; oldsz=sz; writes={}; stores={}; nextpc=pc+n; newpending=None
            def val(v): return old[v] if v in old else int(v,0)
            def fval(v):return struct.unpack('<f',struct.pack('<I',old['R'+v[1:]]&0xffffffff))[0]
            def address(spec):
                a,b=spec.split(',')
                if a.startswith('I'):
                    result=val(a);writes[a]=result+val(b);return result
                return val(a)+val(b)
            for part in text.split(' || '):
                if part.startswith('IF '):
                    match=re.fullmatch(r'IF (NOT SZ|EQ|NE|LT|LE|GE) (.*)',part);assert match,part
                    condition,part=match.groups()
                    if not {'EQ':oldflag==0,'NE':oldflag!=0,'LT':oldflag<0,'LE':oldflag<=0,'GE':oldflag>=0,'NOT SZ':not oldsz}[condition]:continue
                if part=='NOP':continue
                if part.startswith('LCNTR='):
                    match=re.fullmatch(r'LCNTR=(\w+), DO (0x[0-9a-f]+) UNTIL LCE',part);assert match,part
                    loop=[pc+n,int(match[2],16),val(match[1])];assert loop[2]>0;continue
                if part.startswith('BTST '):
                    match=re.fullmatch(r'BTST (\w+) BY (\d+)',part);assert match,part
                    sz=(val(match[1])&(1<<int(match[2])))==0;continue
                if part.startswith('JUMP '):
                    if '(DB)' in part:newpending=[2,target]
                    else:nextpc=target
                    continue
                if part.startswith('COMPU('):
                    a,b=part[6:-1].split(',');a=val(a)&0xffffffff;b=val(b)&0xffffffff
                    flag=(a>b)-(a<b);continue
                part=re.sub(r'^(\w+)([+*])=(\w+)$',r'\1=\1\2\3',part)
                dest,expr=part.split('=',1)
                if dest.startswith('DM('):
                    addr=address(dest[3:-1]);stores[addr]=val(expr);self.stores.append((pc,addr));continue
                if expr.startswith('DM('):
                    addr=address(expr[3:-1]);self.reads.append((pc,addr));out=self.memory[addr]
                elif expr.startswith('MODIFY('):
                    a,b=expr[7:-1].split(',');out=val(a)+val(b)
                elif expr.startswith('PASS '):
                    out=val(expr[5:]);flag=(out>0)-(out<0)
                elif expr.startswith('FLOAT '):out=float(val(expr[6:]))
                elif expr.startswith('TRUNC '):out=int(fval(expr[6:]))
                elif re.fullmatch(r'\w+[+\-*]\w+( \(SSI\))?',expr):
                    a,op,b=re.split(r'([+\-*])',expr.removesuffix(' (SSI)'))
                    read=fval if dest.startswith('F') else val
                    out=read(a)+read(b) if op=='+' else read(a)-read(b) if op=='-' else read(a)*read(b)
                    flag=(out>0)-(out<0)
                else:out=val(expr)
                if dest.startswith('F'):
                    dest='R'+dest[1:];out=struct.unpack('<I',struct.pack('<f',out))[0]
                writes[dest]=out
            regs.update(writes);self.memory.update(stores)
            if pending:
                pending[0]-=1
                if pending[0]==0:nextpc=pending[1];pending=None
            if newpending:pending=newpending
            if loop and pc==loop[1]:
                loop[2]-=1
                if loop[2]>0:nextpc=loop[0]
                else:loop=None
            pc=nextpc
        raise AssertionError('decoded bounded path did not terminate')


class FilterAliases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=LOADER.read_bytes();cls.code=load_shortwords(cls.data)

    def test_full_span_caller_and_source_validation(self):
        report=audit(self.data)
        self.assertEqual(report['decoded_instructions'],16093)
        self.assertFalse(report['dead_field_proven'])
        self.assertFalse(report['fixed_mode5_complete_alias_proof'])
        self.assertEqual(len(report['conditionally_closed_reads']),4)

    def test_changed_source_rejected(self):
        changed=bytearray(self.data);changed[-1]^=1
        with self.assertRaises(ValueError):audit(bytes(changed))

    def test_decoded_constructors_match_independent_allocation_witness(self):
        for d in DOMAINS:
            with self.subTest(domain=d):
                expected=layout(d['channels'],d['capacity']);oracle=Oracle(self.code)
                self.assertEqual(oracle.run(0x12D844,100034,d['channels'],d['capacity'],101178),0)
                self.assertEqual(oracle.memory[100034],expected['descriptor'])
                self.assertEqual(oracle.allocations,[(a['start'],a['end']) for a in expected['allocations']])
                for address,value in expected['memory'].items():
                    if address in (expected['descriptor'],expected['descriptor']+4):continue
                    self.assertEqual(oracle.memory[address],value)
                self.assertEqual(oracle.memory[expected['descriptor']+4],101178)

    def test_decoded_reset_visits_only_owned_states(self):
        for d in DOMAINS:
            oracle=Oracle(self.code)
            oracle.run(0x12D844,100034,d['channels'],d['capacity'],101178)
            desc=oracle.memory[100034];oracle.reads=[]
            oracle.run(0x12D94C,desc)
            visits=[a for pc,a in oracle.reads if pc==0x12D97C]
            self.assertEqual(len(visits),d['channels']*d['capacity'])
            self.assertTrue(all(a>=101182 for a in visits))
            self.assertFalse(set(visits)&set(range(101162,101166)))

    def test_corrupt_descriptor_can_reach_pointer_cell(self):
        oracle=Oracle(self.code)
        oracle.run(0x12D844,100034,2,2,101178)
        desc=oracle.memory[100034];array=oracle.memory[desc+3]
        oracle.memory[array]=101162
        # Valid state pointers let execution continue while the pointer cells
        # themselves are read. This negative control prevents a dead-field claim.
        oracle.memory[101162]=oracle.cursor;oracle.memory[101163]=oracle.cursor+2
        oracle.reads=[];oracle.run(0x12D94C,desc)
        self.assertIn((0x12D97C,101162),oracle.reads)
        self.assertIn((0x12D97C,101163),oracle.reads)

    def test_decoded_process_traversal_uses_owned_channel_arrays(self):
        for d in DOMAINS:
            oracle=Oracle(self.code)
            oracle.run(0x12D844,100034,d['channels'],d['capacity'],101178)
            desc=oracle.memory[100034];array=oracle.memory[desc+3]
            oracle.memory[400002]=300000
            oracle.memory.update({300000+i:500000+1024*i for i in range(d['channels'])})
            oracle.run(0x12DCD1,0,initial={'I2':desc,'R9':0,'R10':d['channels'],
                'R13':d['capacity'],'R11':102000,'R15':512},stop_pc=0x12DC2E)
            self.assertEqual(oracle.process_calls,[oracle.memory[array+i] for i in range(d['channels'])])
            self.assertEqual([a for pc,a in oracle.reads if pc==0x12DCE5],
                             list(range(array,array+d['channels'])))

    def test_retained_domain_fail_closed(self):
        for channels,capacity in [(0,2),(2,0),(9,2),(2,9),(2,2.0)]:
            with self.assertRaises(ValueError):layout(channels,capacity)

    def test_all_eight_retained_banks_fit_actual_constructors(self):
        report=retained_cascade_domains(self.data,IMAGE.read_bytes())
        self.assertEqual(len(report['scenarios']),8)
        for scenario in report['scenarios']:
            self.assertEqual(len(scenario['rows']),9)
            self.assertEqual(scenario['coefficient_pointer_cell_collisions'],[])
            self.assertLess(scenario['coefficient_object_offset_min_max'][1],1162)
            for row in scenario['rows']:
                self.assertLessEqual(row['count'],row['capacity'])
                self.assertLessEqual(row['coefficient_scratch_words_used'],40)

    def test_decoded_cascade_reads_writes_exactly_the_bounded_states(self):
        scenarios=retained_cascade_domains(self.data,IMAGE.read_bytes())['scenarios']
        cases={(row['count'],row['capacity'],row['count_object_offset'])
               for s in scenarios for row in s['rows']}
        coeff_pcs={0x12DB1A,0x12DB1D,0x12DB20,0x12DB23,0x12DB29}
        state_read_pcs={0x12DB44,0x12DB4B,0x12DB51,0x12DB57,0x12DBBB,0x12DBC1}
        state_write_pcs={0x12DB6A,0x12DB73,0x12DB79,0x12DB82,0x12DBD3,0x12DBDC}
        for count,capacity,field in sorted(cases):
            with self.subTest(count=count,capacity=capacity,field=field):
                oracle=Oracle(self.code);witness=layout(2,capacity);oracle.memory.update(witness['memory'])
                descriptor=witness['descriptor'];array=oracle.memory[descriptor+3]
                section_array=oracle.memory[array];coeff=100000+field
                oracle.memory.update({coeff+i:0 for i in range(1,5*count+1)})
                oracle.memory[400002]=500000
                oracle.run(0x12DAA5,section_array,coeff,count,extra=512)
                expected=cascade_domain(count,capacity,field)
                self.assertEqual(sorted(a-100000 for pc,a in oracle.reads if pc in coeff_pcs),expected['coefficient_object_offsets'])
                states={oracle.memory[section_array+i]+j for i in range(count) for j in (0,1)}
                self.assertEqual({a for pc,a in oracle.reads if pc in state_read_pcs},states)
                self.assertEqual({a for pc,a in oracle.stores if pc in state_write_pcs},states)
                self.assertEqual(len([a for pc,a in oracle.reads if pc in state_read_pcs]),2*count)
                self.assertFalse(states&set(range(101162,101166)))

    def test_mutated_count_cannot_reuse_retained_proof(self):
        for count,capacity in [(3,2),(5,4),(9,8),(93,8),(-1,8),(1.25,8)]:
            with self.assertRaises(ValueError):cascade_domain(count,capacity,698)


if __name__=='__main__':unittest.main()
