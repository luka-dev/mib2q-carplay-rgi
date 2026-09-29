#!/usr/bin/env python3
"""Compare frozen TRAIN cache windows against their raw Moises sources.

Read only short, polyphase-aligned windows with padding. Fit one common scale
to all seven source groups; report quantization residual, missing files, rates.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

import mask_net as MN
from separator_fit_probe import write_json


def run(args):
    manifest = json.loads(args.manifest.read_text())
    rows = []
    for record in manifest["windows"]:
        song = record["song"]
        directory = args.raw / song
        metadata = json.loads((directory / "data.json").read_text())
        start, n = record["start_sample"], record["samples"]
        native_start = max(0, (start // 160 - 3) * 147)
        output_offset = start - (native_start // 147) * 160
        frames = int(np.ceil((output_offset + n + 480) * 147 / 160))
        raw = np.zeros((7, 2, n), np.float64)
        headers, missing = [], []
        for stem in metadata["stems"]:
            for track in stem["tracks"]:
                path = directory / stem["stemName"] / (track["id"] + ".wav")
                if not path.exists():
                    missing.append(dict(stem=stem["stemName"], id=track["id"]))
                    continue
                info = sf.info(str(path))
                headers.append(dict(id=track["id"], role=stem["stemName"],
                    native_sr=info.samplerate, channels=info.channels, frames=info.frames))
                if info.samplerate != 44100 or info.channels not in (1, 2):
                    raise ValueError(f"historical resampler assumption fails: {song}")
                samples, rate = sf.read(str(path), start=native_start, frames=frames,
                                        dtype="float32", always_2d=True)
                if samples.shape[1] == 1:
                    samples = np.repeat(samples, 2, axis=1)
                resampled = resample_poly(samples.T, 160, 147, axis=1)
                cut = resampled[:, output_offset:output_offset + n]
                if cut.shape[1] != n:
                    raise ValueError(f"short raw source for frozen cache window: {song}")
                group = MN.group_of(stem["stemName"], track.get("trackType"))
                raw[group] += cut
        cached = MN.read_window(str(args.cache / (song + ".npy")), start, n).astype(np.float64) / 32768
        scale = float((raw * cached).sum() / np.square(raw).sum())
        error = scale * raw - cached
        nse = float(np.square(error).sum() / np.square(cached).sum())
        rows.append(dict(song=song, scale=scale, residual_db=10 * np.log10(max(nse, 1e-30)),
            max_error_lsb=float(np.abs(error).max() * 32768), missing=missing, headers=headers,
            common_scale_all_groups=True))
    write_json(args.out, dict(scope="four frozen TRAIN windows only; best common scale", rows=rows))
    for r in rows:
        print(f"{r['song']} residual {r['residual_db']:.2f} dB "
              f"max {r['max_error_lsb']:.2f} LSB missing {len(r['missing'])} "
              f"native_rates {sorted({h['native_sr'] for h in r['headers']})}", flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--raw", type=Path, required=True)
    p.add_argument("--cache", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    run(args)


if __name__ == "__main__":
    main()
