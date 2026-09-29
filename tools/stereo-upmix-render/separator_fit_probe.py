#!/usr/bin/env python3
"""Finite TRAIN fitting diagnostic, not a model-selection or product run.

Four deterministic mixed-source TRAIN windows, no augmentation/pseudo labels.
Use the existing causal BandSepNet and full linear audio-loss bundle. A
privileged complex ratio reference separates loss-path errors from learnability.
"""
import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np

import mask_net as MN
import sep_net as SN


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    temporary = path.with_suffix(path.suffix + ".new")
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def select(cache, seconds=8, count=4):
    split = MN.load_split(str(cache))
    n = int(seconds * SN.SR)
    ids = sorted(split["train"], key=lambda s: hashlib.sha256(
        ("finite-fit-v1:" + s).encode()).hexdigest())
    arrays, records = [], []
    for song in ids:
        path = cache / (song + ".npy")
        length = MN.npy_header(str(path))[0][2]
        if length < n:
            continue
        for start in np.unique(np.linspace(0, length - n, 12).astype(int)):
            raw = MN.expand_slots(MN.read_window(str(path), int(start), n))
            w = raw.astype(np.float32) / 32768
            power = np.square(w.astype(np.float64)).sum(axis=(1, 2))
            total = power.sum()
            if total <= 1e-9 or not (.03 <= power[6] / total <= .90
                                    and (power[0] + power[1]) / total >= .03):
                continue
            meta = json.loads(path.with_suffix(".json").read_text(encoding="utf-8"))
            records.append(dict(song=song, artist=meta.get("artist"),
                start_sample=int(start), samples=n,
                selected_int16_sha256=hashlib.sha256(raw.tobytes()).hexdigest(),
                source_energy_fraction=(power / total).tolist(),
                int16_boundary_samples=int(np.count_nonzero(np.abs(raw.astype(np.int32)) >= 32766))))
            arrays.append(w)
            break
        if len(arrays) == count:
            break
    if len(arrays) != count:
        raise ValueError("insufficient mixed vocal/eligible TRAIN windows")
    return np.stack(arrays), records


def objective(torch, estimate, target, target_wav, n):
    return SN.loss_audio(torch, estimate, target) + SN.multires_loss(
        torch, SN.istft(torch, estimate, n), target_wav)


def transmissions(torch, mask, sources, n):
    out = {}
    for name, ref in sources.items():
        sent = SN.stft(torch, SN.istft(torch, mask * ref, n))
        reanalysed = SN.stft(torch, SN.istft(torch, ref, n))
        denominator = reanalysed.abs().square().sum(dim=(1, 2, 3))
        numerator = sent.abs().square().sum(dim=(1, 2, 3))
        out[name] = [float(v) if float(d) > 1e-9 else None
                     for v, d in zip(numerator / denominator.clamp_min(1e-9), denominator)]
    return out


