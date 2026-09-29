#!/usr/bin/env python3
"""CPU checkpoint replay and predeclared finite-TRAIN geometry gates."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import mask_net as MN
import sep_net as SN
import separator_band_geometry as BG
from separator_fit_probe import objective, sha, write_json


def run(a):
    if a.out.exists():
        raise FileExistsError("preserve previous audit")
    control = json.loads((a.control / "fit_complete.json").read_text())
    candidate = json.loads((a.candidate / "complete.json").read_text())
    cm = json.loads((a.control / "manifest.json").read_text())
    nm = json.loads((a.candidate / "manifest.json").read_text())
    checks = {
        "control_result": nm["control_sha256"] == sha(a.control / "fit_complete.json"),
        "control_checkpoint": nm["control_checkpoint_sha256"] == sha(a.control / "fit.pt"),
        "windows": nm["windows"] == cm["windows"],
        "geometry": nm["band_geometry"] == "redistributed" and nm["band_widths"] == BG.WIDTHS["redistributed"],
        "geometry_source": nm["geometry_sha256"] == sha(Path(BG.__file__)),
        "training_source": nm["script_sha256"] == sha(Path(__file__).with_name("separator_band_probe.py")),
        "separator_source": nm["separator_sha256"] == sha(Path(SN.__file__)) == cm["separator_sha256"],
        "horizon": control["step"] == candidate["final"]["step"] == nm["steps"] == 10000,
        "split": sha(a.cache / "split.json") == nm["split_sha256"] == cm["split_sha256"],
    }
    if not all(checks.values()):
        raise ValueError(f"frozen experiment changed: {checks}")
    split = MN.load_split(str(a.cache))
    arrays = []
    for r in nm["windows"]:
        if r["song"] not in split["train"]:
            raise ValueError("not TRAIN")
        raw = MN.expand_slots(MN.read_window(str(a.cache / (r["song"]+".npy")), r["start_sample"], r["samples"]))
        if hashlib.sha256(raw.tobytes()).hexdigest() != r["selected_int16_sha256"]:
            raise ValueError("crop changed")
        arrays.append(raw.astype(np.float32)/32768)
    torch, net, device = BG.load(str(a.candidate / "fit.pt"), device="cpu")
    torch.set_num_threads(4)
    base_net = BG.make(torch, "original")
    params = sum(p.numel() for p in net.parameters())
    if params != sum(p.numel() for p in base_net.parameters()) or net.macs != base_net.macs:
        raise ValueError("nominal budget changed")
    if params != nm["parameters"] or net.macs*SN.FPS != nm["estimated_neural_macs_s"]:
        raise ValueError("budget metadata wrong")
    generic_rejects = False
    ck = torch.load(a.candidate / "fit.pt", map_location="cpu")
    try:
        base_net.modules.load_state_dict(ck["state"])
    except RuntimeError:
        generic_rejects = True
    if not generic_rejects:
        raise ValueError("changed geometry must reject original-shaped loader")
    w = torch.as_tensor(np.stack(arrays), device=device)
    mix, eligible = w.sum(1), w[:,6]
    X, E = SN.stft(torch, mix), SN.stft(torch, eligible)
    with torch.no_grad():
        estimate = net.separate_stft(X)
        n = mix.shape[-1]
        replay_loss = float(objective(torch, estimate, E, eligible, n))
        replay_sdr = SN.si_sdr(torch, SN.istft(torch, estimate, n), eligible).mean(-1).numpy()
    final = candidate["final"]
    if abs(replay_loss-final["loss"]) > 1e-4 or np.max(np.abs(replay_sdr-final["sdr_db"])) > .01:
        raise ValueError("CPU checkpoint replay differs")
    base = control["final"]
    delta = np.array(final["sdr_db"])-np.array(base["sdr_db"])
    gates = dict(loss=final["loss"] <= .9*base["loss"],
                 median_sdr=bool(np.median(final["sdr_db"]) >= np.median(base["sdr_db"])+1),
                 per_window=int((delta >= 0).sum()) >= 3 and float(delta.min()) >= -1)
    result = dict(scope="one seed, four TRAIN windows; not product promotion", checks=checks,
        parameters=params, estimated_neural_macs_s=net.macs*SN.FPS,
        original_band_widths=BG.WIDTHS["original"], candidate_band_widths=BG.WIDTHS["redistributed"],
        control_loss=base["loss"], candidate_loss=final["loss"], loss_ratio=final["loss"]/base["loss"],
        control_median_sdr_db=float(np.median(base["sdr_db"])),
        candidate_median_sdr_db=float(np.median(final["sdr_db"])),
        control_sdr_db=base["sdr_db"], candidate_sdr_db=final["sdr_db"], per_window_sdr_delta_db=delta.tolist(),
        control_waveform_nse=base["reconstruction_nse"], candidate_waveform_nse=final["reconstruction_nse"],
        gates=gates, diagnostic_success=all(gates.values()), cpu_checkpoint_loss=replay_loss,
        cpu_checkpoint_sdr_db=replay_sdr.tolist(), generic_original_loader_rejects=generic_rejects,
        candidate_checkpoint_sha256=sha(a.candidate / "fit.pt"),
        candidate_complete_sha256=sha(a.candidate / "complete.json"), script_sha256=sha(Path(__file__)))
    write_json(a.out, result)
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("cache", "control", "candidate", "out"):
        p.add_argument("--"+name, type=Path, required=True)
    run(p.parse_args())
