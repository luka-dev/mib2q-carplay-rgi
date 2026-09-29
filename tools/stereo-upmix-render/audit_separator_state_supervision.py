#!/usr/bin/env python3
"""CPU checkpoint/target/formula audit of the completed TRAIN state pair."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import mask_net as MN
import sep_net as SN
from separator_fit_probe import sha, write_json, objective
from separator_state_supervision_probe import capture, states, state_loss, decode


def load_cpu(torch, path):
    ck = torch.load(path, map_location="cpu")
    if ck.get("arch") != "band" or ck.get("norm") != "stream" or ck.get("mask") != "complex2":
        raise ValueError("frozen baseline topology required")
    net = SN.BandSepNet(torch, norm="stream", mask="complex2").to("cpu")
    net.modules.load_state_dict(ck["state"])
    net.modules.eval()
    SN.STATS.update(ck["stats"])
    return net


def run(a):
    if a.out.exists():
        raise FileExistsError("preserve earlier audit")
    manifest = json.loads((a.run / "manifest.json").read_text())
    terminal = json.loads((a.run / "complete.json").read_text())
    if sha(a.cache / "split.json") != manifest["split_sha256"]:
        raise ValueError("TRAIN split changed")
    if sha(a.fit / "fit.pt") != manifest["checkpoint_sha256"]:
        raise ValueError("original anchor changed")
    if sha(a.run / "targets.pt") != manifest["targets_sha256"]:
        raise ValueError("fixed targets changed")
    arrays = []
    for r in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(a.cache / (r["song"] + ".npy")),
                                           r["start_sample"], r["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != r["selected_int16_sha256"]:
            raise ValueError("TRAIN crop changed")
        arrays.append(raw.astype(np.float32) / 32768)
    torch = SN._torch()
    torch.set_num_threads(4)
    w = torch.as_tensor(np.stack(arrays), device="cpu")
    mix, eligible = w.sum(1), w[:, 6]
    n = mix.shape[-1]
    X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
    targets = torch.load(a.run / "targets.pt", map_location="cpu")
    if targets["windows"] != manifest["windows"] or targets["checkpoint_sha256"] != manifest["checkpoint_sha256"]:
        raise ValueError("target semantics changed")
    ht, gt = targets["h"], targets["g"]
    anchor = load_cpu(torch, a.fit / "fit.pt")
    with torch.no_grad():
        teacher = decode(torch, anchor, X, ht, gt)
        teacher_loss = float(objective(torch, teacher, E, eligible, n))
    reference = json.loads((a.run / "teacher.json").read_text())["rows"][-1]
    if abs(teacher_loss-reference["loss"]) > 1e-4:
        raise ValueError("target/decoder replay mismatch")
    results = []
    for arm in terminal["arms"]:
        path = a.run / arm["mode"] / "fit.pt"
        net = load_cpu(torch, path)
        unchanged = all(torch.equal(x.detach(), y.detach())
                        for x, y in zip(net.dec.parameters(), anchor.dec.parameters()))
        if not unchanged:
            raise ValueError("frozen decoder differs from anchor")
        c, handles = capture(net)
        with torch.no_grad():
            estimate = net.separate_stft(X)
            sh, sg = states(torch, net, c)
            audio = float(objective(torch, estimate, E, eligible, n))
            state = float(state_loss(sh, sg, ht, gt))
            sdr = SN.si_sdr(torch, SN.istft(torch, estimate, n), eligible).mean(-1).numpy()
        for h in handles:
            h.remove()
        reference = arm["final"]
        if abs(audio-reference["audio_loss"]) > 1e-4 or abs(state-reference["state_loss"]) > 1e-5:
            raise ValueError("CPU checkpoint audio/state-loss parity failed")
        if np.max(np.abs(sdr-np.array(reference["sdr_db"]))) > .01:
            raise ValueError("CPU waveform parity failed")
        results.append(dict(mode=arm["mode"], decoder_unchanged=unchanged, cpu_audio_loss=audio,
            cpu_state_loss=state, cpu_sdr_db=sdr.tolist(), checkpoint_sha256=sha(path)))
    base, new = terminal["arms"][0]["final"], terminal["arms"][1]["final"]
    delta = np.array(new["sdr_db"])-np.array(base["sdr_db"])
    gates = dict(audio_loss=new["audio_loss"] <= .9*base["audio_loss"],
        median_sdr=bool(np.median(new["sdr_db"]) >= np.median(base["sdr_db"])+1),
        per_window=int((delta >= 0).sum()) >= 3 and float(delta.min()) >= -1,
        state_mechanism=new["state_loss"] <= .5*terminal["arms"][1]["initial"]["state_loss"])
    if gates != terminal["gates"] or all(gates.values()) != terminal["diagnostic_success"]:
        raise ValueError("stored gate verdict does not match frozen rules")
    result = dict(scope="four fixed TRAIN windows; CPU target/decoder/checkpoint audit",
        teacher_cpu_loss=teacher_loss, arms=results, gates=gates,
        diagnostic_success=all(gates.values()), state_error_ratio=new["state_loss"] /
            terminal["arms"][1]["initial"]["state_loss"], audio_loss_ratio=new["audio_loss"]/base["audio_loss"],
        control_median_sdr=float(np.median(base["sdr_db"])), state_median_sdr=float(np.median(new["sdr_db"])),
        result_sha256=sha(a.run / "complete.json"), script_sha256=sha(Path(__file__)))
    write_json(a.out, result)
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "fit", "run", "out"):
        p.add_argument("--"+name, type=Path, required=True)
    run(p.parse_args())
