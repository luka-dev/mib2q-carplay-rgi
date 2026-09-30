#!/usr/bin/env python3
"""Past-only 2048/512 Hann analysis, reusable neural state and causal WOLA.

Produces a delayed eligible stereo stream. No scene port or int8/device claim.
The block-array delay is1536samples; sample availability can add one512hop.
"""
import argparse
import json
from pathlib import Path
import numpy as np
import torch
import torch.nn.functional as TF
from sep_net import SR, HOP, N_FFT
from unified_upmix_net import UnifiedUpmixNet
from unified_upmix_audio import real_endpoints

DELAY = N_FFT-HOP


def analyze(waveform):
    """[B,2,N] -> [B,T,1025,2]; exactly the online zero-start/flush frames."""
    if waveform.ndim != 3 or waveform.shape[1] != 2 or waveform.shape[-1] < 1:
        raise ValueError('Nonempty stereo batch required')
    n = waveform.shape[-1]
    right = DELAY + (-n)%HOP
    padded = TF.pad(waveform, (DELAY, right))
    frames = padded.unfold(-1, N_FFT, HOP)
    spectra = torch.fft.rfft(frames * torch.hann_window(N_FFT, device=waveform.device), dim=-1)
    return spectra.permute(0, 2, 3, 1)


def synthesize(spectra, samples, compensate=True):
    """Differentiable WOLA. TRAIN targets may compensate the fixed array delay."""
    if not spectra.is_complex() or spectra.ndim != 4 or spectra.shape[-2:] != (N_FFT//2+1, 2):
        raise ValueError('Complex stereo spectra required')
    b, t = spectra.shape[:2]
    win = torch.hann_window(N_FFT, device=spectra.device)
    frames = torch.fft.irfft(real_endpoints(spectra).permute(0,3,1,2), n=N_FFT, dim=-1)*win/1.5
    length = (t-1)*HOP+N_FFT
    flat = frames.reshape(b*2,t,N_FFT).transpose(1,2)
    output = TF.fold(flat, output_size=(1,length),kernel_size=(1,N_FFT),stride=(1,HOP)).reshape(b,2,length)
    start = DELAY if compensate else 0
    if start+samples > t*HOP:
        raise ValueError('Insufficient online frames to emit requested output')
    return output[...,start:start+samples]


class Stream:
    """Caller pushes arbitrary PCM chunks; output available only after a hop.

    push returns0..many full512 blocks. finish emits partial tail and flushes
   1536delay samples. Neural state is one caller-owned instance per stream.
    """
    def __init__(self, net=None, batch=1, device='cpu'):
        self.net = net
        self.batch=batch
        self.device=device
        self.pending=torch.empty(batch,2,0,device=device)
        self.history=torch.zeros(batch,2,DELAY,device=device)
        self.ola=torch.zeros(batch,2,N_FFT,device=device)
        self.window=torch.hann_window(N_FFT,device=device)
        self.state=None
        self.samples=0
        self.emitted=0
        self.closed=False

    def push(self, chunk):
        if self.closed or chunk.ndim != 3 or chunk.shape[:2] != (self.batch,2):
            raise ValueError('Wrong chunk or closed stream')
        self.samples+=chunk.shape[-1]
        self.pending=torch.cat((self.pending,chunk.to(self.device)),dim=-1)
        outputs=[]
        while self.pending.shape[-1] >= HOP:
            block=self.pending[...,:HOP]
            self.pending=self.pending[...,HOP:]
            frame=torch.cat((self.history,block),-1)
            self.history=frame[...,HOP:]
            x=torch.fft.rfft(frame*self.window,dim=-1).permute(0,2,1)[:,None]
            if self.net is None:
                selected=x
            else:
                prediction,self.state=self.net.forward_stft(x,self.state,with_scene=False)
                selected=prediction['source_stft'][:,:,0]
            y=torch.fft.irfft(real_endpoints(selected)[:,0].permute(0,2,1),n=N_FFT,dim=-1)
            self.ola=self.ola+y*self.window/1.5
            outputs.append(self.ola[...,:HOP])
            self.ola=torch.cat((self.ola[...,HOP:],torch.zeros_like(block)),-1)
            self.emitted+=HOP
        return torch.cat(outputs,-1) if outputs else chunk[...,:0].to(self.device)

    def finish(self):
        if self.closed:
            raise ValueError('Already closed')
        samples=self.samples
        missing=(-samples)%HOP
        tail=self.push(torch.zeros(self.batch,2,missing+DELAY,device=self.device))
        self.closed=True
        return tail[...,:samples+DELAY-(self.emitted-tail.shape[-1])]


def smoke(out):
    torch.set_num_threads(1);torch.manual_seed(7)
    n=18001
    x=torch.randn(1,2,n)*.03
    spectra=analyze(x)
    offline=synthesize(spectra,n)
    identity_nse=float((offline-x).square().sum()/x.square().sum())
    assert identity_nse<1e-10
    def replay(net, chunks, signal=x):
        stream=Stream(net)
        parts=[];start=0
        with torch.inference_mode():
            for size in chunks:
                parts.append(stream.push(signal[...,start:start+size]));start+=size
            if start != signal.shape[-1]: raise ValueError('Chunks do not cover input')
            parts.append(stream.finish())
        return torch.cat(parts,-1)
    chunks=[1,173,338,17,2111,n-2640]
    identity=replay(None,chunks)
    assert identity.shape[-1]==n+DELAY
    identity_stream_nse=float((identity[...,DELAY:]-x).square().sum()/x.square().sum())
    assert identity_stream_nse<1e-10
    net=UnifiedUpmixNet().eval()
    with torch.inference_mode():
        pred,_=net.forward_stft(spectra,with_scene=False)
        full=synthesize(pred['source_stft'][:,:,0],n)
    streamed=replay(net,chunks)
    error=float((full-streamed[...,DELAY:]).square().sum()/full.square().sum().clamp_min(1e-12))
    assert error<1e-10
    split=replay(net,[512]*(n//512)+[n%512])
    chunk_error=float((streamed-split).square().sum()/streamed.square().sum().clamp_min(1e-12))
    assert chunk_error<1e-10
    changed=x.clone();changed[...,9000:]*=-8
    other=replay(net,chunks,changed)
    # A hop crossing sample9000 cannot emit earlier than the following end.
    prefix=9000//HOP*HOP
    prefix_error=float((streamed[...,:prefix]-other[...,:prefix]).square().sum())
    assert prefix_error == 0
    result=dict(verified=True,scope='CPU random-model waveform structure; no trained music/int8/device latency result',
                sr=SR,fft=N_FFT,hop=HOP,block_array_delay_samples=DELAY,
                block_array_delay_ms=1000*DELAY/SR,max_frame_acquisition_ms=1000*HOP/SR,
                excluded='neural execution, scheduler, transport and additional C DSP/cabin delay',
                identity_nse=identity_nse,stream_identity_nse=identity_stream_nse,
                neural_offline_stream_nse=error,chunk_nse=chunk_error,future_input_prefix_error=prefix_error,
                transform='zero-start past-only Hann; three-hop zero flush; differs at startup from centered-reflect fitting transform')
    Path(out).write_text(json.dumps(result,indent=2)+'\n');print('UNIFIED_STREAM_SMOKE',json.dumps(result))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--smoke-out',required=True)
    smoke(p.parse_args().smoke_out)
