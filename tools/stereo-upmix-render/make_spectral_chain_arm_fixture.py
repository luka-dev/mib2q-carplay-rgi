#!/usr/bin/env python3
"""Prepare matched full-chain native references for the isolated ARM guest."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import numpy as np


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def main(a):
    if a.out.exists():raise FileExistsError('Preserve earlier ARM chain fixture')
    audit=json.loads((a.audit/'complete.json').read_bytes());protocol=json.loads((a.audit/'protocol.json').read_bytes())
    assets=json.loads((a.assets/'manifest.json').read_bytes())
    if not audit['passed'] or audit['protocol_sha256']!=sha(a.audit/'protocol.json') or protocol['assets_manifest_sha256']!=sha(a.assets/'manifest.json') or protocol['checkpoint_sha256']!=assets['checkpoint_sha256']:raise ValueError('Different or failing native reference audit')
    records=[];identities={}
    def read(path,magic,channels):
        data=path.read_bytes();h=struct.unpack_from('<8sIII',data)
        if h[0]!=magic or h[1]!=4 or h[3]!=channels or len(data)!=20+h[1]*h[2]*channels*2:raise ValueError('Different native reference layout')
        identities[path.name]=sha(path)
        return np.frombuffer(data,dtype='<i2',offset=20).reshape(4,h[2],channels)[:,:64*512].copy().reshape(4,64,512*channels)
    inputs=read(a.audit/'float.input',b'UMX16I01',2)
    if sha(a.audit/'float.input')!=audit['arms']['float']['hashes']['input'] or sha(a.audit/'int8.input')!=sha(a.audit/'float.input'):raise ValueError('Native input changed')
    records.append(inputs)
    for mode in ('float','int8'):
        output=read(a.audit/(mode+'.output'),b'UMX16O01',6)
        source=read(a.audit/(mode+'.source'),b'UMX16S01',2)
        if sha(a.audit/(mode+'.output'))!=audit['arms'][mode]['hashes']['output'] or sha(a.audit/(mode+'.source'))!=audit['arms'][mode]['hashes']['source']:raise ValueError('Saved native output changed')
        rows=np.concatenate([output.reshape(4,64,512,6),source.reshape(4,64,512,2)],-1).reshape(4,64,512*8)
        records.append(rows)
    data=struct.pack('<8sII',b'SMXQA001',4,64)+np.concatenate(records,-1).astype('<i2').tobytes()
    a.out.mkdir(parents=True);(a.out/'arm_fixture.bin').write_bytes(data)
    for name,row in assets['files'].items():
        if sha(a.assets/name)!=row['sha256']:raise ValueError('Changed model blob')
        (a.out/name).write_bytes((a.assets/name).read_bytes())
    metadata=dict(version='spectral-chain-arm-fixture-v1',checkpoint_sha256=assets['checkpoint_sha256'],fixture_sha256=sha(a.out/'arm_fixture.bin'),
                  native_audit_sha256=sha(a.audit/'complete.json'),native_protocol_sha256=sha(a.audit/'protocol.json'),source_identities=identities,
                  selection='First64 consecutive512sample PCM hops of each of four previously inspected TRAIN8s full-chain inputs; same native fitting20k checkpoint float/int8 references',
                  gates='ARM float all6+source PCM16 NSE<=1e-7/max<=4LSB; ARM int8 all6+source PCM16 NSE<=1e-4 to native int8; selected count exact; four independent interleaved streams; record every channel and peak error',
                  scope='ARM/QNX arithmetic integration only; no native-vs-GT int8 qualification, physical loaded deadline/latency, holdout quality, scene semantics or owner listening',
                  script_sha256=sha(Path(__file__)))
    (a.out/'manifest.json').write_text(json.dumps(metadata,indent=2)+'\n');print('SPECTRAL_CHAIN_ARM_FIXTURE',metadata['fixture_sha256'])


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('audit','assets','out'):p.add_argument('--'+name,type=Path,required=True)
    main(p.parse_args())
