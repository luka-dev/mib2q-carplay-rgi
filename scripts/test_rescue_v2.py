#!/usr/bin/env python3
"""Host fault tests for rescue payload; never addresses real /mnt or /net.

These tests validate file handling, not QNX/SWDL execution or firmware acceptance.
"""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CARD = ROOT / 'rescue_Swdlautorun_v2'
SOURCE = (CARD / 'Launcher/rescue.sh').read_text()

def scenario(name, *, missing=False, fail_mount=False, fail_move=False,
             bad_log=False, existing_backup=False, rerun=False):
    with tempfile.TemporaryDirectory(prefix='rescue-v2-test-') as tmp:
        root = Path(tmp)
        card = root / 'sd'
        card.mkdir()
        (card / 'Swdlautorun.txt').touch()
        jars = root / 'mnt/app/eso/hmi/lsd/jars'
        if not missing:
            jars.mkdir(parents=True)
            for filename in ['carplay_hook.jar', 'carplay_hook_cn.jar',
                             '._carplay_hook.jar', 'AppDevelopment.jar']:
                (jars / filename).write_text(filename)
        backup = root / 'mnt/app/eso/hmi/carplay-rescue-v2'
        if existing_backup:
            backup.mkdir(parents=True)
            (backup / 'carplay_hook.jar').write_text('older quarantine')
        if bad_log:
            (card / 'carplay_rescue_v2.log').mkdir()
        bin_dir = root / 'bin'
        bin_dir.mkdir()
        stubs = {
            'mount': '#!/bin/sh\n[ "$FAIL_MOUNT" != 1 ] || [ "$2" != "$TEST_APP" ]\n',
            'sync': '#!/bin/sh\nexit 0\n',
            'mv': '#!/bin/sh\n[ "$FAIL_MOVE" != 1 ] || exit 1\nexec /bin/mv "$@"\n',
        }
        for cmd, body in stubs.items():
            f = bin_dir / cmd
            f.write_text(body)
            f.chmod(0o755)
        script = SOURCE.replace('/mnt/', str(root / 'mnt') + '/')
        script = script.replace('/tmp/carplay_rescue_v2.log', str(root / 'fallback.log'))
        script = re.sub(r'^PATH=.*$', 'PATH=' + str(bin_dir) + ':/bin:/usr/bin', script, flags=re.M)
        payload = root / 'payload.sh'
        payload.write_text(script)
        env = dict(os.environ, FAIL_MOUNT=str(int(fail_mount)), FAIL_MOVE=str(int(fail_move)),
                   TEST_APP=str(root / 'mnt/app'))
        result = subprocess.run(['/bin/sh', str(payload), str(card)], env=env,
                                capture_output=True, text=True)
        failure = missing or fail_mount or fail_move
        assert (result.returncode != 0) == failure, (name, result)
        if failure:
            assert (card / 'Swdlautorun.txt').exists(), name
            if not missing:
                assert (jars / 'carplay_hook.jar').exists(), name
        else:
            assert not (jars / 'carplay_hook.jar').exists(), name
            assert not (jars / 'carplay_hook_cn.jar').exists(), name
            assert not (jars / '._carplay_hook.jar').exists(), name
            assert (jars / 'AppDevelopment.jar').read_text() == 'AppDevelopment.jar', name
            assert (card / '_Swdlautorun.txt').exists(), name
            log = root / 'fallback.log' if bad_log else card / 'carplay_rescue_v2.log'
            assert 'VERIFIED:' in log.read_text(), name
            if existing_backup:
                assert (backup / 'carplay_hook.jar').read_text() == 'older quarantine', name
                assert len(list(backup.glob('carplay_hook.jar.*'))) == 1, name
            if rerun:
                result = subprocess.run(['/bin/sh', str(payload), str(card)], env=env,
                                        capture_output=True, text=True)
                assert result.returncode == 0, name
                assert 'quarantined=0' in log.read_text(), name
        print('PASS', name)

scenario('quarantine and idempotent rerun', rerun=True)
scenario('unavailable MMX filesystem', missing=True)
scenario('read-only app/remount failure', fail_mount=True)
scenario('failed quarantine', fail_move=True)
scenario('card log failure does not skip repair', bad_log=True)
scenario('preserve earlier quarantine', existing_backup=True)

meta = (CARD / 'metainfo2.txt').read_text()
final = (CARD / 'Launcher/final/finalScript.sh').read_bytes()
expected = re.search(r'FinalScriptChecksum = "(.*?)"', meta)[1]
assert hashlib.sha1(final).hexdigest() == expected
assert final == (ROOT / 'rescue_Swdlautorun/Launcher/final/finalScript.sh').read_bytes()
token = (CARD / 'CarplayRescue24/version/RESCUE24').read_bytes()
part = meta.split('[CarplayRescue24\\version\\0\\default\\File]')[1]
assert hashlib.sha1(token).hexdigest() == re.search(r'CheckSum = "(.*?)"', part)[1]
assert len(token) == int(re.search(r'FileSize = "(.*?)"', part)[1])
assert 'DownloadGroup = "CarplayRescue24"' in meta
assert 'Destination = "/HBpersistence/SWDL/Log/service/CARPLAY_RESCUE24_V2"' in meta
assert '[Launcher\\version' not in meta
print('PASS package checksums, original FinalScript, independent component identity')
