#!/usr/bin/env python3
"""Matched original/residual causal encoders, identical frozen trained decoder.

Four frozen TRAIN crops; unchanged true-audio objective; no teacher imitation,
new corpus, gain fitting, compression, DEV selection or renderer intervention.
"""
import argparse
import gc
import hashlib
import json
import time
from pathlib import Path
import numpy as np
import mask_net as MN
import sep_net as SN
import separator_causal_reference as CR
from separator_fit_probe import objective, transmissions, sha, write_json


ANCHOR_SHA = '0e560b9924feea88f3e61b97253734c60a02a3901c7636779e107653d7e77e41'
MODES = ['original', 'residual96x4']


def tensors_sha(module):
    h = hashlib.sha256()
    for key, value in sorted(module.state_dict().items()):
        h.update(json.dumps([key, str(value.dtype), list(value.shape)]).encode())
        h.update(value.detach().cpu().contiguous().numpy().tobytes())
    return h.hexdigest()


def data(cache, fit):
    manifest = json.loads((fit/'manifest.json').read_text())
    if sha(cache/'split.json') != manifest['split_sha256'] or sha(Path(SN.__file__)) != manifest['separator_sha256']:
        raise ValueError('Frozen data/implementation changed')
    split = MN.load_split(str(cache))
    arrays = []
    for row in manifest['windows']:
        if row['song'] not in split['train']:
            raise ValueError('Non-TRAIN window')
        raw = MN.expand_slots(MN.read_window(str(cache/(row['song']+'.npy')), row['start_sample'], row['samples']))
        if hashlib.sha256(raw.tobytes()).hexdigest() != row['selected_int16_sha256']:
            raise ValueError('Frozen crop changed')
        arrays.append(raw.astype(np.float32)/32768)
    if len(arrays) != 4:
        raise ValueError('Exactly four frozen windows required')
    return np.stack(arrays), manifest


def causality(torch, net, x):
    # Perturb future STFT frames at a fixed length. This tests the network only.
    a = x[:1, :145].clone()
    b = a.clone(); b[:, 65:] *= complex(-2, .7)
    net.modules.eval()
    with torch.no_grad():
        first = net.separate_stft(a).clone()
        second = net.separate_stft(b)
        prefix = float((first[:, :65]-second[:, :65]).abs().square().sum()/first[:, :65].abs().square().sum().clamp_min(1e-12))
        suffix = float((first[:, 65:]-second[:, 65:]).abs().square().sum()/first[:, 65:].abs().square().sum().clamp_min(1e-12))
    net.modules.train()
    if not np.isfinite(prefix) or prefix > 1e-10 or suffix <= 1e-9:
        raise ValueError('Future-frame causality/sensitivity failed')
    return dict(prefix_nse=prefix, perturbed_suffix_nse=suffix,
                scope='fixed STFT-frame network; centered transforms/stateful port not certified')


def acoustic_bands(torch, estimate, target, mask, eligible):
    result = {}
    hz = torch.arange(SN.N_BINS, device=target.device)*SN.SR/SN.N_FFT
    for name, lo, hi in [('0-300',0,300), ('300-8k',300,8000), ('8-14k',8000,14000), ('14-24k',14000,24001)]:
        idx = (hz >= lo)&(hz < hi)
        ref = target[:, :, idx]
        power = ref.abs().square().sum((1,2,3)).clamp_min(1e-12)
        result[name] = dict(error_nse=((estimate[:, :, idx]-ref).abs().square().sum((1,2,3))/power).tolist(),
                           eligible_tx=((mask[:, :, idx]*eligible[:, :, idx]).abs().square().sum((1,2,3))/power).tolist())
    return result


