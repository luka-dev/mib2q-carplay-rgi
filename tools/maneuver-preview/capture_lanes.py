#!/usr/bin/env python3
"""Capture the production lane panel in both visible areas; keep a review sheet."""
from pathlib import Path
import json
import subprocess
from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'output/lane-panel'
RAW = OUT / 'raw'


def main():
    RAW.mkdir(parents=True, exist_ok=True)
    subprocess.run(['make', '-C', str(ROOT / 'tools/maneuver-preview'), 'all'], check=True)
    with (RAW / 'capture.log').open('w') as log, (RAW/'renderer.log').open('w') as errors:
        subprocess.run([str(ROOT / 'build/maneuver-preview/preview'), '--capture-lanes', str(RAW)],
                       cwd=ROOT, stdout=log, stderr=errors, check=True, timeout=60)
    lines=(RAW / 'capture.log').read_text().splitlines()
    alpha=[line for line in lines if line.startswith('ALPHA ')]
    assert len(alpha)==22 and all(int(line.split()[4])==0 for line in alpha)
    for c in range(11):
        a=Image.open(RAW/f'{c:02d}-crop.ppm');b=Image.open(RAW/f'{c:02d}-wide.ppm')
        diff=ImageChops.difference(a,b);draw=ImageDraw.Draw(diff);scale=a.width/328
        # Only the independent bottom panel and debug frame may vary by stage.
        boxes=[(0,132,328,181),(55,23,63,181),(265,23,273,181),(55,23,273,31),
               (0,0,328,3),(0,0,3,181),(325,0,328,181)]
        for box in boxes:draw.rectangle(tuple(round(v*scale) for v in box),fill=0)
        assert diff.getbbox() is None, f'Lane stage changed maneuver pixels: {c}'
    hidden_checked=0
    for mode in ('crop','wide'):
        baseline=ROOT/f'output/maneuver-preview/14-{mode}-source.png'
        if baseline.exists():
            assert ImageChops.difference(Image.open(RAW/f'10-{mode}.ppm'),Image.open(baseline)).getbbox() is None
            hidden_checked+=1
    examples=[(0,'8 полос'),(7,'Разворот · выход над полосами'),(9,'Кольцо · отдельные полосы')]
    sheet=Image.new('RGB',(1312,405*len(examples)),'#151515');draw=ImageDraw.Draw(sheet)
    font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',22)
    for row,(case,label) in enumerate(examples):
        for col,mode in enumerate(('crop','wide')):
            x,y=col*656,row*405
            draw.text((x+15,y+8),label+' · '+('Малый' if col==0 else 'Большой'),font=font,fill='white')
            im=Image.open(RAW/f'{case:02d}-{mode}.ppm').resize((656,362),Image.Resampling.LANCZOS)
            sheet.paste(im,(x,y+38))
    sheet.save(OUT/'lane-panel.png')
    reveal=RAW/'reveal';reveal.mkdir(exist_ok=True)
    with (reveal/'capture.log').open('w') as log, (reveal/'renderer.log').open('w') as errors:
        subprocess.run([str(ROOT/'build/maneuver-preview/preview'),'--capture-lane-reveal',str(reveal)],
                       cwd=ROOT,stdout=log,stderr=errors,check=True,timeout=30)
    travel=[line.split() for line in (reveal/'capture.log').read_text().splitlines() if line.startswith('FRAMING ')]
    assert len(travel)==60 and len({t[5] for t in travel})==1, 'Camera framing rebuilt the scene'
    dolly=[float(t[4]) for t in travel]
    assert dolly[11]==0 and dolly[41]>=.12 and dolly[-1]==0
    assert len(set(dolly[12:42]))>=5 and len(set(dolly[42:]))>=5
    assert all(a<=b for a,b in zip(dolly[:41],dolly[1:42]))
    assert all(a>=b for a,b in zip(dolly[42:],dolly[43:]))
    frames=[Image.open(reveal/f'{i:02d}-crop.ppm').convert('RGB') for i in range(60)]
    assert ImageChops.difference(frames[11],frames[-1]).getbbox() is None
    frames[0].save(OUT/'lane-camera.gif',save_all=True,append_images=frames[1:],duration=33,loop=0)
    handoff=RAW/'handoff';handoff.mkdir(exist_ok=True)
    with (handoff/'capture.log').open('w') as log, (handoff/'renderer.log').open('w') as errors:
        subprocess.run([str(ROOT/'build/maneuver-preview/preview'),'--capture-lane-transition',str(handoff),'4','6'],
                       cwd=ROOT,stdout=log,stderr=errors,check=True,timeout=60)
    handoff_lines=(handoff/'capture.log').read_text().splitlines()
    assert any(line.startswith('HANDOFF ') for line in handoff_lines)
    poses=[line.split() for line in handoff_lines if line.startswith('TRAVEL ')]
    assert max(float(t[3])**2+float(t[4])**2 for t in poses)>1
    assert all(abs(float(poses[-1][i]))<1e-5 for i in (3,4,6))
    departing=[float(t[10]) for t in poses if t[12]=='1']
    arriving=[float(t[10]) for t in poses[1:] if t[12]=='0']
    assert departing[-1]==0 and arriving[0]==0 and arriving[-1]==.75
    assert len(set(departing))>5 and len(set(arriving))>5
    assert all(int(line.split()[4])==0 for line in handoff_lines if line.startswith('ALPHA '))
    cache=[tuple(line.split()[3:5]) for line in handoff_lines if line.startswith('CACHE ')]
    assert len(set(cache[1:]))==1
    (OUT/'verification.json').write_text(json.dumps(dict(captures=22,premultiplied_alpha=True,
        stage_changes_only_panel=True,hidden_baselines_checked=hidden_checked,
        reveal_frames=60,camera_reveal_restore=True,scene_cache_preserved=True,
        lane_camera_handoff_frames=len(poses),lane_camera_handoff=True,
        small_capacity=5,wide_capacity=8),indent=2)+'\n')
    print('PASS: 22 lane frames, 60 camera reveal/restore frames, valid alpha, stage parity and unchanged scene cache; hidden baselines:',hidden_checked)


if __name__=='__main__':
    main()
