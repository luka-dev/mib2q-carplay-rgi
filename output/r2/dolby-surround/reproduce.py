"""Read-only evidence extraction for the copied DolbyAudioProcessing.dll.

Run from workspace root: python3 output/r2/dolby-surround/reproduce.py
Requires capstone. Addresses are specific to the recorded SHA-256.
"""
from pathlib import Path
import hashlib
import json
import struct
import capstone

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
data = (ROOT / 'Dolby/DolbyAudioProcessing.dll').read_bytes()
pe = struct.unpack_from('<I', data, 0x3c)[0]
count = struct.unpack_from('<H', data, pe + 6)[0]
optional_size = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]
sections = []
for n in range(count):
    pos = pe + 24 + optional_size + n * 40
    virtual_size, rva, size, offset = struct.unpack_from('<IIII', data, pos + 8)
    sections.append((rva, size, offset))


def offset(va):
    rva = va - base
    for start, size, raw in sections:
        if start <= rva < start + size:
            return raw + rva - start
    raise ValueError(hex(va))


ranges = {
    'surround_boost_set': (0x180004120, 0x18000418c),
    'surround_decoder_enable_set': (0x180004190, 0x1800041d7),
    'commit_parameters': (0x180003590, 0x180003c80),
    'decoder_dispatch_excerpt': (0x18000850f, 0x18000899f),
    'decoder_process': (0x1800217c0, 0x180022640),
    'pair_analysis': (0x180031410, 0x180031860),
    'band_statistics': (0x18004d0b0, 0x18004d574),
    'band_power': (0x180022640, 0x1800228b0),
    'direction_index': (0x18004e1c0, 0x18004e2ac),
    'steering_coefficients': (0x18004e2c0, 0x18004e510),
    'angle_approximation': (0x18005b150, 0x18005b1f0),
    'pair_synthesis': (0x180031860, 0x180031b66),
    'matrix_apply': (0x18004e510, 0x18004eb70),
    'phase_rotate': (0x18004eb70, 0x18004ed26),
    'residual_mix': (0x18004d6d0, 0x18004ddf9),
    'decorrelation_delay': (0x18005a9f0, 0x18005acd8),
    'log2_approx': (0x18003c040, 0x18003c08e),
}
cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
metadata = {'sha256': hashlib.sha256(data).hexdigest(), 'image_base': hex(base),
            'format': 'PE32+ AMD64 little-endian', 'functions': {}}
for name, (start, end) in ranges.items():
    listing = '\n'.join(f'{i.address:#x}: {i.mnemonic:9} {i.op_str}'
                        for i in cs.disasm(data[offset(start):offset(end)], start))
    (OUT / (name + '.capstone.asm')).write_text(listing + '\n')
    metadata['functions'][name] = {'va': hex(start), 'rva': hex(start-base),
                                   'file_offset': hex(offset(start))}
metadata['config_headers'] = []
for va in (0x1802eccc0, 0x1802eccf0, 0x1802ecd20, 0x1802ecd50):
    a, b, gain, pairs, ptr = struct.unpack_from('<IIfIQ', data, offset(va))
    metadata['config_headers'].append({'va': hex(va), 'file_offset': hex(offset(va)),
        'input_count': a, 'output_count': b, 'gain': gain,
        'pair_count': pairs, 'pair_descriptors': hex(ptr)})
(OUT / 'verified-map.json').write_text(json.dumps(metadata, indent=2) + '\n')
print(json.dumps(metadata, indent=2))
