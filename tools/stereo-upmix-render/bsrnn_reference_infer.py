#!/usr/bin/env python3
"""Strict, fixed-TRAIN pretrained inference using the unmodified upstream BSRNN.

No optimizer, fine-tuning, renderer, DEV selection, fitted gain or clipping.
Run only under the exclusive PC GPU runner in pc/run_bsrnn_reference.ps1.
"""
import argparse
import builtins
import collections
import gc
import hashlib
import importlib.metadata
import json
from pathlib import Path
import subprocess
import sys
import time
import typing

import numpy as np
import soundfile as sf
import torch
import torchaudio
from omegaconf import OmegaConf
from omegaconf.base import ContainerMetadata, Metadata
from omegaconf.dictconfig import DictConfig
from omegaconf.listconfig import ListConfig
from omegaconf.nodes import AnyNode


REV = "94adc8c184ac9b1d2e496fcb6e78cfe3fb78fc9c"
TARGETS = ["vocals", "bass", "drums", "other"]
SAFE = [ContainerMetadata, Metadata, DictConfig, ListConfig, AnyNode,
        typing.Any, collections.defaultdict, builtins.int, builtins.dict,
        builtins.list]
SAFE_NAMES = {f"{x.__module__}.{x.__qualname__}" for x in SAFE}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        while b := f.read(8 * 1024 * 1024):
            h.update(b)
    return h.hexdigest()


