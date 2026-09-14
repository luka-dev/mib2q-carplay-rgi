#!/usr/bin/env python3
"""Render and check the fictional stream handoff in all cluster layouts."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
W,H,FPS,FRAMES=1440,542,30,330


def run(*args):
    return subprocess.run(args,check=True,capture_output=True,text=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'output/loader-preview/prism-final')
    parser.add_argument('--binary',type=Path,default=HERE/'prism_preview')
    args=parser.parse_args();exe=args.binary.resolve()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'hook/altscreen/altscreen_hook.c').read_text()
    geometry={name:int(re.search(r'#define\s+'+name+r'\s+(\d+)',source)[1])
              for name in ['CLUSTER_W','CLUSTER_H','CLUSTER_PHYS_W','CLUSTER_PHYS_H']}
    assert list(geometry.values())==[1440,542,292,110],geometry
    report={'geometry':geometry,'mm_per_pixel':[292/1440,110/542],
            'dpi':[1440*25.4/292,542*25.4/110], 'stream_px':[1440,542],
            'fps':FPS,'frames_per_view':FRAMES,'scope':'macOS mock-stream preview',
            'shader_sha256':hashlib.sha256((HERE/'../../common/prism_baked.h').read_bytes()).hexdigest(),'cases':[]}
    full_hash=None;tiles=[]
    with tempfile.TemporaryDirectory(prefix='prism-handoff-') as temp:
        raw=Path(temp)/'frames.rgba';still=Path(temp)/'still.rgba'
        for view,style in [('full','sport'),('full','classic'),('compact','sport'),('compact','classic')]:
            key=f'{view}-{style}';print('Rendering '+key,flush=True)
            result=run(str(exe),'--capture',str(raw),'--frames',str(FRAMES),
                       '--view',view,'--style',style,'--handoff')
            x,y,w,h=map(int,re.search(r'Content: (\d+) (\d+) (\d+) (\d+)',result.stderr).groups())
            a=np.memmap(raw,np.uint8,'r',shape=(FRAMES,H,W,4))
            assert not a[0,:,:,:3].any()
            assert np.all(a[:,:,:,3]==255)
            # The loader alone respects its safe area. The handoff may expand
            # beyond it, and the stream must cover every pixel of the panel.
            for resting in [a[:103],a[300:]]:
                assert not resting[:,:y,:,:3].any() and not resting[:,y+h:,:,:3].any()
                assert not resting[:,y:y+h,:x,:3].any() and not resting[:,y:y+h,x+w:,:3].any()
            sx,sy,sw,sh=map(float,re.search(r'Signature: ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+)',result.stderr).groups())
            assert x<=sx and sx+sw<=x+w and y<=sy and sy+sh<=y+h
            # Stable stream is the unmodified full-canvas frame, including all
            # four corners: this catches accidental loader-area clipping.
            expected=np.array(Image.open(HERE/'stream_demo.png').convert('RGB'))
            assert expected.shape==(H,W,3)
            difference=np.abs(a[198,:,:,:3].astype(int)-expected.astype(int))
            assert difference.max()<=1,int(difference.max())
            # On return there is no replay of the boot sequence or lingering map.
            run(str(exe),'--capture',str(still),'--frames','1',
                '--view',view,'--style',style,'--loader-only','--time','10000')
            reference=np.fromfile(still,np.uint8).reshape(H,W,4)
            assert np.array_equal(a[300],reference)
            digest=hashlib.sha256(a).hexdigest()
            if view=='full':
                if full_hash is not None: assert digest==full_hash
                full_hash=digest
            report['cases'].append({'view':view,'style':style,'content_px':[x,y,w,h],
                'content_mm':[w*292/1440,h*110/542], 'first_frame_black':True,
                'signature_px':[sw,sh],'signature_mm':[sw*292/1440,sh*110/542],
                'resting_loader_outside_content_max':0,'opaque':True,
                'full_panel_stream_max_error':int(difference.max()),
                'return_matches_waiting_loader':True,'rgba_sha256':digest})
            if key!='full-classic':
                run('ffmpeg','-v','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size','1440x542',
                    '-framerate',str(FPS),'-i',str(raw),'-an','-c:v','libx264','-crf','16','-preset','fast',
                    '-pix_fmt','yuv420p','-movflags','+faststart',str(out/f'{key}.mp4'))
                for n in [96,108,114,120,126,198,210,219,225,234,246,300]:
                    im=Image.fromarray(np.array(a[n,:,:,:3]))
                    if key=='full-sport':
                        im.save(out/f'frame-{n:03d}.png')
                    if n in [114,126,225,246]:
                        tile=Image.new('RGB',(720,298),'#101419')
                        tile.paste(im.resize((720,271)),(0,27))
                        ImageDraw.Draw(tile).text((12,8),f'{view} / {style}   {n/30:.2f}s',fill='white')
                        tiles.append(tile)
                if key=='full-sport':
                    sheet=Image.new('RGB',(1440,816),'#101419')
                    for i,n in enumerate([96,108,114,120,126,198,210,219,225,234,246,300]):
                        im=Image.fromarray(np.array(a[n,:,:,:3])).resize((460,173))
                        xx=i%3*480+10;yy=i//3*204+25
                        sheet.paste(im,(xx,yy));ImageDraw.Draw(sheet).text((xx,yy-18),f'{n/30:.2f} s',fill='white')
                    sheet.save(out/'transition-stages.png')
            del a
    sheet=Image.new('RGB',(2880,894),'#101419')
    for i,tile in enumerate(tiles):sheet.paste(tile,((i%4)*720,(i//4)*298))
    sheet.save(out/'viewports.png')
    report['fullscreen_styles_identical']=True
    (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Verified loader safe areas, full-panel stream, physical type metrics and clean return in all four layouts.')


if __name__=='__main__':main()
