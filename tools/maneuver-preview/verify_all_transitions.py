#!/usr/bin/env python3
"""Exercise every preview fixture as both an outgoing and incoming maneuver."""
from pathlib import Path
import re
import subprocess
import tempfile

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
PREVIEW = ROOT / 'build/maneuver-preview/preview'


def sorted_frames(directory):
    return sorted(directory.glob('*-crop.ppm'), key=lambda p: int(p.name.split('-')[0]))


def main():
    subprocess.run(['make', '-C', 'tools/maneuver-preview', 'all'], cwd=ROOT,
                   check=True, stdout=subprocess.DEVNULL)
    count = len(re.findall(r'\{"[^"\n]+", ICON_',
                           (ROOT / 'tools/maneuver-preview/fixtures.h').read_text()))
    assert count > 0
    (ROOT / 'build/diagnostics').mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='all-handoffs-', dir=ROOT / 'build/diagnostics') as temp:
        base = Path(temp)
        stills = base / 'stills'
        stills.mkdir()
        static = subprocess.run([str(PREVIEW), '--capture', str(stills)], cwd=ROOT,
                                capture_output=True, text=True, check=True, timeout=120)
        assert static.stdout.count('FRAME ') == 2 * count
        assert len(sorted_frames(stills)) == count
        for current in range(count):
            nxt = (current + 1) % count
            folder = base / 'moving'
            folder.mkdir(exist_ok=True)
            for frame in folder.glob('*.ppm'):
                frame.unlink()
            result = subprocess.run([str(PREVIEW), '--capture-transition', str(folder),
                                     str(current), str(nxt)], cwd=ROOT, capture_output=True,
                                    text=True, timeout=60)
            assert result.returncode == 0 and 'HANDOFF frames ' in result.stdout, \
                f'{current}->{nxt} never settled: {result.stderr[-300:]}'
            files = sorted_frames(folder)
            alpha = [line.split() for line in result.stdout.splitlines()
                     if line.startswith('ALPHA ')]
            assert len(files) == len(alpha) > 20 and all(int(a[4]) == 0 for a in alpha), \
                f'{current}->{nxt}: invalid alpha or incomplete capture'
            for frame, fixture in ((files[0], current), (files[-1], nxt)):
                diff = ImageChops.difference(Image.open(frame).convert('RGB'),
                                             Image.open(stills / f'{fixture:02d}-crop.ppm').convert('RGB'))
                box = diff.getbbox()
                if fixture == 11 and frame == files[-1]:
                    # The 12-frame static ARRIVED sample still morphs its tip;
                    # the destination flag also cycles forever. Only that
                    # small presentation region may differ after settling.
                    assert box is None or (260 <= box[0] and box[2] <= 420
                                           and 25 <= box[1] and box[3] <= 215), \
                        f'{current}->{nxt}: arrival road changed: {box}'
                else:
                    assert box is None, f'{current}->{nxt}: endpoint {fixture} changed: {box}'
            travel = [line.split() for line in result.stdout.splitlines()
                      if line.startswith('TRAVEL ')]
            assert len(travel) == len(files)
            departing = [float(t[10]) for t in travel if int(t[12])]
            arriving = [float(t[10]) for t in travel[1:] if not int(t[12])]
            assert len(set(departing)) > 5 and departing[-1] == 0
            assert all(a >= b for a, b in zip(departing, departing[1:])), \
                f'{current}->{nxt}: old fill reversed'
            assert arriving[0] == 0 and len(set(arriving)) > 5 and arriving[-1] == .75
            assert all(a <= b for a, b in zip(arriving, arriving[1:])), \
                f'{current}->{nxt}: new fill reversed'
            assert int(travel[-1][8]) == 0 and all(abs(float(travel[-1][i])) < 1e-5
                                                    for i in (3, 4, 6)), \
                f'{current}->{nxt}: camera did not return to origin'
            print(f'{current:02d}->{nxt:02d}: {len(files)} frames PASS', flush=True)
        print(f'All {count} outgoing and incoming fixture handoffs PASS')


if __name__ == '__main__':
    main()
