#!/usr/bin/env python3
"""Render PRISM, verify black/safe-area invariants, export review media.

Requires the built prism_preview, ffmpeg, NumPy and Pillow. Temporary raw captures
are removed after encoding; only previews and the verification report remain.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
LOADER = HERE.parents[1] / "altscreen_render" / "loader"
WIDTH, HEIGHT, FPS, FRAMES = 1440, 542, 30, 180


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = (args.output or HERE.parents[1] / "output/loader-preview" / "prism-final-loader").resolve()
    output.mkdir(parents=True, exist_ok=True)
    cases = [("full", "sport"), ("full", "classic"),
             ("compact", "sport"), ("compact", "classic")]
    report = {"look": "prism", "type": "soft-medium-18px",
              "fps": FPS, "frames_per_view": FRAMES,
              "size": [WIDTH, HEIGHT], "cases": [],
              "shader_sha256": hashlib.sha256((LOADER / "prism_baked.h").read_bytes()).hexdigest(),
              "material_sha256": hashlib.sha256((LOADER / "logo_material.png").read_bytes()).hexdigest(),
              "signature_sha256": hashlib.sha256((LOADER / "sig_prism_soft.png").read_bytes()).hexdigest(),
              "layout_sha256": hashlib.sha256((LOADER / "loader_layout.h").read_bytes()).hexdigest(),
              "scope": "macOS OpenGL preview; QNX performance not measured"}
    tiles = []
    full_hash = None
    with tempfile.TemporaryDirectory(prefix="prism-capture-") as temp:
        raw = Path(temp) / "frames.rgba"
        for view, style in cases:
            key = f"{view}-{style}"
            print(f"Rendering {key}...", flush=True)
            captured = run(str(HERE / "prism_preview"), "--capture", str(raw),
                           "--frames", str(FRAMES), "--view", view, "--style", style)
            match = re.search(r"Content: (\d+) (\d+) (\d+) (\d+)", captured.stderr)
            if not match:
                raise RuntimeError(captured.stderr)
            x, y, width, height = map(int, match.groups())
            if raw.stat().st_size != FRAMES * WIDTH * HEIGHT * 4:
                raise RuntimeError("Incomplete raw capture")
            data = np.memmap(raw, dtype=np.uint8, mode="r",
                             shape=(FRAMES, HEIGHT, WIDTH, 4))
            first_black = not data[0, :, :, :3].any()
            outside = max(int(data[:, :y, :, :3].max(initial=0)),
                          int(data[:, y+height:, :, :3].max(initial=0)),
                          int(data[:, y:y+height, :x, :3].max(initial=0)),
                          int(data[:, y:y+height, x+width:, :3].max(initial=0)))
            opaque = bool(np.all(data[:, :, :, 3] == 255))
            intro_moves = not np.array_equal(data[20], data[48])
            wait_moves = not np.array_equal(data[135], data[149])
            sig_match = re.search(r"Signature: ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)", captured.stderr)
            if not sig_match:
                raise RuntimeError("Capture did not report signature metrics")
            sx, sy, sw, sh = map(float, sig_match.groups())
            signature_visible = bool(data[135, math.floor(sy):math.ceil(sy+sh),
                                           math.floor(sx):math.ceil(sx+sw), :3].max() > 40)
            if not (first_black and outside == 0 and opaque and intro_moves
                    and wait_moves and signature_visible):
                raise RuntimeError(f"Visual invariant failed in {key}")
            digest = hashlib.sha256(data).hexdigest()
            if view == "full":
                if full_hash is not None and digest != full_hash:
                    raise RuntimeError("Fullscreen differs between Sport and Classic")
                full_hash = digest
            report["cases"].append(dict(view=view, style=style,
                content=[x, y, width, height], first_frame_black=first_black,
                outside_content_max=outside, opaque=opaque,
                intro_animates=intro_moves, waiting_animates=wait_moves,
                signature_visible=signature_visible, signature=[sx, sy, sw, sh], rgba_sha256=digest))
            if key != "full-classic":
                run("ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                    "-f", "rawvideo", "-pixel_format", "rgba", "-video_size", "1440x542",
                    "-framerate", str(FPS), "-i", str(raw), "-an", "-c:v", "libx264",
                    "-crf", "16", "-preset", "fast", "-pix_fmt", "yuv420p",
                    "-movflags", "+faststart", str(output / f"{key}.mp4"))
                for number, frame in enumerate([20, 48, 80, 126]):
                    im = Image.fromarray(np.array(data[frame, :, :, :3]))
                    im.save(output / f"{key}-stage-{number+1}.png")
                    tile = Image.new("RGB", (720, 301), "#101419")
                    tile.paste(im.resize((720, 271)), (0, 30))
                    ImageDraw.Draw(tile).text((14, 9), f"{view} / {style}   {frame/FPS:.2f}s", fill="white")
                    tiles.append(tile)
            del data
        sheet = Image.new("RGB", (2880, 903))
        for i, tile in enumerate(tiles):
            sheet.paste(tile, ((i % 4)*720, (i // 4)*301))
        sheet.save(output / "contact-sheet.png")
    report["fullscreen_styles_identical"] = True
    (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    run("ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-i", str(output / "full-sport.mp4"), "-filter_complex",
        "fps=20,scale=1080:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse=dither=sierra2_4a",
        "-loop", "0", str(output / "prism.gif"))
    print(f"Verified all four layouts. Review media: {output}")


if __name__ == "__main__":
    main()
