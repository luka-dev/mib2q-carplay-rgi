#!/usr/bin/env python3
"""One unified model blob: float reference and per-output-row int8 weights.

Preparation for an isolated runtime; no hook/device or int8-quality claim.
Layer norms/biases stay float. Int8 GEMV requires signed symmetric activations
quantized per input vector, ties-to-even, range[-127,127], and int32 accumulation.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES']='-1'
import argparse
import hashlib
import json
from pathlib import Path
import struct
import numpy as np
import torch
import train_unified_upmix_fit as T

MAGIC=b'UMXNN001'


def export(a):
    if a.out.exists():raise FileExistsError('Preserve previous runtime assets')
    net,ck=T.load(a.checkpoint,'cpu');net.eval()
    config=net.config
    dims=[config[k] for k in ('width','depth','local','global_width','decoder')]
    if dims not in ([24,2,16,16,16],[24,3,24,24,24]):raise ValueError('Runtime v2 supports only the frozen default and expanded graphs')
    tensors=list(net.named_parameters())
    arrays={name:p.detach().numpy().astype('<f4',copy=True) for name,p in tensors}
    if any(arr.ndim not in (1,2) or not np.isfinite(arr).all() for arr in arrays.values()):raise ValueError('Invalid parameter layout')
    a.out.mkdir(parents=True)
    result={}
    for mode in (0,1):
        dest=a.out/('unified_int8.bin' if mode else 'unified_float.bin')
        table=[]
        with dest.open('wb') as f:
            f.write(struct.pack('<8s8I3fI',MAGIC,1,mode,*dims,len(net.bands),
                                config['log_mean'],config['log_std'],net.level_alpha,len(arrays)))
            for name,arr in arrays.items():
                key=name.encode('ascii');rows=arr.shape[0];cols=arr.shape[1] if arr.ndim==2 else 1
                storage=1 if mode and arr.ndim==2 else 0
                if len(key)>95:raise ValueError('Runtime tensor key too long')
                f.write(struct.pack('<HHIII',len(key),arr.ndim,rows,cols,storage));f.write(key)
                if storage:
                    maxima=np.abs(arr).max(1)
                    scale=np.where(maxima>0,maxima/127,1).astype('<f4')
                    q=np.clip(np.rint(arr/scale[:,None]),-127,127).astype('i1')
                    if not np.all(np.abs(q.astype(np.float32)*scale[:,None]-arr)<=.50001*scale[:,None]+1e-7):raise ValueError('Per-row quantization error exceeded bound')
                    f.write(scale.tobytes());f.write(q.tobytes())
                    table.append(dict(name=name,shape=list(arr.shape),storage='int8',
                        maximum_weight_error=float(np.abs(q.astype(np.float32)*scale[:,None]-arr).max())))
                else:
                    f.write(arr.tobytes());table.append(dict(name=name,shape=list(arr.shape),storage='float32'))
        # Independent readback guards accidental name/order/shape/byte errors.
        data=dest.read_bytes();offset=struct.calcsize('<8s8I3fI')
        for name,arr in arrays.items():
            length,ndim,rows,cols,storage=struct.unpack_from('<HHIII',data,offset);offset+=16
            if data[offset:offset+length]!=name.encode('ascii') or ndim!=arr.ndim or (rows,cols)!=(arr.shape[0],arr.shape[1] if arr.ndim==2 else 1):raise ValueError('Blob table changed on readback')
            offset+=length
            if storage:
                scale=np.frombuffer(data,dtype='<f4',count=rows,offset=offset);offset+=rows*4
                q=np.frombuffer(data,dtype='i1',count=rows*cols,offset=offset).reshape(rows,cols);offset+=rows*cols
                if not np.isfinite(scale).all() or (scale<=0).any() or (q==-128).any():raise ValueError('Invalid int8 readback')
            else:
                actual=np.frombuffer(data,dtype='<f4',count=rows*cols,offset=offset);offset+=rows*cols*4
                if not np.array_equal(actual.reshape(arr.shape),arr):raise ValueError('Float parameters not bit-exact')
        if offset!=len(data):raise ValueError('Blob length differs')
        result[dest.name]=dict(sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),tensor_table=table)
    T.F.write_json(a.out/'manifest.json',dict(version='unified-runtime-assets-v2',checkpoint_sha256=T.F.sha(a.checkpoint),
        checkpoint_mode=ck.get('mode'),checkpoint_step=ck.get('step'),source_sha256=T.F.sha(Path(__file__)),
        architecture=net.config,cost=net.cost(),files=result,
        contract='48bands2048FFT/512hop/48k; features [normalized real L/R, imaginary L/R, standardized logmag L/R]; PyTorch GRU gate order reset/update/new with reset AFTER recurrent affine; per-stream level/time/global/family state; current-frame frequency bidirectionality',
        quantization='matrices per output-row int8, symmetric maxabs127; LN/bias float; runtime activation scale per vector maxabs127, nearest-even signed[-127,127], exact int32 dot',
        status='assets only; no runtime/NEON, quantization-aware training, acoustic parity, device benchmark or hook integration. Scene status must be read from training manifest; export does not train it. Neural cost is estimated, not measured-device proof.'))
    print('UNIFIED_RUNTIME_ASSETS_EXPORTED',json.dumps({name:{k:v[k] for k in ('bytes','sha256')} for name,v in result.items()}),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('checkpoint','out'):p.add_argument('--'+name,type=Path,required=True)
    export(p.parse_args())
