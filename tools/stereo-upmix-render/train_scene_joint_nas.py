"""Finite CPU scene pilot on frozen authored AlbumDB TRAIN/DEV assignments.

No protected-source admission/product claim; no automatic-upmix teachers,
heldout access, environment installs, Track A dependencies or deployment.
"""
import argparse
from dataclasses import asdict
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import time

import numpy as np
import soundfile as sf
import torch

from scene_joint_net import Config, CHANNELS, JointSceneNet, analysis, synthesize


def sha(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for part in iter(lambda:stream.read(2**20), b""):
            result.update(part)
    return result.hexdigest()


def now():
    return datetime.now(timezone.utc).isoformat()


def write(path, value):
    temporary = path.with_suffix(path.suffix+".new")
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False)+"\n")
    temporary.replace(path)


def catalog(path, root):
    data = json.loads(path.read_text())
    if data.get("pair_domain") != "known_downmix_of_same_target":
        raise ValueError("Expected same-target downmix pairs")
    if data.get("research_admission", {}).get("scope") != "authored_reconstruction_only":
        raise ValueError("Explicit research-only authored reconstruction admission required")
    rows = {"train":[], "dev":[]}
    groups = {}
    for original in data["rows"]:
        split = original["split"]
        group = original["song_group"]
        if group in groups and groups[group] != split:
            raise ValueError("Song group crosses partitions")
        groups[group] = split
        if split not in rows:
            continue
        row = dict(original)
        if row["provenance"] != "authored_multichannel" or row["source_order"] != ["FL","FR","FC","LFE","SL","SR"]:
            raise ValueError("Only explicitly ordered authored targets")
        source = (root/row["path"]).resolve(strict=True)
        if not source.is_relative_to(root.resolve()):
            raise ValueError("Source escapes the corpus")
        info = sf.info(source)
        if (info.frames,info.samplerate,info.channels) != (row["frames"],48000,6):
            raise ValueError("Audio geometry mismatch")
        if sha(source) != row["sha256"]:
            raise ValueError("Frozen audio hash mismatch")
        print(json.dumps({"verified_song":row["id"],"partition":split}),flush=True)
        row["resolved_path"] = str(source)
        rows[split].append(row)
    if len(rows["train"]) != 7 or len(rows["dev"]) != 2:
        raise ValueError("Expected existing seven TRAIN/two inspected DEV AlbumDB songs")
    return data, rows


def excerpt(row, length, fraction):
    start = int(max(0,row["frames"]-length)*fraction)
    with sf.SoundFile(row["resolved_path"]) as source:
        source.seek(start)
        six = source.read(length, dtype="float32", always_2d=True)
    if six.shape != (length,6) or not np.isfinite(six).all():
        raise ValueError("Expected finite complete excerpt")
    if np.abs(six[:,3]).max() > 1e-7:
        raise ValueError("This frozen corpus declares silent LFE")
    # Reproduce catalog's single common loader attenuation, never per-channel.
    target = torch.from_numpy(six[:,[0,1,2,4,5]].copy()) * row["common_gain"]
    h = math.sqrt(.5)
    mixture = target[:,:2] + h*(target[:,2:3]+target[:,3:])
    return mixture, target, start


def loss(pred, target, denominator, warm):
    p, y = pred[:,warm:], target[:,warm:]
    relative = (p-y).abs().square().mean((0,1,2))/denominator
    magnitude = (p.abs()-y.abs()).abs().mean((0,1,2))/denominator.sqrt()
    return relative.mean()+.05*magnitude.mean(), relative


