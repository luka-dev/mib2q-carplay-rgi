"""Bounded first-B log/pow library formulas with firmware table provenance.

124393 natural log ->12D80B log10, and actual1257E6 positive-base pow path.
Not exp(log(x)) substituted for pow. RECIPS/Newton divisions are represented
by ideal division and arithmetic by float64, NOT SHARC40-bit/flags/ROM seeds.
MODE1 FIX rounding is an explicit caller hypothesis; no live state inferred.
"""
import copy
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
PROOFS = (
    (0x124393,0x12440b,'4156768d627eaad2ae7407c871bce2086c1219583ab8a6c4d0d0de1759f9db7f'),
    (0x12d80b,0x12d844,'21cac447dae589b6e1eb2e9b65bffe805ae9f9b49710d7c91f25869099424b10'),
    (0x1257e6,0x125949,'ee74f89772e7e5149bb7483ef2eb6774219532e7186f1d957195f739a5968d1c'),
    (0x125db3,0x125dc4,'a9b5e66ff7d501cf3fceebf5ab68babb710e95c4a187fa8bcbf9e1ee0218a1e1'),
)
PINS = {
    0x1243a0:'I4=0xb9393',0x1243a3:'R1=LOGB F4',
    0x1243a9:'F12=SCALB F12 BY R4 || R0=DM(I4,M6)',
    0x1243ac:'COMP(F12,F8) || R8=DM(I4,M6)',
    0x1243cf:'F0=F2*F2 || R1=DM(I4,M6)',
    0x1243d2:'F12=F1+F0 || R1=DM(I4,M6)',
    0x1243d5:'F4=F1*F0 || R1=DM(I4,M6)',
    0x1243f4:'F4=F0*F4, F0=F8+F12',
    0x12d824:'R14=0x3ede5bd9',0x12d833:'F2=F0*F14',
    0x12580b:'I4=0xb939b',0x125844:'I4=0xb93ad',0x125856:'I4=0xb93b6',
    0x125835:'R14=R14+1 || R4=R14',
    0x125871:'F2=FLOAT R2 BY R3',0x125875:'R8=FIX F1 BY R4',
    0x125877:'F8=FLOAT R8 BY R3',0x125881:'R14=FIX F15 BY R4',
    0x12588b:'R14=FIX F12 BY R4',0x125893:'R12=FIX F13 BY R4',
    0x125899:'R10=FIX F10 BY R4',0x1258b8:'R6=ABS R10',
    0x1258c5:'R6=R6-R10 || R5=DM(+6,I4)',
    0x1258de:'F4=F4*F13 || R5=DM(M4,I4)',
    0x1258e3:'F0=SCALB F4 BY R7 || I12=DM(M7,I6)',
}


def _finite(values):
    if np.iscomplexobj(values):
        raise ValueError('real numerical inputs required')
    result = np.asarray(values,dtype=np.float64)
    if not np.isfinite(result).all():
        raise ValueError('finite numerical inputs required')
    return result


def _floats(words):
    return np.array([struct.unpack('<f',struct.pack('<I',w))[0] for w in words])


def load_math_tables(loader=LOADER):
    sys.path.insert(0,str(ROOT/'tools'))
    from extract_amp_sharc_app import BITREV_SHA256,SOURCE_SHA256,parse_blocks
    from audit_amp_output_bindings import loader_dm
    from inspect_amp_visa import load_shortwords,decode
    raw=Path(loader).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=BITREV_SHA256:
        raise ValueError('exact audited SHARC loader required')
    dm=loader_dm(raw,parse_blocks(raw));mem=load_shortwords(raw)
    for start,end,wanted in PROOFS:
        actual=hashlib.sha256(struct.pack(f'<{end-start}H',*[mem[p] for p in range(start,end)])).hexdigest()
        if actual!=wanted:raise ValueError(f'B math code proof differs at{start:x}')
    for pc,wanted in PINS.items():
        if decode(mem,pc)[1]!=wanted:raise ValueError(f'B math instruction differs at{pc:x}')
    tables={};records={}
    for name,address,count in (('log',0xb9393,8),('pow_anchors',0xb939b,18),
                               ('pow_corrections',0xb93ad,9),('pow_polynomial',0xb93b6,8)):
        words=[dm[address+i] for i in range(count)]
        tables[name]=_floats(words)
        records[name]={'address':hex(address),'words':words,
            'sha256':hashlib.sha256(struct.pack(f'<{count}I',*words)).hexdigest()}
    tables['log10_factor']=_floats([0x3ede5bd9])[0]
    tables['pow_integer_bounds']=(struct.unpack('<i',struct.pack('<I',dm[0xb93bf]))[0],dm[0xb93be])
    tables['metadata']={'source_sha256':SOURCE_SHA256,'loader_sha256':BITREV_SHA256,
        'tables':records,'log10_factor_word':'0x3ede5bd9',
        'pow_integer_bounds':list(tables['pow_integer_bounds']),
        'code_ranges':[{'start':hex(a),'end_exclusive':hex(b),'sha256':h} for a,b,h in PROOFS],
        'instruction_pins':{hex(k):v for k,v in PINS.items()},
        'instruction_reference':'local docs/papers/sharc_programming_rev2.4.txt tables12-3/4; scaled FIX/FLOAT §11-37..39',
        'address_units':'SHARC shortword code PCs;32-bit DM words'}
    return tables


