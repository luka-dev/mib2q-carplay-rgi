#!/usr/bin/env python3
"""Real GL/protocol evidence for the isolated arrow-progress experiment.
No production port, installed service, or reference artifacts are modified.
"""
import csv, hashlib, json, shutil, socket, struct, subprocess, time
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'output/arrow-progress'

def packet(cmd,payload=b'',flags=0):
    return bytes([cmd,flags])+payload.ljust(46,b'\0')
def progress(level,state):
    return packet(6,bytes([level,0 if state==0 else 1,state]),32)

def run():
    shots=OUT/'frames'; shots.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'maneuver_render/resources/flag_atlas.rgba',shots/'flag_atlas.rgba')
    binary=OUT/'host/maneuver_render'
    rows=list(csv.DictReader((ROOT/'output/maneuver-parity/java_mapping.csv').open()))
    cases=[(2,90,0),(8,30,0),(48,160,0),(28,90,0),(28,90,1),(4,-180,0)]
    # Some audit datasets only have canonical +/-135 sharp examples.
    selected=[]
    for mt,angle,side in cases:
        candidates=[r for r in rows if int(r['mt'])==mt and int(r['traffic'])==side and r['case']=='canonical']
        row=dict(min(candidates,key=lambda r:abs(int(r['angle'])-angle)))
        if mt==48 and angle==160:
            wire=bytearray.fromhex(row['renderer_hex']);wire[4:6]=struct.pack('>h',320)
            row['renderer_hex']=wire.hex();row['angle']='160'
        selected.append(row)
    server=socket.socket();server.bind(('127.0.0.1',0));server.listen(1);server.settimeout(15)
    port=server.getsockname()[1]
    log=(OUT/'renderer.log').open('w')
    proc=subprocess.Popen([str(binary),f'--port={port}'],cwd=shots,stdout=log,stderr=log)
    conn=None; serial=0; checks=[]
    try:
        conn,_=server.accept();conn.settimeout(10)
        def event(wanted):
            while True:
                b=b''
                while len(b)<48:
                    chunk=conn.recv(48-len(b))
                    if not chunk:raise RuntimeError('renderer disconnected')
                    b+=chunk
                if b[0]==wanted:return
        def snap():
            nonlocal serial
            label=f'a{serial:04d}'; path=shots/f'snap_{serial:03d}_{label}.ppm';serial+=1
            path.unlink(missing_ok=True);conn.sendall(packet(2,label.encode()))
            until=time.monotonic()+5
            while time.monotonic()<until:
                try:
                    with Image.open(path) as im:im.load();result=im.convert('RGB')
                    path.unlink();return result
                except (OSError,ValueError):time.sleep(.004)
            raise TimeoutError(path)
        def wait_still(timeout=20):
            # Queued turns can outlast a fixed GIF duration. Compare palette
            # colors only after route/camera motion has actually stopped.
            previous=np.asarray(snap());matches=0;until=time.monotonic()+timeout
            while time.monotonic()<until:
                time.sleep(.12);im=snap();current=np.asarray(im)
                matches=matches+1 if np.array_equal(previous,current) else 0
                if matches>=3:return im
                previous=current
            raise TimeoutError('renderer never settled')
        def maneuver(row,clear=True,level=16,state=1):
            if clear:conn.sendall(packet(7));event(0x83)
            p=bytearray.fromhex(row['renderer_hex']);p[1]|=32|2;p[44]=state;p[46]=level;p[47]=0 if state==0 else 1
            conn.sendall(p)
            if clear:event(0x82)
            return p
        def capture_anim(name,seconds,step=None):
            frames=[]; durations=[];start=time.monotonic();last=start
            while time.monotonic()-start<seconds:
                t=time.monotonic()-start
                if step:step(t)
                frames.append(snap()); now=time.monotonic();durations.append(max(30,int((now-last)*1000)));last=now
                time.sleep(.035)
            frames[0].save(OUT/name,save_all=True,append_images=frames[1:],duration=durations,loop=0)
            return len(frames)
        event(0x81)
        levels=[16,12,8,4,0]
        font=ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf',18)
        poster=Image.new('RGB',(5*328,60+len(selected)*220),'#111820');draw=ImageDraw.Draw(poster)
        for j,level in enumerate(levels):draw.text((j*328+16,20),f'{(16-level)*100//16}% filled',font=font,fill='white')
        for i,r in enumerate(selected):
            maneuver(r);time.sleep(1.6)
            values=[]
            for j,level in enumerate(levels):
                conn.sendall(progress(level,1));time.sleep(.7)
                im=snap();im.save(shots/f'case-{i}-level-{level}.png');values.append(np.asarray(im).astype(float))
                tile=im.copy();tile.thumbnail((328,181));poster.paste(tile,(j*328,60+i*220+26))
            draw.text((12,60+i*220),f"CarPlay {r['mt']}, angle {r['angle']}, traffic {r['traffic']}",font=font,fill='#c1d8e8')
            # More fill must add brightness to the same mesh, no extra bar rectangle.
            means=[v.mean() for v in values]
            assert all(b>a+.05 for a,b in zip(means,means[1:])),('fill not increasing',i,means)
            changed=np.max(abs(values[-1]-values[0]),axis=2)>8
            assert changed.sum()>50,('no route fill',i)
            checks.append(dict(case=i,mt=r['mt'],brightness=means,changed_pixels=int(changed.sum())))
            print('captured',i,r['mt'],flush=True)
        poster.save(OUT/'all-maneuvers.png')
        maneuver(selected[1]);time.sleep(1.6)
        first_steps=[]
        early_sheet=Image.new('RGB',(7*328,2*210),'#111820');early_draw=ImageDraw.Draw(early_sheet)
        for view_index,(view,rect) in enumerate([('split',(0,0,328,180)),('big',(59,27,210,153))]):
            conn.sendall(packet(8,struct.pack('>4H',*rect)));time.sleep(.4)
            conn.sendall(progress(16,1));time.sleep(.7);empty=snap()
            conn.sendall(progress(0,1));time.sleep(.8);full=snap()
            before=np.asarray(empty).astype(float);difference=np.asarray(full).astype(float)-before
            x=empty.width//2;bottom=round(180*empty.height/181);top=empty.height//2
            delta=difference[top:bottom,x-2:x+3]
            norm=np.sum(delta*delta,axis=2);valid=norm>100
            lengths=[]
            for n in range(7):
                conn.sendall(progress(16-n,1));time.sleep(.7);im=snap()
                sample=np.asarray(im).astype(float)[top:bottom,x-2:x+3]-before[top:bottom,x-2:x+3]
                ratio=np.sum(sample*delta,axis=2)/np.maximum(norm,1)
                length=int(np.sum(np.mean(np.where(valid,ratio,0),axis=1)>=.5))
                lengths.append(length)
                tile=im.copy();tile.thumbnail((328,181));early_sheet.paste(tile,(n*328,view_index*210+28))
                early_draw.text((n*328+8,view_index*210+5),f'{view}: {n}/16 — {length}px',font=font,fill='white')
                im.save(shots/f'first-step-{view}-{n}.png')
            increments=np.diff(lengths)
            assert min(increments)>0 and max(increments)-min(increments)<=2,('uneven first steps',view,lengths)
            first_steps.append(dict(view=view,lit_shaft_pixels=lengths,increments=increments.tolist()))
            print('first increments',view,increments.tolist(),flush=True)
        early_sheet.save(OUT/'first-steps.png')
        conn.sendall(packet(8,struct.pack('>4H',0,0,328,180)));time.sleep(.4)
        conn.sendall(progress(16,1));time.sleep(.7)
        last_level=[None]
        def fill_step(t):
            level=max(0,16-int(t/.30))
            if level!=last_level[0]:conn.sendall(progress(level,1));last_level[0]=level
        nf=capture_anim('fill.gif',5.3,fill_step)
        # Rapid moving targets reverse before the previous response would finish.
        conn.sendall(progress(16,1));time.sleep(.7)
        fast_target=[None]
        def fast_step(t):
            if t<.5: level=16
            elif t<1.78: level=max(0,16-int((t-.5)/.08))
            elif t<2.5: level=0
            elif t<3.78: level=min(16,int((t-2.5)/.08))
            else: level=16
            if level!=fast_target[0]:conn.sendall(progress(level,1));fast_target[0]=level
        nr=capture_anim('fast-retarget.gif',4.5,fast_step)
        last_phase=[None]
        def blink_step(t):
            high=int(t/.6)%2==0
            if high!=last_phase[0]:conn.sendall(progress(16 if high else 0,3 if high else 2));last_phase[0]=high
        nb=capture_anim('blink.gif',2.5,blink_step)
        # Wire ordering: newer LOW must survive deferred HIGH on a pushed maneuver.
        p=maneuver(selected[3],clear=False);p[44]=3;p[46]=16
        conn.sendall(p+progress(0,2))
        nt=capture_anim('maneuver-transition.gif',3.0)
        low=wait_still();conn.sendall(progress(16,3));time.sleep(.25);high=snap()
        assert np.asarray(high).mean()>np.asarray(low).mean()+.5,'newer blink phase lost to deferred maneuver'
        low.save(shots/'blink-low.png');high.save(shots/'blink-high.png')
        # Only two palette endpoints: low == off == unfilled, high == full.
        conn.sendall(progress(0,2));time.sleep(.25);blink_base=snap()
        conn.sendall(progress(0,0));time.sleep(.25);off_base=snap()
        conn.sendall(progress(16,1));time.sleep(.8);empty_base=snap()
        assert np.array_equal(np.asarray(blink_base),np.asarray(off_base)), 'blink low -> off changes color'
        assert np.array_equal(np.asarray(off_base),np.asarray(empty_base)), 'off and empty use different base colors'
        conn.sendall(progress(0,1));time.sleep(.8);full=snap()
        conn.sendall(progress(16,3));time.sleep(.25);glow=snap()
        assert np.array_equal(np.asarray(full),np.asarray(glow)), 'blink glow adds a third palette endpoint'
        palette=Image.new('RGB',(4*328,210),'#111820');palette_draw=ImageDraw.Draw(palette)
        for j,(label,im) in enumerate([('blink low',blink_base),('off',off_base),('empty',empty_base),('full / glow',glow)]):
            tile=im.copy();tile.thumbnail((328,181));palette.paste(tile,(j*328,29))
            palette_draw.text((j*328+12,5),label,font=font,fill='white')
        palette.save(OUT/'two-color-palette.png')
        # Start HIGH -> OFF at the maneuver command, while geometry is moving.
        maneuver(selected[1],clear=False,level=0,state=0)
        nd=capture_anim('blink-off-maneuver.gif',3.0)
        settled=wait_still();time.sleep(.3);later=snap()
        assert np.array_equal(np.asarray(settled),np.asarray(later)), 'color still changing after maneuver settles'
        # Actual VC crop, same path fill must remain observable and need no side relocation.
        stage=Image.new('RGB',(5*210,185),'#111820')
        conn.sendall(packet(8,struct.pack('>4H',59,27,210,153)));time.sleep(.4)
        for j,level in enumerate(levels):
            conn.sendall(progress(level,1));time.sleep(.7);im=snap()
            sx,sy=im.width/328,im.height/181
            crop=im.crop((round(59*sx),round(27*sy),round(269*sx),round(180*sy)))
            crop=crop.resize((210,153),Image.Resampling.LANCZOS);stage.paste(crop,(j*210,32))
        d=ImageDraw.Draw(stage)
        for j,level in enumerate(levels):d.text((j*210+8,8),f'{(16-level)*100//16}% / big screen',font=font,fill='white')
        stage.save(OUT/'big-screen.png')
        # Level-only senders fill the same arrow; no secondary overlay exists.
        conn.sendall(progress(8,1));time.sleep(.7);marked=snap()
        conn.sendall(packet(6,bytes([8,1])));time.sleep(.2);level_only=snap()
        assert np.array_equal(np.asarray(marked),np.asarray(level_only)), 'level-only fill mismatch'
        conn.sendall(progress(0,0));time.sleep(.2);quiet=snap()
        conn.sendall(packet(6,bytes([8,1,255]),32));time.sleep(.2);malformed=snap()
        assert np.array_equal(np.asarray(quiet),np.asarray(malformed)), 'invalid extension is not quiet'
        conn.sendall(packet(6,bytes([8,2])));time.sleep(.2);unphased=snap()
        time.sleep(.8);unphased_later=snap()
        assert np.array_equal(np.asarray(quiet),np.asarray(unphased)), 'unphased mode is not quiet'
        assert np.array_equal(np.asarray(unphased),np.asarray(unphased_later)), 'independent blink clock'
        # Disconnect/reconnect clears the old arrow before any new maneuver.
        conn.close();conn,_=server.accept();conn.settimeout(10);event(0x81)
        time.sleep(.15);reconnected=snap()
        assert np.asarray(reconnected).max()==0,'reconnect retained stale progress/geometry'
        maneuver(selected[0]);time.sleep(.4)
        # CLEAR must erase both geometry and progress.
        conn.sendall(packet(7));event(0x83);time.sleep(.1)
        blank=snap();assert np.asarray(blank).max()==0,'CLEAR left visible pixels'
        manifest=dict(cases=checks,fill_frames=nf,blink_frames=nb,transition_frames=nt,fast_retarget_frames=nr,blink_off_maneuver_frames=nd,two_color_endpoints=True,
            ordering='latest phase survives transition',level_only_arrow=True,invalid_state_quiet=True,no_local_blink=True,clear=True,reconnect=True,first_steps=first_steps,
            sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
                list((ROOT/'maneuver_render').glob('*.c'))+list((ROOT/'maneuver_render').glob('*.h'))})
        (OUT/'checks.json').write_text(json.dumps(manifest,indent=2)+'\n')
    finally:
        if conn:
            try:conn.sendall(packet(3));conn.close()
            except OSError:pass
        try:proc.wait(timeout=5)
        except subprocess.TimeoutExpired:proc.terminate();proc.wait(timeout=5)
        server.close();log.close()
    print('GL arrow-progress captures PASS',flush=True)
if __name__=='__main__':run()
