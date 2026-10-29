"""Execute the production wireless codec/queue on ARM/QNX; no network or HU."""
import hashlib
import argparse
import fcntl
import json
import math
import re
import struct
from pathlib import Path
import shutil
import subprocess
import time
from . import qemu_capture as capture

ROOT=capture.REPO
OUT=ROOT/'build/wireless-validation-20261022/audio-qnx'
SNAPSHOT=ROOT/'build/wired-altscreen-30fps-re/native/qnx-network'

def aac_alignment(text, reference):
    """Independent decoder reference distinguishes suppression from delay.

    Cinemo suppresses the priming AU. Later output belongs to the current AU,
    so retaining the suppressed timestamp would shift the whole audio clock.
    Compare several nonperiodic frames, allowing decoder rounding/DRC.
    """
    evidence=[]
    for match in re.finditer(r'AAC_PCM frame=(\d+) hex=([0-9a-f]{1024})',text):
        index=int(match[1]);pcm=struct.unpack('<256h',bytes.fromhex(match[2]))
        errors={}
        for delta in (-2,-1,0,1,2):
            offset=(index+delta)*4096
            if offset<0 or offset+512>len(reference):continue
            expected=struct.unpack_from('<256h',reference,offset)
            errors[delta]=math.sqrt(sum((a-b)**2 for a,b in zip(pcm,expected))/256)
        best=min(errors,key=errors.get)
        evidence.append({'input_au':index,'best_reference_offset_aus':best,'rmse':errors})
    if len(evidence)<4 or any(row['best_reference_offset_aus'] for row in evidence):
        raise RuntimeError('AAC output timestamp alignment disagrees with independent decoder')
    return {'passed':True,'reference':'FFmpeg AAC decoder, deterministic pink noise',
            'suppressed_priming_au':True,'frames':evidence}

