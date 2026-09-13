#!/usr/bin/env python3
"""Capture ordinary maneuvers and verify native camera/progress handoffs."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
from PIL import Image, ImageDraw, ImageFont, ImageChops, ImageStat
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'output/maneuver-preview'

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--baseline',type=Path,help='Optional former preview captures for ordinary-scene parity')
    baseline=parser.parse_args().baseline
    titles=re.findall(r'\{"([^"\n]+)", ICON_', (ROOT/'tools/maneuver-preview/fixtures.h').read_text())
    OUT.mkdir(parents=True,exist_ok=True)
    subprocess.run(['make','-C','tools/maneuver-preview','all'],cwd=ROOT,check=True)
    binary=ROOT/'build/maneuver-preview/preview'
    with (OUT/'capture.log').open('w') as log,(OUT/'renderer.log').open('w') as errors:
        subprocess.run([str(binary),'--capture',str(OUT)],cwd=ROOT,stdout=log,stderr=errors,check=True,timeout=120)
    font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',20)
    sheet=Image.new('RGB',(4*348,((len(titles)+3)//4)*232),'#11161d');draw=ImageDraw.Draw(sheet)
    captures=[];baseline_matches=0
    for c,title in enumerate(titles):
        for mode in ('crop','wide'):
            ppm=OUT/f'{c:02d}-{mode}.ppm'
            im=Image.open(ppm).convert('RGB');im.save(OUT/f'{c:02d}-{mode}-source.png');ppm.unlink()
            bounds=(59,27,269,180) if mode=='crop' else (0,0,328,180)
            cropped=im.crop(tuple(round(v*(im.width/328 if i%2==0 else im.height/181)) for i,v in enumerate(bounds)))
            cropped.resize((210,153) if mode=='crop' else (328,180),Image.Resampling.LANCZOS).save(OUT/f'{c:02d}-{mode}-native.png')
            captures.append(dict(case=c,area=mode,title=title))
            if baseline and c<15:
                prior=Image.open(baseline/f'{c+18:02d}-{mode}-source.png')
                assert ImageChops.difference(im,prior).getbbox() is None,f'Accepted ordinary scene changed: {c}, {mode}'
                baseline_matches+=1
        x,y=(c%4)*348,(c//4)*232
        draw.text((x+10,y+8),f'{c:02d}  {title}',font=font,fill='#e3edf5')
        im=Image.open(OUT/f'{c:02d}-crop-source.png').resize((328,181),Image.Resampling.LANCZOS)
        sheet.paste(im,(x+10,y+39))
        # The small frame remains a guide, never a baked-in clip plane.
        small=Image.open(OUT/f'{c:02d}-crop-source.png');wide=Image.open(OUT/f'{c:02d}-wide-source.png')
        diff=ImageChops.difference(small,wide);d=ImageDraw.Draw(diff);scale=small.width/328
        for box in ((55,23,63,181),(265,23,273,181),(55,23,273,31),(55,176,273,181),
                    (0,0,328,3),(0,177,328,181),(0,0,3,181),(325,0,328,181)):
            d.rectangle(tuple(round(v*scale) for v in box),fill=0)
        # Arrival's existing flag layout adapts to the stage; its old/new
        # baseline parity is checked separately above.
        if c != 11: assert diff.getbbox() is None,f'Stage changed scene {c}'
    sheet.save(OUT/'comparison.png')
    lines=(OUT/'capture.log').read_text().splitlines()
    frames=[line for line in lines if line.startswith('FRAME ')]
    assert len(frames)==2*len(titles) and all(line.split()[3]=='PASS' for line in frames)
    alpha=[line for line in lines if line.startswith('ALPHA ')]
    assert len(alpha)==len(frames) and all(int(line.split()[4])==0 for line in alpha)
    transition_dir=OUT/'transition';transition_dir.mkdir(exist_ok=True)
    transitions=[]
    for current,nxt in ((14,16),(1,6),(2,3),(4,5),(6,7),(25,28),(12,31),(8,9),(10,24),(14,1)):
        for ppm in transition_dir.glob('*.ppm'):ppm.unlink()
        with (transition_dir/'capture.log').open('w') as log, (transition_dir/'renderer.log').open('w') as errors:
            subprocess.run([str(binary),'--capture-transition',str(transition_dir),str(current),str(nxt)],
                           cwd=ROOT,stdout=log,stderr=errors,check=True,timeout=60)
        files=sorted(transition_dir.glob('*-crop.ppm'),key=lambda f:int(f.name.split('-')[0]))
        assert 'HANDOFF frames ' in (transition_dir/'capture.log').read_text(), \
            f'Transition {current}->{nxt}: capture was closed before the native handoff completed'
        moving=[Image.open(f).convert('RGB') for f in files]
        assert 25<len(moving)<901, 'Native handoff did not run through camera settle'
        endpoint_error=[]
        for frame,c in ((moving[0],current),(moving[-1],nxt)):
            diff=ImageChops.difference(frame,Image.open(OUT/f'{c:02d}-crop-source.png'))
            peak=max(hi for lo,hi in diff.getextrema());mean=max(ImageStat.Stat(diff).mean)
            assert peak==0, f'Transition {current}->{nxt}: endpoint moved ({peak}, {mean})'
            endpoint_error.append(dict(max_channel_error=peak,mean_channel_error=mean))
        hashes={hashlib.sha256(frame.tobytes()).hexdigest() for frame in moving}
        assert len(hashes)>25, 'Transition did not advance'
        travel=[]
        for line in (transition_dir/'capture.log').read_text().splitlines():
            if line.startswith('TRAVEL '):
                t=line.split();travel.append(dict(frame=int(t[1]),x=float(t[3]),y=float(t[4]),rotation=float(t[6]),phase=int(t[8]),fill=float(t[10]),handoff=int(t[12])))
        cache=[tuple(sorted(map(int,line.split()[3:5])))
               for line in (transition_dir/'capture.log').read_text().splitlines() if line.startswith('CACHE ')]
        assert len(cache)==len(moving) and len(set(cache[1:]))==1, 'Scene rebuilt during camera animation'
        assert len(travel)==len(moving)
        distance=max((f['x']**2+f['y']**2)**.5 for f in travel)
        rotation=max(abs(f['rotation']) for f in travel)
        exit_heading=float(next(line.split()[1] for line in (transition_dir/'capture.log').read_text().splitlines()
                                if line.startswith('EXIT_HEADING ')))
        # A circle traversed to its forward exit still finishes facing forward.
        # Assert camera travel for all paths and rotation when the real route
        # changes exit heading, rather than maintaining a fixture-number list.
        assert distance>1 and (abs(exit_heading)<=.5 or rotation>.5), 'Camera did not follow the joined route'
        assert travel[-1]['phase']==0 and all(abs(travel[-1][k])<1e-5 for k in ('x','y','rotation'))
        departing=[f['fill'] for f in travel if f['handoff']]
        arriving=[f['fill'] for f in travel[1:] if not f['handoff']]
        assert len(set(departing))>5 and departing[-1]==0, 'Old fill did not visibly retreat'
        assert all(b<=a for a,b in zip(departing,departing[1:])), 'Incoming target leaked into departure'
        assert arriving[0]==0 and len(set(arriving))>5, 'New fill appeared instantly at commit'
        assert all(b>=a for a,b in zip(arriving,arriving[1:])), 'New fill did not grow continuously'
        assert arriving[-1]==travel[0]['fill']==.75
        transitions.append(dict(current=current,next=nxt,frames=len(moving),engine='native joined-route',
                                exact_endpoints=True,endpoint_error=endpoint_error,valid_rgba=True,scene_rebuilt_during_animation=False,
                                progress_handoff='retreat to zero, hold through push, refill on commit',
                                max_camera_travel=distance,max_rotation=rotation))
        if current==25 and nxt==28:
            moving[0].save(OUT/'roundabout-transition.gif',save_all=True,append_images=moving[1:],
                           duration=[650]+[33]*(len(moving)-2)+[650],loop=0)
        if current==14 and nxt==16:
            moving[0].save(OUT/'maneuver-transition.gif',save_all=True,append_images=moving[1:],
                           duration=[650]+[33]*(len(moving)-2)+[650],loop=0)
            contact=Image.new('RGB',(656*3,362*2))
            for slot,i in enumerate((0,15,30,45,60,len(moving)-1)):
                contact.paste(moving[i],((slot%3)*656,(slot//3)*362))
            contact.save(transition_dir/'contact-sheet.png')
        for ppm in transition_dir.glob('*.ppm'):ppm.unlink()
    manifest=dict(scope='Ordinary maneuver renderer; lane-road experiment removed',
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  scene_engine='C++11 cached scene compiler with C ABI',captures=captures,
                  source_size=[328,181],fov_degrees=40,arrow_width=.14,
                  accepted_ordinary_baseline_matches=baseline_matches,transitions=transitions)
    (OUT/'captures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(captures)} stills, {len(transitions)} camera/progress handoffs, {baseline_matches} baseline matches PASS')
    print(OUT/'comparison.png')

if __name__=='__main__': main()
