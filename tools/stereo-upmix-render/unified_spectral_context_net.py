#!/usr/bin/env python3
"""Track B wider spectral prototype with bounded current-frame frequency mixing.

Hypothesis only: spend fewer dense MACs on frequency recurrence and more on
per-band features. No checkpoint compatibility, fitting or source-quality claim.
The time groups communicate through a full-width projection and frequency mix.
"""
import argparse
import json
from pathlib import Path
import torch
from torch import nn
import unified_upmix_net as U

ARCHITECTURE='unified-spectral-context-prototype-v1'


class SpectralContextBlock(nn.Module):
    def __init__(self,width,bands,time_groups=4,frequency_channels=8,kernel=7):
        super().__init__()
        if width%time_groups or not kernel%2:raise ValueError('Divisible time groups and odd kernel required')
        self.width,self.groups,self.channels,self.kernel=width,time_groups,frequency_channels,kernel
        self.time_norm=nn.LayerNorm(width)
        self.time=nn.ModuleList([nn.GRU(width//time_groups,width//time_groups,batch_first=True) for _ in range(time_groups)])
        self.time_proj=nn.Linear(width,width)
        self.frequency_norm=nn.LayerNorm(width)
        self.neighbours=nn.Conv1d(width,width,kernel,padding=kernel//2,groups=width)
        self.down=nn.Linear(width,frequency_channels)
        self.full_band=nn.ModuleList([nn.Linear(bands,bands) for _ in range(frequency_channels)])
        self.up=nn.Linear(frequency_channels,width)

    def forward(self,x,state=None):
        b,t,k,w=x.shape
        old=[None]*self.groups if state is None else state
        if len(old)!=self.groups:raise ValueError('Wrong grouped recurrent state')
        normalized=self.time_norm(x).permute(0,2,1,3).reshape(b*k,t,w)
        values,updated=[],[]
        for module,part,previous in zip(self.time,normalized.chunk(self.groups,-1),old):
            y,s=module(part,previous);values.append(y);updated.append(s)
        temporal=torch.cat(values,-1).reshape(b,k,t,w).permute(0,2,1,3)
        x=x+self.time_proj(temporal)
        f=self.frequency_norm(x)
        local=self.neighbours(f.reshape(b*t,k,w).transpose(1,2)).transpose(1,2).reshape(b,t,k,w)
        narrow=torch.relu(self.down(f)).permute(0,1,3,2)
        global_frequency=torch.stack([module(narrow[:,:,i]) for i,module in enumerate(self.full_band)],2)
        global_frequency=self.up(torch.relu(global_frequency.permute(0,1,3,2)))
        return x+local+global_frequency,updated


class SpectralContextNet(U.UnifiedUpmixNet):
    def __init__(self,width=48,depth=2,local=24,global_width=24,decoder=24,
                 time_groups=4,frequency_channels=8,kernel=7,**kwargs):
        super().__init__(width=width,depth=depth,local=local,global_width=global_width,decoder=decoder,**kwargs)
        self.blocks=nn.ModuleList([SpectralContextBlock(width,len(self.bands),time_groups,frequency_channels,kernel) for _ in range(depth)])
        self.config.update(time_groups=time_groups,frequency_channels=frequency_channels,kernel=kernel)

    def cost(self):
        w,d,l,g,r=(self.config[n] for n in ('width','depth','local','global_width','decoder'))
        groups,c,kernel=(self.config[n] for n in ('time_groups','frequency_channels','kernel'))
        bands,bins=len(self.bands),U.N_BINS
        common=dict(input_projection=bins*6*w,
            grouped_time_grus=d*bands*groups*6*(w//groups)**2,
            time_projection=d*bands*w*w,
            frequency_neighbours=d*bands*w*kernel,
            full_band_down_up=d*bands*2*w*c,
            full_band_matrix=d*c*bands*bands,
            interface=bands*w*l,
            global_gru=3*(l*g+g*g),
            decoder_bottleneck=bands*(l+g)*r,
            scene_projection=bands*(l+g)*len(U.FAMILIES)*3)
        all_sources=common|dict(source_projection=bins*r*len(U.SOURCES)*4)
        selected=common|dict(source_projection=bins*r*4)
        return dict(parameters=sum(p.numel() for p in self.parameters()),fps=U.FPS,
            all_source_dense_macs_per_frame=all_sources,
            eligible_scene_dense_macs_per_frame=selected,
            all_source_dense_macs_s=sum(all_sources.values())*U.FPS,
            eligible_scene_dense_macs_s=sum(selected.values())*U.FPS,
            excluded='FFT/iFFT, norms, activations, level/features, complex products, pooling, scene state, renderer, int8 kernels and memory traffic; not device measured')


def smoke(out):
    if out.exists():raise FileExistsError('Preserve earlier prototype smoke')
    torch.set_num_threads(2);torch.manual_seed(20260930)
    net=SpectralContextNet().eval()
    x=torch.complex(torch.randn(2,29,U.N_BINS,2),torch.randn(2,29,U.N_BINS,2))
    error=lambda y,r:float((y-r).abs().double().square().sum()/r.abs().double().square().sum().clamp_min(1e-12))
    with torch.inference_mode():
        full,_=net.forward_stft(x);state=None;pieces=[]
        for lo,hi in [(0,1),(1,7),(7,8),(8,19),(19,29)]:
            pred,state=net.forward_stft(x[:,lo:hi],state);pieces.append(pred)
        chunk={k:error(torch.cat([p[k] for p in pieces],1),v) for k,v in full.items()}
        changed=x.clone();changed[:,13:]*=-7
        future,_=net.forward_stft(changed)
        prefix={k:error(future[k][:,:13],v[:,:13]) for k,v in full.items()}
        reset,_=net.forward_stft(x)
        reset_exact=all(torch.equal(v,reset[k]) for k,v in full.items())
        silence,_=net.forward_stft(torch.zeros_like(x))
        finite_silence=all(bool(torch.isfinite(v).all()) for v in silence.values())
        sources=torch.stack([x*.1,x*.2,x*.3,x*.4],2)
        sent=full['source_masks'][:,:,0:1]*sources
        closure=error(sent.sum(2),full['source_stft'][:,:,0])
        # Isolate the block: whole-network level normalization/global pooling
        # would also transmit a low-band perturbation and confound this check.
        features=torch.randn(1,3,len(net.bands),net.config['width'])
        isolated=features.clone();isolated[:,:,0]*=-3
        block_reference,_=net.blocks[0](features)
        block_changed,_=net.blocks[0](isolated)
        cross_band_change=error(block_changed[:,:,-1],block_reference[:,:,-1])
        # Independently count observed dense operations by module input shapes.
        observed=[0]
        def count(module,args,output):
            value=args[0]
            if isinstance(module,nn.Linear):observed[0]+=value.numel()*module.out_features
            elif isinstance(module,nn.Conv1d):
                observed[0]+=output.numel()*(module.in_channels//module.groups)*module.kernel_size[0]
            elif isinstance(module,nn.GRU):
                batch,frames=value.shape[:2]
                observed[0]+=batch*frames*3*module.hidden_size*(module.input_size+module.hidden_size)
        handles=[module.register_forward_hook(count) for module in net.modules() if isinstance(module,(nn.Linear,nn.Conv1d,nn.GRU))]
        net.forward_stft(x[:1,:3])
        for handle in handles:handle.remove()
        observed_dense_per_frame=observed[0]//3
        declared_dense_per_frame=sum(net.cost()['all_source_dense_macs_per_frame'].values())
    net.train();prediction,_=net.forward_stft(x[:,:4]);loss=prediction['source_stft'].abs().square().mean()
    loss+=(prediction['membership']-.2).square().mean()+prediction['family_pan'].square().mean();loss.backward()
    modules={'encoder':net.enc,'time_groups':nn.ModuleList([m for b in net.blocks for m in b.time]),
             'time_communication':nn.ModuleList([b.time_proj for b in net.blocks]),
             'frequency_neighbours':nn.ModuleList([b.neighbours for b in net.blocks]),
             'full_band_mix':nn.ModuleList([m for b in net.blocks for m in [b.down,b.full_band,b.up]]),
             'decoder':net.dec,'scene':net.scene}
    gradients={n:sum(float(p.grad.abs().sum()) for p in m.parameters() if p.grad is not None) for n,m in modules.items()}
    gates=dict(chunk=max(chunk.values())<1e-9,future_prefix=max(prefix.values())<1e-12,
               reset_exact=reset_exact,finite_silence=finite_silence,source_attribution=closure<1e-10,
               cross_band_communication=cross_band_change>1e-10,
               all_module_gradients=all(v>0 and torch.isfinite(torch.tensor(v)) for v in gradients.values()),
               observed_dense_count_matches=observed_dense_per_frame==declared_dense_per_frame,
               selected_dense_under150M=net.cost()['eligible_scene_dense_macs_s']<=150e6)
    result=dict(architecture=ARCHITECTURE,trained=False,passed=all(gates.values()),gates=gates,
        config=net.config,cost=net.cost(),chunk_nse=chunk,future_prefix_nse=prefix,
        closure_nse=closure,cross_band_change_nse=cross_band_change,gradient_absolute_sums=gradients,
        observed_all_source_dense_macs_per_frame=observed_dense_per_frame,
        scope='synthetic CPU causal/state/attribution/gradient/dense-count checks only; no source fit, quality, quantization, C port, device or owner result')
    out.write_text(json.dumps(result,indent=2)+'\n')
    if not result['passed']:raise ValueError('Spectral-context prototype failed; evidence retained')
    print('SPECTRAL_CONTEXT_STRUCTURE_PASS',json.dumps(result),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--smoke-out',type=Path,required=True);smoke(p.parse_args().smoke_out)
