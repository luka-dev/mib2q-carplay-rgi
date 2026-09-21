"""Read-only binary evidence collection. Usage: python inspect_dolby.py PACKAGE_PATH

Uses the already installed pefile package to enumerate PE exports.
Writes evidence/binary-findings.json alongside this script. Does not execute DLLs.
"""
import datetime
import hashlib
import json
import pathlib
import re
import sys

import pefile

package = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(__file__).resolve().parent / 'evidence' / 'binary-findings.json'
names = ['DolbyAccess.dll', 'DLLs/x64/DolbyAudioProcessing.dll',
         'DLLs/x64/DolbyHrtfEnc.dll', 'DLLs/x64/DolbyMatEngine.dll']
pattern = re.compile(
    r'upmix|stereo.?bypass|GetStereoBypassAllowed|surround_decoder|'
    r'virtualizer|dap_vr_output_mode|DolbyAudioProcessing|'
    r'ConfigureStaticChannelMetadata|static channel config 7\.1\.4|'
    r'DAPVRModule|OARModule|AIDEModule|VlldpModule', re.I)
records = []
for name in names:
    path = package / name
    data = path.read_bytes()
    pe = pefile.PE(data=data)
    exports = [symbol.name.decode('ascii', errors='replace')
               for symbol in getattr(getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', [])
               if symbol.name]
    matches = []
    for encoding, regex in [('ascii', rb'[\x20-\x7e]{5,}'),
                            ('utf-16le', rb'(?:[\x20-\x7e]\x00){5,}')]:
        for match in re.finditer(regex, data):
            value = match.group().decode(encoding)
            if pattern.search(value):
                matches.append({'offset_hex': hex(match.start()), 'encoding': encoding,
                                'text': value})
    records.append({'file': name, 'source': str(path), 'bytes': len(data),
                    'sha256': hashlib.sha256(data).hexdigest(),
                    'exports': exports, 'strings': matches})
    pe.close()
output.parent.mkdir(exist_ok=True)
output.write_text(json.dumps({'collected_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                              'method': 'Static strings and PE export inspection; no runtime DSP measurement',
                              'files': records}, ensure_ascii=False, indent=2), encoding='utf-8')
print(output)
for record in records:
    print(record['file'], 'matching strings:', len(record['strings']))
