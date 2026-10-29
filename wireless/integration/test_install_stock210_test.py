#!/usr/bin/env python3
"""Exercise stock210 staging/rollback with host executables in a disposable root."""
from __future__ import annotations

import hashlib
import argparse
import os
import re
from pathlib import Path
import subprocess
import tarfile
import tempfile

from build_stock210_hu_test import FW, OUT, REPO


def checksum(stage: Path) -> None:
    (stage / 'checksums.txt').write_text(''.join(
        f'{hashlib.sha256(path.read_bytes()).hexdigest()} {path.relative_to(stage)}\n'
        for path in sorted(stage.rglob('*')) if path.is_file() and path.name != 'checksums.txt'))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, default=OUT)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='stock210-install-') as temp:
        stage, root = Path(temp) / 'stage', Path(temp) / 'root'
        stage.mkdir(); root.mkdir()
        with tarfile.open(args.archive) as archive:
            archive.extractall(stage, filter='data')
        script = stage / 'integration/install_stock210_test.sh'
        (stage / 'baseline-status.txt').write_text('pending\n')
        config = stage / 'integration/wireless_carplay.env'
        config.write_text(re.sub(r'^export MHI2Q_CP_MODEL=.*$', "export MHI2Q_CP_MODEL=''",
                                 config.read_text(), flags=re.MULTILINE))
        # Never execute QNX binaries on the host. The exact installer still
        # verifies all bytes after these substitutions in this throwaway stage.
        daemon = stage / 'bin/mhi2q-wireless-daemon'
        daemon.write_text('#!/bin/sh\n[ "$1" = --check-config ] && [ -n "$MHI2Q_CP_MODEL" ]\n')
        daemon.chmod(0o755)
        subprocess.run(['cc', '-std=c11', '-Os', '-Wall', '-Wextra', '-Werror',
                        str(REPO / 'wireless/integration/dio_owner_exec.c'),
                        '-o', str(stage / 'bin/dio_owner_exec')], check=True)
        originals = {}
        for name in ('btstack', 'pf.conf', 'pf.ecm0.conf', 'pf.mlan0.conf', 'pf.ppp0.conf', 'carplay_startup.sh'):
            path = ('/mnt/app/eso/bin/apps/btstack' if name == 'btstack' else
                    '/mnt/app/root/hooks/carplay_startup.sh' if name.endswith('.sh') else f'/mnt/system/etc/{name}')
            originals[path] = (stage / 'reference' / name).read_bytes()
        for path, source in (('/mnt/app/eso/lib/libairplay.so', FW / 'eso/lib/libairplay.so'),
                             ('/mnt/app/eso/bin/apps/dio_manager', FW / 'eso/bin/apps/dio_manager'),
                             ('/mnt/app/armle/usr/sbin/mdnsd', FW / 'armle/usr/sbin/mdnsd'),
                             ('/mnt/app/armle/usr/lib/libdns_sd.so.1', FW / 'armle/usr/lib/libdns_sd.so.1')):
            originals[path] = source.read_bytes()
        protected = {
            '/mnt/app/root/hooks/libcarplay_hook.so': b'existing wired hook',
            '/mnt/app/root/hooks/libcarplay_apac.so': b'existing wired APAC',
            '/mnt/app/root/rc.local': b'existing user startup\n',
        }
        for path, data in {**originals, **protected}.items():
            target = root / path.lstrip('/')
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (root / 'mnt/persist').mkdir(parents=True)
        (root / 'tmp').mkdir()
        # Also guard the wired library, independently of the replaced wrapper.
        with (stage / 'untouched.txt').open('w') as stream:
            for path, data in {**{path: data for path, data in originals.items()
                                if path.endswith(('/libairplay.so', '/dio_manager', '/mdnsd', '/libdns_sd.so.1'))}, **protected}.items():
                stream.write(f'{hashlib.sha256(data).hexdigest()} {path}\n')
        checksum(stage)

        def run(action: str, success: bool, fail_path: str = '') -> str:
            result = subprocess.run(['sh', str(script), action], text=True, capture_output=True,
                env=dict(os.environ, MHI2Q_STAGE=str(stage), MHI2Q_TARGET_ROOT=str(root),
                         MHI2Q_TEST_FAIL_PATH=fail_path))
            assert (result.returncode == 0) == success, (action, result.stdout, result.stderr)
            return result.stdout + result.stderr

        def unchanged() -> None:
            for path, data in {**originals, **protected}.items():
                assert (root / path.lstrip('/')).read_bytes() == data, path
            assert not list(root.rglob('*.stock210-prev'))
            assert not list(root.rglob('.mhi2q-stock210-new.*'))
            assert not (root / 'mnt/persist/.mhi2q_stock210_install.lock').exists()

        assert 'baseline not captured' in run('preflight', False)
        (stage / 'baseline-status.txt').write_text('verified\n'); checksum(stage)
        assert 'incomplete' in run('preflight', False)
        config.write_text(config.read_text().replace("export MHI2Q_CP_MODEL=''", 'export MHI2Q_CP_MODEL=HostTest'))
        checksum(stage)
        run('preflight', True); unchanged()
        pending = root / 'tmp/mhi2q_wifi_control.pending'
        pending.touch()
        assert 'radio control pending' in run('install', False)
        pending.unlink(); unchanged()
        backup = root / 'mnt/app/eso/bin/apps/btstack.stock210-prev'
        backup.symlink_to(root / 'missing-backup')
        assert 'existing backup' in run('install', False)
        backup.unlink()
        lock = root / 'mnt/persist/.mhi2q_stock210_install.lock'
        lock.mkdir()
        assert 'cannot acquire' in run('install', False)
        lock.rmdir(); unchanged()
        guard = root / 'mnt/app/root/hooks/libcarplay_hook.so'
        guard.write_bytes(b'foreign wired build')
        assert 'dependency differs' in run('install', False)
        guard.write_bytes(protected['/mnt/app/root/hooks/libcarplay_hook.so'])
        occupied = root / 'mnt/app/root/hooks/libcarplay_wireless_stock210.so'
        occupied.symlink_to(root / 'missing')
        assert 'owned path exists' in run('install', False)
        occupied.unlink()
        for failure in ('/mnt/app/root/hooks/mhi2q_mdns_probe', '/mnt/app/root/hooks/libcarplay_wireless_apac.so', '/mnt/system/etc/pf.ecm0.conf'):
            assert 'automatic rollback' in run('install', False, failure)
            unchanged()
            assert not occupied.exists()
        run('install', True)
        for name in ('wireless_runtime', 'mhi2q_mdns_probe'):
            assert (root / 'mnt/app/root/hooks' / name).read_bytes() == (stage / 'bin' / name).read_bytes()
        for path, data in protected.items():
            assert (root / path.lstrip('/')).read_bytes() == data
        assert (root / 'mnt/app/eso/lib/libairplay.so').read_bytes() == originals['/mnt/app/eso/lib/libairplay.so']
        assert (root / 'mnt/app/root/hooks/wireless_carplay.env').stat().st_mode & 0o777 == 0o600
        assert 'existing backup' in run('install', False)
        saved = occupied.read_bytes(); occupied.write_bytes(b'manual edit')
        assert 'changed owned file' in run('rollback', False)
        assert list(root.rglob('*.stock210-prev'))  # Nothing restored before full validation.
        occupied.write_bytes(saved)
        run('rollback', True); unchanged()
        run('rollback', True); unchanged()
        payload = stage / 'pf/pf.conf'
        payload.write_bytes(payload.read_bytes() + b'corrupt')
        assert 'checksum mismatch' in run('preflight', False)
        unchanged()
    print('stock210: baseline/config guards, quarantine, wired isolation, injected failures, integrity and rollback passed')


if __name__ == '__main__':
    main()
