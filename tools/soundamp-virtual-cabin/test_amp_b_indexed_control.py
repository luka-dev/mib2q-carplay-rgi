"""Decoded single-record FIFO oracle; callees return success without side effects."""
import functools
import re
import struct
import unittest
from amp_b_indexed_control import indexed_command_plan
from test_amp_b_control import OUT
from test_amp_b_mode_control import decoded_pointer_writes


def float_word(v):return struct.unpack('<I',struct.pack('<f',v))[0]


@functools.lru_cache(None)
def queue_code():
    code={}
    for line in (OUT/'first_b_extended_queue_apply.asm').read_text().splitlines():
        m=re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
        if m:code[int(m[1],16)]=(len(m[2].split()),m[3])
    return code


@functools.lru_cache(None)
def validation_code():
    code={}
    for name in ('first_b_extended_write_validation','first_b_extended_special_validation'):
        for line in (OUT/f'{name}.asm').read_text().splitlines():
            m=re.match(r'([0-9a-f]{8})\s+((?:[0-9a-f]{4} )+)\s*(.*)',line)
            if m:code[int(m[1],16)]=(len(m[2].split()),m[3])
    return code


def decoded_validation(command,payload,count):
    """Canonical write branch after ready/pointer checks; error call = rejection.

    Special3 table dispatch is an explicit bridge pinned by extraction tests.
    R1 retains ready1 from131A88, used by13207E to accept raw bank2.
    """
    code=validation_code();r={'R0':count,'R1':1,'R9':command,'R10':command&65535,
        'R13':10000,'M5':0};pc=0x131D22;cmp=0;sz=False;pending=None
    for _ in range(80):
        if pc==0x131D40:return r['R0']==0
        if pc==0x131DDC:
            if command!=0x20000003:raise AssertionError('only canonical special3 bridge is supported')
            pc=0x132072;continue
        n,insn=code[pc]
        if insn.startswith('CJUMP '):return False  # validation failure/logger
        old=r.copy();ocmp=cmp;osz=sz;updates={};nextpc=pc+n;newpending=None
        def v(x):return old[x] if x in old else int(x,0)
        for part in insn.split(' || '):
            if part.startswith('IF '):
                cond,part=part[3:].split(' ',1)
                if cond=='NOT':cond='NOT '+part.split(' ',1)[0];part=part.split(' ',1)[1]
                if not {'EQ':ocmp==0,'NE':ocmp!=0,'LT':ocmp<0,'GE':ocmp>=0,'GT':ocmp>0,
                        'SZ':osz,'NOT SZ':not osz}[cond]:continue
            if part=='NOP':continue
            j=re.fullmatch(r'JUMP (0x[0-9a-f]+)( \(DB\))?',part)
            if j:
                if j[2]:newpending=[2,int(j[1],16)]
                else:nextpc=int(j[1],16)
                continue
            c=re.fullmatch(r'COMPU?\((\w+),(\w+)\)',part)
            if c:
                a,b=v(c[1]),v(c[2]);cmp=(a>b)-(a<b);continue
            if part.startswith('BTST '):
                a,b=part[5:].split(' BY ');sz=not (v(a)&(1<<v(b)));continue
            dest,expr=part.split('=',1)
            if expr.startswith('PASS '):value=v(expr[5:]);cmp=(value>0)-(value<0)
            elif expr.startswith('BTGL '):
                a,b=expr[5:].split(' BY ');value=v(a)^(1<<v(b));sz=value==0
            elif expr.startswith('LSHIFT '):
                a,b=expr[7:].split(' BY ');value=v(a)>>-v(b);sz=value==0
            elif expr=='DM(M5,I4)':value=payload
            elif expr=='DM(M4,I4)':value=0x132072  # only special3 bridge
            else:value=v(expr)
            updates[dest]=value
        r.update(updates)
        if pending:
            pending[0]-=1
            if pending[0]==0:nextpc=pending[1];pending=None
        if newpending:pending=newpending
        pc=nextpc
    raise AssertionError('validation did not terminate')


