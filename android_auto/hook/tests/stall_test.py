"""Car logs 2026-09-29: the phone sends ONE keyframe at cluster-stream start. A 1.5 s stall at the first
presented frame (window adoption) filled the 8-slot ring; the hook then waited for a keyframe that never
came and the player got 8 packets in total while the hook received 600+. With the reader thread the ring
is drained into the player's own queue, so every frame must decode despite the stall. Host build only.
"""
from pathlib import Path
import subprocess, tempfile, time, select
P=Path(__file__).resolve().parents[1]
def run(*cmd):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,timeout=60)
    if r.returncode: raise AssertionError(f'{cmd}\n{r.stdout}\n{r.stderr}')
    return r.stdout
with tempfile.TemporaryDirectory(prefix='sq5-stall-') as td:
    t=Path(td)
    flags=['gcc','-O1','-no-pie','-std=gnu99','-I'+str(P/'src'),'-I'+str(P/'vendor')]
    run(*flags,P/'src/player.c','-DCL_HOST','-pthread','-lavcodec','-lavutil','-o',t/'player')
    run(*flags,P/'tests/producer.c',P/'src/transport.c','-pthread','-lavcodec','-lavutil','-o',t/'producer')
    movie=t/'onekey.h264'
    # 120 frames at 1280x720, keyframe ONLY at the start (like the phone's cluster stream)
    run('ffmpeg','-v','error','-f','lavfi','-i','testsrc2=size=1280x720:rate=30','-frames:v','120',
        '-c:v','libx264','-preset','ultrafast','-tune','zerolatency','-g','100000','-keyint_min','100000',
        '-sc_threshold','0','-f','h264',movie)
    flags_=run('ffprobe','-v','error','-select_streams','v','-show_entries','packet=flags','-of','csv=p=0',movie).split()
    assert len(flags_)==120 and sum('K' in f for f in flags_)==1, flags_[:5]
    producer=subprocess.Popen([str(t/'producer'),str(movie)],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
    try:
        assert select.select([producer.stdout],[],[],5)[0], 'producer startup timed out'
        pid=producer.stdout.readline().strip()
        with (t/'player.log').open('w') as log:
            player=subprocess.Popen([str(t/'player'),pid],stdout=log,stderr=log,
                                    env={'CL_TEST_STALL_MS':'1500','PATH':'/usr/bin:/bin'})
            time.sleep(8)       # 120 frames at 30 fps = 4 s, plus producer start and the stall
            producer.kill(); producer.wait(timeout=2)
            assert player.wait(timeout=10)==0, (t/'player.log').read_text()
    finally:
        if producer.poll() is None: producer.kill(); producer.wait()
    log=(t/'player.log').read_text()
    done=Path(f'/tmp/sq5_cluster_live-{pid}.h264.done').read_text()
    for suffix in ('.h264','.h264.done','.ppm'):
        f=Path(f'/tmp/sq5_cluster_live-{pid}{suffix}')
        if f.exists(): f.unlink()
    # 119 = every frame the test producer sends: its H.264 parser keeps the last of the 120 until a next
    # start code that never comes (run.py: 89 of 90 for the same reason). Frame threads hold more; the
    # player drains the decoder at end of stream, so exactly 119 must come out.
    assert 'decoded=119' in done, done+log
    assert 'ring_drops=0' in log and 'queue_drops=0' in log, log
    print('PASS: one-keyframe 1280x720 stream + 1.5 s first-present stall -> every sent frame decoded (decoder drained at end), no ring drops')
