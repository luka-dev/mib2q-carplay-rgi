#!/usr/bin/env python3
"""Symbolic trace of the J525 V850 biquad builders (CodeFlash 8776fdb8..., SW0420).

Produces the exact formulas behind tools/soundamp-virtual-cabin/amp_dsp_chain.py.
Input = radare2 V850 listing of the builder bodies:

  r2 -a v850 -b 32 -q -e scr.color=0 -e asm.lines=false -e asm.bytes=false \
     -c 'pD 0x6d0 @ 0xab936' CodeFlash_24082026_172027.bin > builders.asm
  python3 v850_biquad_trace.py builders.asm

Soft-float helpers (verified from 0xEC170 unpack: r7 -> r9/r10, r6 -> r11/r12):
0xEB984 mul, 0xEBA38 add, 0xEBB70(r6,r7) = r7 - r6, 0xEBBCC(r6,r7) = r7 / r6,
0xEBCA4 itof, 0xEBAEC compare (bl taken when r7 < r6).  Dispatcher 0xA96C8 passes
r6 = cot(pi f/48000) (table 0x6BCE4 / 0x6CC84), r7 = P, r8 = 10^(dB/20)
(table 0x69398 - 4*tenths), r9 = gain tenths.  P: types 3/15 -> q/10 (zeta),
table types -> float[0x6EA30 + 4*type] (HP types read type-12), others -> 10/q.
Outputs: gp-17240/-17236/-17232 = b0 b1 b2, gp-17228 = 1/a0, gp-17224/-17220 = a1 a2;
0xABF7C packs [-a1, -a2, b0, b1, b2] * (1/a0).
"""
import re, struct, sys, sympy as sp
K, P, G, g = sp.symbols('K P G g')   # cot(pi f/fs), Q-param (r7), 10^(dB/20) (r8), gain tenths (r9)
OUTNAME = {-17240:'b0', -17236:'b1', -17232:'b2', -17228:'inv_a0', -17224:'a1', -17220:'a2'}
OPS = {0xeb984: lambda a,b: a*b, 0xeba38: lambda a,b: a+b, 0xebb70: lambda a,b: b-a, 0xebbcc: lambda a,b: b/a, 0xebca4: lambda a,b: a}
asm = {}
for line in open(sys.argv[1]):
    m = re.match(r'\s*0x([0-9a-f]+)\s+(\S+)\s*(.*)', line)
    if m: asm[int(m.group(1),16)] = (m.group(2), m.group(3).split(';')[0].strip())
addrs = sorted(asm)
def f32(bits): return sp.nsimplify(struct.unpack('<f', struct.pack('<I', bits & 0xffffffff))[0])
def run(entry, decisions, label):
    R = {'r0':0,'r6':K,'r7':P,'r8':G,'r9':g}; mem = {}; out = {}; pc = entry; dec = list(decisions); cmpv=None
    while True:
        mn, ops = asm[pc]; a = [x.strip() for x in ops.split(',')] if ops else []
        nxt = addrs[addrs.index(pc)+1]
        if mn == 'mov':
            if a[0].startswith('r'): R[a[1]] = R.get(a[0], sp.Symbol(a[0]))
            else: R[a[1]] = int(a[0],0)
        elif mn == 'movhi': R[a[2]] = f32(int(a[0]) << 16)
        elif mn == 'movea': R[a[2]] = int(a[0]) + (R.get(a[1],0) if a[1]!='r0' else 0)
        elif mn.startswith('st.'):
            m = re.match(r'(-?\d+)\[(\w+)\]', a[1]); off, base = int(m.group(1)), m.group(2)
            if base == 'gp': out[OUTNAME.get(off, f'gp{off}')] = R[a[0]]
            else: mem[(base,off)] = R[a[0]]
        elif mn.startswith('ld.'):
            m = re.match(r'(-?\d+)\[(\w+)\]', a[0]); off, base = int(m.group(1)), m.group(2)
            if base == 'gp': R[a[1]] = out[OUTNAME.get(off, f'gp{off}')]
            else: R[a[1]] = mem[(base,off)]
        elif mn == 'jarl':
            t = int(a[0],0)
            if t in OPS: R['r6'] = sp.simplify(OPS[t](R['r6'], R['r7']))
            elif t == 0xebaec: cmpv = (R['r6'], R['r7'])
            else: raise SystemExit(f'unknown call {t:x} at {pc:x}')
        elif mn in ('br','jr'): nxt = int(a[0],0)
        elif mn in ('bl','bge','be','bne','bh','bp','bn'):
            d = dec.pop(0); print(f'   {label}: branch {mn} at {pc:x} on cmp {cmpv} -> {"taken" if d else "fallthrough"}')
            if d: nxt = int(a[0],0)
        elif mn == 'cmp': cmpv = (a[0], R.get(a[1]))
        elif mn in ('prepare','dispose','stsr','ldsr','nop'):
            if mn == 'dispose': break
        elif mn == 'jmp': break
        elif mn in ('add','shl','and','addi','ld.bu','ld.h','truncated'): pass
        else: raise SystemExit(f'unhandled {mn} at {pc:x}')
        pc = nxt
    print(f'== {label} (entry {entry:x})')
    for k in ('b0','b1','b2','inv_a0','a1','a2'):
        if k in out: print(f'   {k} = {sp.factor(sp.simplify(out[k]))}')
    return out
if __name__ == '__main__':
    for entry, decisions, label in [
        (0xabf4e, [], 'type0 bypass'),
        (0xaba4c, [True], 'type1 peak, gain>=0 (bge taken)'),
        (0xaba4c, [False], 'type1 peak, gain<0'),
        (0xabc52, [], 'types 2/4/6 LP1(K)'),
        (0xabcaa, [], 'types 14/16/18 HP1(K)'),
        (0xabd0e, [], 'type3 + table types LP2(K,P)'),
        (0xabdc4, [], 'type15 + table types HP2(K,P)'),
        (0xab850, [True], 'type26(K,G) bl taken'),
        (0xab850, [False], 'type26(K,G) bl not taken'),
        (0xab940, [True], 'type27(K,G) bl taken'),
        (0xab940, [False], 'type27(K,G) bl not taken'),
        (0xabe8e, [], 'type28(K,P)'),
    ]:
        try: run(entry, decisions, label)
        except Exception as e: print('!!', label, repr(e))
