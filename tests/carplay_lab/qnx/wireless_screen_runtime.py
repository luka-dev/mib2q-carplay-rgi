"""Bounded ARM/QNX wireless 110 wrapper check; no external network or HU."""
import fcntl
import hashlib
import json
import shutil
import subprocess
import time
from . import qemu_capture as capture

ROOT = capture.REPO
OUT = ROOT / 'build/wireless-validation-20261022/receiver-video/native'
SNAPSHOT = ROOT / 'build/wired-altscreen-30fps-re/native/qnx-network'


def run(*, audio=False):
    out = ROOT/'build/wireless-validation-20261023/receiver-audio/native' if audio else OUT
    out.mkdir(parents=True, exist_ok=True)
    with (out / 'runtime.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        receipt = out / 'result.json'
        receipt.write_text('{"passed":false,"state":"running"}\n')
        extracted = SNAPSHOT / 'extracted/proc/boot'
        for name in ('startup-virt', 'libstartup.a'):
            shutil.copy2(SNAPSHOT / name, out / name)
        project = '/src/Patches/mhi2-carplay/'
        source = 'tests/wireless_audio_concurrency_test.c' if audio else 'tests/carplay_lab/qnx/wireless_screen_reader_probe.c'
        flags = ['-DWIRELESS_TEST_REAL_CRYPTO=1',
                 '-I'+project+'build/wireless-codecs/opus-1.5.2/include'] if audio else []
        extra = [project+'hook/wireless/audio_codec.c', project+'hook/wireless/audio_queue.c',
                 project+'hook/audio/aac_bridge.c',
                 project+'build/wireless-codecs/opus-qnx/.libs/libopus.a'] if audio else []
        capture.docker(out, 'arm-unknown-nto-qnx6.5.0eabi-gcc', '-O2',
            '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
            '-mno-unaligned-access', '-mfpu=neon', '-mfloat-abi=softfp',
            *flags, project + source, *extra,
            project + 'build/wireless-sdk/libwireless_pairing.a',
            '-lm', '-o', '/work/probe')
        prefix = (SNAPSHOT / 'network.build').read_text().split('[+script]', 1)[0]
        prefix = prefix.replace('/work/extracted/proc/boot/procnto-smp',
            '/src/' + str((extracted / 'procnto-smp').relative_to(capture.AUDI)))
        script = '''[+script] .script = {
    procmgr_symlink /proc/boot/libc.so.3 /usr/lib/ldqnx.so.2
    /proc/boot/devc-serdebug -F -S
    waitfor /dev/ser1 4
    reopen /dev/ser1
    LD_LIBRARY_PATH=/proc/boot
    /proc/boot/screen-probe
}
[type=link] /usr/lib/ldqnx.so.2=/proc/boot/libc.so.3
[type=file perms=0755 data=copy] /proc/boot/screen-probe=/work/probe
'''
        for name in ('devc-serdebug', 'libc.so.3', 'libm.so.2', 'libgcc_s.so.1'):
            script += f'[type=file perms=0755 data=copy] /proc/boot/{name}=/src/{(extracted/name).relative_to(capture.AUDI)}\n'
        (out / 'probe.build').write_text(prefix + script)
        capture.docker(out, 'mkifs', 'probe.build', 'probe.bin', capture_output=True)
        command = [str(capture.QEMU), '-M', 'virt', '-m', '512', '-cpu',
            'cortex-a15,cntfrq=62500000', '-device',
            f'loader,file={out}/probe.bin,addr=0x40200000,force-raw=on,cpu-num=0',
            '-display', 'none', '-serial', 'stdio', '-monitor', 'none', '-nic', 'none']
        marker = 'WIRELESS_AUDIO_CONCURRENCY_PASS' if audio else 'WIRELESS_SCREEN_READER_PASS'
        with (out / 'serial.log').open('w') as log, (out / 'host.log').open('w') as host:
            process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=log, stderr=host)
            try:
                deadline = time.monotonic() + 35
                while time.monotonic() < deadline:
                    text = (out / 'serial.log').read_text(errors='replace')
                    if (marker in text or 'assertion' in text.lower()
                            or '(screen-probe) exited' in text or '(screen-probe) terminated' in text
                            or process.poll() is not None):
                        break
                    time.sleep(.2)
                if marker not in text:
                    raise RuntimeError('wireless probe failed; inspect ' + str(out/'serial.log'))
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
        sources = [ROOT/'hook/wireless/media.c', ROOT/'build/wireless-sdk/libwireless_pairing.a', ROOT/source]
        if audio:
            sources += [ROOT/name for name in ('hook/wireless/audio_stream.c',
                'hook/wireless/audio_queue.c', 'hook/wireless/audio_codec.c', 'hook/audio/aac_bridge.c',
                'build/wireless-codecs/opus-qnx/.libs/libopus.a')]
        scope = ('native ARM/QNX media/codec RX-TX lifetime, real Opus and SDK AEAD; '
                 'jitter/socket resource calls modeled, no real phone/QSA output or HU FPS claim') if audio else (
                 'native ARM/QNX screen_read + real SDK AEAD; '
                 'in-memory NetSocket source, no phone/OMX/presentation or HU FPS claim')
        result = {'passed': True, 'scope': scope,
                  'command': command, 'ifs_sha256': hashlib.sha256((out/'probe.bin').read_bytes()).hexdigest(),
                  'sources': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sources}}
        receipt.write_text(json.dumps(result, indent=2) + '\n')
        print(text)
        return result


if __name__ == '__main__':
    print(json.dumps(run(), indent=2))
