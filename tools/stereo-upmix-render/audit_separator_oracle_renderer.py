#!/usr/bin/env python3
"""Exact TRAIN reference versus complex-ratio reference through the real C renderer.

CPU only. This checks metric meaning and renderer input parity, not generalization,
causal inference, source protection in the car, or a deployable oracle.
"""
import argparse
import hashlib
import json
import subprocess
import wave
from pathlib import Path

import numpy as np
import mask_net as MN
import sep_net as SN
import upmix_behavior_tests as T
from separator_fit_probe import sha, write_json


def pcm(path):
    with wave.open(str(path), "rb") as f:
        return np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").astype(np.int32)


def run(a):
    if a.out.exists():
        raise FileExistsError("preserve previous results; choose a new output")
    manifest = json.loads((a.fit / "manifest.json").read_text())
    if sha(a.cache / "split.json") != manifest["split_sha256"]:
        raise ValueError("cache split changed")
    arrays = []
    for item in manifest["windows"]:
        raw = MN.expand_slots(MN.read_window(str(a.cache / (item["song"] + ".npy")),
                                           item["start_sample"], item["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != item["selected_int16_sha256"]:
            raise ValueError("TRAIN crop changed")
        arrays.append(raw.astype(np.float32) / 32768)
    a.out.mkdir(parents=True)
    torch = SN._torch()
    torch.set_num_threads(4)
    w = torch.as_tensor(np.stack(arrays), device="cpu")
    mix, target = w.sum(1), w[:, 6]
    n = mix.shape[-1]
    with torch.no_grad():
        X, E = SN.stft(torch, mix), SN.stft(torch, target)
        M = E * X.conj() / (X.abs().square() + 1e-12)
        estimate = SN.istft(torch, M * X, n).numpy()
    mix, target = mix.numpy(), target.numpy()
    gain = .7 / max(float(np.abs(x).max()) for x in (mix, target, estimate))
    args = T.VARIANTS["shipped"] + ["--model", str(a.model), "--guard-model", str(a.guard),
                                    "--mask-drive", "1.0"]
    result = dict(scope="four fixed TRAIN windows; CPU; historical harness shipped knobs",
                  common_gain=gain, renderer_sha256=sha(a.render),
                  model_sha256=sha(a.model), guard_sha256=sha(a.guard),
                  script_sha256=sha(Path(__file__)), render_args=args, rows=[])
    for i, item in enumerate(manifest["windows"]):
        src = a.out / f"{i}.mix.wav"
        T.write_wav(str(src), mix[i] * gain)
        injections, outputs = [], []
        for label, rear in (("direct", target[i]), ("ratio", estimate[i])):
            inj, dst = a.out / f"{i}.{label}.rear.wav", a.out / f"{i}.{label}.6ch.wav"
            T.write_wav(str(inj), rear * gain)
            subprocess.run([str(a.render)] + args + ["--rear-audio", str(inj), str(src), str(dst)],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            injections.append(pcm(inj))
            outputs.append(T.read_wav6(str(dst)))
        diff = outputs[1] - outputs[0]
        row = dict(song=item["song"],
                   input_pcm_max_lsb=int(np.abs(injections[1] - injections[0]).max()),
                   input_pcm_changed_samples=int(np.count_nonzero(injections[1] != injections[0])),
                   six_channel_nse=float(np.square(diff).sum() / np.square(outputs[0]).sum()),
                   six_channel_max_lsb=float(np.abs(diff).max() * 32768), coverage={})
        sources = dict(eligible=arrays[i][6], vocal=arrays[i][0] + arrays[i][1],
                       drums=arrays[i][2] + arrays[i][3] + arrays[i][4], bass=arrays[i][5])
        for role, source in sources.items():
            lead = T.stft_pow(source[0]) + T.stft_pow(source[1])
            rest = mix[i] - source
            interference = T.stft_pow(rest[0]) + T.stft_pow(rest[1])
            own = lead > 10 * (interference + 1e-12)
            own[:, :int(300 * T.N_FFT / T.SR)] = False
            denominator = float(lead.sum())
            row["coverage"][role] = dict(source_energy_in_owned_cells=
                float(lead[own].sum() / denominator) if denominator > 1e-9 else None,
                owned_cells=int(own.sum()))
        result["rows"].append(row)
        print(json.dumps(row), flush=True)
    write_json(a.out / "result.json", result)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "fit", "render", "model", "guard", "out"):
        p.add_argument("--" + name, type=Path, required=True)
    run(p.parse_args())
