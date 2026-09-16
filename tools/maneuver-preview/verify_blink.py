#!/usr/bin/env python3
"""Check blink palette and blink-to-next-maneuver handoff on the real GL path."""
from pathlib import Path
import subprocess

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build/diagnostics/blink-validation'
PREVIEW = ROOT / 'build/maneuver-preview/preview'


def run_capture(mode, directory, *args):
    directory.mkdir(parents=True, exist_ok=True)
    for ppm in directory.glob('*.ppm'):
        ppm.unlink()
    result = subprocess.run([str(PREVIEW), mode, str(directory), *map(str, args)],
                            cwd=ROOT, capture_output=True, text=True, check=True, timeout=60)
    (directory / 'capture.log').write_text(result.stdout)
    (directory / 'renderer.log').write_text(result.stderr)
    images = [Image.open(p).convert('RGB') for p in
              sorted(directory.glob('*-crop.ppm'), key=lambda p: int(p.name.split('-')[0]))]
    lines = result.stdout.splitlines()
    alpha = [line.split() for line in lines if line.startswith('ALPHA ')]
    assert len(alpha) == len(images) and all(int(a[4]) == 0 for a in alpha), \
        f'{mode}: invalid framebuffer alpha'
    return images, lines


def identical(a, b):
    return ImageChops.difference(a, b).getbbox() is None


def monotonic(values, increasing):
    return all(a <= b if increasing else a >= b for a, b in zip(values, values[1:]))


def main():
    subprocess.run(['make', '-C', 'tools/maneuver-preview', 'all'], cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL)
    frames, lines = run_capture('--capture-blink', OUT / 'steady')
    states = []
    for line in lines:
        if line.startswith('BLINK '):
            p = line.split()
            states.append((int(p[1]), int(p[3]), float(p[5]), float(p[7]), float(p[9])))
    assert len(frames) == len(states) == 41
    assert [s[0] for s in states] == list(range(41))
    assert all(s[2] == .75 and 0 <= s[3] <= 1 and 0 <= s[4] <= 1
               and s[3] + s[4] <= 1.00001 for s in states)
    for start, end, field, up in ((1, 8, 4, True), (9, 16, 4, False),
                                  (17, 24, 4, True), (25, 32, 4, False),
                                  (33, 40, 3, True)):
        values = [states[i][field] for i in range(start, end + 1)]
        assert monotonic(values, up) and len(set(values)) >= 4, \
            f'Blink stage {states[start][1]} jumped or reversed'
    assert states[0][3:] == (1, 0)
    assert states[4][3:] == states[20][3:] == (0, 1)
    assert states[14][3:] == states[30][3:] == (0, 0)
    assert states[40][3:] == (1, 0)
    assert identical(frames[0], frames[40]), 'Blink off to fill changed base color'
    assert identical(frames[4], frames[8]) and identical(frames[4], frames[20]), \
        'Repeated high blink endpoint differs'
    assert identical(frames[14], frames[16]) and identical(frames[14], frames[30]), \
        'Low blink and off produced different palettes'
    diff = ImageChops.difference(frames[4], frames[14])
    bounds = diff.getbbox()
    assert bounds and 280 < bounds[0] < bounds[2] < 460, \
        f'Blink altered the road or viewport outside the arrow: {bounds}'

    lane_frames, lane_lines = run_capture('--capture-blink-lanes', OUT / 'with-lanes')
    assert len(lane_frames) == 41 and len([s for s in lane_lines if s.startswith('BLINK ')]) == 41
    assert identical(lane_frames[0], lane_frames[40])
    assert identical(lane_frames[4], lane_frames[20])
    assert identical(lane_frames[14], lane_frames[30])
    lane_diff = ImageChops.difference(lane_frames[4], lane_frames[14]).getbbox()
    assert lane_diff and 280 < lane_diff[0] < lane_diff[2] < 460 and lane_diff[3] < 310, \
        f'Blink leaked through the faded lane row or changed lane icons: {lane_diff}'

    moving, lines = run_capture('--capture-transition-blink', OUT / 'handoff', 14, 16)
    travel, tint = [], []
    for line in lines:
        p = line.split()
        if line.startswith('TRAVEL '):
            travel.append((int(p[1]), float(p[3]), float(p[4]), int(p[8]),
                           float(p[10]), int(p[12])))
        if line.startswith('BLINK_TRAVEL '):
            tint.append((int(p[1]), float(p[3]), float(p[5])))
    assert 'HANDOFF frames ' in '\n'.join(lines) and len(moving) == len(travel) == len(tint)
    assert len(moving) > 25 and len({im.tobytes() for im in moving}) > 25
    assert travel[0][4] == .75 and tint[0][2] == 1
    # The old blink fades while the camera and fill are already moving.
    assert travel[1][1:3] != (0, 0) and 0 < tint[1][2] < 1
    assert any(tint[i][2] == 0 and travel[i][5] and travel[i][4] > 0
               for i in range(1, len(travel))), 'Glow waited until after the handoff'
    assert monotonic([s[2] for s in tint], False), 'Old blink restarted in the next scene'
    departing = [s[4] for s in travel if s[5]]
    arriving = [s[4] for s in travel[1:] if not s[5]]
    assert monotonic(departing, False) and departing[-1] == 0
    assert arriving[0] == 0 and monotonic(arriving, True) and arriving[-1] == .75
    assert tint[-1][1:] == (1, 0), 'Incoming arrow did not return to base/fill palette'
    assert len(set(s[1] for s in tint if s[0] >= 70)) > 3, \
        'Incoming path color jumped to its target'
    print(f'{len(frames)} blink frames (ordinary and lanes), {len(moving)} blink-handoff frames PASS')
    print(OUT)


if __name__ == '__main__':
    main()
