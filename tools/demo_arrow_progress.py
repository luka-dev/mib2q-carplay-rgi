#!/usr/bin/env python3
"""Loop the real arrow renderer: fast fill, reverse fill, then HUD-clock blink.
Close the window / Escape to stop. Uses its own ephemeral loopback port.
"""
import argparse
import csv
import select
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def packet(command, payload=b'', flags=0):
    return bytes([command, flags]) + payload.ljust(46, b'\0')


def run(interval):
    row = None
    with (ROOT/'output/maneuver-parity/java_mapping.csv').open() as source:
        rows=list(csv.DictReader(source))
        row=next(r for r in rows if r['case']=='canonical' and
                 (int(r['mt']),int(r['angle']),int(r['traffic']))==(8,30,0))
        next_row=next(r for r in rows if r['case']=='canonical' and
                      (int(r['mt']),int(r['angle']),int(r['traffic']))==(2,90,0))
        sharp_row=dict(next(r for r in rows if r['case']=='canonical' and int(r['mt'])==48 and int(r['traffic'])==0))
        wire=bytearray.fromhex(sharp_row['renderer_hex']);wire[1]|=4;wire[4:6]=bytes([1,64]) # 160 degrees in half-degrees
        sharp_row['renderer_hex']=wire.hex();sharp_row['angle']='160'
        scenes=[sharp_row,next_row,row]
    # Final holds expose braking; source phases, not a native timer, drive blink.
    sequence = [(16, 1, .7)]
    sequence += [(level, 1, interval) for level in range(15, -1, -1)]
    sequence += [(0, 1, .7)]
    sequence += [(level, 1, interval) for level in range(1, 17)]
    sequence += [(16, 1, .7)]
    sequence += [(level, 1, interval) for level in range(15, -1, -1)]
    sequence += [(0, 1, .7)]
    sequence += [(16 if n % 2 == 0 else 0, 3 if n % 2 == 0 else 2, .6) for n in range(6)]
    sequence += [(16, 3, .6)]  # finish bright, then dim during the maneuver push
    with tempfile.TemporaryDirectory(prefix='mhi2-arrow-demo-') as temp:
        shutil.copyfile(ROOT/'maneuver_render/resources/flag_atlas.rgba', Path(temp)/'flag_atlas.rgba')
        listener = socket.socket()
        listener.bind(('127.0.0.1', 0)); listener.listen(1); listener.settimeout(15)
        port = listener.getsockname()[1]
        with (Path(temp)/'renderer.log').open('w') as log:
            proc = subprocess.Popen([str(ROOT/'output/arrow-progress/host/maneuver_render'),
                f'--port={port}'], cwd=temp, stdout=log, stderr=log)
            conn = None
            try:
                conn, _ = listener.accept(); conn.settimeout(10)
                def event(kind):
                    while True:
                        data = b''
                        while len(data) < 48:
                            chunk = conn.recv(48-len(data))
                            if not chunk:
                                raise EOFError()
                            data += chunk
                        if data[0] == kind:
                            return
                event(0x81)
                cmd = bytearray.fromhex(scenes[0]['renderer_hex'])
                cmd[1] |= 32 | 2; cmd[44] = 1; cmd[46] = 16; cmd[47] = 1
                conn.sendall(cmd); event(0x82)
                print(f'Demo running: PID={proc.pid}, port={port}, targets every {interval*1000:.0f} ms. '
                      'Fill / reverse / blink / maneuver fade. Escape or close window to stop.', flush=True)
                def hold(duration):
                    deadline=time.monotonic()+duration
                    while proc.poll() is None and time.monotonic()<deadline:
                        readable,_,_=select.select([conn],[],[],max(0,min(.03,deadline-time.monotonic())))
                        if readable and not conn.recv(4096): raise EOFError()
                scene_index=0
                while proc.poll() is None:
                    for level, state, duration in sequence:
                        conn.sendall(packet(6, bytes([level, 0 if state == 0 else 1, state]), 32))
                        hold(duration)
                        if proc.poll() is not None:
                            break
                    if proc.poll() is not None: break
                    scene_index=(scene_index+1)%len(scenes)
                    cmd=bytearray.fromhex(scenes[scene_index]['renderer_hex'])
                    cmd[1]|=32|2;cmd[44]=0;cmd[46]=0;cmd[47]=0
                    conn.sendall(cmd)
                    print('New maneuver: glow fades to base during motion.',flush=True)
                    hold(2.5)
            except (EOFError, ConnectionError, KeyboardInterrupt):
                pass
            finally:
                if conn:
                    try:
                        conn.sendall(packet(3)); conn.close()
                    except OSError:
                        pass
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.terminate(); proc.wait(timeout=3)
                listener.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interval', type=float, default=.08, help='seconds between level targets (default .08)')
    args = parser.parse_args()
    if not .01 <= args.interval <= 2:
        parser.error('--interval must be between .01 and 2 seconds')
    run(args.interval)
