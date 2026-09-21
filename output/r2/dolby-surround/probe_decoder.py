"""Run isolated original x64 math kernels in Unicorn; no Windows/OS calls.

This probes synthetic subband inputs, not the full DLL or a Windows profile.
Run from any directory. Requires unicorn and numpy.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import numpy as np
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
from unicorn.x86_const import *

OUT = Path(__file__).resolve().parent
DLL = OUT.parents[2] / 'Dolby/DolbyAudioProcessing.dll'
EXPECTED = '0fada40251880d03c657a357745d362799c93fb6024f733f1c8bf1057f5e4715'


class Machine:
    def __init__(self):
        data = DLL.read_bytes()
        assert hashlib.sha256(data).hexdigest() == EXPECTED
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        opt = pe + 24
        self.base = struct.unpack_from('<Q', data, opt + 24)[0]
        image_size = struct.unpack_from('<I', data, opt + 56)[0]
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(self.base, (image_size + 4095) & ~4095)
        sections = opt + struct.unpack_from('<H', data, pe + 20)[0]
        for n in range(struct.unpack_from('<H', data, pe + 6)[0]):
            pos = sections + 40*n
            _, rva, size, raw = struct.unpack_from('<IIII', data, pos + 8)
            if size:
                self.uc.mem_write(self.base + rva, data[raw:raw+size])
        self.uc.mem_map(0x50000000, 0x1000000)
        self.uc.mem_map(0x60000000, 0x100000)
        self.next = 0x50000000
        self.uc.reg_write(UC_X86_REG_MXCSR, 0x1f80)

    def alloc(self, size=4096, payload=b''):
        address = self.next
        self.next += (max(size, len(payload)) + 63) & ~63
        if payload:
            self.uc.mem_write(address, payload)
        return address

    def read(self, address, fmt):
        return struct.unpack(fmt, self.uc.mem_read(address, struct.calcsize(fmt)))

    def floats(self, values):
        return self.alloc(payload=np.asarray(values, dtype='<f4').tobytes())

    def call(self, address, *args):
        sp = 0x600ff000 - 8
        self.uc.mem_write(sp, struct.pack('<Q', 0x60000000) + bytes(256))
        self.uc.reg_write(UC_X86_REG_RSP, sp)
        for reg, value in zip((UC_X86_REG_RCX, UC_X86_REG_RDX,
                               UC_X86_REG_R8, UC_X86_REG_R9), args):
            self.uc.reg_write(reg, value)
        for n, value in enumerate(args[4:]):
            self.uc.mem_write(sp + 0x28 + 8*n, struct.pack('<Q', value))
        self.uc.emu_start(address, 0x60000000, count=2000000)
        assert self.uc.reg_read(UC_X86_REG_RIP) == 0x60000000, 'instruction budget exhausted'
        return self.uc.reg_read(UC_X86_REG_RAX)


def tables(m, va):
    inp, out, gain, pairs, desc = m.read(va, '<IIfIQ')
    pair, a, b, route = m.read(desc, '<QIIQ')
    n, mode = m.read(pair, '<II')
    coeff, targets, bounds, sectors = m.read(pair+8, '<QQQI')
    phase = m.read(pair+0x28, '<Q')[0]
    mixing = m.read(pair+0x40, '<Q')[0]
    return dict(va=hex(va), pair=hex(pair), input_count=inp, output_count=out,
                output_gain=gain, local_count=n, analysis_pair=[a,b], mode=mode,
                route=list(m.read(route, '<'+'I'*n)),
                bounds=list(m.read(bounds, '<'+'I'*sectors)),
                targets=np.array(m.read(targets, '<'+'I'*(2*sectors))).reshape(-1,2).tolist(),
                coefficients=np.array(m.read(coeff, '<'+'f'*(4*sectors))).reshape(-1,4).tolist(),
                phase=list(m.read(phase, '<'+'i'*n)),
                residual_matrix_2=np.array(m.read(mixing, '<'+'f'*(2*n))).reshape(-1,2).tolist())


def coefficients(m, config, S, D, C):
    ps,pd,pc = [m.floats(v) for v in (S,D,C)]
    indices, result = m.alloc(),m.alloc()
    m.call(0x18004e1c0, pd,pc,indices)
    m.call(0x18004e2c0, result,int(config['pair'],16)+8,ps,pd,pc,indices)
    idx = np.array(m.read(indices,'<20i'))
    l = np.array(m.read(result,'<40f')).reshape(20,2)
    r = np.array(m.read(result+0xa0,'<40f')).reshape(20,2)
    residual = np.array(m.read(result+0x140,'<60f')).reshape(20,3)
    targets = np.array(m.read(result+0x230,'<40i')).reshape(20,2)
    epsilon = m.read(0x18032e880,'<f')[0]
    u = C / (2*(S+epsilon))
    v = D / (4*(S+epsilon))
    radius = np.sqrt(u*u+v*v)
    sector = np.searchsorted(config['bounds'],idx,side='right')
    t = np.array(config['coefficients'])[sector]
    model_l = np.column_stack((t[:,0]*(radius+v)+t[:,2]*u,
                               t[:,1]*(radius+v)+t[:,3]*u))
    model_r = np.column_stack((t[:,0]*u+t[:,2]*(radius-v),
                               t[:,1]*u+t[:,3]*(radius-v)))
    model_res = np.column_stack((.5-radius-v,.5-radius+v,-u))
    for actual,model in ((l,model_l),(r,model_r),(residual,model_res)):
        np.testing.assert_allclose(actual,model,atol=2e-7,rtol=3e-6)
    np.testing.assert_array_equal(targets,np.array(config['targets'])[sector])
    return result,idx,l,r,residual,targets


def apply_case(m, config, L, R, S, D, C):
    arrays = [np.full(20,x,dtype=np.float64) for x in (S,D,C)]
    result,idx,l,r,res,targets = coefficients(m,config,*arrays)
    n = config['local_count']
    # One complex bin per group; this also exercises the scalar tail.
    left=m.floats([L.real,L.imag]*20)
    right=m.floats([R.real,R.imag]*20)
    outputs=[m.alloc() for _ in range(n)]
    output_ptr=m.alloc(payload=struct.pack('<'+'Q'*n,*outputs))
    residuals=[m.alloc() for _ in range(5)]
    residual_ptr=m.alloc(payload=struct.pack('<5Q',*residuals))
    bounds=m.alloc(payload=struct.pack('<20I',*range(1,21)))
    m.call(0x18004e510,output_ptr,residual_ptr,left,right,
           result,result+0xa0,result+0x140,result+0x230,n,20,bounds)
    def get_values(addresses):
        return [complex(*m.read(a,'<2f')) for a in addresses]
    direct=get_values(outputs)
    residual=get_values(residuals[:2])
    model=np.zeros(n,dtype=complex)
    for j in range(2): model[targets[0,j]]=16*(l[0,j]*L+r[0,j]*R)
    np.testing.assert_allclose(direct,model,atol=3e-6)
    np.testing.assert_allclose(residual,[2*(res[0,0]*L+res[0,2]*R),
                                        2*(res[0,2]*L+res[0,1]*R)],atol=3e-6)
    phase_ptr=m.read(int(config['pair'],16)+0x28,'<Q')[0]
    m.call(0x18004eb70,output_ptr,n,phase_ptr,20)
    rotated=get_values(outputs)
    np.testing.assert_allclose(rotated,np.array(direct)*np.exp(1j*np.pi/2*np.array(config['phase'])),atol=3e-6)
    def pairs(values):return [[round(z.real,8),round(z.imag,8)] for z in values]
    return dict(direction_index=int(idx[0]),local_targets=targets[0].tolist(),
                direct=pairs(direct),residual=pairs(residual),after_phase=pairs(rotated))


def delay_probe():
    m=Machine()
    a,b,c=[np.array(m.read(x,'<77B')) for x in
           (0x180338220,0x180338280,0x180338340)]
    strides=m.read(0x1803382e0,'<77B')
    # The original ring cursor is decremented AFTER the reads: delays are tap+1.
    expected=[[a+1,c+1,b+1],[b+1,a+1,c+1]]
    for source in range(2):
        state=m.alloc(0x3400)
        buf=[m.alloc() for _ in range(5)]
        ptr=m.alloc(payload=struct.pack('<5Q',*buf))
        observed=np.zeros((3,77),dtype=int)
        for frame in range(33):
            for ch in range(2):
                x=np.zeros(154,dtype='<f4')
                if frame==0 and ch==source:x[::2]=1
                m.uc.mem_write(buf[ch],x.tobytes())
            m.call(0x18005a9f0,state,ptr)
            for j in range(3):
                x=np.array(m.read(buf[j+2],'<154f')).reshape(77,2)
                np.testing.assert_array_equal(x[:,1],0)
                hits=np.flatnonzero(x[:,0])
                if len(hits):
                    np.testing.assert_array_equal(x[hits,0],1)
                    assert not observed[j,hits].any()
                    observed[j,hits]=frame
        np.testing.assert_array_equal(observed,np.array(expected[source]))
    return {'tested':'2 input impulses × 3 delayed outputs × 77 bins',
            'delay_a_frames':(a+1).tolist(),'delay_b_frames':(b+1).tolist(),
            'delay_c_frames':(c+1).tolist(),'ring_strides':list(strides),
            'delay_unit':'calls to 0x18005a9f0; milliseconds not established'}


def statistics_probe():
    m=Machine()
    configs=[]
    for root in (0x1802e89c0,0x1802e9520):
        bounds_ptr=m.read(root+0x50,'<Q')[0]
        bounds=np.array(m.read(bounds_ptr,'<20I'))
        info=m.read(root+0x98,'<Q')[0]
        alpha_ptr,beta_bits=m.read(info,'<QI')
        alpha=np.array(m.read(alpha_ptr,'<20f'))
        state=m.alloc()
        rng=np.random.default_rng(384)
        model=np.zeros((3,20))
        for frame in range(2):
            z=rng.normal(size=(2,4,77,2)).astype('<f4')
            ptrs=[m.alloc(payload=struct.pack('<4Q',*[m.floats(slot.ravel()) for slot in channel])) for channel in z]
            m.call(0x18004d0b0,state,ptrs[0],ptrs[1],bounds_ptr,alpha_ptr,beta_bits,4)
            el,er=(np.sum(channel.astype(float)**2,axis=(0,2)) for channel in z)
            cross=np.sum(z[0].astype(float)*z[1],axis=(0,2))
            sums=np.array([[x[start:end].sum() for start,end in zip(np.r_[0,bounds[:-1]],bounds)]
                           for x in (el+er,el-er,cross)])
            model=(1-alpha)*model+alpha*sums
            actual=np.array(m.read(state,'<60f')).reshape(3,20)
            np.testing.assert_allclose(actual,model,rtol=2e-5,atol=2e-6)
        hz_ptr=m.read(root+0x48,'<Q')[0]
        configs.append({'config':hex(root),'header':list(m.read(root,'<4I')),
                        'bounds':bounds.tolist(),'frequency_values':list(m.read(hz_ptr,'<20I')),
                        'alpha':alpha.tolist(),'beta':m.read(info+8,'<f')[0]})
    return configs


def full_pair_probe(config):
    """Original pair synthesis including residual mix; injected stationary statistics."""
    m=Machine()
    state=m.alloc(0x7000)
    result,*_=coefficients(m,config,*[np.full(20,x) for x in (2.,0.,0.)])
    params=m.alloc()
    # The return layout of pair_analysis has a 16-byte header before coefficients.
    m.uc.mem_write(params+16,bytes(m.uc.mem_read(result,0x2d0)))
    pair=int(config['pair'],16)
    matrix=m.read(pair+0x40,'<Q')[0]
    m.uc.mem_write(params,struct.pack('<IIQ',0,2,matrix))
    left=m.floats([1,0]*77)
    right=m.floats([0,1]*77)
    n=config['local_count']
    outputs=[m.alloc() for _ in range(n)]
    ptr=m.alloc(payload=struct.pack('<'+'Q'*n,*outputs))
    scratch=m.alloc(0x10000)
    m.call(0x180031860,state,0x1802e8c20,pair,params,left,right,ptr,scratch,0,0,0,0)
    actual=np.array([m.read(p,'<154f') for p in outputs]).reshape(n,77,2)
    expected=np.asarray(config['residual_matrix_2'])[:,None,:].repeat(77,axis=1)
    np.testing.assert_allclose(actual,expected,atol=3e-6)
    return {'config':config['va'],'tested':'77 complex bins, stationary residual-only quadrature input',
            'output_complex':actual[:,0,:].tolist(),'full_pair_original_code':True,
            'note':'Injected two-residual mode; excludes outer gain and time-domain filterbanks'}


def transient_probe():
    m=Machine()
    pair=0x1802eae80
    root=0x1802ea1a0  # header 48000, 48000, 256, 20
    bounds=m.read(root+0x50,'<Q')[0]
    limits=np.array(m.read(bounds,'<20I'))
    widths=np.diff(np.r_[0,limits])
    info=m.read(root+0x98,'<Q')[0]
    beta=m.read(info+8,'<f')[0]
    state=m.alloc(0x7000)
    baseline=8*widths.astype(float)  # four slots; unit-magnitude L and R
    m.uc.mem_write(state,baseline.astype('<f4').tobytes())
    m.uc.mem_write(state+0xf0,baseline.astype('<f4').tobytes())
    scratch=m.alloc(0x10000)
    left=[m.alloc() for _ in range(4)]
    right=[m.alloc() for _ in range(4)]
    lp=m.alloc(payload=struct.pack('<4Q',*left))
    rp=m.alloc(payload=struct.pack('<4Q',*right))
    A=np.array(m.read(m.read(pair+0x38,'<Q')[0],'<25f')).reshape(5,5)
    B=np.array(m.read(m.read(pair+0x50,'<Q')[0],'<25f')).reshape(5,5)
    rho=m.read(pair+0x48,'<f')[0]
    rows=[]
    for amplitude in [1,4,1,1,1]:
        for ptr in left:m.uc.mem_write(ptr,np.array([amplitude,0]*77,dtype='<f4').tobytes())
        for ptr in right:m.uc.mem_write(ptr,np.array([0,amplitude]*77,dtype='<f4').tobytes())
        output=m.call(0x180031410,state,info,bounds,pair,0,1,1,lp,rp,4,scratch)
        flag,n,matrix=m.read(output,'<IIQ')
        assert flag==1 and n==5
        w=np.array(m.read(state+0x140,'<20f'))
        actual=np.array(m.read(matrix,'<500f')).reshape(20,5,5)
        s=np.sqrt(1-w*w)
        scale=1/np.sqrt(1+2*rho*w*s)
        expected=(w[:,None,None]*B+s[:,None,None]*A)*scale[:,None,None]
        np.testing.assert_allclose(actual,expected,atol=2e-6)
        rows.append({'input_amplitude':amplitude,'w_first_band':float(w[0]),
                     'matrix_first_band':actual[0].tolist()})
    np.testing.assert_allclose([r['w_first_band'] for r in rows],[0,1,beta,beta**2,beta**3],atol=2e-6)
    return {'config':hex(root),'pair':hex(pair),'beta':beta,'rho':rho,
            'tested':'Original pair_analysis; steady quadrature → +12.04 dB energy burst → steady',
            'matrix_steady':A.tolist(),'matrix_transient':B.tolist(),'frames':rows}


def transient_detector_probe():
    m=Machine()
    root=0x1802ea1a0
    bounds_ptr=m.read(root+0x50,'<Q')[0]
    bounds=np.array(m.read(bounds_ptr,'<20I'))
    baseline=8*np.diff(np.r_[0,bounds]).astype(float)
    info=m.read(root+0x98,'<Q')[0]
    alpha_ptr,beta_bits=m.read(info,'<QI')
    k1,k2=m.read(0x18032e838,'<2f')
    rows=[]
    for db in (0,2,3,4,6,8,9,10,12):
        state=m.alloc()
        m.uc.mem_write(state,baseline.astype('<f4').tobytes())
        m.uc.mem_write(state+0xf0,baseline.astype('<f4').tobytes())
        amplitude=10**(db/20)
        left=[m.floats([amplitude,0]*77) for _ in range(4)]
        right=[m.floats([0,amplitude]*77) for _ in range(4)]
        lp=m.alloc(payload=struct.pack('<4Q',*left))
        rp=m.alloc(payload=struct.pack('<4Q',*right))
        m.call(0x18004d0b0,state,lp,rp,bounds_ptr,alpha_ptr,beta_bits,4)
        observed=m.read(state+0x140,'<20f')
        x=np.float32(k1*10**(db/10))
        m.uc.reg_write(UC_X86_REG_XMM0,struct.unpack('<I',struct.pack('<f',x))[0])
        m.call(0x18003c040)
        log_value=struct.unpack('<f',struct.pack('<I',m.uc.reg_read(UC_X86_REG_XMM0)&0xffffffff))[0]
        expected=min(1,max(0,k2*log_value))
        np.testing.assert_allclose(observed,expected,atol=2e-6)
        rows.append({'energy_rise_db':db,'attack_weight':observed[0],
                     'ideal_log2_weight':min(1,max(0,k2*math.log2(float(x))))})
    return {'k1':k1,'k2':k2,'tested':'9 energy rises, 20 bands each; zero previous attack weight',
            'rows':rows,'note':'Uses original approximate log2, so 3/9 dB thresholds are approximate'}


def main():
    m=Machine()
    configs=[tables(m,va) for va in (0x1802eccc0,0x1802eccf0,0x1802ecd20,0x1802ecd50)]
    rng=np.random.default_rng(701)
    comparisons=0
    for config in configs:
        for _ in range(10):
            el,er=rng.uniform(.001,5,(2,20))
            c=rng.uniform(-1,1,20)*np.sqrt(el*er)
            coefficients(m,config,el+er,el-er,c)
            comparisons+=20
    cases={'left':(1+0j,0j,1,1,0),'right':(0j,1+0j,1,-1,0),
           'mono':(1+0j,1+0j,2,0,1),'antiphase':(1+0j,-1+0j,2,0,-1),
           'quadrature':(1+0j,1j,2,0,0)}
    experiments=[]
    for config in configs:
        experiments.append({'config':config['va'],
                            'cases':{name:apply_case(m,config,*args) for name,args in cases.items()}})
    report={'sha256':EXPECTED,'method':'Original x64 kernels in Unicorn, synthetic spectral inputs; not full DSP/Windows',
            'validated_band_config_cases':comparisons,'configurations':configs,'experiments':experiments,
            'delay_probe':delay_probe(),'statistics_probe':statistics_probe(),
            'full_pair_probes':[full_pair_probe(c) for c in configs],
            'transient_probe':transient_probe(),
            'transient_detector_probe':transient_detector_probe()}
    (OUT/'kernel-probes.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Validated',comparisons,'band/config cases and',len(configs)*len(cases),'spectral input probes;',
          '462 delay responses, two statistics configurations, four full-pair probes, transient matrix switching and 180 attack-detector cases.')
    print(json.dumps(experiments[0],indent=2))


if __name__=='__main__':main()