def write_json(path, value):
    new = path.with_suffix(path.suffix + ".new")
    new.write_text(json.dumps(value, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    new.replace(path)


def read_crop(path, start, samples):
    # Plain reads: WinBtrfs cache mmap has caused access violations in this project.
    with path.open("rb") as f:
        version = np.lib.format.read_magic(f)
        reader = (np.lib.format.read_array_header_1_0 if version == (1, 0)
                  else np.lib.format.read_array_header_2_0)
        shape, fortran, dtype = reader(f)
        offset = f.tell()
        if (shape[:2] != (7, 2) or fortran or dtype != np.dtype("int16")
                or start < 0 or start + samples > shape[2]):
            raise RuntimeError(f"Unexpected cache layout/crop: {path.name}")
        out = np.empty((7, 2, samples), dtype=np.int16)
        for g in range(7):
            for c in range(2):
                f.seek(offset + ((g * 2 + c) * shape[2] + start) * 2)
                out[g, c] = np.frombuffer(f.read(samples * 2), dtype=np.int16)
    return out


def save_audio(path, audio, sr=48000):
    a = audio.detach().cpu().numpy().astype(np.float32)
    if a.ndim != 2 or a.shape[0] != 2 or not np.isfinite(a).all():
        raise RuntimeError(f"Invalid audio: {path}")
    sf.write(path, a.T, sr, subtype="FLOAT")
    readback, rate = sf.read(path, dtype="float32", always_2d=True)
    if rate != sr or not np.array_equal(a.T, readback):
        raise RuntimeError(f"Float WAV readback differs: {path}")
    return {"path": str(path), "sha256": sha(path), "samples": a.shape[-1],
            "peak": float(np.abs(a).max()),
            "samples_over_unit_peak": int((np.abs(a) > 1).sum()),
            "rms": float(np.sqrt(np.mean(a.astype(np.float64) ** 2))),
            "float32_readback_exact": True}


def acoustic_sanity(est, ref):
    # Descriptive TRAIN-only numbers; not source attribution or a product gate.
    e, r = est.double(), ref.double()
    nsdr = 10 * torch.log10((r.square().sum() + 1e-12) /
                            ((e - r).square().sum() + 1e-12))
    e = e - e.mean(-1, keepdim=True)
    r = r - r.mean(-1, keepdim=True)
    proj = ((e * r).sum(-1, keepdim=True) /
            (r.square().sum(-1, keepdim=True) + 1e-9)) * r
    sisdr = 10 * torch.log10((proj.square().sum(-1) + 1e-9) /
                             ((e - proj).square().sum(-1) + 1e-9))
    return {"other_nsdr_db": float(nsdr),
            "other_si_sdr_channel_mean_db": float(sisdr.mean())}


def load_strict(path, expected, targets, code):
    from models.bsrnn import BSRNN
    if not path.is_file() or sha(path) != expected["sha256"]:
        raise RuntimeError(f"Checkpoint absent/hash changed: {path}")
    if path.stat().st_size != expected["bytes"]:
        raise RuntimeError("Checkpoint size changed")
    unsafe = set(torch.serialization.get_unsafe_globals_in_checkpoint(path))
    if not unsafe <= SAFE_NAMES:
        raise RuntimeError(f"Unapproved pickle globals: {sorted(unsafe - SAFE_NAMES)}")
    # Public Lightning bundles include optimizer defaultdict objects. PyTorch
    # 2.11's weights-only unpickler rejects SETITEM on them even when allowlisted.
    # Deserialize this trusted, hash-verified publisher artifact explicitly,
    # after screening its nondefault globals; never accept arbitrary checkpoints.
    ckpt = torch.load(path, map_location="cpu", weights_only=False)
    hp = dict(ckpt["hyper_parameters"])
    saved_hp = OmegaConf.to_container(OmegaConf.create(hp), resolve=True)
    # Older public weights have win_dur; current PLModule requires sdr_win/hop.
    # Evaluation metadata only. All model architecture kwargs remain as saved.
    hp["cfg_eval"] = OmegaConf.merge(
        OmegaConf.load(code / "conf/eval/default.yaml"), hp["cfg_eval"])
    state = ckpt["state_dict"]
    origin = {"epoch": ckpt["epoch"], "global_step": ckpt["global_step"],
              "lightning_version": ckpt["pytorch-lightning_version"]}
    del ckpt
    gc.collect()  # Discard optimizer states before constructing the model.
    model = BSRNN(**hp)
    if list(model.targets) != targets or model.sample_rate != 44100:
        raise RuntimeError(f"Checkpoint source/rate mismatch: {model.targets}")
    for k, v in state.items():
        if not torch.isfinite(v).all():
            raise RuntimeError(f"Nonfinite checkpoint tensor: {k}")
    result = model.load_state_dict(state, strict=True)
    if result.missing_keys or result.unexpected_keys:
        raise RuntimeError("Incomplete pretrained load")
    if any(not torch.equal(v, model.state_dict()[k]) for k, v in state.items()):
        raise RuntimeError("Post-load tensor differs from checkpoint")
    audit = {"path": str(path), "sha256": expected["sha256"],
             "strict": True, "missing_keys": [], "unexpected_keys": [],
             "state_tensors": len(state), "all_loaded_tensors_exact": True,
             "parameters": sum(p.numel() for p in model.parameters()),
             "targets": targets, "saved_hyperparameters": saved_hp,
             "eval_metadata_patch": "defaults supply sdr_win/sdr_hop",
             "pickle_mode": "trusted publisher SHA-256 + screened globals; weights_only=False",
             "origin": origin}
    del state
    gc.collect()
    model.eval().requires_grad_(False).to("cuda")
    return model, audit


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--root", type=Path, required=True)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    code = a.root / "code"
    actual = subprocess.check_output(["git", "-C", str(code), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(["git", "-C", str(code), "status", "--porcelain"], text=True).strip()
    if actual != REV or dirty:
        raise RuntimeError("Upstream source revision/cleanliness changed")
    sys.path.insert(0, str(code))
    if not torch.cuda.is_available():
        raise RuntimeError("CUDA unavailable")
    torch.set_num_threads(4)
    torch.backends.cuda.matmul.allow_tf32 = False
    torch.backends.cudnn.allow_tf32 = False
    manifest = json.loads(a.manifest.read_text(encoding="utf-8"))
    split_path = a.cache / "split.json"
    if sha(split_path) != manifest["split_sha256"]:
        raise RuntimeError("Frozen split changed")
    split = json.loads(split_path.read_text())["split"]
    train = {song for song, partition in split.items() if partition == "train"}
    if len(manifest["windows"]) != 4:
        raise RuntimeError("Expected the four frozen TRAIN crops")
    a.out.mkdir(parents=True, exist_ok=False)
    runtime = {name: importlib.metadata.version(name) for name in
               ["torch", "torchaudio", "lightning", "pytorch-lightning", "omegaconf",
                "numpy", "soundfile", "pandas", "museval", "musdb"]}
    summary = {"scope": "pretrained fixed TRAIN inference only; no fine-tuning",
               "source_revision": REV, "script_sha256": sha(__file__),
               "crop_manifest_sha256": sha(a.manifest), "runtime": runtime,
               "python": sys.executable, "torch_location": torch.__file__,
               "device": torch.cuda.get_device_name(), "sample_rate": 48000,
               "gain": "peak normalize 44.1k input once; restore same scalar to outputs; no fitted gain",
               "target_mapping": {"vocals": [0, 1], "bass": [5],
                                  "drums": [2, 3, 4], "other": [6]},
               "normalization_causality": "noncausal upstream, not an Audi candidate",
               "windows": [], "models": [], "inference": []}
    crops = []
    for w in manifest["windows"]:
        if w["song"] not in train:
            raise RuntimeError("Crop is not TRAIN")
        raw = read_crop(a.cache / (w["song"] + ".npy"), w["start_sample"], w["samples"])
        if hashlib.sha256(raw.tobytes()).hexdigest() != w["selected_int16_sha256"]:
            raise RuntimeError("Frozen crop bytes differ")
        stems = torch.from_numpy(raw.astype(np.float32) / 32768.0)
        mix = stems.sum(0)
        refs = {"vocals": stems[0] + stems[1], "bass": stems[5],
                "drums": stems[2:5].sum(0), "other": stems[6]}
        mix44 = torchaudio.functional.resample(mix, 48000, 44100)
        peak = float(mix44.abs().max())
        if peak == 0:
            raise RuntimeError("Silent crop")
        folder = a.out / w["song"]
        folder.mkdir()
        record = {**w, "start_seconds": w["start_sample"] / 48000,
                  "input44_peak": peak, "input44_samples": mix44.shape[-1],
                  "mixture": save_audio(folder / "mixture.wav", mix),
                  "truth": {t: save_audio(folder / ("truth_" + t + ".wav"), r)
                            for t, r in refs.items()}}
        summary["windows"].append(record)
        crops.append((w, mix44 / peak, peak, refs, folder))
    intake = json.loads((a.root / "INTAKE_COMPLETE.json").read_text())
    jobs = []
    for bundle in intake["bundles"]:
        for entry in bundle["extracted"]["checkpoints"]:
            path = a.root / "weights" / bundle["name"] / entry["path"]
            jobs.append((bundle["name"], path, entry["hashes"],
                         TARGETS if bundle["name"] == "simo-bsrnn-opt" else [path.stem]))
    if len(jobs) != 5:
        raise RuntimeError("Expected both complete pretrained bundles")
    for tag, path, hashes, targets in jobs:
        print("LOADING", tag, targets, flush=True)
        model, audit = load_strict(path, hashes, targets, code)
        summary["models"].append(audit)
        write_json(a.out / "progress.json", summary)
        for w, normalized, peak, refs, folder in crops:
            torch.cuda.reset_peak_memory_stats()
            torch.cuda.synchronize()
            started = time.perf_counter()
            with torch.inference_mode():
                out = model(normalized.unsqueeze(0).to("cuda"))["waveforms"]
            torch.cuda.synchronize()
            elapsed = time.perf_counter() - started
            if tuple(out.shape) != (1, len(targets), 2, normalized.shape[-1]):
                raise RuntimeError(f"Output shape mismatch: {out.shape}")
            out = torchaudio.functional.resample(out[0].cpu() * peak, 44100, 48000)
            if out.shape[-1] != w["samples"] or not torch.isfinite(out).all():
                raise RuntimeError("Output duration/finiteness failed")
            dest = folder / tag
            dest.mkdir(exist_ok=True)
            record = {"model": tag, "targets": targets, "song": w["song"],
                      "gpu_forward_seconds": elapsed,
                      "gpu_peak_allocated_bytes": torch.cuda.max_memory_allocated(),
                      "outputs": {t: save_audio(dest / (t + ".wav"), out[i])
                                  for i, t in enumerate(targets)}}
            if "other" in targets:
                record.update(acoustic_sanity(out[targets.index("other")], refs["other"]))
            summary["inference"].append(record)
            write_json(a.out / "progress.json", summary)
            print("INFERRED", tag, targets, w["song"], f"{elapsed:.3f}s", flush=True)
            del out
        del model
        gc.collect()
        torch.cuda.empty_cache()
    summary["complete"] = True
    summary["audio_files"] = 52  # Four mixtures, 16 truth stems, 32 predictions.
    if len(summary["inference"]) != 20 or len(list(a.out.rglob("*.wav"))) != 52:
        raise RuntimeError("Output inventory incomplete")
    write_json(a.out / "complete.json", summary)
    print("COMPLETE", str(a.out), flush=True)


if __name__ == "__main__":
    main()