def assess(net, rows, denominator, warm, frames=256):
    metrics=[]
    n = net.config.fft_size+(frames-1)*net.config.hop
    d = net.downmix.to(torch.complex64)
    net.eval()
    with torch.inference_mode():
        for row in rows:
            for fraction in (.25,.65):
                wave, target_wave, start = excerpt(row,n,fraction)
                x,y=analysis(wave),analysis(target_wave)
                pred,_=net(x)
                p=pred["base"][:,warm:]; ref=y[:,warm:]
                denom=ref.abs().square().sum((0,1,2))
                nmse=(p-ref).abs().square().sum((0,1,2))/denom.clamp_min(1e-8)
                cosine=(p.conj()*ref).real.sum((0,1,2))/(p.abs().square().sum((0,1,2))*denom).sqrt().clamp_min(1e-8)
                energy=p.abs().square().sum((0,1,2))/denom.clamp_min(1e-8)
                dry=synthesize(pred["base"])
                input_pcm=synthesize(x)
                delta=(dry@net.downmix.T-input_pcm).abs()
                rms_error=(delta.square().mean()/input_pcm.square().mean().clamp_min(1e-10)).sqrt()
                fx=.1*pred["base"][...,3:]
                effect=torch.cat([torch.zeros_like(pred["base"][...,:3]),fx],-1)
                unexplained=torch.einsum("oc,btfc->btfo",d,pred["base"]+effect)-torch.einsum("oc,btfc->btfo",d,effect)-x
                metrics.append(dict(song=row["id"],start_sample=start,frames=frames,
                    nmse=nmse.tolist(),cosine=cosine.tolist(),energy_ratio=energy.tolist(),
                    dry_pcm_null_max=delta.max().item(),dry_pcm_null_relative_rms=rms_error.item(),
                    accounted_effect_stft_residual_max=unexplained.abs().max().item(),
                    baseline_zero_rear_nmse=1.,denominator_known=(denom>1e-8).tolist()))
    net.train()
    values=np.asarray([v["nmse"] for v in metrics])
    return dict(rows=metrics,median_nmse_by_channel=np.median(values,axis=0).tolist(),
                median_rear_nmse=float(np.median(values[:,3:])),
                max_pcm_null=max(v["dry_pcm_null_max"] for v in metrics),
                max_relative_pcm_null_rms=max(v["dry_pcm_null_relative_rms"] for v in metrics),
                scope="fixed short crops, inspected AlbumDB only; no source attribution, entry/device/ear gate")


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--catalog",type=Path,required=True)
    parser.add_argument("--audio-root",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--steps",type=int,default=4096)
    parser.add_argument("--threads",type=int,default=3)
    parser.add_argument("--checkpoint-every",type=int,default=256)
    a=parser.parse_args()
    if a.out.exists() or a.steps<1:
        raise ValueError("New output root and positive fixed horizon required")
    a.out.mkdir(parents=True)
    torch.set_num_threads(a.threads)
    torch.set_num_interop_threads(1)
    torch.manual_seed(20261003)
    rng=np.random.default_rng(20261003)
    begun=time.monotonic()
    write(a.out/"status.json",dict(state="preparing",pid=os.getpid(),started_utc=now()))
    try:
        source,rows=catalog(a.catalog,a.audio_root)
        config=Config();net=JointSceneNet(config)
        warm,scored=32,96
        n=config.fft_size+(warm+scored-1)*config.hop
        powers=[]
        for row in rows["train"]:
            for fraction in (.25,.65):
                _,target,_=excerpt(row,n,fraction)
                powers.append(analysis(target).abs().square().mean((0,1,2)))
        denominator=torch.stack(powers).mean(0).clamp_min(1e-4)
        protocol=dict(schema="joint-scene-nas-pilot-v1",started_utc=now(),pid=os.getpid(),
            steps=a.steps,seed=20261003,lr=.001,threads=a.threads,warm_frames=warm,score_frames=scored,
            catalog_sha256=sha(a.catalog),split_sha256=source["split_sha256"],
            train=[r["id"] for r in rows["train"]],dev=[r["id"] for r in rows["dev"]],
            input_domain="known downmix of same authored target; not independent stereo masters",
            artist_disjoint=source["artist_disjoint"],heldout_opened=False,
            training_target_cabin_safe=False,role_policy_approved=False,
            train_loss="fixed TRAIN channel-power-normalized complex MSE + .05 magnitude MAE",
            normalization_power=denominator.tolist(),learned_channels=list(CHANNELS),
            lfe="absent from pilot model/targets; separate DSP proposed, not implemented",
            effect="0 in supervised baseline; declared .10 rear duplication accounting diagnostic only",
            changed_mechanism="full-bin five-head operators, band/global recurrence, channel-normalized joint reconstruction",
            comparison="fresh recipe fitting screen; not a single-factor architecture ablation against P1",
            pilot_gates="TRAIN median rear NMSE<=.50; inspected DEV<=.80; both front medians nonworse than initial; dry PCM null relative RMS<=1e-5; fitting screen only, never release approval",
            specification=net.specification(),
            source_hashes={p.name:sha(p) for p in [Path(__file__),Path(__file__).with_name("scene_joint_net.py")]},
            release_gates="original full40 source/entry/stability, new untouched recordings, native int8 complete device<=150M MAC/s<=85ms, owner live CarPlay; not evaluated by this pilot")
        write(a.out/"protocol.json",protocol)
        optimizer=torch.optim.AdamW(net.parameters(),lr=.001,weight_decay=1e-4)
        initial=dict(train=assess(net,rows["train"],denominator,warm),
                     dev=assess(net,rows["dev"],denominator,warm))
        write(a.out/"initial.json",initial)
        print(json.dumps({"started":True,"specification":net.specification(),
                          "initial_train_rear_nmse":initial["train"]["median_rear_nmse"],
                          "initial_dev_rear_nmse":initial["dev"]["median_rear_nmse"]}),flush=True)
        history=[]; exposures={r["id"]:0 for r in rows["train"]}
        net.train()
        for step in range(1,a.steps+1):
            if (a.out/"STOP").exists():
                raise RuntimeError("Owner stop marker observed; retain outputs")
            row=rows["train"][(step-1)%len(rows["train"])]
            wave,target,start=excerpt(row,n,float(rng.uniform(.01,.99)))
            x,y=analysis(wave),analysis(target)
            pred,_=net(x)
            objective,channels=loss(pred["base"],y,denominator,warm)
            if not torch.isfinite(objective):
                raise RuntimeError("Nonfinite objective")
            optimizer.zero_grad(set_to_none=True)
            objective.backward()
            gradient=torch.nn.utils.clip_grad_norm_(net.parameters(),5.)
            if not torch.isfinite(gradient):raise RuntimeError("Nonfinite gradient")
            optimizer.step()
            exposures[row["id"]]+=scored*config.hop
            item=dict(step=step,song=row["id"],start_sample=start,loss=objective.item(),
                      channel_loss=channels.tolist(),gradient=float(gradient),elapsed_seconds=time.monotonic()-begun)
            history.append(item)
            if step%16==0 or step==1:
                status=dict(state="training",pid=os.getpid(),step=step,steps=a.steps,
                            updated_utc=now(),last_loss=item["loss"],elapsed_seconds=item["elapsed_seconds"],
                            observed_seconds_per_update=item["elapsed_seconds"]/step)
                write(a.out/"status.json",status)
                print(json.dumps(status),flush=True)
            if step%a.checkpoint_every==0 or step==a.steps:
                checkpoint=a.out/f"step{step:05d}.pt"
                torch.save(dict(model=net.state_dict(),optimizer=optimizer.state_dict(),config=asdict(config),
                                step=step,protocol_sha256=sha(a.out/"protocol.json"),rng_numpy=rng.bit_generator.state,
                                rng_torch=torch.get_rng_state()),checkpoint)
                monitor=dict(step=step,train=assess(net,rows["train"],denominator,warm),
                             dev=assess(net,rows["dev"],denominator,warm))
                write(a.out/f"monitor{step:05d}.json",monitor)
                write(a.out/"latest.json",dict(checkpoint=checkpoint.name,sha256=sha(checkpoint),step=step,monitor=f"monitor{step:05d}.json"))
                print(json.dumps({"monitor_step":step,"train_rear_nmse":monitor["train"]["median_rear_nmse"],
                                  "dev_rear_nmse":monitor["dev"]["median_rear_nmse"]}),flush=True)
        checks=dict(train_rear=monitor["train"]["median_rear_nmse"]<=.50,
                    inspected_dev_rear=monitor["dev"]["median_rear_nmse"]<=.80,
                    front_nonworse=all(monitor[part]["median_nmse_by_channel"][i]<=initial[part]["median_nmse_by_channel"][i]
                                       for part in ("train","dev") for i in (0,1)),
                    dry_null=all(monitor[part]["max_relative_pcm_null_rms"]<=1e-5 for part in ("train","dev")))
        final=dict(complete=True,terminal_step=a.steps,elapsed_seconds=time.monotonic()-begun,
                   initial=initial,terminal=monitor,history=history,score_sample_exposures=exposures,
                   fitting_checks=checks,fitting_pass=all(checks.values()),
                   role_policy_approved=False,car_approved=False,device_measured=False)
        write(a.out/"report.json",final)
        write(a.out/"status.json",dict(state="complete",pid=os.getpid(),step=a.steps,updated_utc=now()))
    except BaseException as exc:
        write(a.out/"status.json",dict(state="failed",pid=os.getpid(),updated_utc=now(),error=str(exc),
                                     elapsed_seconds=time.monotonic()-begun))
        raise


if __name__=="__main__":
    main()
