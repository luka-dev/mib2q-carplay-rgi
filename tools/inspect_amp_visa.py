#!/usr/bin/env python3
"""Bounded ADSP-214xx VISA listing for J525 research, not a general emulator.

Instruction fields follow ADI Programming Reference rev 2.4, chapters 10/12.
Unknown instruction types raise; unimplemented compute operations are labelled
UNKNOWN, never presented as decoded semantics. Only the audited ranges are
validated: this is not a replacement for the vendor's full disassembler.
Parallel clauses read their operands before any clause writes its result.
Addresses are short-word addresses; loader halfwords are little endian, but
successive instruction halfwords are MSB first. No firmware is modified.
"""
import argparse
import struct
from pathlib import Path
from extract_amp_sharc_app import parse_blocks, BITREV_SHA256, sha256

COND = ['EQ','LT','LE','AC','AV','MV','MS','SV','SZ','FLAG0','FLAG1','FLAG2',
        'FLAG3','TF','BM','NOT LCE','NE','GE','GT','NOT AC','NOT AV','NOT MV',
        'NOT MS','NOT SV','NOT SZ','NOT FLAG0','NOT FLAG1','NOT FLAG2',
        'NOT FLAG3','NOT TF','NBM','TRUE']

def sx(x, n):
    return x - (1 << n) if x & (1 << (n-1)) else x

