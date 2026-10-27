"""Stored-table complex FFT reference for the fixed first-B 1024-real path.

12EF65 uses decimation in frequency, doubled PM strides and final bit-reversed
stores. This is its mathematical DIF factorization with an exact final radix4,
NOT an instruction emulator: SIMD scheduling, 40-bit registers and32-bit
intermediate stores are not reproduced. No analytic twiddles are made.
"""
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT/'assets/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
KERNEL_SHA256 = '66341298899d9deb79e5f3a6d725f45578c4cbddc9fb8e13a280946368c2eb4d'
PINS = {
    0x12e8d3: 'I14=0xe4754',
    0x12b5aa: 'R2=LSHIFT R2 BY -1',
    0x12b5b8: 'R14=LSHIFT R14 BY 1',
    0x12b5bf: 'CJUMP 0x12ef65 (DB); R2=I6, I6=I7',
    0x12ef85: 'R14=LSHIFT R10 BY 1 || DM(I7,M7)=R12',
    0x12ef91: 'R11=R11-1 || M12=R14',
    0x12efc3: 'F7=F4+F8, F1=F4-F8 || R4=DM(I2,M2)',
    0x12eff8: 'R14=LSHIFT R14 BY 1',
    0x12f000: 'R4=R4+R0 || M12=R14',
    0x12f00b: 'R0=LSHIFT R0 BY -1',
    0x12efb0: 'BIT SET MODE1 0x200000',
    0x12f071: 'F0=F0+F10, F10=F0-F10 || R12=DM(I2,M2)',
    0x12f074: 'F2=F2+F12, F12=F2-F12 || I5=R13',
    0x12f077: 'F12=F12*F14 || I4=R6',
    0x12f07a: 'F0=F0+F2, F2=F0-F2 || SWAP R12,S12',
    0x12f07d: 'I4=MODIFY(I4,+0)',
    0x12f080: 'F10=F10+F12, F12=F10-F12 || R6=I4',
    0x12f08f: 'BIT CLR MODE1 0x200000',
}


def load_complex_fft_table(loader=LOADER):
    """Load W512[0:256] from every SECOND complex entry at PM E4754.

The real wrapper doubles its table stride before calling the 512-complex
kernel. In DM-word units its first PM increment is4; every stage doubles it.
Table identity + instruction pins establish provenance, not bit equivalence.
"""
    sys.path.insert(0, str(ROOT/'tools'))
    from extract_amp_sharc_app import BITREV_SHA256, parse_blocks
    from audit_amp_output_bindings import loader_dm
    from inspect_amp_visa import load_shortwords, decode
    raw = Path(loader).read_bytes()
    if hashlib.sha256(raw).hexdigest() != BITREV_SHA256:
        raise ValueError('exact audited SHARC loader required')
    mem = load_shortwords(raw)
    words = [mem[p] for p in range(0x12ef65, 0x12f0b8)]
    if hashlib.sha256(struct.pack(f'<{len(words)}H', *words)).hexdigest() != KERNEL_SHA256:
        raise ValueError('complex FFT instruction range differs')
    for pc, expected in PINS.items():
        if decode(mem, pc)[1] != expected:
            raise ValueError(f'complex FFT instruction differs at {pc:x}')
    dm = loader_dm(raw, parse_blocks(raw))
    words = [dm[0xe4754+i] for i in range(1024)]
    values = np.asarray(words, dtype='<u4').view('<f4').astype(float)
    full = values[::2]+1j*values[1::2]
    return {'twiddles': full[::2].copy(), 'metadata': {
        'kind': 'stored_table_DIF_mathematical_reference', 'complex_size': 512,
        'loader_path': str(Path(loader).resolve()), 'loader_sha256': BITREV_SHA256,
        'table_base': '0xe4754', 'table_complex_index_stride': 2,
        'table_full_sha256': hashlib.sha256(struct.pack('<1024I', *words)).hexdigest(),
        'kernel_start': '0x12ef65', 'kernel_end_exclusive': '0x12f0b8',
        'kernel_sha256': KERNEL_SHA256,
        'instruction_pins': {hex(k): v for k,v in PINS.items()},
        'normalization': 'none; unscaled forward transform for BOTH real wrappers',
        'instruction_execution': False, 'hardware_bit_exact': False,
        'boundary': 'float64 DIF with exact final radix4; not SIMD schedule, SHARC40-bit/register flags or intermediate32-bit stores'}}


def table_fft(samples, twiddles):
    """Unscaled forward DIF FFT using a supplied N/2-entry half-circle table."""
    x = np.asarray(samples, dtype=complex)
    w = np.asarray(twiddles, dtype=complex)
    n = len(x)
    if (x.shape != (n,) or n < 4 or n & (n-1) or w.shape != (n//2,)
            or not np.isfinite(x).all() or not np.isfinite(w).all()):
        raise ValueError('finite power-of-two complex vector and half-circle table required')
    x = x.copy()
    span = n
    while span > 4:
        half = span//2
        blocks = x.reshape(-1, span)
        left, right = blocks[:, :half].copy(), blocks[:, half:].copy()
        blocks[:, :half] = left+right
        blocks[:, half:] = (left-right)*w[np.arange(half)*(n//span)]
        span //= 2
    #12F06A..12F08C performs the final four-point sums and ±j rotation
    #without PM twiddle loads. The supplied table cannot alter this final stage.
    blocks=x.reshape(-1,4)
    a=blocks[:,0]+blocks[:,2];b=blocks[:,1]+blocks[:,3]
    c=blocks[:,0]-blocks[:,2];d=-1j*(blocks[:,1]-blocks[:,3])
    blocks[:,0]=a+b;blocks[:,1]=a-b
    blocks[:,2]=c+d;blocks[:,3]=c-d
    indices = np.arange(n)
    reversed_indices = np.zeros(n, dtype=int)
    for _ in range(n.bit_length()-1):
        reversed_indices = 2*reversed_indices+(indices & 1)
        indices >>= 1
    return x[reversed_indices]
