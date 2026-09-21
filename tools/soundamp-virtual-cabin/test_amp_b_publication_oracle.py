"""Execute the decoded12FAF2 scalar path independently of host state helper."""
import re
import struct
import sys
import unittest

import numpy as np

from amp_b_publication import ROOT,LOADER,BConfigPublicationReference,publication_evidence


def decoded_advance(mem,current,target,step,offset=122):
    """Small fail-closed float32 ISA subset, bounded to this leaf helper.

    Not a SHARC40-bit emulator. Branches/loads/stores and parallel old operands
    come from the decoded binary; it does not call the Python reference kernel.
    """
    from inspect_amp_visa import decode
    bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
    floating=lambda v:struct.unpack('<f',struct.pack('<I',v&0xffffffff))[0]
    regs={f'R{i}':0 for i in range(16)}|{f'I{i}':0 for i in range(16)}
    regs.update(R4=10000+offset,M5=0,M6=1,M7=-1,M14=1,I6=40000)
    data={0xB903A:10000,0xB903B:20000,
          10000+offset:bits(current),20000+offset:bits(target),0xB8CFD+offset:bits(step)}
    pc=0x12FAF9;comparison=0
    for _ in range(100):
        size,text,*_=decode(mem,pc);nextpc=pc+size
        if text.startswith('JUMP (M14,I12)'):
            return floating(data[10000+offset]),floating(data[0xB8CFD+offset])
        old=regs.copy();writes={}
        def value(v):return old[v] if v in old else int(v,0)
        def f(v):return floating(value('R'+v[1:]))
        def address(spec):
            if ','not in spec:return int(spec,0)
            a,b=spec.split(',')
            if a.startswith('I'):
                result=value(a);writes[a]=result+value(b);return result
            return value(a)+value(b)
        for op in text.split(' || '):
            if op.startswith('IF '):
                match=re.fullmatch(r'IF (GT|EQ) JUMP (0x[0-9a-f]+)',op)
                if not match:raise AssertionError(op)
                if (comparison>0 if match[1]=='GT' else comparison==0):nextpc=int(match[2],16)
                continue
            if op.startswith('COMP(') or op.startswith('COMPU('):
                a,b=op[op.index('(')+1:-1].split(',')
                x,y=(f(a),f(b)) if a.startswith('F') else (value(a)&0xffffffff,value(b)&0xffffffff)
                comparison=(x>y)-(x<y);continue
            if op=='F2+=F0':writes['R2']=bits(np.float32(f('F2')+f('F0')));continue
            dest,expr=op.split('=',1)
            if dest.startswith('DM('):data[address(dest[3:-1])]=value(expr);continue
            if expr.startswith('DM('):out=data.get(address(expr[3:-1]),0)
            elif expr=='ABS(F1-F2)':out=bits(abs(np.float32(f('F1')-f('F2'))))
            elif expr=='ABS F0':out=bits(abs(f('F0')))
            elif expr=='R4-R2':out=(value('R4')-value('R2'))&0xffffffff
            else:out=value(expr)
            writes[('R'+dest[1:]) if dest.startswith('F') else dest]=out
        regs.update(writes);pc=nextpc
    raise AssertionError('decoded scalar helper did not terminate')


class PublicationDecodedOracleTests(unittest.TestCase):
    def test_threshold_sign_equality_and_lower_bound_against_binary(self):
        sys.path.insert(0,str(ROOT/'tools'))
        from inspect_amp_visa import load_shortwords
        publication_evidence();mem=load_shortwords(LOADER.read_bytes())
        cases=[(0.,.5,.5),(0.,float(np.nextafter(np.float32(.5),np.float32(1))),.5),
               (0.,float(np.nextafter(np.float32(.5),np.float32(0))),.5),
               (0.,-.5,-.5),(1.,1.,.8),(0.,1.,0.),(0.,1.,-.25),(0.,-1.,.25)]
        rng=np.random.default_rng(0x12FAF2)
        cases += [tuple(rng.uniform(-4,4,3).astype(np.float32)) for _ in range(80)]
        for current,target,step in cases:
            for offset in (122,-202):
                effective=np.zeros(829);desired=np.zeros(829);steps=np.zeros(829)
                if offset>=0:effective[offset]=current;desired[offset]=target;steps[offset]=step
                state=BConfigPublicationReference(effective_config=effective,target_config=desired,steps=steps)
                state.advance_offsets([offset])
                got=(state.effective[offset],state.steps[offset]) if offset>=0 else (
                    float(np.float32(current)),float(np.float32(step)))
                self.assertEqual(got,decoded_advance(mem,current,target,step,offset))


if __name__=='__main__':unittest.main()