class BMathReference:
    """Source-derived finite log10/10**x formulas; explicit FIX rounding.

Log inputs: positive normal-float32 domain, represented here as float64.
Pow exponent range: [-37,37], excludes firmware errno/overflow/underflow and
negative-base branches. No implicit rounding of inputs to float32, no output
normalization, and no claim to emulate the entire firmware arithmetic chain.
"""
    def __init__(self,*,fix_rounding,loader=LOADER):
        if fix_rounding not in ('nearest','floor'):
            raise ValueError('explicit FIX rounding nearest or floor required')
        self.fix_rounding=fix_rounding
        self.tables=load_math_tables(loader)

    def _fix16(self,value):
        # ADI FIX BY4 scales by16; MODE1 TRUNC=1 rounds toward -infinity,
        # whereas the separate TRUNC instruction rounds toward zero.
        x=value*16
        result=np.rint(x) if self.fix_rounding=='nearest' else np.floor(x)
        if np.any((result<-(2**31))|(result>2**31-1)):
            raise ValueError('scaled FIX overflow outside bounded reference')
        return result.astype(np.int64)

    def natural_log(self,values):
        x=_finite(values)
        if np.any((x<np.finfo(np.float32).tiny)|(x>np.finfo(np.float32).max)):
            raise ValueError('positive normal-float32 log domain required')
        c=self.tables['log'];mantissa,exponent=np.frexp(x)
        high=mantissa>c[0]
        numerator=np.where(high,mantissa-2*c[1],mantissa-c[1])
        denominator=np.where(high,mantissa*c[1]+c[1],numerator*c[1]+c[1])
        exponent=exponent-np.logical_not(high)
        z=numerator/denominator  #1243C1..CD RECIPS/refinement mathematical limit.
        square=z*z
        rational=(c[4]*square+c[5])/(square+c[3])
        reduced=z+(z*(rational*square))
        return (exponent*c[6]+reduced)+exponent*c[7]

    def log10(self,values):
        return self.natural_log(values)*self.tables['log10_factor']

    def pow10_trace(self,exponents):
        """Actual1257E6 path for base10, retaining inspectable range splits."""
        y=_finite(exponents)
        if np.any((y < -37)|(y > 37)):
            raise ValueError('pow10 exponent outside bounded normal-output range[-37,37]')
        anchors=self.tables['pow_anchors'];correction=self.tables['pow_corrections']
        p=self.tables['pow_polynomial']
        #125804..125855: positive-base logarithm around an even-index anchor.
        mantissa,exponent=np.frexp(10.)
        index=1
        compare=anchors[5]
        if anchors[9]>=mantissa:index=9;compare=anchors[13]
        next_compare=anchors[index+2]
        if compare>=mantissa:next_compare=anchors[index+6];index+=4
        if next_compare>=mantissa:index+=2
        anchor_index=index+1
        anchor=anchors[anchor_index]
        z=2*((mantissa-anchor)-correction[anchor_index//2])/(anchor+mantissa)
        square=z*z
        cubic=((square*p[1]+p[0])*square)*z
        low_log=(cubic+cubic*p[7])+z*p[7]
        low_log+=z
        high_log=(int(exponent)*16-index)/16
        #125875..125899: successive SIXTEENTH splits, not one pow/exp call.
        exponent16=self._fix16(y)/16
        residual_exponent=y-exponent16
        partial=high_log*residual_exponent+low_log*y
        partial16=self._fix16(partial)/16
        residual=partial-partial16
        upper=partial16+high_log*exponent16
        upper16=self._fix16(upper)/16
        residual=(upper-upper16)+residual
        carry16=self._fix16(residual)/16
        integer16=self._fix16(carry16+upper16)
        residual=residual-carry16
        lower_bound,upper_bound=self.tables['pow_integer_bounds']
        if np.any((integer16<=lower_bound)|(integer16>=upper_bound)):
            raise ValueError('pow errno branch outside bounded reference')
        positive=residual>0
        residual=np.where(positive,residual-1/16,residual)
        integer16=integer16+positive
        binary_exponent=np.where(integer16<0,-(np.abs(integer16)//16),integer16//16+1)
        table_index=16*binary_exponent-integer16+1
        if np.any((table_index<1)|(table_index>17)):
            raise ValueError('pow reconstruction table index outside1..17')
        polynomial=np.full(y.shape,p[6])
        for coefficient in (p[5],p[4],p[3],p[2]):
            polynomial=polynomial*residual+coefficient
        polynomial*=residual
        scale=anchors[table_index]
        out=np.ldexp(polynomial*scale+scale,binary_exponent.astype(np.int32))
        if not np.isfinite(out).all() or np.any(out<=0):
            raise ValueError('nonfinite/nonpositive pow reference result')
        return {'out':out,'base_anchor_index':anchor_index,'base_log2_high':high_log,
                'base_log2_low':low_log,'exponent_sixteenth':exponent16,
                'integer_sixteenths':integer16,'residual':residual,
                'reconstruction_index':table_index,'binary_exponent':binary_exponent}

    def pow10(self,exponents):
        return self.pow10_trace(exponents)['out']

    def metadata(self):
        result=copy.deepcopy(self.tables['metadata'])
        result.update(kind='firmware_library_float64_reference',fix_rounding=self.fix_rounding,
            fix_rounding_live_verified=False,
            domain={'log':'positive normal-float32 domain','pow10_exponent':[-37,37]},
            hardware_bit_exact=False,actual_vehicle_state_proven=False,
            precision_boundary='storedfloat32 constants, float64 operations and ideal division; no RECIPS seed-ROM, SHARC40-bit, intermediate stores or exceptional/errno emulation',
            pow_algorithm='1257E6 anchor/correction tables + sixteenth splits + exp2 polynomial, NOT natural_exp substitution')
        return result
