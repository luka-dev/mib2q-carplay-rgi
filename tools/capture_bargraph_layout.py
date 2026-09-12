#!/usr/bin/env python3
"""Exercise the real renderer over loopback: stage crops, blue-route overlap, animation.

Build: make -C maneuver_render BUILD_DIR=../output/bargraph-layout/host
Consumes the committed Java maneuver audit; creates fresh GL screenshots, not mockups.
"""
import csv
import hashlib
import json
import shutil
import socket
import struct
import subprocess
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageSequence

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'output/bargraph-layout'
VIEWS = {'split-stage': (0, 0, 328, 180), 'big-screen': (59, 27, 210, 153)}


def packet(command, payload=b''):
    return bytes([command, 0])+payload.ljust(46, b'\0')


def animation_evidence():
    with Image.open(OUT/'stage-transition.gif') as im:
        frames = [f.convert('RGB').copy() for f in ImageSequence.Iterator(im)]
    positions = []
    for f in frames:
        pixels = np.asarray(f).astype(int)
        blue = ((pixels[:,:,2]-pixels[:,:,0] > 12) &
                (pixels[:,:,1]-pixels[:,:,0] > 8) & (pixels[:,:,2]-pixels[:,:,1] > 10))
        score = blue.sum(axis=0)
        edges = {}
        for name, lo, hi in [('left',0,f.width//3), ('right',2*f.width//3,f.width)]:
            peak = score[lo:hi].max()
            cols = np.where(score[lo:hi] > peak*.8)[0]+lo
            edges[name] = round(float(cols.mean()),2) if peak > f.height*.2 else None
        positions.append(edges)
    assert max(len({p[side] for p in positions if p[side] is not None})
               for side in ('left','right')) >= 5, 'animation has no intermediate positions'
    assert positions[0]['right'] is not None and positions[-1]['right'] is not None
    assert any(p['left'] is not None for p in positions), 'exit-tip avoidance was not exercised'
    sides = []
    for p in positions:
        active = [side for side in ('left','right') if p[side] is not None]
        assert len(active) == 1, ('side change must draw exactly one bargraph', p)
        if not sides or sides[-1] != active[0]:
            sides.append(active[0])
    assert sides == ['right','left','right'], ('unexpected side oscillation', sides)
    indices = sorted(set(round(i*(len(frames)-1)/8) for i in range(9)))
    sheet = Image.new('RGB', (3*656, 3*400), '#11161d')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 20)
    for i, n in enumerate(indices):
        frame = frames[n].copy(); frame.thumbnail((656,362))
        x, y = (i%3)*656, (i//3)*400
        draw.text((x+10,y+8), f"Frame {n} / L={positions[n]['left']} R={positions[n]['right']}", font=font, fill='white')
        sheet.paste(frame, (x,y+35))
    sheet.save(OUT/'stage-transition-frames.png')
    return dict(animation_frames=len(frames), animation_bar_positions=positions,
                animation_side_changes=sides, one_bargraph_per_frame=True)


def maneuver_animation_evidence():
    with Image.open(OUT/'maneuver-transition.gif') as im:
        frames = [f.convert('RGB').copy() for f in ImageSequence.Iterator(im)]
    sheet = Image.new('RGB',(3*656,3*400),'#11161d')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',20)
    for i,n in enumerate(sorted(set(round(i*(len(frames)-1)/8) for i in range(9)))):
        frame = frames[n]; frame.thumbnail((656,362))
        x,y = i%3*656,i//3*400
        draw.text((x+10,y+8),f'Maneuver transition / frame {n}',font=font,fill='white')
        sheet.paste(frame,(x,y+35))
    sheet.save(OUT/'maneuver-transition-frames.png')
    return dict(maneuver_animation_frames=len(frames))


def main():
    shots = OUT/'frames'
    shots.mkdir(parents=True, exist_ok=True)
    binary = OUT/'host/maneuver_render'
    source = ROOT/'output/maneuver-parity/java_mapping.csv'
    rows = list(csv.DictReader(source.open()))
    cases = [(1, -90, 0), (2, 90, 0), (8, 30, 0), (22, -30, 0),
             (47, -135, 0), (48, 135, 0), (28, 90, 0), (28, 90, 1),
             (19, 180, 0), (4, -180, 0), (3, 0, 0), (52, -30, 0)]
    selected = [next(r for r in rows if r['case'] == 'canonical' and
                    (int(r['mt']), int(r['angle']), int(r['traffic'])) == c) for c in cases]
    results = []
    shutil.copyfile(ROOT/'maneuver_render/resources/flag_atlas.rgba', shots/'flag_atlas.rgba')
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('127.0.0.1', 19800)); server.listen(1); server.settimeout(15)
    log = (OUT/'renderer.log').open('w')
    proc = subprocess.Popen([str(binary)], cwd=shots, stdout=log, stderr=log)
    conn = None
    try:
        conn, _ = server.accept(); conn.settimeout(10)

        def wait_event(wanted):
            while True:
                data = b''
                while len(data) < 48:
                    chunk = conn.recv(48-len(data))
                    if not chunk:
                        raise RuntimeError('renderer disconnected')
                    data += chunk
                if data[0] == wanted:
                    return

        serial = 0

        def snap():
            nonlocal serial
            label = f'b{serial:04d}'; serial += 1
            path = shots/f'snap_{serial-1:03d}_{label}.ppm'
            path.unlink(missing_ok=True)
            conn.sendall(packet(2, label.encode()))
            end = time.monotonic()+5
            while time.monotonic() < end:
                try:
                    with Image.open(path) as im:
                        im.load(); result = im.convert('RGB')
                    path.unlink()
                    return result
                except (OSError, ValueError):
                    time.sleep(.004)
            raise TimeoutError(path)

        def area(view):
            conn.sendall(packet(8, struct.pack('>4H', *VIEWS[view])))

        wait_event(0x81)
        for index, r in enumerate(selected):
            conn.sendall(packet(7)); wait_event(0x83)
            command = bytearray.fromhex(r['renderer_hex'])
            command[1] &= ~2
            conn.sendall(command); wait_event(0x82)
            time.sleep(1.6)
            for view, rect in VIEWS.items():
                area(view)
                conn.sendall(packet(6, bytes([0, 0])))
                time.sleep(.75)
                baseline = snap()
                conn.sendall(packet(6, bytes([16, 1])))
                time.sleep(.75)
                visible = snap()
                original, current = np.asarray(baseline).astype(int), np.asarray(visible).astype(int)
                changed = np.max(np.abs(current-original), axis=2) > 8
                ys, xs = np.where(changed)
                assert len(xs) > 100, ('bargraph invisible', r['mt'], view)
                sx, sy = visible.width/328, visible.height/181
                x, y, w, h = rect
                bbox = (int(xs.min()), int(ys.min()), int(xs.max()+1), int(ys.max()+1))
                assert bbox[0] >= x*sx and bbox[1] >= y*sy and bbox[2] <= (x+w)*sx and bbox[3] <= (y+h)*sy, (r['mt'], view, bbox)
                blue_route = (original[:, :, 2] > original[:, :, 1]*1.1) & (original[:, :, 1] > original[:, :, 0]+20)
                overlap = int(np.count_nonzero(changed & blue_route))
                assert overlap == 0, ('bargraph covers blue arrow', r['mt'], view, overlap)
                name = f'{index:02d}-{view}.png'
                visible.save(shots/name)
                results.append(dict(mt=int(r['mt']), angle=int(r['angle']), traffic=int(r['traffic']),
                                    view=view, visible_area=rect, png='frames/'+name,
                                    bargraph_bbox=bbox, covered_route_pixels=overlap,
                                    side='right' if (bbox[0]+bbox[2])/2 > (x+w/2)*sx else 'left'))
                print(f"MT {r['mt']} / {view}: visible, blue-route overlap={overlap}", flush=True)

        # Uninterrupted real snapshots of a viewport-only animation, same arrow/level.
        conn.sendall(packet(7)); wait_event(0x83)
        command = bytearray.fromhex(selected[1]['renderer_hex'])
        command[1] |= 2; command[46] = 10; command[47] = 1
        conn.sendall(command); wait_event(0x82); area('split-stage'); time.sleep(1.8)
        frames, durations = [], []
        frame = snap()
        frames.append(frame); durations.append(600)
        for view in ('big-screen', 'split-stage'):
            area(view)
            start = time.monotonic()
            previous = start
            while time.monotonic()-start < .9:
                frame = snap(); now = time.monotonic()
                frames.append(frame); durations.append(max(20, int((now-previous)*1000)))
                previous = now
            frames.append(frame.copy()); durations.append(600)
        frames[0].save(OUT/'stage-transition.gif', save_all=True, append_images=frames[1:],
                       duration=durations, loop=0, disposal=2)

        # Actual maneuver-to-maneuver pushes. Check that fully visible bar segments
        # use the incoming target's side throughout, including reappearance.
        conn.sendall(packet(7)); wait_event(0x83); area('big-screen')
        command = bytearray.fromhex(selected[0]['renderer_hex'])
        command[1] |= 2; command[46] = 10; command[47] = 1
        conn.sendall(command); wait_event(0x82); time.sleep(1)
        maneuver_frames, maneuver_durations = [snap()], [600]
        maneuver_checks = []
        for row, expected in [(selected[1], 'left'), (selected[0], 'right')]:
            command = bytearray.fromhex(row['renderer_hex'])
            command[1] |= 2; command[46] = 10; command[47] = 1
            conn.sendall(command)
            start = previous = time.monotonic()
            detected = []
            while time.monotonic()-start < 15:
                frame = snap(); now = time.monotonic()
                pixels = np.asarray(frame).astype(int)
                solid = np.max(np.abs(pixels-np.array([90,170,230])),axis=2) < 4
                sx, sy = frame.width/328, frame.height/181
                for side, x in [('left',69), ('right',245.89)]:
                    score = solid[round(37*sy):round(170*sy),round(x*sx):round((x+13.11)*sx)].sum(axis=0)
                    if score.max() > 45*sy:
                        assert side == expected, ('bar changed during maneuver animation',row['mt'],side,expected)
                        detected.append(side)
                maneuver_frames.append(frame)
                maneuver_durations.append(max(20,int((now-previous)*1000)))
                previous = now
                if now-start > .6 and len(detected) >= 8:
                    break
            assert len(detected) >= 5, ('new bargraph did not reappear',row['mt'],detected)
            maneuver_checks.append(dict(mt=int(row['mt']),side=expected,verified_frames=len(detected)))
            maneuver_frames.append(frame.copy()); maneuver_durations.append(600)
        maneuver_frames[0].save(OUT/'maneuver-transition.gif',save_all=True,
                                append_images=maneuver_frames[1:],duration=maneuver_durations,
                                loop=0,disposal=2)
    finally:
        if conn:
            try:
                conn.sendall(packet(3))
            except OSError:
                pass
            conn.close()
        server.close()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.terminate(); proc.wait(timeout=5)
        log.close(); (shots/'flag_atlas.rgba').unlink(missing_ok=True)
    assert proc.returncode == 0

    # Each card shows only the region actually visible in that stage.
    sheet = Image.new('RGB', (1320, 100+6*280), '#11161d')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 19)
    bold = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial Bold.ttf', 23)
    draw.text((20, 18), 'Bargraph: split-stage и big screen', font=bold, fill='white')
    draw.text((20, 53), 'OpenGL, видимая область VC: справа по умолчанию; влево при пересечении наконечника.', font=font, fill='#a8dce6')
    for i, r in enumerate(selected):
        x, y = (i%2)*660, 100+(i//2)*280
        draw.text((x+16, y), f"MT {r['mt']} / {r['angle']}° / {'LHT' if r['traffic']=='1' else 'RHT'}", font=bold, fill='white')
        for j, item in enumerate(results[2*i:2*i+2]):
            im = Image.open(OUT/item['png'])
            vx, vy, vw, vh = item['visible_area']
            sx, sy = im.width/328, im.height/181
            im = im.crop((round(vx*sx), round(vy*sy), round((vx+vw)*sx), round((vy+vh)*sy)))
            im.thumbnail((328, 181))
            xx = x+12 if j == 0 else x+362
            draw.text((xx, y+37), item['view'], font=font, fill='#edc77e')
            sheet.paste(im, (xx, y+69))
    sheet.save(OUT/'bargraph-stages.png', optimize=True)
    report = dict(cases=results, count=len(results), no_blue_route_overlap=True,
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  scope='macOS OpenGL; actual packets and source crop; no vehicle test')
    report.update(animation_evidence())
    report.update(maneuver_animation_evidence())
    report['maneuver_transitions'] = maneuver_checks
    sources = [ROOT/'java_patch/com/luka/carplay/cluster/ClusterLayerController.java',
               *[ROOT/'java_patch/com/luka/carplay/rgd'/name for name in
                 ('BAPBridge.java','RendererServer.java','RouteGuidance.java')],
               *[ROOT/'maneuver_render'/name for name in
                 ('main.c','protocol.h','render.c','render.h','maneuver.c','maneuver.h',
                  'route_path.c','route_path.h','bargraph_layout.h')]]
    report['source_sha256'] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in sources}
    (OUT/'checks.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'PASS: {len(results)} visible stage cases, no blue-route overlap; actual transition GIF')


if __name__ == '__main__':
    main()