def decoded_record(command,payload,effective):
    """Enter after queue/pending checks, stop before advancing record index.

    Separate arithmetic and shift flags, old parallel operands and two branch
    delay slots. Subcalls are recorded with explicit ABI args, return0, and do
    not execute. This independently checks the plan, not full firmware results.
    """
    code=queue_code();obj=10000;target=20000;record=30000
    r={f'R{i}':0 for i in range(16)}|{f'I{i}':0 for i in range(16)}
    r.update(R15=obj,R13=obj+92,R11=obj+59,R3=obj+299,R10=obj+1156,
             R6=829,R7=0xffff0000,R5=0x20000003,I2=record,I5=obj,I7=40000,
             M4=0,M5=0,M7=-1,M13=0)
    mem={record:command,record+1:payload,obj+1180:target,obj+1179:50000,0xb909a:1}
    mem.update({obj+1128+k-16:v for k,v in effective.items()})
    stores=[];calls=[];pc=0x13272A;pending=None;cmp=0;sz=False
    for _ in range(180):
        if pc==0x132745:return stores,calls
        n,insn=code[pc]
        if insn.startswith('CJUMP '):
            callee=int(insn.split()[1],16)
            call={'helper':hex(callee)}
            if callee==0x12B348:call.update(api=r['R12'],value_word=mem[r['I7']+1])
            elif callee==0x132346:call['value_word']=r['R8']
            elif callee==0x132AEF:
                call.update(source_offset=r['R8']-50000,effective_config_object_offset=r['R4']+240-obj,
                            words=829,refresh_crc=True)
            elif callee not in (0x132168,0x1320CE):raise AssertionError(f'unexpected callee {callee:x}')
            calls.append(call);r['R0']=0;pc+=n
            for _ in range(2):pc+=code[pc][0]
            continue
        old=r.copy();oldcmp=cmp;oldsz=sz;updates={};mw={};nextpc=pc+n;newpending=None
        def v(s):return old[s] if s in old else int(s,0)
        def addr(s):
            if ',' not in s:return v(s)
            a,b=s.split(',')
            if a.startswith('I'):
                updates[a]=v(a)+v(b);return v(a)
            return v(a)+v(b)
        for part in insn.split(' || '):
            if part.startswith('IF '):
                cond,part=part[3:].split(' ',1)
                if cond=='NOT':cond='NOT '+part.split(' ',1)[0];part=part.split(' ',1)[1]
                if not {'EQ':oldcmp==0,'NE':oldcmp!=0,'LE':oldcmp<=0,
                        'SZ':oldsz,'NOT SZ':not oldsz}[cond]:continue
            if part=='NOP':continue
            j=re.fullmatch(r'JUMP (0x[0-9a-f]+)( \(DB\))?',part)
            if j:
                if j[2]:newpending=[2,int(j[1],16)]
                else:nextpc=int(j[1],16)
                continue
            c=re.fullmatch(r'COMPU?\((\w+),(\w+)\)',part)
            if c:
                a,b=v(c[1]),v(c[2]);cmp=(a>b)-(a<b);continue
            if part.startswith('BTST '):
                a,b=part[5:].split(' BY ');sz=not (v(a)&(1<<v(b)));continue
            part=re.sub(r'^(\w+)\*=([\w]+) \(SSI\)$',r'\1=\1*\2',part)
            dest,expr=part.split('=',1)
            if dest.startswith('DM('):mw[addr(dest[3:-1])]=v(expr);continue
            if expr.startswith('DM('):value=mem[addr(expr[3:-1])]
            elif expr.startswith('MODIFY('):
                a,b=expr[7:-1].split(',');value=v(a)+v(b)
            elif expr.startswith('PASS '):value=v(expr[5:]);cmp=(value>0)-(value<0)
            elif expr.startswith('FEXT '):
                a,spec=expr[5:].split(' BY ');start,width=map(int,spec.split(':'))
                value=(v(a)>>start)&((1<<width)-1);sz=value==0
            elif expr.startswith('BTGL '):
                a,b=expr[5:].split(' BY ');value=v(a)^(1<<v(b));sz=value==0
            elif expr.startswith('LSHIFT '):
                a,b=expr[7:].split(' BY ');shift=v(b)
                value=(v(a)<<shift)&0xffffffff if shift>=0 else v(a)>>-shift;sz=value==0
            elif re.fullmatch(r'\w+[+\-&*]\w+',expr):
                a,op,b=re.split(r'([+\-&*])',expr);a,b=v(a),v(b)
                value={'+':lambda:a+b,'-':lambda:a-b,'&':lambda:a&b,'*':lambda:a*b}[op]()
                cmp=(value>0)-(value<0)
            else:value=v(expr)
            updates[dest]=value
        r.update(updates);mem.update(mw)
        for address,word in mw.items():
            if obj<=address<obj+1200:stores.append({'space':'object','offset':address-obj,'word':word})
            elif target<=address<target+900:stores.append({'space':'target','offset':address-target,'word':word})
        if pending:
            pending[0]-=1
            if pending[0]==0:nextpc=pending[1];pending=None
        if newpending:pending=newpending
        pc=nextpc
    raise AssertionError('single record did not terminate')