def train(args):
    if args.out.exists():
        raise FileExistsError("new output directory required; no implicit resume/overwrite")
    args.out.mkdir(parents=True)
    arrays, records = select(args.cache)
    write_json(args.out / "manifest.json", dict(scope="fixed TRAIN fitting only", windows=records,
        split_sha256=sha(args.cache / "split.json"), script_sha256=sha(Path(__file__)),
        separator_sha256=sha(Path(SN.__file__)), steps_cap=args.steps, lr=args.lr,
        optimizer="AdamW constant lr, weight_decay=1e-4; diagnostic, not production schedule",
        norm="stream", mask="complex2", gate="80% initial-to-oracle objective gap reduction twice"))
    torch = SN._torch()
    torch.manual_seed(1)
    device = "cuda" if torch.cuda.is_available() else "cpu"
    w = torch.as_tensor(arrays, device=device)
    mix, eligible = w.sum(1), w[:, 6]
    n = mix.shape[-1]
    X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
    sources = dict(eligible=E, vocal=SN.stft(torch, w[:, 0] + w[:, 1]),
                   drums=SN.stft(torch, w[:, 2] + w[:, 3] + w[:, 4]),
                   bass=SN.stft(torch, w[:, 5]))
    with torch.no_grad():
        lm = torch.log(X.abs() + 1e-5)
        SN.STATS.update(mean=float(lm.mean()), std=float(lm.std() + 1e-3))
        identity = SN.istft(torch, X, n)
        identity_nse = float((identity - mix).square().sum() / mix.square().sum())
        # Stable, unbounded privileged ratio. Report mask size; no device claim.
        oracle_mask = E * X.conj() / (X.abs().square() + 1e-12)
        oracle = oracle_mask * X
        oracle_loss = float(objective(torch, oracle, E, eligible, n))
        oracle_sdr = SN.si_sdr(torch, SN.istft(torch, oracle, n), eligible).mean(-1).tolist()
        pure_mask = (E.abs() / (X.abs() + 1e-7)).clamp(0, 1).to(X.dtype)
        references = dict(identity_nse=identity_nse, oracle_loss=oracle_loss,
            oracle_sdr_db=oracle_sdr,
            oracle_mask_abs_quantiles=torch.quantile(oracle_mask.abs().flatten(),
                torch.tensor([.5, .9, .99, .999], device=device)).tolist(),
            oracle_source_tx=transmissions(torch, oracle_mask, sources, n),
            bounded_real_loss=float(objective(torch, pure_mask * X, E, eligible, n)),
            bounded_real_source_tx=transmissions(torch, pure_mask, sources, n))
        write_json(args.out / "references.json", references)
    if identity_nse > 1e-10 or not np.isfinite(oracle_loss):
        raise ValueError("STFT identity/oracle path failed before training")
    net = SN.BandSepNet(torch, norm="stream", mask="complex2").to(device)
    optimizer = torch.optim.AdamW(net.parameters(), lr=args.lr, weight_decay=1e-4)
    with torch.no_grad():
        initial = float(objective(torch, net.separate_stft(X), E, eligible, n))
    if initial <= oracle_loss:
        raise ValueError("invalid initial-to-oracle loss gap")
    start_time = time.monotonic()
    passes = 0
    last_grad = 0.0
    history = []
    for step in range(args.steps + 1):
        if step:
            optimizer.zero_grad(set_to_none=True)
            estimate = net.separate_stft(X)
            loss = objective(torch, estimate, E, eligible, n)
            if not bool(torch.isfinite(loss)):
                raise ValueError("nonfinite training loss")
            loss.backward()
            last_grad = float(torch.nn.utils.clip_grad_norm_(net.parameters(), 5.0))
            if not np.isfinite(last_grad):
                raise ValueError("nonfinite parameter gradient")
            optimizer.step()
        if step % args.eval_every == 0 or step == args.steps:
            with torch.no_grad():
                estimate = net.separate_stft(X)
                value = float(objective(torch, estimate, E, eligible, n))
                progress = (initial - value) / (initial - oracle_loss)
                waveform = SN.istft(torch, estimate, n)
                row = dict(step=step, loss=value, gap_closed=progress,
                    sdr_db=SN.si_sdr(torch, waveform, eligible).mean(-1).tolist(),
                    reconstruction_nse=((waveform - eligible).square().sum(dim=(1, 2)) /
                        eligible.square().sum(dim=(1, 2))).tolist(),
                    source_tx=transmissions(torch, net.last_mask, sources, n),
                    grad_norm_before_clip=last_grad, seconds=time.monotonic() - start_time)
                history.append(row)
                write_json(args.out / "progress.json", dict(initial_loss=initial,
                    oracle_loss=oracle_loss, rows=history))
                print(f"step {step} objective {value:.6f} gap_closed {progress:.4f} "
                      f"median_sdr {np.median(row['sdr_db']):.2f} "
                      f"grad {last_grad:.3g} elapsed {row['seconds']:.0f}s", flush=True)
                torch.save(dict(state=net.modules.state_dict(), stats=dict(SN.STATS),
                    step=step, arch="band", norm="stream", mask="complex2"), args.out / "fit.pt")
            passes = passes + 1 if progress >= .80 else 0
            if passes >= 2:
                break
    write_json(args.out / "fit_complete.json", dict(status="FIT_GATE_PASSED" if passes >= 2
        else "FIT_GATE_NOT_REACHED", step=step, final=history[-1], scope="TRAIN sanity only"))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--steps", type=int, default=10000)
    p.add_argument("--eval-every", type=int, default=250)
    p.add_argument("--lr", type=float, default=3e-4)
    args = p.parse_args()
    if min(args.steps, args.eval_every) < 1 or args.lr <= 0:
        p.error("positive steps, eval interval and lr required")
    train(args)


if __name__ == "__main__":
    main()