def _run(streams=False):
    global OUT
    if streams:OUT=OUT.with_name('audio-stream-qnx')
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'result.json').write_text(json.dumps({'passed':False,'state':'running'})+'\n')
    extracted=SNAPSHOT/'extracted/proc/boot'
    for name in ('startup-virt','libstartup.a'):shutil.copy2(SNAPSHOT/name,OUT/name)
    cc='arm-unknown-nto-qnx6.5.0eabi-gcc'
    project='/src/Patches/mhi2-carplay/'
    flags=['-O2','-mno-unaligned-access','-mfpu=neon','-mfloat-abi=softfp',
           '-I'+project+'build/wireless-codecs/opus-1.5.2/include']
    sources=[project+'hook/wireless/audio_codec.c',project+'hook/wireless/audio_queue.c',
             project+'hook/audio/aac_bridge.c',project+'build/wireless-codecs/opus-qnx/.libs/libopus.a']
    if streams:
        subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-f','lavfi','-i',
            'anoisesrc=color=pink:seed=1234:sample_rate=48000:duration=0.17','-ac','2','-c:a','aac',
            '-b:a','128k','-f','adts','-y',str(OUT/'vectors.aac')],check=True)
        wire=(OUT/'vectors.aac').read_bytes();vectors=[]
        while wire:
            if wire[:2]!=b'\xff\xf1':raise ValueError('unexpected AAC ADTS header')
            size=((wire[3]&3)<<11)|(wire[4]<<3)|(wire[5]>>5)
            if size<7 or size>len(wire):raise ValueError('truncated AAC vector')
            vectors.append(wire[7:size]);wire=wire[size:]
        header=f'#define AAC_VECTOR_COUNT {len(vectors)}\n'
        for i,v in enumerate(vectors):header+=f'static const unsigned char aac_{i}[]={{'+','.join(map(str,v))+'};\n'
        header+='static const unsigned char *aac_vectors[]={'+','.join('aac_'+str(i) for i in range(len(vectors)))+'};\n'
        header+='static const unsigned aac_lengths[]={'+','.join(str(len(v)) for v in vectors)+'};\n'
        (OUT/'wireless_audio_aac_vectors.h').write_text(header)
        subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-i',str(OUT/'vectors.aac'),
            '-f','s16le','-acodec','pcm_s16le','-y',str(OUT/'reference.pcm')],check=True)
        capture.docker(OUT,cc,*flags,'-fPIC','-shared','-DHOOK_AUDIO=1','-DENABLE_LOGGING=0','-DSPA_AAC_TRACE',
            project+'hook/wireless/audio_stream.c',project+'hook/wireless/advertisement.c',*sources,
            '-lm','-o','/work/libwireless_audio_probe.so')
        capture.docker(OUT,cc,*flags,'-I/work',project+'tests/carplay_lab/qnx/wireless_audio_stream_probe.c',
            '-Wl,-E','-Wl,--allow-shlib-undefined','-L/work','-lwireless_audio_probe',
            '-L/src/'+str(extracted.relative_to(capture.AUDI)),'-lairplay','-lm','-o','/work/probe')
    else:capture.docker(OUT,cc,*flags,
        project+'tests/wireless_audio_codec_test.c',*sources,'-lm','-o','/work/probe')
    prefix=(SNAPSHOT/'network.build').read_text().split('[+script]',1)[0]
    prefix=prefix.replace('/work/extracted/proc/boot/procnto-smp',
        '/src/'+str((extracted/'procnto-smp').relative_to(capture.AUDI)))
    script='''[+script] .script = {
    procmgr_symlink /proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    LD_LIBRARY_PATH=/proc/boot:/eso/lib
    /proc/boot/audio-probe
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=file perms=0755 data=copy] /proc/boot/audio-probe=/work/probe
'''
    names=['devc-serdebug','libc.so.3','libm.so.2','libgcc_s.so.1']
    if streams:
        script=script.replace('    /proc/boot/audio-probe\n',
            '    /proc/boot/pipe &\n    waitfor /dev/pipe 4\n'
            '    CACHE_NOP=1 /proc/boot/io-pkt-v6-hc\n    waitfor /dev/socket 5\n'
            '    /proc/boot/audio-probe\n')
        names+=['pipe','io-pkt-v6-hc','pidin']
        names+=sorted(p.name for p in extracted.iterdir() if '.so' in p.name and p.name not in names)
    for name in names:
        path=extracted/name
        if path.exists():script+=f'[type=file perms=0755 data=copy] /proc/boot/{name}=/src/{path.relative_to(capture.AUDI)}\n'
    if streams:
        script+='[type=file perms=0755 data=copy] /proc/boot/libwireless_audio_probe.so=/work/libwireless_audio_probe.so\n'
        plugins=capture.AUDI/'Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/advanced/MU1316-appimg/armle/usr/lib/cinemo'
        for name in ('cinemo_classes.xml','libNmeAudioAAC.so','libNmeEncoder.so'):
            script+=f'[type=file perms=0755 data=copy] /mnt/app/armle/usr/lib/cinemo/{name}=/src/{(plugins/name).relative_to(capture.AUDI)}\n'
        for p in sorted((extracted.parent.parent/'eso/lib').rglob('*')):
            if p.is_file():script+=f'[type=file perms=0755 data=copy] /eso/lib/{p.relative_to(extracted.parent.parent/"eso/lib")}=/src/{p.relative_to(capture.AUDI)}\n'
    (OUT/'probe.build').write_text(prefix+script)
    capture.docker(OUT,'mkifs','probe.build','probe.bin',capture_output=True)
    command=[str(capture.QEMU),'-M','virt','-m','512','-cpu','cortex-a15,cntfrq=62500000',
             '-device',f'loader,file={OUT}/probe.bin,addr=0x40200000,force-raw=on,cpu-num=0',
             '-display','none','-serial','stdio','-monitor','none','-nic','none']
    with (OUT/'serial.log').open('w') as log,(OUT/'host.log').open('w') as host:
        process=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=log,stderr=host)
        try:
            deadline=time.monotonic()+45
            marker='AUDIO_STREAM_PASS' if streams else 'bounds PASS'
            while time.monotonic()<deadline:
                text=(OUT/'serial.log').read_text(errors='replace')
                if marker in text or 'AUDIO_STREAM_FAIL' in text or process.poll() is not None:break
                if 'exited' in text or 'assertion' in text.lower():break
                time.sleep(.2)
            if marker not in text:raise RuntimeError('ARM audio failed; inspect '+str(OUT/'serial.log'))
            if streams and 'AAC_RX_OPUS_TX_PASS' not in text:
                raise RuntimeError('missing concurrent native AAC RX / stalled Opus TX acceptance')
        finally:
            if process.poll() is None:
                process.terminate()
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
    receipt={'passed':True,'scope':'real ARM/QNX codec and queue, no external interfaces',
             'command':command,'ifs_sha256':hashlib.sha256((OUT/'probe.bin').read_bytes()).hexdigest(),
             'probe_sha256':hashlib.sha256((OUT/'probe').read_bytes()).hexdigest(),
             'sources':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in
                        (ROOT/'hook/wireless/audio_codec.c',ROOT/'hook/wireless/audio_queue.c',ROOT/'hook/wireless/audio_stream.c',ROOT/'hook/audio/aac_bridge.c',
                         ROOT/('tests/carplay_lab/qnx/wireless_audio_stream_probe.c' if streams else 'tests/wireless_audio_codec_test.c'))}}
    if streams:
        receipt['aac_timestamp_alignment']=aac_alignment(text,(OUT/'reference.pcm').read_bytes())
        receipt['native_aac_rx_during_blocked_opus_tx']=True
    (OUT/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(text)
    return receipt

def run(streams=False):
    directory=OUT.with_name('audio-stream-qnx') if streams else OUT
    directory.mkdir(parents=True,exist_ok=True)
    with (directory/'runtime.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        return _run(streams)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--streams',action='store_true')
    args=parser.parse_args()
    print(json.dumps(run(args.streams),indent=2))
