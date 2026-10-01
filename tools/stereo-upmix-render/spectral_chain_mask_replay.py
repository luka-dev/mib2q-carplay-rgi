#!/usr/bin/env python3
"""CPU replay and actual-mixture mask attribution for the standalone C chain.

The network runs once on rounded PCM16 mixture. Its recorded float/int8 masks
are then applied to true sources, never by running the network on each source.
PCM rounding/sum residual is a separate component; true labels stay unchanged.
Offline scoring compensates only the fixed1536sample array delay. The actual
C output remains delayed and is preserved. No device/ear qualification here.
"""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

import numpy as np
import torch
import unified_upmix_stream as S


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for part in iter(lambda: handle.read(1024*1024), b""):
            digest.update(part)
    return digest.hexdigest()


def read_samples(path, magic, dtype, channels, streams):
    with Path(path).open("rb") as handle:
        header = handle.read(20)
        if len(header) != 20:
            raise ValueError("Truncated sample header")
        tag, batch, samples, count = struct.unpack("<8sIII", header)
        if tag != magic or batch != streams or count != channels or not samples:
            raise ValueError("Unexpected sample layout")
        if os.fstat(handle.fileno()).st_size != 20+batch*samples*channels*np.dtype(dtype).itemsize:
            raise ValueError("Truncated or trailing sample bytes")
        data = np.fromfile(handle, dtype=dtype)
    if data.size != batch*samples*channels:
        raise ValueError("Truncated or trailing sample data")
    if not np.isfinite(data).all():
        raise ValueError("Nonfinite sample data")
    return data.reshape(batch, samples, channels)


def read_masks(path, streams, hops):
    with Path(path).open("rb") as handle:
        header = handle.read(32)
        if len(header) != 32 or struct.unpack("<8s6I", header) != (b"UMXMSK01", 1, streams, hops, 1025, 2, 2):
            raise ValueError("Unexpected actual-mixture mask layout")
        if os.fstat(handle.fileno()).st_size != 32+streams*hops*1025*2*2*4:
            raise ValueError("Truncated or trailing mask bytes")
        data = np.fromfile(handle, dtype="<f4")
    if data.size != streams*hops*1025*2*2 or not np.isfinite(data).all():
        raise ValueError("Truncated, trailing or nonfinite mask data")
    return data.view("<c8").reshape(streams, hops, 1025, 2)


def energy_nse(value, reference):
    value = torch.as_tensor(value); reference = torch.as_tensor(reference)
    den = reference.abs().double().square().sum()
    num = (value-reference).abs().double().square().sum()
    return float(num/den) if den > 1e-12 else None


def round_input(mixture):
    if mixture.dtype != np.float32 or mixture.ndim != 2 or mixture.shape[0] != 2 or not mixture.shape[1]:
        raise ValueError("Nonempty float32 stereo [2,N] required")
    if not np.isfinite(mixture).all() or np.max(np.abs(mixture)) >= 1:
        raise ValueError("Input would clip; no gain rescue")
    rounded = np.rint(mixture.T*32768)
    if rounded.min() < -32768 or rounded.max() > 32767:
        raise ValueError("PCM16 rounding would overflow")
    return rounded.astype("<i2")


def execute(executable, model, mixture, out):
    out = Path(out)
    if out.exists():
        raise FileExistsError("Preserve earlier replay")
    out.mkdir(parents=True)
    paths = {name: out/name for name in ("input.pcm", "output6.pcm", "source2.pcm", "source.float", "masks.float")}
    pcm = round_input(mixture)
    n = mixture.shape[1]
    paths["input.pcm"].write_bytes(struct.pack("<8sIII", b"UMX16I01", 1, n, 2)+pcm.tobytes())
    command = [str(executable), str(model), *(str(p) for p in paths.values())]
    result = subprocess.run(command, capture_output=True, text=True)
    (out/"stderr.log").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError("Native chain failed; partial own artifacts retained")
    performance = json.loads(result.stderr)
    hops = (n+1536+512+511)//512
    if performance["frames"] != hops or performance["streams"] != 1:
        raise ValueError("Native frame/stream count differs")
    if performance["old_neural_invocations"] or performance["old_blobs"] or performance["worker_stack_bytes"] != 262144:
        raise ValueError("Single-network/worker contract differs")
    if performance["neural_dense_macs_s"] != (0 if str(model) == "identity" else 116712000):
        raise ValueError("Selected graph dense count differs")
    output = read_samples(paths["output6.pcm"], b"UMX16O01", "<i2", 6, 1)
    source = read_samples(paths["source2.pcm"], b"UMX16S01", "<i2", 2, 1)
    floating = read_samples(paths["source.float"], b"UMXFLS01", "<f4", 2, 1)
    masks = read_masks(paths["masks.float"], 1, hops)
    if output.shape[1] != hops*512 or source.shape[1] != hops*512 or floating.shape[1] != hops*512:
        raise ValueError("Native flush length differs")
    rounded_source = np.rint(floating*32768)
    if not np.array_equal(rounded_source, source):
        raise ValueError("Source PCM16 is not once-rounded native float")
    return {"original_mixture": mixture.copy(), "input_pcm16": pcm, "output_pcm16": output, "source_pcm16": source,
            "source_float": floating, "masks": masks, "performance": performance,
            "hashes": {name: sha(path) for name, path in paths.items()}, "paths": paths, "samples": n}


