#!/usr/bin/env python3
"""Capture the real macOS maneuver renderer using exported Java commands.

Uses existing CLEAR/FRAME_CLEARED and SCREENSHOT protocol, loopback only.
Deduplicates identical commands; does not alter production sources or firmware.
Build: make -C maneuver_render BUILD_DIR=../output/maneuver-audit/host
"""
import argparse
import csv
import hashlib
import json
import shutil
import socket
import subprocess
import time
from pathlib import Path
from PIL import Image


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('out', type=Path, nargs='?', default=Path('output/maneuver-audit'))
    args = ap.parse_args()
    out = args.out.resolve()
    repo = Path(__file__).resolve().parents[1]
    binary = out / 'host/maneuver_render'
    rows = list(csv.DictReader((out / 'java_mapping.csv').open()))
    chosen = [r for r in rows if r['case'] == 'canonical' or
        r['case'] == 'matrix' and r['junction'] == '0' and
        ((r['mt'] == '1' and r['angle'] in ('-1', '0', '90')) or
         (r['mt'] == '23' and r['angle'] in ('45', '90', '135')) or
         (r['mt'] == '4' and r['angle'] == '-1')) or
        r['case'] == 'matrix' and r['mt'] == '1' and r['junction'] == '-1' and r['angle'] == '0' or
        r['case'] == 'matrix' and r['mt'] == '28' and r['junction'] == '1' and r['angle'] == '-67' or
        r['case'] == 'matrix' and r['junction'] == '0' and
        ((r['mt'] == '47' and r['angle'] == '-157') or (r['mt'] == '48' and r['angle'] == '157'))]
    shots = out / 'renderer'
    shots.mkdir(exist_ok=True)
    shutil.copyfile(repo / 'maneuver_render/resources/flag_atlas.rgba', shots / 'flag_atlas.rgba')
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', 19800))  # fail if another renderer session owns port
    server.listen(1); server.settimeout(15)
    log = (shots / 'renderer.log').open('w')
    proc = subprocess.Popen([str(binary)], cwd=shots, stdout=log, stderr=log)
    conn = None
    manifest = dict(binary=str(binary), binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    scope='Real macOS OpenGL renderer; Java-generated commands; HUD images are separate firmware assets', captures=[])
    try:
        conn, _ = server.accept(); conn.settimeout(10)
        def wait_event(wanted):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                data = b''
                while len(data) < 48:
                    chunk = conn.recv(48-len(data))
                    if not chunk: raise RuntimeError('renderer disconnected')
                    data += chunk
                if data[0] == wanted: return
            raise TimeoutError(f'event {wanted}')
        wait_event(0x81)
        by_command = {}
        for r in chosen:
            command = r['renderer_hex']
            if command not in by_command:
                label = f'c{len(by_command):03d}'
                conn.sendall(bytes([7]) + bytes(47)); wait_event(0x83)
                conn.sendall(bytes.fromhex(command)); wait_event(0x82)
                time.sleep(2.0)  # allow route reveal/camera animation to settle
                snap = bytes([2, 0]) + label.encode().ljust(46, b'\0')
                conn.sendall(snap)
                deadline = time.monotonic() + 8
                ppm = shots / f'snap_{len(by_command):03d}_{label}.ppm'
                while not ppm.exists() and time.monotonic() < deadline: time.sleep(.05)
                # Writer closes before its next render-loop/event iteration.
                time.sleep(.1)
                with Image.open(ppm) as im:
                    png = shots / f'{label}.png'; im.save(png)
                    if r['renderer_icon'] == '0':
                        assert all(hi == 0 for lo, hi in im.convert('RGB').getextrema()), 'NONE retained visible geometry'
                ppm.unlink()
                by_command[command] = str(png.relative_to(out))
                print(f'{label}: MT {r["mt"]}, angle {r["angle"]}, traffic {r["traffic"]}', flush=True)
            manifest['captures'].append(dict(row=r, png=by_command[command]))
        manifest['unique_commands'] = len(by_command)
        (out / 'renderer-captures.json').write_text(json.dumps(manifest, indent=2) + '\n')
    finally:
        if conn:
            try: conn.sendall(bytes([3]) + bytes(47))
            except OSError: pass
            conn.close()
        server.close()
        try: proc.wait(timeout=5)
        except subprocess.TimeoutExpired: proc.terminate(); proc.wait(timeout=5)
        log.close()
        (shots / 'flag_atlas.rgba').unlink(missing_ok=True)
    if proc.returncode != 0: raise RuntimeError(f'renderer exited {proc.returncode}')
    print(f'{len(chosen)} cases / {len(by_command)} unique real renderer captures')


if __name__ == '__main__':
    main()
