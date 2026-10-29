#!/usr/bin/env python3
"""Real-process lifecycle checks; DNS-SD itself is exercised in the QNX fixture."""
import os
import fcntl
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

HERE = Path(__file__).resolve().parent


def wait_for(check, timeout=6):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if check(): return
        time.sleep(.03)
    raise AssertionError('process event timeout')


def stop(p):
    if p.poll() is None: p.terminate()
    try: p.wait(timeout=5)
    except subprocess.TimeoutExpired: p.kill(); p.wait(); raise


def main():
    with tempfile.TemporaryDirectory(prefix='mdns-runtime-') as temp:
        root = Path(temp)
        binary = root / 'wireless_runtime'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O1',
                        '-fsanitize=address,undefined', str(HERE / 'wireless_runtime.c'), '-o', str(binary)], check=True)
        fake = root / 'fake.py'
        fake.write_text('#!' + sys.executable + '\n' + '''import os,signal,sys,time
from pathlib import Path
root=Path(os.environ['FAKE_ROOT']); mode=Path(sys.argv[0]).name
def log(text):
 with (root/'events').open('a') as f: f.write(text+'\\n')
if mode=='probe':
 if os.environ.get('HANG_PROBE'):
  signal.signal(signal.SIGTERM,signal.SIG_IGN)
  while True: time.sleep(1)
 if os.environ.get('BAD_PROBE'): sys.exit(1)
 sys.exit(0 if (root/'ready').exists() else 2)
if mode=='supervisor':
 assert os.environ.get('MHI2Q_CP_MDNS_MANAGED')=='1'
 assert os.environ.get('MHI2Q_CP_MDNS_BINARY')==str(root/'mdns')
def done(n,f):
 log(mode+' stop')
 if mode=='mdns' and (root/'ready').exists(): (root/'ready').unlink()
 sys.exit(0)
signal.signal(signal.SIGTERM,done)
(root/(mode+'.pid')).write_text(str(os.getpid()))
log(mode+' start')
if mode=='mdns' and not os.environ.get('NO_READY'): (root/'ready').touch()
while True: time.sleep(.1)
''')
        fake.chmod(0o755)
        for name in ('probe', 'mdns', 'supervisor', 'daemon'):
            (root / name).symlink_to(fake)
        base = dict(os.environ, FAKE_ROOT=str(root), MHI2Q_CP_MDNS_PROBE=str(root / 'probe'),
                    MHI2Q_CP_MDNS_BINARY=str(root / 'mdns'), MHI2Q_CP_MDNS_LOCK=str(root / 'lock'),
                    MHI2Q_CP_MDNS_RUN_DIR=str(root / 'run'), MHI2Q_CP_MDNS_PROBE_MS='500',
                    MHI2Q_CP_MDNS_READY_MS='600', MHI2Q_CP_MDNS_HEALTH_MS='200',
                    MHI2Q_CP_RUNTIME_STOP_MS='1000', MHI2Q_DIO_OWNER_LOCK=str(root / 'owner'),
                    MHI2Q_DIO_WIRED_PRIORITY_LOCK=str(root / 'priority'))
        def events(): return (root / 'events').read_text().splitlines() if (root / 'events').exists() else []
        def reset():
            for name in ('events', 'ready', 'mdns.pid', 'supervisor.pid'):
                (root / name).unlink(missing_ok=True)
        def launch(extra=None):
            return subprocess.Popen([str(binary), str(root / 'supervisor')], env=base | (extra or {}),
                                    stderr=None)
        reset(); p = launch()
        try:
            wait_for(lambda: 'supervisor start' in events())
            other = launch(); assert other.wait(timeout=3) == 75
            stop(p)
            assert p.returncode == 0
            assert events() == ['mdns start', 'supervisor start', 'supervisor stop', 'mdns stop']
        finally: stop(p)
        reset(); p = launch()
        try:
            wait_for(lambda: 'supervisor start' in events())
            with (root/'owner').open('w') as owner, (root/'priority').open('w') as priority:
                fcntl.lockf(owner,fcntl.LOCK_EX|fcntl.LOCK_NB)
                fcntl.lockf(priority,fcntl.LOCK_EX|fcntl.LOCK_NB)
                wait_for(lambda: 'supervisor stop' in events())
                # Factory wired DIO may stop discovery before releasing locks.
                os.kill(int((root/'mdns.pid').read_text()),signal.SIGTERM)
                wait_for(lambda: 'mdns stop' in events())
                assert p.poll() is None and events().count('supervisor start')==1
                assert events().count('mdns start')==1
            wait_for(lambda: events().count('supervisor start')==2)
            assert events().count('mdns start')==2
            stop(p); assert p.returncode==0
            assert events()==['mdns start','supervisor start','supervisor stop','mdns stop',
                              'mdns start','supervisor start','supervisor stop','mdns stop']
        finally: stop(p)
        reset()
        external = subprocess.Popen([str(root / 'mdns'), '-f'], env=base)
        try:
            wait_for(lambda: (root / 'ready').exists())
            p = launch()
            try:
                wait_for(lambda: 'supervisor start' in events())
                (root / 'ready').unlink()
                assert p.wait(timeout=5) == 1
                assert external.poll() is None and 'mdns stop' not in events()
            finally: stop(p)
        finally: stop(external)
        # A still-held DIO lock prevents teardown of our owned responder.
        reset()
        with (root / 'owner').open('w') as owner:
            fcntl.lockf(owner, fcntl.LOCK_EX | fcntl.LOCK_NB)
            p = launch()
            mdns_pid = None
            try:
                wait_for(lambda: 'supervisor start' in events())
                mdns_pid = int((root / 'mdns.pid').read_text())
                stop(p)
                assert p.returncode == 1 and 'mdns stop' not in events()
                os.kill(mdns_pid, 0)
            finally:
                stop(p)
                if mdns_pid:
                    os.kill(mdns_pid, signal.SIGTERM)
                    wait_for(lambda: 'mdns stop' in events())
        reset(); p = launch()
        try:
            wait_for(lambda: 'supervisor start' in events())
            os.kill(int((root / 'mdns.pid').read_text()), signal.SIGKILL)
            assert p.wait(timeout=5) == 1
            assert 'supervisor stop' in events()
        finally: stop(p)
        for extra in ({'HANG_PROBE': '1'}, {'BAD_PROBE': '1'}, {'NO_READY': '1'}):
            reset(); p = launch(extra)
            try:
                assert p.wait(timeout=5) == 1
                assert 'supervisor start' not in events()
                if 'NO_READY' in extra: assert events() == ['mdns start', 'mdns stop']
                else: assert not events()
            finally: stop(p)
        reset(); p = launch({'NO_READY': '1'})
        try:
            wait_for(lambda: 'mdns start' in events())
            stop(p)
            assert events() == ['mdns start', 'mdns stop']
        finally: stop(p)
        # SI mode retains the bootstrap across wired ownership and replaces a
        # responder lost to factory teardown without restarting the daemon.
        reset()
        with (root/'owner').open('w') as owner, (root/'priority').open('w') as priority:
            fcntl.lockf(owner,fcntl.LOCK_EX|fcntl.LOCK_NB)
            fcntl.lockf(priority,fcntl.LOCK_EX|fcntl.LOCK_NB)
            p = subprocess.Popen([str(binary),str(root/'daemon')],
                env=base|{'MHI2Q_CP_SI_CONTROL':str(root/'si.sock')})
            try:
                wait_for(lambda: 'daemon start' in events())
                old = int((root/'mdns.pid').read_text())
                os.kill(old,signal.SIGTERM)
                wait_for(lambda: events().count('mdns start')==2)
                assert events().count('daemon start')==1 and p.poll() is None
            finally: stop(p)
            # The live native DIO owns discovery at shutdown; our runtime must
            # preserve that responder, even though bootstrap has exited.
            assert p.returncode==1 and events().count('daemon stop')==1
            os.kill(int((root/'mdns.pid').read_text()),signal.SIGTERM)
            wait_for(lambda: events().count('mdns stop')==2)
    print('runtime ASan/UBSan: ready ordering, wired suspension/discovery recovery, external adoption, exclusive owner, busy DIO preservation, IPC loss, crash, probe deadline and cleanup passed')


if __name__ == '__main__': main()