class BIndexedControlTests(unittest.TestCase):
    def setUp(self):
        self.effective={api:float_word(0) for api in range(16,30)}
        self.effective.update({16:float_word(5),17:float_word(1),20:float_word(3)})

    def plan(self,cmd,payload,**kw):
        return indexed_command_plan(cmd,payload,ready=kw.get('ready',1),queue_count=kw.get('count',0),effective_words=self.effective)

    def test_all842_config_indices_against_decoded_record(self):
        for i in range(1,843):
            p=self.plan(0x14000000|i,0x3f800000)
            writes,calls=decoded_record(0x14000000|i,0x3f800000,self.effective)
            self.assertEqual((p['direct_writes'],p['calls']),(writes,calls),i)

    def test_all203_core_indices_against_decoded_record(self):
        for i in range(4,207):
            p=self.plan(0x12000000|i,0xdeadbeef)
            self.assertEqual((p['direct_writes'],p['calls']),decoded_record(0x12000000|i,0xdeadbeef,self.effective),i)

    def test_defer_values_and_source_store_against_decoded_record(self):
        for cmd,values in [(0x10000000,range(3)),(0x20000003,range(1,5))]:
            for v in values:
                p=self.plan(cmd,v)
                self.assertEqual((p['direct_writes'],p['calls']),decoded_record(cmd,v,self.effective))

    def test829_is_not830_and_delay_slot_target_store_is_retained(self):
        a=self.plan(0x1400033d,float_word(2));b=self.plan(0x1400033e,float_word(2))
        self.assertEqual(a['calls'],[{'helper':'0x1320ce'}]);self.assertEqual(len(b['calls']),4)
        self.assertEqual(a['target_offsets_outside_config'],[829])
        self.assertEqual(a['effective_words_after_direct_write'][16],float_word(2))
        self.assertEqual(b['effective_words_after_direct_write'][17],float_word(2))

    def test_both_mode_helpers_receive_twenty_not_sixteen(self):
        p=self.plan(0x14000347,float_word(7))
        self.assertEqual([(c['api'],c['value_word']) for c in p['calls'][1:3]],[(20,float_word(3)),(16,float_word(3))])
        # This inner helper bypasses the normal API16 validator; decoded3 uses default pointers.
        writes=decoded_pointer_writes(20,3);writes.update(decoded_pointer_writes(16,3))
        self.assertEqual(writes[1156],121);self.assertEqual(writes[1161],31)
        self.assertEqual(writes[1166],439);self.assertEqual(writes[1171],195)

    def test_write_to_api20_used_by_both_followup_calls(self):
        p=self.plan(0x14000341,float_word(1))  #833 -> object1132
        self.assertEqual([c['value_word'] for c in p['calls'][1:3]],[float_word(1)]*2)
        self.assertEqual(self.effective[20],float_word(3))

    def test_raw_words_not_float_payload_validation(self):
        for word in (0,0xffffffff,0x7fc00000):
            self.assertEqual(self.plan(0x1400001f,word)['direct_writes'][0]['word'],word)
        with self.assertRaises(ValueError):self.plan(0x20000003,float_word(1))

    def test_core_index5_reaches_gain97(self):
        self.assertEqual(self.plan(0x12000005,float_word(2))['direct_writes'],
            [{'space':'object','offset':97,'word':float_word(2)}])

    def test_rejects_boundaries_unknown_headers_and_queue_overflow(self):
        for cmd in (0x14000000,0x1400034b,0x12000003,0x120000cf,16,0x0c000001):
            with self.assertRaises(ValueError):self.plan(cmd,1)
        for count in (-1,64,65):
            with self.assertRaises(ValueError):self.plan(0x14000001,0,count=count)
        self.plan(0x14000001,0,count=63)
        with self.assertRaises(ValueError):self.plan(0x14000001,0,ready=0)

    def test_snapshot_is_explicit_and_not_mutated(self):
        before=self.effective.copy();self.plan(0x1400033e,float_word(2))
        self.assertEqual(self.effective,before)
        with self.assertRaises(ValueError):indexed_command_plan(0x14000001,0,ready=1,queue_count=0,effective_words={})

    def test_canonical_validation_boundaries_against_decoded_branches(self):
        for base,limit in [(0x14000000,845),(0x12000000,210)]:
            for index in range(limit):
                for count in (0,63,64):
                    try:self.plan(base|index,0,count=count);accepted=True
                    except ValueError:accepted=False
                    self.assertEqual(accepted,decoded_validation(base|index,0,count),(hex(base),index,count))
        for command in (0x10000000,0x20000003):
            for payload in (0,1,2,3,4,5,float_word(1),0xffffffff):
                for count in (0,63,64):
                    try:self.plan(command,payload,count=count);accepted=True
                    except ValueError:accepted=False
                    self.assertEqual(accepted,decoded_validation(command,payload,count),(hex(command),payload,count))


if __name__=='__main__':unittest.main()