def ureg(x):
    if x < 96:
        return ['R','I','M','L','B','S'][x//16]+str(x%16)
    return ['FADDR','DADDR','MODE1','PC','PCSTK','PCSTKP','LADDR','CURLCNTR',
            'LCNTR','EMUCLK','EMUCLK2','PX','PX1','PX2','TPERIOD','TCOUNT',
            'USTAT1','USTAT2','MODE1','MMASK','MODE2','FLAGS','ASTATx','ASTATy',
            'STKYx','STKYy','IRPTL','IMASK','IMASKP','LRPTL','USTAT3','USTAT4'][x-96]

def compute(c):
    if not c:
        return ''
    n,x,y = (c>>8)&15,(c>>4)&15,c&15
    op,cu = (c>>12)&255,(c>>20)&3
    if c & 0x400000:
        mo=(c>>16)&63
        # ADI rev2.4 table12-10: MR transfer, bit16 is direction.
        if mo in (0,1):
            mr={0:'MR0F',1:'MR1F',2:'MR2F',4:'MR0B',5:'MR1B',6:'MR2B'}.get((c>>12)&15)
            if mr is not None:return f'{mr}=R{n}' if mo else f'R{n}={mr}'
        if mo in (0x18,0x19):
            return (f'F{(c>>12)&15}=F{(c>>6)&3}*F{4+((c>>4)&3)}, '
                    f'F{n}=F{8+((c>>2)&3)}'+('+' if mo==0x18 else '-')+f'F{12+(c&3)}')
        if mo==0x1d:
            return (f'F{(c>>12)&15}=F{(c>>6)&3}*F{4+((c>>4)&3)}, '
                    f'F{n}=ABS F{8+((c>>2)&3)}')
        if 0x30<=mo<=0x3f:
            a,b=8+((c>>2)&3),12+(c&3)
            return (f'F{(c>>12)&15}=F{(c>>6)&3}*F{4+((c>>4)&3)}, '
                    f'F{n}=F{a}+F{b}, F{mo&15}=F{a}-F{b}')
    elif cu==0:
        if 0xf0<=op<=0xff:
            return f'F{n}=F{x}+F{y}, F{op&15}=F{x}-F{y}'
        binary={1:('+','R'),2:('-','R'),0x40:('&','R'),0x41:('|','R'),0x42:('^','R'),0x81:('+','F'),0x82:('-','F')}
        if op in binary:
            s,p=binary[op];return f'{p}{n}={p}{x}{s}{p}{y}'
        if op in (0x21,0xa1):
            p='R' if op==0x21 else 'F';return f'{p}{n}=PASS {p}{x}'
        if op in (0xa,0x8a):
            p='R' if op==0xa else 'F';return f'COMP({p}{x},{p}{y})'
        if op==0xb:return f'COMPU(R{x},R{y})'
        if op in (0x61,0x62):return f'R{n}={"MIN" if op==0x61 else "MAX"}(R{x},R{y})'
        if op in (0xe1,0xe2):return f'F{n}={"MIN" if op==0xe1 else "MAX"}(F{x},F{y})'
        if op in (0x22,0xa2):
            p='R' if op==0x22 else 'F';return f'{p}{n}=-{p}{x}'
        if op==0x89:return f'F{n}=(F{x}+F{y})/2'
        if op==0x92:return f'F{n}=ABS(F{x}-F{y})'
        if op==0xb0:return f'F{n}=ABS F{x}'
        if op==0xe0:return f'F{n}=F{x} COPYSIGN F{y}'
        if op==0xc4:return f'F{n}=RECIPS F{x}'
        if op==0xc5:return f'F{n}=RSQRTS F{x}'
        if op==0xc1:return f'R{n}=LOGB F{x}'
        if op==0xad:return f'R{n}=MANT F{x}'
        if op==0xa5:return f'F{n}=RND F{x}'
        if op==0xbd:return f'F{n}=SCALB F{x} BY R{y}'
        if op in (0x29,0x2a):return f'R{n}=R{x}'+('+' if op==0x29 else '-')+'1'
        if op==0xca:return f'F{n}=FLOAT R{x}'
        if op==0xc9:return f'R{n}=FIX F{x}'
        if op==0xcd:return f'R{n}=TRUNC F{x}'
        # ADI rev2.4 tables12-3/4: scaled conversion and fixed-point ABS.
        if op==0x30:return f'R{n}=ABS R{x}'
        if op==0xd9:return f'R{n}=FIX F{x} BY R{y}'
        if op==0xdd:return f'R{n}=TRUNC F{x} BY R{y}'
        if op==0xda:return f'F{n}=FLOAT R{x} BY R{y}'
    elif cu==1:
        if op==0x30:return f'F{n}=F{x}*F{y}'
        # ADI rev2.4 tables12-5/7/8: integer multiply/accumulate and SAT.
        dest=(f'R{n}',f'R{n}','MRF','MRB')[(op>>1)&3]
        source='MRB' if op&2 else 'MRF'
        if op in (0x14,0x16):return f'{dest}=0'
        if op<16:return f'{dest}=SAT {source} ({"S" if op&1 else "U"}{"F" if op&8 else "I"})'
        if op>=0x40:
            mod=('S' if op&0x10 else 'U')+('S' if op&0x20 else 'U')+('F' if op&8 else 'I')+('R' if op&1 else '')
            accum='' if op<0x80 else source+('+' if op<0xc0 else '-')
            return f'{dest}={accum}R{x}*R{y} ({mod})'
    elif cu==2:
        if op==0:return f'R{n}=LSHIFT R{x} BY R{y}'
        if op==4:return f'R{n}=ASHIFT R{x} BY R{y}'
        if op==0x88:return f'R{n}=LEFTZ R{x}'
        if op==0xc0:return f'R{n}=BSET R{x} BY R{y}'
        if op==0xc4:return f'R{n}=BCLR R{x} BY R{y}'
        if op==0xc8:return f'R{n}=BTGL R{x} BY R{y}'
        if op==0xcc:return f'BTST R{x} BY R{y}'
    return f'COMPUTE_UNKNOWN({c:#08x})'

def shortcompute(h):
    op,n,x=(h>>8)&15,(h>>4)&15,h&15
    return [f'R{n}+=R{x}',f'R{n}-=R{x}',f'R{n}=PASS R{x}',f'COMP(R{n},R{x})',
            f'R{n}=NOT R{x}',f'R{n}=R{x}+1',f'R{n}=R{x}-1',f'R{n}*=R{x} (SSI)',
            f'F{n}+=F{x}',f'F{n}-=F{x}',f'F{n}=FLOAT R{x}',f'COMP(F{n},F{x})',
            f'R{n}&=R{x}',f'R{n}|=R{x}',f'R{n}^=R{x}',f'F{n}*=F{x}'][op]

def load_shortwords(data):
    if sha256(data)!=BITREV_SHA256:raise ValueError('wrong loader hash')
    out={}
    for b in parse_blocks(data):
        if b.tag==3:
            out.update((b.address+i,w) for i,w in enumerate(struct.unpack_from(f'<{b.count}H',data,b.offset+12)))
        elif b.tag==1 and b.address>=0x100000:
            out.update((b.address+i,0) for i in range(b.count))
    return out

def decode(mem, pc):
    h=mem[pc]; top=h>>8; size=3; desc=''; target=None
    def move(reg,addr,write=False):return f'{addr}={reg}' if write else f'{reg}={addr}'
    if h==1:return 1,'NOP',None
    if h==0x1901:return 1,'RFRAME',None
    if h>>12==12:return 1,shortcompute(h),None
    if h>>12==9 and h&16:
        return 1,move(f'R{h&15}',f'DM(I{(h>>9)&7},M{(h>>6)&7})',bool(h&32)),None
    if top in (10,11) and h&128:
        return 1,f'IF {COND[(h>>1)&31]} '+('RTS' if top==10 else 'RTI')+(' (DB)' if h&64 else ''),None
    if h>>12==9 and (h>>2)&7==2:
        size=2; w=(h<<16)|mem[pc+1]
        reg=ureg((w>>7)&127);addr=f'{"PM" if h&32 else "DM"}({sx(w&127,7):+d},I{((h>>9)&7)+(8 if h&32 else 0)})'
        return size,move(reg,addr,bool(h&256)),None
    if h>>12==9 and (h>>2)&7==1:
        return 2,f'{"PM" if h&32 else "DM"}(I{((h>>9)&7)+(8 if h&32 else 0)},M{((h>>6)&7)+(8 if h&32 else 0)})={sx(mem[pc+1],16)}',None
    if top==15 and h&128:return 2,f'{ureg(h&127)}={sx(mem[pc+1],16):#x}',None
    if top==1 and h&128:return 2,compute(((h&127)<<16)|mem[pc+1]),None
    can_short=(h>>13 in (1,2) or h>>12 in (6,7) or top in (4,8,9))
    if can_short and mem[pc+1]&127==63:size=2
    w=(h<<32)|(mem[pc+1]<<16)|(mem[pc+2] if size==3 else 0)
    c=w&0x7fffff if size==3 else 0
    cond=(w>>33)&31; prefix='' if cond==31 else f'IF {COND[cond]} '
    comp=compute(c)
    if h>>13==2:
        g=(w>>32)&1;i=((w>>41)&7)+8*g;m=((w>>38)&7)+8*g
        args=f'I{i},M{m}' if w&(1<<44) else f'M{m},I{i}'
        desc=move(ureg((w>>23)&127),f'{"PM" if g else "DM"}({args})',bool(w&(1<<31)))
    elif h>>12==7:
        if w&(1<<43):desc=f'SWAP R{(w>>23)&15},S{(w>>38)&15}'
        else:desc=f'{ureg((w>>23)&127)}={ureg((((w>>38)&31)<<2)|((w>>31)&3))}'
    elif h>>12==6:
        g=(w>>40)&1;i=((w>>41)&7)+8*g;imm=sx((w>>27)&63,6)
        args=f'I{i},{imm:+d}' if w&(1<<38) else f'{imm:+d},I{i}'
        desc=move(f'R{(w>>23)&15}',f'{"PM" if g else "DM"}({args})',bool(w&(1<<39)))
    elif top==4:
        g=(w>>38)&1;i=((w>>30)&7)+8*g;m=((w>>27)&7)+8*g
        dest=i^((w>>24)&7)
        desc=f'I{dest}=MODIFY(I{i},M{m})'
    elif top in (6,7):
        dest=w&0xffffff;target=pc+sx(dest,24) if top==7 else dest
        desc=f'{"CALL" if w&(1<<39) else "JUMP"} {target:#x}'+(' (DB)' if w&(1<<26) else '')
        comp=''
    elif top in (8,9):
        if top==9:target=pc+sx((w>>27)&63,6);dest=hex(target)
        else:dest=f'(M{8+((w>>27)&7)},I{8+((w>>30)&7)})'
        desc=f'{"CALL" if w&(1<<39) else "JUMP"} {dest}'+(' (DB)' if w&(1<<26) else '')
    elif top in (12,13):
        target=pc+sx(w&0xffffff,24)
        return size,f'LCNTR={ureg((w>>32)&127) if top==13 else str((w>>24)&65535)}, DO {target:#x} UNTIL LCE',target
    elif top==15:return size,f'{ureg((w>>32)&127)}={w&0xffffffff:#x}',None
    elif h>>12==9:
        g=(w>>37)&1
        return size,f'{"PM" if g else "DM"}(I{((w>>41)&7)+8*g},M{((w>>38)&7)+8*g})={w&0xffffffff:#x}',None
    elif h>>13==5:
        g=(w>>44)&1
        return size,move(ureg((w>>32)&127),f'{"PM" if g else "DM"}({sx(w&0xffffffff,32):+d},I{((w>>41)&7)+8*g})',bool(w&(1<<40))),None
    elif top in (10,11):
        desc=('RTS' if top==10 else 'RTI')+(' (DB)' if w&(1<<26) else '')
    elif top&252==16:
        return size,move(ureg((w>>32)&127),f'{"PM" if w&(1<<41) else "DM"}({w&0xffffffff:#x})',bool(w&(1<<40))),None
    elif top==20:
        return size,f'BIT { ["SET","CLR","TGL","?","TST","XOR","?","?"][(w>>37)&7]} {ureg(0x70+((w>>32)&15))} {w&0xffffffff:#x}',None
    elif top==22:
        i=(w>>32)&7;g=(w>>38)&1;dest=i^((w>>35)&7)
        return size,f'I{dest+8*g}=MODIFY(I{i+8*g},{sx(w&0xffffffff,32):+d})',None
    elif top==24:
        target=pc+sx(w&0xffffff,24) if w&(1<<38) else w&0xffffff
        return size,f'CJUMP {target:#x} (DB); R2=I6, I6=I7',target
    elif top==1:desc=''
    elif top==2 or h>>12==8:
        op=(w>>16)&63;n=(w>>4)&15;x=w&15;v=sx((w>>8)&255,8)
        if op in (0x10,0x11,0x12,0x13):
            data=(((w>>27)&15)<<8)|((w>>8)&255)
            desc=f'R{n}={"FEXT" if op%2==0 else "FDEP"} R{x} BY {data&63}:{data>>6}'+(' (SE)' if op&2 else '')
        elif op in (8,9):desc=f'R{n}=R{n} OR {"LSHIFT" if op==8 else "ASHIFT"} R{x} BY {v}'
        elif op==0x33:desc=f'BTST R{x} BY {v}'
        else:desc=f'R{n}={ {0:"LSHIFT",1:"ASHIFT",2:"ROT",0x30:"BSET",0x31:"BCLR",0x32:"BTGL"}.get(op,"SHIFT_UNKNOWN_"+hex(op))} R{x} BY {v}'
        comp=''
        if h>>12==8:
            g=(w>>32)&1
            desc+=' || '+move(f'R{(w>>23)&15}',f'{"PM" if g else "DM"}(I{((w>>41)&7)+8*g},M{((w>>38)&7)+8*g})',bool(w&(1<<31)))
    elif h>>13==1:
        desc=move(f'R{(w>>33)&15}',f'DM(I{(w>>41)&7},M{(w>>38)&7})',bool(w&(1<<44)))+', '+move(f'R{(w>>23)&15}',f'PM(I{8+((w>>30)&7)},M{8+((w>>27)&7)})',bool(w&(1<<37)))
        prefix=''
    elif top==0 and w==0:return 3,'NOP48',None
    else:raise ValueError(f'unsupported {pc:#x}: {w:012x}')
    return size,prefix+' || '.join(v for v in (comp,desc) if v),target

def listing(mem,start,end):
    pc=start
    while pc<end:
        n,s,t=decode(mem,pc)
        yield f'{pc:08x}  {" ".join(f"{mem[pc+i]:04x}" for i in range(n)):<15} {s}'
        pc+=n
    if pc!=end:raise ValueError(f'end {end:#x} is inside instruction ending at {pc:#x}')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('loader',type=Path);p.add_argument('start',type=lambda x:int(x,0));p.add_argument('end',type=lambda x:int(x,0))
    a=p.parse_args()
    for line in listing(load_shortwords(a.loader.read_bytes()),a.start,a.end):print(line)
