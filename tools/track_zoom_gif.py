#!/usr/bin/env python3
"""Interactive: draw a box on frame 0, get a stabilized/zoomed GIF following it.

Usage:
  track_zoom_gif.py INPUT.mp4 [--start 0:39] [--end 0:45]
                    [--zoom 2.0] [--smooth 7] [--fps 15] [--out OUT.gif]
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

import cv2
import numpy as np


def parse_time(s):
    if s is None:
        return None
    parts = s.split(":")
    parts = [float(p) for p in parts]
    while len(parts) < 3:
        parts.insert(0, 0.0)
    h, m, sec = parts
    return h * 3600 + m * 60 + sec


def make_tracker():
    if hasattr(cv2, "TrackerCSRT_create"):
        return cv2.TrackerCSRT_create()
    return cv2.legacy.TrackerCSRT_create()


def smooth_series(values, window, kind="gauss"):
    if window <= 1:
        return np.asarray(values, dtype=np.float64)
    values = np.asarray(values, dtype=np.float64)
    if kind == "gauss":
        # gaussian kernel, sigma = window/4 → ~95% mass inside window
        sigma = max(window / 4.0, 1.0)
        radius = int(round(3 * sigma))
        xs = np.arange(-radius, radius + 1)
        kernel = np.exp(-(xs ** 2) / (2 * sigma ** 2))
        kernel /= kernel.sum()
    else:
        kernel = np.ones(window) / window
        radius = window // 2
    padded = np.pad(values, (radius, radius), mode="edge")
    return np.convolve(padded, kernel, mode="valid")[: len(values)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("--start", default=None, help="e.g. 0:39 or 39")
    ap.add_argument("--end", default=None, help="e.g. 0:45")
    ap.add_argument("--roi-at", default=None,
                    help="time to pick ROI at (defaults to --start). use ←/→ in picker to scrub.")
    ap.add_argument("--zoom", type=float, default=2.0, help="zoom factor (>=1)")
    ap.add_argument("--smooth", type=int, default=21,
                    help="gaussian smoothing window in frames (higher = less shake, more drift)")
    ap.add_argument("--lock", action="store_true",
                    help="pin element to its mean position — fully kills shake (best if element doesn't pan)")
    ap.add_argument("--fps", type=int, default=15, help="output GIF fps")
    ap.add_argument("--width", type=int, default=640, help="output GIF width")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    start = parse_time(args.start)
    end = parse_time(args.end)

    cap = cv2.VideoCapture(args.input)
    if not cap.isOpened():
        sys.exit("could not open " + args.input)
    src_fps = cap.get(cv2.CAP_PROP_FPS) or 30.0
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    f_start = int(round((start or 0) * src_fps))
    f_end = int(round(end * src_fps)) if end is not None else total_frames
    f_end = min(f_end, total_frames)
    if f_start >= f_end:
        sys.exit("empty range")

    roi_time = parse_time(args.roi_at) if args.roi_at else start
    f_roi = int(round((roi_time or 0) * src_fps))
    f_roi = max(f_start, min(f_roi, f_end - 1))

    # Interactive scrubber: arrow keys to find a good frame, then ENTER to draw ROI.
    disp_scale = min(1.0, 1200.0 / max(width, height))

    def read_at(idx):
        cap.set(cv2.CAP_PROP_POS_FRAMES, idx)
        ok, fr = cap.read()
        return fr if ok else None

    cur = f_roi
    print("Scrub: ←/→ = ±1 frame, ↑/↓ = ±10 frames, j/l = ±30 frames, ENTER = pick ROI here, q = quit")
    win = "scrub (ENTER to pick)"
    cv2.namedWindow(win, cv2.WINDOW_AUTOSIZE)
    picked = None
    while True:
        fr = read_at(cur)
        if fr is None:
            cur = max(f_start, min(cur, f_end - 1))
            continue
        disp = cv2.resize(fr, None, fx=disp_scale, fy=disp_scale) if disp_scale < 1 else fr.copy()
        t_sec = cur / src_fps
        label = f"frame {cur}  t={t_sec:.2f}s   [{f_start}..{f_end - 1}]"
        cv2.putText(disp, label, (10, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.imshow(win, disp)
        k = cv2.waitKeyEx(0)
        # macOS arrow key codes vary; cover common variants
        if k in (13, 10, 32):  # ENTER / SPACE
            picked = (cur, fr)
            break
        elif k in (ord('q'), 27):
            cv2.destroyAllWindows()
            sys.exit("cancelled")
        elif k in (63234, 65361, 2424832, ord('h')):  # ←
            cur = max(f_start, cur - 1)
        elif k in (63235, 65363, 2555904, ord('l')):  # → (also l = +30 below if held)
            cur = min(f_end - 1, cur + 1)
        elif k in (63232, 65362, 2490368, ord('k')):  # ↑
            cur = min(f_end - 1, cur + 10)
        elif k in (63233, 65364, 2621440, ord('j')):  # ↓
            cur = max(f_start, cur - 10)
        elif k == ord('L'):
            cur = min(f_end - 1, cur + 30)
        elif k == ord('J'):
            cur = max(f_start, cur - 30)
    cv2.destroyWindow(win)

    f_roi, roi_frame = picked
    disp = cv2.resize(roi_frame, None, fx=disp_scale, fy=disp_scale) if disp_scale < 1 else roi_frame.copy()
    print("Drag a box around the element to follow, then press ENTER or SPACE.")
    roi = cv2.selectROI("track target", disp, showCrosshair=True, fromCenter=False)
    cv2.destroyAllWindows()
    if roi == (0, 0, 0, 0):
        sys.exit("no ROI selected")
    rx, ry, rw, rh = [int(v / disp_scale) for v in roi]
    if rw <= 1 or rh <= 1:
        sys.exit("ROI too small")

    tracker = make_tracker()
    tracker.init(roi_frame, (rx, ry, rw, rh))

    # Seek to the start of the output range. If ROI was picked past f_start,
    # we still produce the full range — re-seek and let the tracker re-acquire
    # by feeding intermediate frames forward from f_roi, OR backward from f_roi
    # to f_start. Simplest: track forward from f_roi only and trim output start
    # to f_roi. Tell the user what we did.
    if f_roi != f_start:
        print(f"note: tracking starts at frame {f_roi} (t={f_roi/src_fps:.2f}s) — output range trimmed accordingly.")
        f_start = f_roi
    cap.set(cv2.CAP_PROP_POS_FRAMES, f_start + 1)
    first = roi_frame

    crop_w = int(round(width / args.zoom))
    crop_h = int(round(height / args.zoom))
    crop_w -= crop_w % 2
    crop_h -= crop_h % 2

    centers_x = []
    centers_y = []
    frames = []

    cx = rx + rw / 2.0
    cy = ry + rh / 2.0
    centers_x.append(cx)
    centers_y.append(cy)
    frames.append(first)

    n_frames = f_end - f_start
    for i in range(1, n_frames):
        ok, fr = cap.read()
        if not ok:
            break
        ok, box = tracker.update(fr)
        if ok:
            x, y, w, h = box
            cx = x + w / 2.0
            cy = y + h / 2.0
        # if lost, keep last center
        centers_x.append(cx)
        centers_y.append(cy)
        frames.append(fr)
        if i % 20 == 0:
            print(f"  tracked {i}/{n_frames}")
    cap.release()

    if args.lock:
        mx = float(np.mean(centers_x))
        my = float(np.mean(centers_y))
        sx = np.full(len(centers_x), mx)
        sy = np.full(len(centers_y), my)
        print(f"lock mode: pinned to ({mx:.1f}, {my:.1f})")
    else:
        sx = smooth_series(centers_x, args.smooth, kind="gauss")
        sy = smooth_series(centers_y, args.smooth, kind="gauss")

    tmpdir = tempfile.mkdtemp(prefix="trackzoom_")
    try:
        # Subpixel-accurate stabilization via affine translation, then crop the
        # zoom window from the center. This avoids 0.5px integer jitter you get
        # from rounding crop x/y.
        out_w, out_h = crop_w, crop_h
        cx_out = width / 2.0
        cy_out = height / 2.0
        for idx, fr in enumerate(frames):
            # translation that brings (sx, sy) to image center, then we crop center.
            tx = cx_out - float(sx[idx])
            ty = cy_out - float(sy[idx])
            M = np.array([[1.0, 0.0, tx], [0.0, 1.0, ty]], dtype=np.float32)
            shifted = cv2.warpAffine(
                fr, M, (width, height),
                flags=cv2.INTER_CUBIC,
                borderMode=cv2.BORDER_REPLICATE,
            )
            x0 = int(round(cx_out - out_w / 2.0))
            y0 = int(round(cy_out - out_h / 2.0))
            crop = shifted[y0:y0 + out_h, x0:x0 + out_w]
            cv2.imwrite(os.path.join(tmpdir, f"f_{idx:05d}.png"), crop)

        out = args.out or os.path.splitext(args.input)[0] + "_tracked.gif"
        # encode via ffmpeg with palette for quality
        palette = os.path.join(tmpdir, "palette.png")
        in_pattern = os.path.join(tmpdir, "f_%05d.png")
        vf_pal = f"fps={args.fps},scale={args.width}:-1:flags=lanczos,palettegen"
        vf_use = (
            f"fps={args.fps},scale={args.width}:-1:flags=lanczos[x];"
            f"[x][1:v]paletteuse"
        )
        subprocess.run(
            ["ffmpeg", "-y", "-framerate", str(src_fps), "-i", in_pattern,
             "-vf", vf_pal, palette],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            ["ffmpeg", "-y", "-framerate", str(src_fps), "-i", in_pattern,
             "-i", palette, "-lavfi", vf_use, out],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        size = os.path.getsize(out)
        print(f"wrote {out} ({size/1024/1024:.1f} MB, {len(frames)} frames)")
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main()
