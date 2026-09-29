#!/usr/bin/env python3
"""TRAIN-only decoder-state targets versus matched audio-loss continuation.

The same trained causal encoder and fixed decoder start both 2k arms. One
learns from audio, the other directly regresses privileged decoder states.
Actual waveform quality decides success. No inference receives true stems.
"""
import argparse
import hashlib
import json
import time
import shutil
from pathlib import Path
import numpy as np
import mask_net as MN
import sep_net as SN
from separator_fit_probe import objective, sha, write_json


def data(cache, fit):
    manifest = json.loads((fit / "manifest.json").read_text())
    if sha(cache / "split.json") != manifest["split_sha256"]:
        raise ValueError("frozen split changed")
    if sha(Path(SN.__file__)) != manifest["separator_sha256"]:
        raise ValueError("baseline separator changed")
    arrays = []
    for r in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(cache / (r["song"] + ".npy")),
                                           r["start_sample"], r["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != r["selected_int16_sha256"]:
            raise ValueError("frozen TRAIN window changed")
        arrays.append(raw.astype(np.float32) / 32768)
    return arrays, manifest


def capture(net):
    captured = {}
    handles = [d.register_forward_pre_hook(
        lambda module, inputs, i=i: captured.__setitem__(i, inputs[0]))
               for i, d in enumerate(net.dec)]
    return captured, handles


def states(torch, net, captured):
    return (torch.stack([captured[i][..., :net.h2] for i in range(len(net.bands))], 2),
            captured[0][..., net.h2:])


def decode(torch, net, X, h, g):
    pieces = [d(torch.cat((h[:, :, i], g), -1)).reshape(X.shape[0], X.shape[1], hi-lo, 4)
              for i, (d, (lo, hi)) in enumerate(zip(net.dec, net.bands))]
    m = torch.cat(pieces, 2)
    return X * torch.complex(m[..., 0::2], m[..., 1::2])


def state_loss(h, g, ht, gt):
    return .5*((h-ht).square().mean()+(g-gt).square().mean())


def freeze_decoder(net):
    for p in net.dec.parameters():
        p.requires_grad_(False)


def gradient_norm(torch, gradients):
    return float(sum(g.detach().square().sum() for g in gradients if g is not None).sqrt())


def checkpoint(net, step):
    return dict(state=net.modules.state_dict(), stats=dict(SN.STATS), step=step,
                arch="band", norm="stream", mask="complex2")


def run(a):
    if a.out.exists():
        raise FileExistsError("new output required; no automatic overwrite/resume")
    terminal = json.loads((a.fit / "fit_complete.json").read_text())
    if terminal["step"] != 10000:
        raise ValueError("original 10k anchor required")
    arrays, manifest = data(a.cache, a.fit)
    torch, net, device = SN.load(str(a.fit / "fit.pt"))
    if net.mask_mode != "complex2" or net.norm != "stream" or net.dec_skip or net.nbrnn or net.lookahead:
        raise ValueError("frozen causal baseline topology required")
    w = torch.as_tensor(np.stack(arrays), device=device)
    mix, eligible = w.sum(1), w[:, 6]
    n = mix.shape[-1]
    X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
    a.out.mkdir(parents=True)
    captured, handles = capture(net)
    with torch.no_grad():
        baseline = net.separate_stft(X)
        h0, g0 = states(torch, net, captured)
        if any(float((captured[i][..., net.h2:]-g0).abs().max()) > 1e-7 for i in captured):
            raise ValueError("global state not shared across bands")
        h0, g0 = h0.detach().clone(), g0.detach().clone()
        replay = decode(torch, net, X, h0, g0)
        replay_nse = float((replay-baseline).abs().square().sum()/baseline.abs().square().sum())
        initial_loss = float(objective(torch, baseline, E, eligible, n))
    for handle in handles:
        handle.remove()
    if replay_nse > 1e-10 or abs(initial_loss-terminal["final"]["loss"]) > 1e-6:
        raise ValueError("original checkpoint replay mismatch")
    if a.prepared_targets:
        source = a.prepared_targets
        info = json.loads((source / "teacher.json").read_text())
        if sha(source / "targets.pt") != info["targets_sha256"]:
            raise ValueError("prepared target checksum changed")
        saved = torch.load(source / "targets.pt", map_location=device)
        if saved["checkpoint_sha256"] != sha(a.fit / "fit.pt") or saved["windows"] != manifest["windows"]:
            raise ValueError("prepared target source/window mismatch")
        ht, gt = saved["h"], saved["g"]
        if ht.shape != h0.shape or gt.shape != g0.shape:
            raise ValueError("prepared target dimensions changed")
        if float(ht.abs().max()) > 3.000001 or float(gt.abs().max()) > 1.000001:
            raise ValueError("prepared state bounds violated")
        reference = json.loads(a.teacher_reference.read_text())["arms"][0]["rows"][-1]
        with torch.no_grad():
            replay_loss = float(objective(torch, decode(torch, net, X, ht, gt), E, eligible, n))
        if abs(replay_loss-reference["loss"]) > 1e-4:
            raise ValueError("prepared target reconstruction mismatch")
        shutil.copyfile(source / "targets.pt", a.out / "targets.pt")
        info["copied_from"] = str(source)
        info["reuse_audio_loss"] = replay_loss
        write_json(a.out / "teacher.json", info)
        print(f"Reused verified targets: audio_loss {replay_loss:.6f}", flush=True)
    else:
        for p in net.parameters():
            p.requires_grad_(False)
        h, g = torch.nn.Parameter(h0.clone()), torch.nn.Parameter(g0.clone())
        opt = torch.optim.Adam([h, g], lr=.03)
        teacher_rows, clock = [], time.monotonic()
        for step in range(2001):
            if step:
                opt.zero_grad(set_to_none=True)
                loss = objective(torch, decode(torch, net, X, h, g), E, eligible, n)
                if not bool(torch.isfinite(loss)):
                    raise ValueError("nonfinite teacher fit")
                loss.backward()
                opt.step()
                with torch.no_grad():
                    h.clamp_(-3, 3)
                    g.clamp_(-1, 1)
            if step % 250 == 0:
                with torch.no_grad():
                    y = decode(torch, net, X, h, g)
                    row = dict(step=step, loss=float(objective(torch, y, E, eligible, n)),
                        sdr_db=SN.si_sdr(torch, SN.istft(torch, y, n), eligible).mean(-1).tolist())
                teacher_rows.append(row)
                print(f"teacher step {step} loss {row['loss']:.6f} "
                      f"median_sdr {np.median(row['sdr_db']):.2f}", flush=True)
        ht, gt = h.detach().clone(), g.detach().clone()
        # Require reproduction of the completed fixed-weight privileged-state arm.
        reference = json.loads(a.teacher_reference.read_text())["arms"][0]["rows"][-1]
        if abs(teacher_rows[-1]["loss"]-reference["loss"]) > 1e-4:
            raise ValueError("teacher-state reconstruction differs from frozen reference")
        torch.save(dict(h=ht.cpu(), g=gt.cpu(), initial_h=h0.cpu(), initial_g=g0.cpu(),
                        checkpoint_sha256=sha(a.fit / "fit.pt"), windows=manifest["windows"]),
                   a.out / "targets.pt")
        write_json(a.out / "teacher.json", dict(rows=teacher_rows, seconds=time.monotonic()-clock,
                   replay_nse=replay_nse, targets_sha256=sha(a.out / "targets.pt")))

    # Calibration uses only these TRAIN tensors before either continuation.
    torch.manual_seed(1)
    _, calibration_net, _ = SN.load(str(a.fit / "fit.pt"))
    freeze_decoder(calibration_net)
    calibration_net.modules.train()  # cuDNN GRU backward requires training mode
    c, hs = capture(calibration_net)
    estimate = calibration_net.separate_stft(X)
    ch, cg = states(torch, calibration_net, c)
    parameters = [p for p in calibration_net.parameters() if p.requires_grad]
    la = objective(torch, estimate, E, eligible, n)
    ls = state_loss(ch, cg, ht, gt)
    ga = torch.autograd.grad(la, parameters, retain_graph=True, allow_unused=True)
    gs = torch.autograd.grad(ls, parameters, allow_unused=True)
    na, ns = gradient_norm(torch, ga), gradient_norm(torch, gs)
    if min(na, ns) <= 1e-12 or not np.isfinite(na+ns):
        raise ValueError("invalid TRAIN gradient calibration")
    coefficient = na/ns
    dot = float(sum((x*y).sum() for x, y in zip(ga, gs) if x is not None and y is not None))
    calibration = dict(audio_gradient_norm=na, state_gradient_norm=ns,
                       state_coefficient=coefficient, gradient_cosine=dot/(na*ns),
                       initial_audio_loss=float(la.detach()), initial_state_loss=float(ls.detach()),
                       method="scale pure state MSE to equal initial audio gradient L2 norm; TRAIN only")
    for handle in hs:
        handle.remove()
    write_json(a.out / "calibration.json", calibration)
    write_json(a.out / "manifest.json", dict(scope="four fixed TRAIN windows; two continuation arms",
        windows=manifest["windows"], split_sha256=manifest["split_sha256"],
        checkpoint_sha256=sha(a.fit / "fit.pt"), script_sha256=sha(Path(__file__)),
        separator_sha256=sha(Path(SN.__file__)), targets_sha256=sha(a.out / "targets.pt"),
        teacher_reference_sha256=sha(a.teacher_reference), continuation_steps=a.steps,
        optimizer="fresh AdamW per arm lr3e-4 decay1e-4 clip5, fixed decoder",
        calibration=calibration,
        success="state arm audio loss <= .9 matched control; median SI-SDR >= control+1dB; >=3/4 nonworse, no window >1dB worse; >=50% state-error reduction"))

    results = []
    for mode in ("audio", "states"):
        torch.manual_seed(1)
        _, student, _ = SN.load(str(a.fit / "fit.pt"))
        freeze_decoder(student)
        student.modules.train()
        frozen = [p.detach().clone() for p in student.dec.parameters()]
        encoder_parameters = [p for p in student.parameters() if p.requires_grad]
        optimizer = torch.optim.AdamW(encoder_parameters, lr=3e-4, weight_decay=1e-4)
        captured, handles = capture(student)
        destination = a.out / mode
        destination.mkdir()
        rows, start = [], time.monotonic()
        last_grad = 0.
        for step in range(a.steps+1):
            if step:
                optimizer.zero_grad(set_to_none=True)
                estimate = student.separate_stft(X)
                sh, sg = states(torch, student, captured)
                training_loss = (objective(torch, estimate, E, eligible, n) if mode == "audio"
                                 else coefficient*state_loss(sh, sg, ht, gt))
                if not bool(torch.isfinite(training_loss)):
                    raise ValueError("nonfinite student loss")
                training_loss.backward()
                last_grad = float(torch.nn.utils.clip_grad_norm_(encoder_parameters, 5.))
                if not np.isfinite(last_grad):
                    raise ValueError("nonfinite student gradient")
                optimizer.step()
            if step % 250 == 0 or step == a.steps:
                with torch.no_grad():
                    estimate = student.separate_stft(X)
                    sh, sg = states(torch, student, captured)
                    waveform = SN.istft(torch, estimate, n)
                    row = dict(step=step, audio_loss=float(objective(torch, estimate, E, eligible, n)),
                        state_loss=float(state_loss(sh, sg, ht, gt)),
                        sdr_db=SN.si_sdr(torch, waveform, eligible).mean(-1).tolist(),
                        reconstruction_nse=((waveform-eligible).square().sum(dim=(1,2))/
                            eligible.square().sum(dim=(1,2))).tolist(),
                        grad_norm_before_clip=last_grad, seconds=time.monotonic()-start)
                rows.append(row)
                write_json(destination / "progress.json", dict(rows=rows))
                torch.save(checkpoint(student, 10000+step), destination / "fit.pt")
                print(f"{mode} step {step} audio_loss {row['audio_loss']:.6f} "
                      f"state_loss {row['state_loss']:.6f} median_sdr {np.median(row['sdr_db']):.2f} "
                      f"elapsed {row['seconds']:.0f}s", flush=True)
        unchanged = all(torch.equal(p.detach(), before)
                        for p, before in zip(student.dec.parameters(), frozen))
        if not unchanged:
            raise ValueError("frozen decoder changed")
        for handle in handles:
            handle.remove()
        result = dict(mode=mode, decoder_unchanged=unchanged, final=rows[-1], initial=rows[0])
        results.append(result)
        write_json(destination / "complete.json", result)
    base, new = results[0]["final"], results[1]["final"]
    delta = np.array(new["sdr_db"])-np.array(base["sdr_db"])
    gates = dict(audio_loss=new["audio_loss"] <= .9*base["audio_loss"],
        median_sdr=bool(np.median(new["sdr_db"]) >= np.median(base["sdr_db"])+1),
        per_window=int((delta >= 0).sum()) >= 3 and float(delta.min()) >= -1,
        state_mechanism=new["state_loss"] <= .5*results[1]["initial"]["state_loss"])
    write_json(a.out / "complete.json", dict(status="complete", scope="TRAIN only, not a product candidate",
        arms=results, gates=gates, diagnostic_success=all(gates.values()),
        per_window_sdr_delta_db=delta.tolist(), state_coefficient=coefficient))


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "fit", "teacher-reference", "out"):
        p.add_argument("--"+name, type=Path, required=True)
    p.add_argument("--steps", type=int, default=2000)
    p.add_argument("--prepared-targets", type=Path)
    a = p.parse_args()
    if a.steps < 1:
        p.error("positive steps required")
    run(a)
