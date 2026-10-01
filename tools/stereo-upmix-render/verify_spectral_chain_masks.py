#!/usr/bin/env python3
"""Actual C mask observation/attribution audit, no source-quality promotion."""
import os
os.environ["CUDA_VISIBLE_DEVICES"] = "-1"
import argparse
import json
from pathlib import Path
import struct
import subprocess

import numpy as np
import torch
import spectral_chain_mask_replay as R


FIXTURE_SHA = "47dc5768a7ff38100a7d7cfbfa209fd5ea95e43101382d6e5cbf7819400e7233"


def run_batch(executable, model, input_path, folder, observe):
    folder.mkdir()
    paths = [folder/n for n in ["output6.pcm", "source2.pcm", "source.float"]]
    if observe:
        paths.append(folder/"masks.float")
    result = subprocess.run([str(executable), str(model), str(input_path), *map(str, paths)], capture_output=True, text=True)
    (folder/"stderr.log").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError("Observation comparison execution failed; files retained")
    return paths, json.loads(result.stderr)


def main(a):
    if a.out.exists():
        raise FileExistsError("Preserve previous audit")
    torch.set_num_threads(2)
    raw = a.fixture.read_bytes()
    if R.sha(a.fixture) != FIXTURE_SHA or struct.unpack_from("<8sIII", raw) != (b"UMXPC001", 4, 384000, 2):
        raise ValueError("Original four TRAIN8s mixture fixture differs")
    mixture = np.frombuffer(raw, dtype="<f4", offset=20).reshape(4, 384000, 2).copy()
    assets = json.loads((a.assets/"manifest.json").read_bytes())
    if assets["checkpoint_sha256"] != "ff072970aef01749f44afc0c8bc010c542eb7e2384cdf88d8b7473b6a3854da2":
        raise ValueError("Predeclared fitting checkpoint differs")
    for name, record in assets["files"].items():
        if R.sha(a.assets/name) != record["sha256"]:
            raise ValueError("Exported asset differs")
    a.out.mkdir(parents=True)
    protocol = {
        "scope": "Original four TRAIN8s mixtures, synthetic proportional source partitions; arithmetic/observation only, no true-stem/DEV/scene/device/ear pass",
        "fixture_sha256": FIXTURE_SHA, "assets_manifest_sha256": R.sha(a.assets/"manifest.json"),
        "old_executable_sha256": R.sha(a.old_executable), "new_executable_sha256": R.sha(a.executable),
        "script_sha256": R.sha(Path(__file__)), "library_sha256": R.sha(Path(R.__file__)),
        "numeric_gates": "All4x8s float/int8 old/new-observed PCM6/sourcePCM/unrounded source bit exact; actual-mask source replay NSE<=1e-7/max4LSB; true partitions+rounding residual closure<=1e-9; selected116.712M; prefix/reset exact",
        "rounding_case": "First original mixture's8193samples multiplied by.913, synthetic.4/.6 partitions; not a candidate gain or source quality comparison",
        "malformed_case": "One trailing byte rejected for mask and sample formats",
    }
    (a.out/"protocol.json").write_text(json.dumps(protocol, indent=2)+"\n")
    pcm = np.stack([R.round_input(x.T) for x in mixture])
    input_path = a.out/"all4.input.pcm"
    input_path.write_bytes(struct.pack("<8sIII", b"UMX16I01", 4, 384000, 2)+pcm.tobytes())
    results = {}; gates = {}
    for mode in ["float", "int8"]:
        model = a.assets/("unified_"+mode+".bin")
        old, old_perf = run_batch(a.old_executable, model, input_path, a.out/(mode+"_old"), False)
        new, new_perf = run_batch(a.executable, model, input_path, a.out/(mode+"_observed"), True)
        equality = {old_path.name: R.sha(old_path) == R.sha(new_path) for old_path, new_path in zip(old, new)}
        gates[mode+"_observation_bit_exact"] = all(equality.values())
        gates[mode+"_count_contract"] = all(p["neural_dense_macs_s"] == 116712000 and p["frames"] == 3016 and p["old_neural_invocations"] == 0 and p["old_blobs"] == 0 for p in [old_perf, new_perf])
        masks = R.read_masks(new[-1], 4, 754)
        floating = R.read_samples(new[2], b"UMXFLS01", "<f4", 2, 4)
        out6 = R.read_samples(new[0], b"UMX16O01", "<i2", 6, 4)
        source = R.read_samples(new[1], b"UMX16S01", "<i2", 2, 4)
        parity = []
        for i in range(4):
            replay = {"masks": masks[i:i+1], "source_float": floating[i:i+1],
                      "output_pcm16": out6[i:i+1], "source_pcm16": source[i:i+1],
                      "input_pcm16": pcm[i], "samples": 384000, "original_mixture": mixture[i].T.copy()}
            x = torch.from_numpy(mixture[i].T.copy())
            value = R.attribute(replay, torch.stack([x*.4, x*.6]))
            parity.append(value["parity"])
        gates[mode+"_mask_replay_and_partition_closure"] = all(p["native_source_replay_nse"] <= 1e-7 and p["native_source_replay_max_pcm16_lsb"] <= 4 and p["true_sources_plus_input_residual_closure_nse"] <= 1e-9 for p in parity)
        varied = mixture[0, :8193].T.copy()*np.float32(.913)
        base = R.execute(a.executable, model, varied, a.out/(mode+"_partial_rounding"))
        x = torch.from_numpy(varied)
        parts = R.attribute(base, torch.stack([x*.4, x*.6]))
        gates[mode+"_nontrivial_rounding_exercised"] = parts["parity"]["input_residual_max_pcm16_lsb"] >= .49
        repeat = R.execute(a.executable, model, varied, a.out/(mode+"_repeat"))
        gates[mode+"_reset_exact"] = all(np.array_equal(base[k], repeat[k]) for k in ["output_pcm16", "source_pcm16", "source_float", "masks"])
        future = varied.copy(); future[:, 4096:] *= np.float32(-.8)
        changed = R.execute(a.executable, model, future, a.out/(mode+"_future"))
        gates[mode+"_future_audio_prefix_exact"] = all(np.array_equal(base[k][:, :4096], changed[k][:, :4096]) for k in ["output_pcm16", "source_pcm16", "source_float"])
        gates[mode+"_future_mask_prefix_exact"] = np.array_equal(base["masks"][:, :8], changed["masks"][:, :8])
        results[mode] = {"bit_equality": equality, "old_performance": old_perf, "observed_performance": new_perf,
                         "all4_actual_mask_parity": parity, "partial_rounding_parity": parts["parity"],
                         "mask_sha256": R.sha(new[-1]), "partial_hashes": base["hashes"]}
        print("MASK_OBSERVATION_ARM", mode, json.dumps({k: v for k, v in gates.items() if k.startswith(mode)}), flush=True)
    silence = R.execute(a.executable, "identity", np.zeros((2, 4097), dtype=np.float32), a.out/"identity_silent_partial")
    identity = R.attribute(silence, torch.zeros(2, 2, 4097))
    gates["identity_silent_mask_and_source"] = bool(np.all(silence["masks"] == 1) and np.all(silence["source_float"] == 0) and identity["parity"]["true_sources_plus_input_residual_closure_nse"] is None)
    for name, source_path, reader in [
        ("mask", new[-1], lambda p: R.read_masks(p, 4, 754)),
        ("sample", new[0], lambda p: R.read_samples(p, b"UMX16O01", "<i2", 6, 4)),
    ]:
        malformed = a.out/("malformed_"+name)
        malformed.write_bytes(source_path.read_bytes()+b"x")
        try:
            reader(malformed)
        except ValueError:
            gates[name+"_trailing_byte_rejected"] = True
        else:
            gates[name+"_trailing_byte_rejected"] = False
    result = {"complete": True, "passed": all(gates.values()), "gates": gates, "arms": results,
              "protocol_sha256": R.sha(a.out/"protocol.json"), "product_pass": False,
              "scope": protocol["scope"], "limits": "Proportional partitions do not test actual stem discrimination; no broad int8 quality, source protection, full60s, device timing or owner CarPlay result."}
    (a.out/"complete.json").write_text(json.dumps(result, indent=2)+"\n")
    if not result["passed"]:
        raise ValueError("Mask observation gate failed; all artifacts retained")
    print("MASK_OBSERVATION_AUDIT_PASS", json.dumps(gates), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["fixture", "assets", "old-executable", "executable", "out"]:
        parser.add_argument("--"+name, type=Path, required=True)
    main(parser.parse_args())
