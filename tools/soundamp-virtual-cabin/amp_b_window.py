"""Firmware-derived first-B analysis/synthesis tables, bounded host reference.

1024-point constructor130A1C + trig125C2B positive first-quadrant path only.
The stored polynomial and pi literal replace the ideal np.sin default. This
is NOT instruction/40-bit rounding or math-library RECIPS emulation. No live
allocated window is read and no default transform behavior is changed.
"""
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT / 'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
RANGES = (
    (0x130a1c, 0x130aeb, '571e04d9240903bf8e441c51f086a22bf0dbf70387a78907a8766841a7e34708'),
    (0x130aeb, 0x130b6d, '36f8da5a939dad4e60c27b918e2be0c0a561fc2299a06b449422d040035d40c0'),
    (0x125c2b, 0x125c9a, '134c974773ca0622a6b1eaa65bdf91215478ed4a54408287db901891b4911cb9'),
)
PINS = {
    0x130a36: 'R10=0x3f000000',
    0x130a39: 'F2=F15*F10 || DM(-2,I6)=R2',
    0x130a3c: 'F13=F2+F10 || DM(-7,I6)=R13',
    0x130a89: 'R8=R15', 0x130a8b: 'R4=0x40490fdb',
    0x130a97: 'F2=F2+F10 || I3=DM(M5,I5)', 0x130a9a: 'F4=F2*F7',
    0x125c2b: 'I4=0xb93c6', 0x125c5c: 'R2=FIX F4 || R1=DM(I4,+1)',
    0x125c7a: 'F12=F12*F12 || R4=DM(I4,+1)',
    0x125c7d: 'LCNTR=6, DO 0x125c83 UNTIL LCE',
    0x125c80: 'F4=F12*F4 || R2=DM(I4,+1)',
    0x125c83: 'F4=F2+F4', 0x125c86: 'F4=F12*F4',
    0x125c88: 'F4*=F8', 0x125c89: 'F12=F4+F8',
    0x125c8b: 'F0=F12*F7', 0x125c8d: 'F0=RND F0 || MODE1=M4',
    0x130b18: 'R15=R2-1 || DM(I7,M7)=R13',
    0x130b36: 'R2=DM(M4,I3)',
    0x130b48: 'R1=R1-1 || R2=DM(M4,I3)',
}


def _floats(words):
    return np.array([struct.unpack('<f', struct.pack('<I', word))[0] for word in words])


def _word_hash(words):
    return hashlib.sha256(struct.pack(f'<{len(words)}I', *words)).hexdigest()


def load_spectral_tables(loader=LOADER):
    """Return window1024, real-FFT packing twiddles256 and JSON provenance.

Constructor angles are evaluated in float64 from literal float32(pi)/1024.
All512 positive angles have range-reduction index0 whether FIX is nearest or
truncate; sign=+1 and no quadrant changes. Evaluate the exact stored Horner
polynomial with float64 intermediates, then float32-store/mirror. SHARC
intermediate rounding and division helper precision remain an explicit gap.
NumPy's internal complex FFT still uses its own ideal transform primitive;
these twiddles bind only recovered real pre/postpacking, not that kernel.
"""
    sys.path.insert(0, str(ROOT / 'tools'))
    from extract_amp_sharc_app import SOURCE_SHA256, BITREV_SHA256, parse_blocks
    from audit_amp_output_bindings import loader_dm
    from inspect_amp_visa import load_shortwords, decode
    path = Path(loader)
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != BITREV_SHA256:
        raise ValueError('exact audited SHARC loader required')
    dm = loader_dm(raw, parse_blocks(raw)); mem = load_shortwords(raw)
    for start, end, wanted in RANGES:
        digest = hashlib.sha256(struct.pack(f'<{end-start}H', *[mem[p] for p in range(start, end)])).hexdigest()
        if digest != wanted:
            raise ValueError(f'window code proof differs at {start:x}')
    for pc, wanted in PINS.items():
        if decode(mem, pc)[1] != wanted:
            raise ValueError(f'window instruction differs at {pc:x}')
    trig_words = [dm[0xb93c6+i] for i in range(14)]
    trig = _floats(trig_words)
    pi = _floats([0x40490fdb])[0]
    theta = (np.arange(512, dtype=float)+.5)*(pi/1024)
    reduced_index = theta*trig[2]
    if (not np.all((0 < reduced_index) & (reduced_index < .5))
            or not np.all((trig[6] < theta) & (theta < trig[1])) or trig[0]*2 != 1):
        raise ValueError('outside proven positive first-quadrant polynomial path')
    square = theta*theta
    polynomial = np.full(512, trig[7])
    for coefficient in trig[8:14]:
        polynomial = polynomial*square+coefficient
    half = (theta+(polynomial*square)*theta).astype(np.float32).astype(np.float64)
    window = np.concatenate((half, half[::-1]))
    packing_words = [dm[0xe4754+i] for i in range(512)]
    packing = _floats(packing_words)
    twiddles = packing[::2]+1j*packing[1::2]
    metadata = {'kind': 'firmware_polynomial_window_and_stored_real_fft_packing_twiddles',
        'source_sha256': SOURCE_SHA256, 'loader_sha256': BITREV_SHA256, 'loader_path': str(path.resolve()),
        'frame_size': 1024, 'hop_size': 512, 'half_window_count': 512,
        'window_layout': 'first512 generated values followed by their reverse;130AEB',
        'window_formula': 'theta=(i+.5)*float32(pi)/1024; theta+theta^3*Horner(theta^2)',
        'pi_word': '0x40490fdb', 'trig_constants_base': '0xb93c6',
        'trig_constant_words': trig_words, 'trig_constants_sha256': _word_hash(trig_words),
        'polynomial_words': trig_words[7:14],
        'twiddle_base': '0xe4754', 'twiddle_complex_entries_used': 256,
        'twiddle_used_words_sha256': _word_hash(packing_words),
        'twiddle_full1024_words_sha256': _word_hash([dm[0xe4754+i] for i in range(1024)]),
        'generated_window_float32_sha256': hashlib.sha256(window.astype('<f4').tobytes()).hexdigest(),
        'code_ranges': [{'start': hex(a), 'end_exclusive': hex(b), 'sha256': digest} for a,b,digest in RANGES],
        'instruction_pins': {hex(k): v for k,v in PINS.items()},
        'address_units': 'SHARC shortword code PCs;32-bit DM words',
        'normalization': 'none', 'hardware_bit_exact': False, 'live_allocated_window_readback': False,
        'precision_boundary': 'float64 intermediates and ideal division, float32 final store; no SHARC40-bit/flags/RECIPS instruction emulation',
        'complex_fft_boundary': 'NumPy complex FFT remains mathematical primitive; stored tables bind only real packing'}
    return {'window': window, 'twiddles': twiddles, 'metadata': metadata}