def evaluate(torch, net, x, target, wav, sources, n):
    net.modules.eval()
    capture = []
    handles = [module.register_forward_hook(lambda m, i, o: capture.append(o.detach())) for module in net.enc] if isinstance(net, SN.BandSepNet) and not isinstance(net, CR.CausalReference) else []
    with torch.no_grad():
        estimate = net.separate_stft(x)
        mask = net.last_mask.clone()
        y = SN.istft(torch, estimate, n)
        result = dict(loss=float(objective(torch, estimate, target, wav, n)),
                      sdr_db=SN.si_sdr(torch, y, wav).mean(-1).tolist(),
                      reconstruction_nse=((y-wav).square().sum((1,2))/wav.square().sum((1,2))).tolist(),
                      source_tx=transmissions(torch, mask, sources, n),
                      bands=acoustic_bands(torch, estimate, target, mask, sources['eligible']))
        result['encoder_tanh_saturation_by_band'] = [float((torch.tanh(v).abs()>.95).float().mean()) for v in capture] if capture else None
    for handle in handles:
        handle.remove()
    net.modules.train()
    if not np.isfinite(result['loss']) or not np.isfinite(result['sdr_db']).all():
        raise ValueError('Nonfinite acoustic measurement')
    return result


def promotion(candidate, control, historical):
    cs = np.asarray(candidate['sdr_db'])
    result = {}
    for name, base in [('matched',control), ('historical',historical)]:
        bs = np.asarray(base['sdr_db'])
        delta = cs-bs
        result.update({name+'_loss_10pct':candidate['loss'] <= .9*base['loss'],
                       name+'_median_plus_1db':float(np.median(cs)) >= float(np.median(bs))+1,
                       name+'_three_nonworse':int(np.sum(delta >= 0)) >= 3,
                       name+'_no_loss_over_1db':bool(np.all(delta >= -1))})
    return result


