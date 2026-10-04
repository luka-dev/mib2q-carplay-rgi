"""Finite one-TRAIN-crop fitting diagnostic, unchanged joint scene recipe.

No DEV decode, source-role/product qualification, environment installs or hook
dependencies. Reuse the parent TRAIN normalization and exact training geometry.
"""
import argparse
from dataclasses import asdict
import json
import math
import os
from pathlib import Path
import time

import numpy as np
import soundfile as sf
import torch

from scene_joint_net import Config, JointSceneNet, analysis, synthesize
from train_scene_joint_nas import loss, now, sha, write


def measure(net, x, y, denominator, warm):
    net.eval()
    with torch.inference_mode():
        prediction, _ = net(x)
        p, target = prediction["base"][:, warm:], y[:, warm:]
        power = target.abs().square().sum((0, 1, 2))
        nmse = (p-target).abs().square().sum((0, 1, 2))/power.clamp_min(1e-8)
        estimated = p.abs().square().sum((0, 1, 2))
        cosine = (p.conj()*target).real.sum((0, 1, 2))/(estimated*power).sqrt().clamp_min(1e-8)
        objective, channel_loss = loss(prediction["base"], y, denominator, warm)
        input_pcm = synthesize(x)
        delta = synthesize(prediction["base"])@net.downmix.T-input_pcm
        null_rms = (delta.square().mean()/input_pcm.square().mean().clamp_min(1e-10)).sqrt()
        metrics = dict(loss=objective.item(), channel_loss=channel_loss.tolist(),
                       nmse=nmse.tolist(), energy_ratio=(estimated/power.clamp_min(1e-8)).tolist(),
                       cosine=cosine.tolist(), target_power=power.tolist(),
                       denominator_known=(power>1e-8).tolist(),
                       worst_rear_nmse=max(nmse[3:].tolist()),
                       dry_pcm_null_max=delta.abs().max().item(),
                       dry_pcm_null_relative_rms=null_rms.item())
    net.train()
    return metrics


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--catalog", type=Path, required=True)
    parser.add_argument("--audio-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--steps", type=int, default=2048)
    parser.add_argument("--threads", type=int, default=3)
    a = parser.parse_args()
    if a.out.exists() or a.steps<1 or a.threads<1:
        raise ValueError("New output root and positive horizon/threads required")
    a.out.mkdir(parents=True)
    started = time.monotonic()
    write(a.out/"status.json", dict(state="preparing", pid=os.getpid(), updated_utc=now()))
    try:
        parent = json.loads((a.parent/"protocol.json").read_text())
        if parent["schema"] != "joint-scene-nas-pilot-v1" or sha(a.catalog) != parent["catalog_sha256"]:
            raise ValueError("Frozen parent protocol/catalog mismatch")
        for name in ("scene_joint_net.py", "train_scene_joint_nas.py"):
            if sha(Path(__file__).with_name(name)) != parent["source_hashes"][name]:
                raise ValueError("Model or loss changed from the parent pilot")
        catalog = json.loads(a.catalog.read_text())
        if catalog["pair_domain"] != "known_downmix_of_same_target":
            raise ValueError("Known same-target downmix required")
        row = next(r for r in catalog["rows"] if r["id"]=="albumdb-01")
        if (row["split"] != "train" or row["id"] not in parent["train"] or
                row["provenance"] != "authored_multichannel" or
                row["source_order"] != ["FL", "FR", "FC", "LFE", "SL", "SR"]):
            raise ValueError("Explicit ordered authored TRAIN admission required")
        source = (a.audio_root/row["path"]).resolve(strict=True)
        if not source.is_relative_to(a.audio_root.resolve()):
            raise ValueError("Audio source escapes the corpus")
        info = sf.info(source)
        if (info.frames, info.samplerate, info.channels) != (row["frames"], 48000, 6) or sha(source) != row["sha256"]:
            raise ValueError("Frozen selected TRAIN audio mismatch")
        recorded = json.loads((a.parent/"initial.json").read_text())
        start = next(r["start_sample"] for r in recorded["train"]["rows"] if r["song"]==row["id"])
        config = Config(**parent["specification"]["config"])
        warm, scored = parent["warm_frames"], parent["score_frames"]
        length = config.fft_size+(warm+scored-1)*config.hop
        with sf.SoundFile(source) as stream:
            stream.seek(start)
            six = stream.read(length, dtype="float32", always_2d=True)
        if six.shape != (length, 6) or not np.isfinite(six).all() or np.abs(six[:, 3]).max()>1e-7:
            raise ValueError("Finite complete crop with silent LFE required")
        target = torch.from_numpy(six[:, [0, 1, 2, 4, 5]].copy())*row["common_gain"]
        mixture = target[:, :2]+math.sqrt(.5)*(target[:, 2:3]+target[:, 3:])
        torch.set_num_threads(a.threads)
        torch.set_num_interop_threads(1)
        torch.manual_seed(parent["seed"])
        net = JointSceneNet(config)
        x, y = analysis(mixture), analysis(target)
        denominator = torch.tensor(parent["normalization_power"], dtype=torch.float32)
        powers = y[:, warm:].abs().square().sum((0, 1, 2))
        input_power = x[:, warm:].abs().square().sum()
        if (powers[3:]<=1e-8).any() or (powers[3:]/input_power.clamp_min(1e-8)<1e-3).any():
            raise ValueError("Preselected crop has insufficient rear target energy")
        protocol = dict(schema="joint-scene-fixed-train-v1", started_utc=now(), pid=os.getpid(),
                        parent_protocol_sha256=sha(a.parent/"protocol.json"),
                        catalog_sha256=sha(a.catalog), song=row["id"], partition="train",
                        audio_sha256=row["sha256"], common_gain=row["common_gain"],
                        start_sample=start, length_samples=length,
                        warm_frames=warm, score_frames=scored, steps=a.steps, threads=a.threads,
                        seed=parent["seed"], normalization_power=denominator.tolist(),
                        initialization="fresh same-seed weights; no parent checkpoint", state_reset_each_update=True,
                        optimizer="AdamW", lr=parent["lr"], weight_decay=1e-4, gradient_clip=5.,
                        loss=parent["train_loss"], specification=net.specification(),
                        intervention="one unchanged TRAIN crop repeatedly instead of changing songs/offsets",
                        pass_rule="both rear channel NMSE<=.10 and dry PCM relative null RMS<=1e-5 on this exact scored crop",
                        dev_audio_opened=False, heldout_opened=False, effect_gain=0.,
                        scope="same-crop memorization only; not generalization, source safety or product qualification",
                        source_hashes={p.name:sha(p) for p in [Path(__file__), Path(__file__).with_name("scene_joint_net.py"), Path(__file__).with_name("train_scene_joint_nas.py")]})
        write(a.out/"protocol.json", protocol)
        initial = measure(net, x, y, denominator, warm)
        write(a.out/"initial.json", initial)
        optimizer = torch.optim.AdamW(net.parameters(), lr=parent["lr"], weight_decay=1e-4)
        history, monitors = [], []
        best = dict(step=0, metrics=initial)
        for step in range(1, a.steps+1):
            if (a.out/"STOP").exists():
                raise RuntimeError("Owner stop marker observed; retain outputs")
            pred, _ = net(x)
            objective, _ = loss(pred["base"], y, denominator, warm)
            if not torch.isfinite(objective):
                raise RuntimeError("Nonfinite loss")
            optimizer.zero_grad(set_to_none=True)
            objective.backward()
            gradient = torch.nn.utils.clip_grad_norm_(net.parameters(), 5.)
            if not torch.isfinite(gradient):
                raise RuntimeError("Nonfinite gradient")
            optimizer.step()
            history.append(dict(step=step, loss=objective.item(), gradient=float(gradient)))
            if step==1 or step%32==0:
                status = dict(state="training", pid=os.getpid(), step=step, steps=a.steps,
                              updated_utc=now(), elapsed_seconds=time.monotonic()-started, last_loss=objective.item())
                write(a.out/"status.json", status)
                print(json.dumps(status), flush=True)
            if step%128==0 or step==a.steps:
                current = dict(step=step, metrics=measure(net, x, y, denominator, warm))
                monitors.append(current)
                write(a.out/f"monitor{step:05d}.json", current)
                if current["metrics"]["worst_rear_nmse"]<best["metrics"]["worst_rear_nmse"]:
                    best = current
                    best_path = a.out/"best.pt"
                    torch.save(dict(model=net.state_dict(), config=asdict(config), step=step,
                                    protocol_sha256=sha(a.out/"protocol.json")), best_path)
                    write(a.out/"best.json", dict(step=step, sha256=sha(best_path), metrics=best["metrics"]))
                write(a.out/"latest.json", current)
                print(json.dumps(current), flush=True)
        torch.save(dict(model=net.state_dict(), optimizer=optimizer.state_dict(), config=asdict(config),
                        step=a.steps, protocol_sha256=sha(a.out/"protocol.json")), a.out/"terminal.pt")
        terminal = monitors[-1]
        def passed(metrics):
            return metrics["worst_rear_nmse"]<=.10 and metrics["dry_pcm_null_relative_rms"]<=1e-5
        report = dict(complete=True, terminal_step=a.steps, elapsed_seconds=time.monotonic()-started,
                      initial=initial, terminal=terminal, best=best, monitor_history=monitors,
                      optimizer_history=history, terminal_pass=passed(terminal["metrics"]),
                      best_pass=passed(best["metrics"]), terminal_checkpoint_sha256=sha(a.out/"terminal.pt"),
                      product_qualified=False, generalization_tested=False)
        write(a.out/"report.json", report)
        write(a.out/"status.json", dict(state="complete", pid=os.getpid(), step=a.steps, updated_utc=now()))
    except BaseException as exc:
        write(a.out/"status.json", dict(state="failed", pid=os.getpid(), updated_utc=now(),
                                       error=str(exc), elapsed_seconds=time.monotonic()-started))
        raise


if __name__=="__main__":
    main()