@torch.inference_mode()
def attribute(replay, sources):
    sources = torch.as_tensor(sources)
    if sources.device.type != "cpu" or sources.dtype != torch.float32 or sources.ndim != 3 or sources.shape[1:] != (2, replay["samples"]):
        raise ValueError("CPU float32 true sources [K,2,N] required")
    if not torch.isfinite(sources).all():
        raise ValueError("Nonfinite true source")
    n = replay["samples"]; emitted = replay["source_float"].shape[1]
    mixture = torch.from_numpy(replay["input_pcm16"].astype(np.float32).T/32768)
    original = torch.from_numpy(replay["original_mixture"])
    summed = sources.sum(0)
    if not torch.allclose(summed, original, atol=2e-7, rtol=2e-6):
        raise ValueError("True sources do not sum to the original unrounded mixture")
    residual = mixture-summed
    original_delta = float((summed-original).abs().max()*32768)
    if float(residual.abs().max()*32768) > .5+original_delta+2e-4:
        raise ValueError("Input residual exceeds PCM rounding plus original sum error")
    mask = torch.from_numpy(replay["masks"])
    def apply(wave):
        spectrum = S.analyze(torch.nn.functional.pad(wave[None], (0, 512)))
        if spectrum.shape != mask.shape:
            raise ValueError("Actual native mask and past-only analysis frames differ")
        return S.synthesize(mask*spectrum, emitted, compensate=False)[0]
    expected = apply(mixture)
    actual = torch.from_numpy(replay["source_float"][0].T.copy())
    native_nse = energy_nse(expected, actual)
    maximum_lsb = float((expected-actual).abs().max()*32768)
    if (native_nse is not None and native_nse > 1e-7) or maximum_lsb > 4:
        raise ValueError("Actual-mask replay differs from native source; preserve evidence")
    components = [apply(wave)[:, 1536:1536+n] for wave in sources]
    quantized_component = apply(residual)[:, 1536:1536+n]
    target = expected[:, 1536:1536+n]
    total = torch.stack(components).sum(0)+quantized_component
    closure = energy_nse(total, target)
    if closure is not None and closure > 1e-9:
        raise ValueError("True sources plus input residual do not close under the actual mask")
    if closure is None and float((total-target).abs().max()) > 1e-6:
        raise ValueError("Silent-source closure differs")
    parity = {"native_source_replay_nse": native_nse,
              "native_source_replay_max_pcm16_lsb": maximum_lsb,
              "true_sources_plus_input_residual_closure_nse": closure,
              "true_sources_only_closure_nse": energy_nse(torch.stack(components).sum(0), target),
              "input_residual_max_pcm16_lsb": float(residual.abs().max()*32768),
              "original_source_sum_vs_mixture_max_pcm16_lsb": original_delta,
              "original_source_sum_vs_mixture_nse": energy_nse(summed, original),
              "input_residual_energy": float(residual.double().square().sum()),
              "offline_scoring_alignment_samples": 1536,
              "native_emitted_samples": emitted,
              "scope": "Actual PCM16 mixture masks from native float/int8; true labels unchanged, input rounding/sum residual separate; fixed offline array-delay alignment only, actual C remains delayed; no physical-source audibility or device claim"}
    # Record residual power relative to the estimate, with undefined coverage
    # when the estimate has no energy; never present missing coverage as zero.
    den = actual[:, 1536:1536+n].double().square().sum()
    parity["residual_component_power_relative_to_estimate"] = float(quantized_component.double().square().sum()/den) if den > 1e-12 else None
    return {"estimate": actual[:, 1536:1536+n], "components": torch.stack(components),
            "input_residual_component": quantized_component,
            "output6": replay["output_pcm16"][0, 1536:1536+n].T.astype(np.float32)/32768,
            "parity": parity}