def run(a):
    if a.out.exists():
        raise FileExistsError('Preserve prior attempt; new output required')
    if sha(a.fit/'fit.pt') != ANCHOR_SHA:
        raise ValueError('Anchor checkpoint changed')
    old = json.loads((a.fit/'fit_complete.json').read_text())
    if old['step'] != 10000:
        raise ValueError('Completed historical 10k required')
    arrays, manifest = data(a.cache, a.fit)
    torch = SN._torch()
    torch.set_num_threads(4)
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    device = 'cuda' if torch.cuda.is_available() else 'cpu'
    ck = torch.load(a.fit/'fit.pt', map_location=device, weights_only=True)
    anchor = CR.make(torch,'original').to(device)
    anchor.modules.load_state_dict(ck['state'], strict=True)
    SN.STATS.update(ck['stats'])
    w = torch.from_numpy(arrays).to(device)
    x, target = SN.stft(torch,w.sum(1)), SN.stft(torch,w[:,6])
    n = w.shape[-1]
    sources = dict(eligible=target, vocal=SN.stft(torch,w[:,0]+w[:,1]),
                   drums=SN.stft(torch,w[:,2:5].sum(1)), bass=SN.stft(torch,w[:,5]))
    historical = evaluate(torch,anchor,x,target,w[:,6],sources,n)
    if abs(historical['loss']-old['final']['loss']) > 1e-5 or np.max(np.abs(np.asarray(historical['sdr_db'])-old['final']['sdr_db'])) > 1e-3:
        raise ValueError('Historical acoustic replay failed')
    decoder = {k:v.detach().cpu().clone() for k,v in anchor.dec.state_dict().items()}
    decoder_sha = tensors_sha(anchor.dec)
    del anchor; gc.collect(); torch.cuda.empty_cache() if device == 'cuda' else None
    a.out.mkdir(parents=True)
    write_json(a.out/'manifest.json', dict(scope='four fixed TRAIN crops; causal encoder package, frozen decoder',
               windows=manifest['windows'], split_sha256=manifest['split_sha256'],
               anchor_checkpoint_sha256=ANCHOR_SHA, frozen_decoder_sha256=decoder_sha,
               separator_sha256=sha(Path(SN.__file__)), model_sha256=sha(Path(CR.__file__)),
               probe_sha256=sha(Path(__file__)), objective_sha256=sha(Path(__file__).with_name('separator_fit_probe.py')),
               seed=1, steps=a.steps, lr=3e-4, weight_decay=1e-4, gradient_clip=5,
               stats=dict(SN.STATS), tf32=False, historical=historical,
               gate='loss <= .9 both controls, median SDR >= both+1dB, >=3/4 nonworse and no crop worse by >1dB'))
    results, global_sha = {}, None
    for mode in MODES:
        torch.manual_seed(1)
        net = CR.make(torch,mode).to(device)
        net.dec.load_state_dict(decoder, strict=True)
        for parameter in net.dec.parameters():
            parameter.requires_grad_(False)
        if tensors_sha(net.dec) != decoder_sha:
            raise ValueError('Decoder parity failed')
        current_global = tensors_sha(net.ggru)
        if global_sha is not None and current_global != global_sha:
            raise ValueError('Global-GRU initialization differs')
        global_sha = current_global
        before = causality(torch,net,x)
        folder = a.out/mode; folder.mkdir()
        info = dict(parameters=sum(p.numel() for p in net.parameters()),
                    trainable_parameters=sum(p.numel() for p in net.parameters() if p.requires_grad),
                    estimated_neural_macs_s=net.macs*SN.FPS, initial_causality=before,
                    global_gru_initial_sha256=global_sha, frozen_decoder_sha256=decoder_sha)
        write_json(folder/'architecture.json',info)
        print('ARCHITECTURE',mode,json.dumps(info),flush=True)
        optimizer = torch.optim.AdamW([p for p in net.parameters() if p.requires_grad],lr=3e-4,weight_decay=1e-4)
        rows, start, grad, clipped = [], time.monotonic(), 0., 0
        for step in range(a.steps+1):
            if step:
                net.modules.train()
                optimizer.zero_grad(set_to_none=True)
                loss = objective(torch,net.separate_stft(x),target,w[:,6],n)
                if not bool(torch.isfinite(loss)):
                    raise ValueError('Nonfinite training loss')
                loss.backward()
                grad = float(torch.nn.utils.clip_grad_norm_([p for p in net.parameters() if p.requires_grad],5))
                if not np.isfinite(grad):
                    raise ValueError('Nonfinite gradients')
                clipped += int(grad>5)
                optimizer.step()
            if step % a.eval_every == 0 or step == a.steps:
                row = evaluate(torch,net,x,target,w[:,6],sources,n)
                row.update(step=step, seconds=time.monotonic()-start,
                           grad_norm_before_clip=grad, clipped_steps=clipped)
                if tensors_sha(net.dec) != decoder_sha:
                    raise ValueError('Frozen head changed')
                rows.append(row)
                write_json(folder/'progress.json',dict(rows=rows))
                temporary = folder/'fit.pt.new'
                torch.save(dict(architecture='causal-reference-v1',mode=mode,state=net.modules.state_dict(),
                                stats=dict(SN.STATS),step=step,decoder_sha256=decoder_sha,
                                model_sha256=sha(Path(CR.__file__))),temporary)
                temporary.replace(folder/'fit.pt')
                print(f'{mode} step {step} loss {row["loss"]:.6f} median_sdr {np.median(row["sdr_db"]):.3f} grad {grad:.3g} elapsed {row["seconds"]:.1f}s',flush=True)
        after = causality(torch,net,x)
        results[mode] = dict(final=rows[-1],architecture=info,final_causality=after,
                             checkpoint_sha256=sha(folder/'fit.pt'))
        write_json(folder/'complete.json',results[mode])
        del net, optimizer; gc.collect(); torch.cuda.empty_cache() if device == 'cuda' else None
    gates = promotion(results[MODES[1]]['final'],results[MODES[0]]['final'],historical)
    write_json(a.out/'complete.json',dict(complete=True, scope='TRAIN acoustic diagnostic only',
                steps=a.steps,smoke=a.steps!=10000,arms=results,historical=historical,gates=gates,
                diagnostic_pass=a.steps==10000 and all(gates.values()),
                device_c_streaming_independent_and_listening='not measured'))
    print('PAIR_COMPLETE',json.dumps(gates),flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['cache','fit','out']:
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--steps',type=int,default=10000)
    parser.add_argument('--eval-every',type=int,default=250)
    args = parser.parse_args()
    if min(args.steps,args.eval_every) < 1:
        parser.error('Positive horizon and evaluation interval required')
    run(args)
