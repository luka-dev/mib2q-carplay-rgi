"""Wide-band geometry (2026-09-29): the hook asks the phone for 1280x720 with height_margin 315, so the
phone draws only a centred 1280x405 band; the player must crop exactly that band and fill 1440x455.
Input: black 1280x720 with a white band at rows 157..561. Correct crop -> white edge to edge.
An 800x480 input must keep the whole picture (letterboxed, black side bars). Host build only.
"""
from pathlib import Path
import subprocess, tempfile, time, select
P=Path(__file__).resolve().parents[1]
def run(*cmd):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=60)
    if r.returncode: raise AssertionError(f'{cmd}\n{r.stdout}\n{r.stderr}')
    return r.stdout
def ppm(path):
    data=path.read_bytes(); head=b'P6\n1440 455\n255\n'
    assert data.startswith(head), data[:20]
    return data[len(head):]
def px(img,x,y): i=(y*1440+x)*3; return img[i:i+3]
def present(t,movie,name,extra=None):
    dump=t/(name+'.ppm')
    producer=subprocess.Popen([str(t/'producer'),str(movie)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
    try:
        assert select.select([producer.stdout],[],[],5)[0], 'producer startup timed out'
        pid=producer.stdout.readline().strip()
        with (t/(name+'.log')).open('w') as log:
            # the 1080p cases below were written for the old band geometry: CL_TEST_BAND unless a test clears it
            env=dict({'CL_TEST_PRESENT_PPM':str(dump),'PATH':'/usr/bin:/bin','CL_TEST_BAND':'1'},**(extra or {}))
            player=subprocess.Popen([str(t/'player'),pid],stdout=log,stderr=log,
                                    env={k:v for k,v in env.items() if v is not None})
            deadline=time.monotonic()+8
            while not dump.exists() and time.monotonic()<deadline: time.sleep(0.1)
            time.sleep(0.3)
            producer.kill(); producer.wait(timeout=2); player.wait(timeout=5)
    finally:
        if producer.poll() is None: producer.kill(); producer.wait()
    for suffix in ('.h264','.h264.done','.ppm'):
        f=Path(f'/tmp/sq5_cluster_live-{pid}{suffix}')
        if f.exists(): f.unlink()
    assert dump.exists(), (t/(name+'.log')).read_text()
    return ppm(dump)
with tempfile.TemporaryDirectory(prefix='sq5-band-') as td:
    t=Path(td)
    flags=['gcc','-O1','-no-pie','-std=gnu99','-I'+str(P/'src'),'-I'+str(P/'vendor')]
    run(*flags,P/'src/player.c','-DCL_HOST','-lavcodec','-lavutil','-o',t/'player')
    run(*flags,P/'tests/producer.c',P/'src/transport.c','-pthread','-lavcodec','-lavutil','-o',t/'producer')
    band=t/'band.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=black:s=1280x720:r=30',
        '-vf','drawbox=x=0:y=157:w=1280:h=405:color=white:t=fill','-frames:v','60',
        '-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',band)
    img=present(t,band,'band')
    for x,y in ((2,2),(1437,2),(2,452),(1437,452),(720,227)):
        assert min(px(img,x,y))>200, f'band not filling 1440x455 at {x},{y}: {tuple(px(img,x,y))}'
    print('PASS: 1280x720 frame -> centred 1280x405 band fills 1440x455 edge to edge (no bars, no margin rows)')
    band2=t/'band1080.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=black:s=1920x1080:r=30',
        '-vf','drawbox=x=0:y=236:w=1920:h=607:color=white:t=fill','-frames:v','60',
        '-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',band2)
    img=present(t,band2,'band1080')
    for x,y in ((2,2),(1437,2),(2,452),(1437,452),(720,227)):
        assert min(px(img,x,y))>200, f'1080 band not filling 1440x455 at {x},{y}: {tuple(px(img,x,y))}'
    print('PASS: 1920x1080 frame (sq5_cluster_1080) -> centred 1920x607 band fills 1440x455 edge to edge')
    full=t/'full.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=white:s=800x480:r=30','-frames:v','60',
        '-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',full)
    img=present(t,full,'full')
    assert max(px(img,2,227))<30 and max(px(img,1437,227))<30, 'expected side bars for 800x480'
    assert min(px(img,720,227))>200 and min(px(img,720,2))>200 and min(px(img,720,452))>200, 'picture not centred'
    print('PASS: 800x480 frame -> whole picture kept, letterboxed with side bars (sq5_cluster_800 fallback)')
    # Colour order (car photos 2026-09-29: blue route shown red, green card olive = R/B swapped on the car
    # window, which is BGRA). The host output is RGB: a blue frame must come out blue.
    blue=t/'blue.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=0x1a3cd8:s=1280x720:r=30','-frames:v','60',
        '-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',blue)
    img=present(t,blue,'blue')
    r,g,b=px(img,720,227)
    assert b>180 and r<60 and g<90, f'blue frame came out as rgb={r},{g},{b}'
    print(f'PASS: colour order (blue 1a3cd8 -> rgb {r},{g},{b}); car window writes BGRA from the same code')
    # Picture shift (run 10: Google's car sat under the Audi footer): "<left> <up>" in cockpit pixels.
    img=present(t,band2,'shift',{'CL_TEST_SHIFT':'30 60'})
    # Run 108: the phone draws the whole frame (GAL 4.3), so rows uncovered by the shift come from the REAL frame
    # below the band (black in this synthetic frame) and columns past the frame edge repeat it (no black strip).
    assert max(px(img,720,452))<30 and max(px(img,720,400))<30, 'rows uncovered by the up shift must come from the frame below the band'
    assert min(px(img,720,390))>200 and min(px(img,720,2))>200, 'picture should still fill above the shifted strip'
    assert min(px(img,1437,227))>200 and min(px(img,1405,227))>200, 'right columns repeat the frame edge after shifting left'
    print('PASS: shift left 30 / up 60 -> real frame rows below the band, right edge repeated (no black strip)')
    # Roller zoom (player magnification, 2026-09-29; x focus unchanged at 45 %): a dark 8 px line at source x 1500 sits at output
    # x ~1125 unzoomed; at x1.5 around the focus (50 % of the width = 720, run 108) it moves to ~1327.
    zl=t/'zoomline.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=white:s=1920x1080:r=30',
        '-vf','drawbox=x=1496:y=0:w=8:h=1080:color=black:t=fill','-frames:v','60',
        '-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',zl)
    img=present(t,zl,'zoom1')
    assert max(px(img,1125,227))<60 and min(px(img,1327,227))>200, 'unzoomed line position'
    img=present(t,zl,'zoom15',{'CL_TEST_ZOOM':'384'})
    assert min(px(img,1125,227))>200 and max(px(img,1327,227))<60, f'zoomed line not at ~1327: {tuple(px(img,1327,227))}'
    assert min(px(img,5,5))>200 and min(px(img,1435,450))>200, 'zoom must still fill the window (no black edges)'
    print('PASS: roller zoom x1.5 around the focus point, window still filled')
    # 2026-09-30 viewport mode: 1920x1080 with margins 480x540 -> window 99 = frame x 240..1679, y 296..750, 1:1.
    vp=t/'viewport.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=black:s=1920x1080:r=30',
        '-vf','drawbox=x=240:y=296:w=1440:h=455:color=white:t=fill,drawbox=x=700:y=0:w=8:h=1080:color=black:t=fill',
        '-frames:v','60','-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',vp)
    img=present(t,vp,'viewport',{'CL_TEST_BAND':None})
    for x,y in ((3,3),(1436,3),(3,451),(1436,451),(720,227)):
        assert min(px(img,x,y))>200, f'viewport not filling 1440x455 1:1 at {x},{y}: {tuple(px(img,x,y))}'
    assert max(px(img,463,227))<60 and min(px(img,455,227))>200 and min(px(img,472,227))>200, 'viewport not 1:1 (line at frame x 700 -> window 460..467)'
    print('PASS: 1080p viewport -> frame (240,296) 1440x455 shown 1:1, no scaling')
    # run 127 (lag): the 1:1 fast path must show the same picture as the bilinear path (colour test pattern)
    tp=t/'pattern.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=1920x1080:rate=30','-frames:v','40',
        '-c:v','libx264','-preset','ultrafast','-g','30','-f','h264',tp)
    fa=present(t,tp,'fast',{'CL_TEST_BAND':None})
    sl=present(t,tp,'slow',{'CL_TEST_BAND':None,'CL_TEST_NOFAST':'1'})
    diff=[abs(a-b) for a,b in zip(fa,sl)]
    mean=sum(diff)/len(diff)
    assert len(fa)==len(sl) and mean<3.0, f'fast path differs: mean {mean:.2f}'
    print(f'PASS: 1:1 fast path matches the bilinear path (mean |diff| {mean:.2f}, max {max(diff)})')
    # 2026-09-30 card cut-out: zoomed map, Google's card stays 1:1 at its unzoomed place; the zoomed card is gone.
    cv=t/'cardcut.h264'
    run('ffmpeg','-v','error','-f','lavfi','-i','color=0x323E57:s=1920x1080:r=30',
        '-vf','drawbox=x=1340:y=464:w=312:h=130:color=0x09786E:t=fill,drawbox=x=1340:y=594:w=312:h=55:color=0x121212:t=fill',
        '-frames:v','60','-c:v','libx264','-preset','ultrafast','-qp','0','-g','30','-f','h264',cv)
    img=present(t,cv,'cardcut1',{'CL_TEST_BAND':None})
    r,g,b=px(img,1250,230); assert g>90 and r<50, f'unzoomed card not at 1250,230: {(r,g,b)}'
    img=present(t,cv,'cardcut15',{'CL_TEST_BAND':None,'CL_TEST_ZOOM':'384'})
    r,g,b=px(img,1250,230); assert g>90 and r<50, f'card moved/scaled when zoomed: {(r,g,b)} at 1250,230'
    r,g,b=px(img,1105,175); assert g>90 and r<50, f'card top-left corner not 1:1: {(r,g,b)}'
    r,g,b=px(img,1430,120); assert not (g>90 and r<50), f'zoomed copy of the card still visible at 1430,120: {(r,g,b)}'
    r,g,b=px(img,1250,330); assert max(r,g,b)<40, f'ETA strip not pasted 1:1 at 1250,330: {(r,g,b)}'
    print('PASS: card cut-out - zoom x1.5 keeps the card 1:1 at its place, zoomed copy replaced by map colour')
