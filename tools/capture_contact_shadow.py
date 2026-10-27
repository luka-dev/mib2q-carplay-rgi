#!/usr/bin/env python3
"""Compare real renderer frames with a supplied build from before contact AO.
Uses isolated loopback ports; does not touch installed services or vehicle state.
"""
import argparse,hashlib,json,shutil,socket,struct,subprocess,time
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageFilter
from capture_arrow_progress import packet
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'output/arrow-progress/contact-shadow'
CASES=[('sharp right',2,160,1,8),('sharp left',2,-160,1,8),
       ('near return',2,175,1,0),('U-turn',3,-180,1,8),
       ('roundabout',6,160,1,8),('flat sharp',2,160,0,8),('ordinary turn',2,90,1,8),
       ('full sharp right',2,160,1,0)]

def capture(binary,label):
    directory=OUT/label;directory.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'maneuver_render/resources/flag_atlas.rgba',directory/'flag_atlas.rgba')
    listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen(1);listener.settimeout(15)
    images=[];conn=None
    with (directory/'renderer.log').open('w') as log:
        proc=subprocess.Popen([str(binary),f'--port={listener.getsockname()[1]}'],cwd=directory,stdout=log,stderr=log)
        try:
            conn,_=listener.accept();conn.settimeout(10)
            def event(kind):
                while True:
                    data=b''
                    while len(data)<48:
                        part=conn.recv(48-len(data))
                        if not part:raise EOFError()
                        data+=part
                    if data[0]==kind:return
            event(0x81)
            for i,(name,icon,angle,perspective,level) in enumerate(CASES):
                conn.sendall(packet(7));event(0x83)
                p=bytearray(46);p[0]=icon;p[1]=255 if angle<0 else 1
                p[2:4]=struct.pack('>h',angle*2);p[5]=3
                for j,road in enumerate([-90,90,0]):p[6+j*2:8+j*2]=struct.pack('>h',road*2)
                p[42]=1;p[43]=perspective;p[44]=level;p[45]=1
                conn.sendall(packet(1,p,32|4|2|1));event(0x82);time.sleep(1.8)
                path=directory/f'snap_{i:03d}_contact.ppm';path.unlink(missing_ok=True)
                conn.sendall(packet(2,b'contact'));until=time.monotonic()+5
                while time.monotonic()<until:
                    try:
                        with Image.open(path) as im:im.load();im=im.convert('RGB')
                        break
                    except (OSError,ValueError):time.sleep(.01)
                else:raise TimeoutError(path)
                im.save(directory/f'{i}.png');path.unlink();images.append(im)
                print(label,name,flush=True)
        finally:
            if conn:
                try:conn.sendall(packet(3));conn.close()
                except OSError:pass
            try:proc.wait(timeout=5)
            except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=5)
            listener.close()
    (directory/'flag_atlas.rgba').unlink()
    return images

def main():
    global OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before',type=Path,required=True,help='renderer binary built before the AO change')
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument("--origin",action="store_true",help="verify shadow reaches the start of the crease")
    mode.add_argument("--taper",action="store_true",help="compare the tapered width with a constant-width shadow")
    args=parser.parse_args()
    if args.origin:OUT=ROOT/"output/arrow-progress/contact-origin"
    if args.taper:OUT=ROOT/"output/arrow-progress/contact-taper"
    before=capture(args.before.resolve(),'before')
    binary=ROOT/'output/arrow-progress/host/maneuver_render';after=capture(binary,'after')
    font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',20)
    w,h=before[0].size;sheet=Image.new('RGB',(w*2,len(CASES)*(h+32)),'#111820');draw=ImageDraw.Draw(sheet)
    metrics=[]
    for i,(a,b) in enumerate(zip(before,after)):
        aa=np.asarray(a).astype(int);bb=np.asarray(b).astype(int);delta=aa-bb
        # FXAA can redistribute a few pixels around a newly dark crease.
        # Bound that local response; AO itself preserves alpha and multiplies RGB.
        dark=np.any(delta>2,axis=2);bright=np.any(delta < -2,axis=2)
        if not (args.origin or args.taper):
            edge_envelope=np.asarray(Image.fromarray((dark*255).astype('uint8')).filter(ImageFilter.MaxFilter(7)))>0
            assert not np.any(bright&~edge_envelope),('brightening outside the antialiased crease',CASES[i][0])
            assert delta.sum()>=0,'AO added net light'
        envelope=np.asarray(Image.fromarray((np.any(aa>0,axis=2)*255).astype('uint8')).filter(ImageFilter.MaxFilter(5)))>0
        assert not np.any(np.any(bb>0,axis=2)&~envelope), 'AO expanded silhouette'
        changed=np.any(abs(delta)>2,axis=2) if args.origin or args.taper else np.any(delta>2,axis=2)
        metric=dict(case=CASES[i][0],changed_pixels=int(changed.sum()),max_darken=int(delta.max()))
        if args.origin and i in (0,1,7):
            # Fixed framing: the first quarter of the sharp-turn crease.
            # A later/stronger shadow cannot satisfy this onset regression.
            early=int(dark[181:207,310:346].sum())
            assert early>20,('crease start is still unshadowed',CASES[i][0],early)
            metric['early_crease_pixels']=early
        if args.taper and i in (0,1,7):
            early=int(bright[181:207,310:346].sum())
            assert early>20,('shadow is still wide at the crease start',CASES[i][0],early)
            metric['narrowed_crease_pixels']=early
        metrics.append(metric)
        if i in (0,1,2):assert changed.sum()>20,('no visible contact change',CASES[i][0])
        if i in (3,6):assert not changed.any(),('non-overlap received unrelated AO',CASES[i][0])
        for j,im in enumerate([a,b]):
            y=i*(h+32);sheet.paste(im,(j*w,y+32))
            draw.text((j*w+12,y+5),f'{CASES[i][0]} / '+('previous' if j==0 else ('tapered width' if args.taper else ('crease origin' if args.origin else 'contact AO'))),font=font,fill='white')
    sheet.save(OUT/'comparison.png')
    sheet.crop((0,0,w*2,h+32)).save(OUT/'sharp-right.png')
    sheet.crop((0,7*(h+32),w*2,8*(h+32))).save(OUT/'full-sharp-right.png')
    (OUT/'checks.json').write_text(json.dumps(dict(cases=metrics,
        before_binary_sha256=hashlib.sha256(args.before.read_bytes()).hexdigest(),
        after_binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
                 list((ROOT/'maneuver_render').glob('*.c'))+list((ROOT/'maneuver_render').glob('*.h'))}),indent=2)+'\n')
    print('Contact AO real GL comparison PASS',metrics,flush=True)
if __name__=='__main__':main()
