#!/usr/bin/env python3
"""Reproduce the selected deva bytes; no target access or in-place mutation."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOG = ROOT / 'docs/stock-patches/evidence/binary-patches.json'

def build(source, output):
    if source.resolve() == output.resolve():
        raise ValueError('output must differ from input')
    spec = next(p for p in json.loads(CATALOG.read_text())['patches']
                if p['name'] == 'deva-ctrl-qc.so')
    data = bytearray(source.read_bytes())
    if hashlib.sha256(data).hexdigest() != spec['stock_sha256']:
        raise ValueError('input is not the fingerprinted stock deva')
    for span in spec['changed_ranges']:
        offset = int(span['file_offset'], 16)
        old, new = bytes.fromhex(span['old']), bytes.fromhex(span['new'])
        if len(old) != len(new) or data[offset:offset+len(old)] != old:
            raise ValueError('patch span mismatch')
        data[offset:offset+len(old)] = new
    sha = hashlib.sha256(data).hexdigest()
    if sha != spec['patched_sha256']:
        raise ValueError('result differs from selected deva')
    output.write_bytes(data)
    print(sha)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stock', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    try:
        build(args.stock, args.output)
    except ValueError as error:
        parser.exit(1, str(error) + '\n')
