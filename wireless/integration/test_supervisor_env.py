#!/usr/bin/env python3
"""Check the real launch script's factory environment and missing-config refusal."""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    script = Path(__file__).with_name('wireless_dio_supervisor.sh')
    with tempfile.TemporaryDirectory() as temp:
        home = Path(temp)
        (home / 'wireless_carplay.env').write_text('export MHI2Q_CP_RECEIVER=stock210\n')
        native = home / 'wireless_dio_supervisor_bin'
        native.write_text('#!/bin/sh\nprintf "%s\\n" "$IPL_CONFIG_DIR" "$IPL_CONFIG_DIR_DIO_MANAGER" "$LD_LIBRARY_PATH"\n')
        native.chmod(0o755)
        runtime = home / 'wireless_runtime'
        runtime.write_text('#!/bin/sh\nexec "$1"\n')
        runtime.chmod(0o755)
        env = {k: v for k, v in os.environ.items() if k not in
               ('IPL_CONFIG_DIR', 'IPL_CONFIG_DIR_DIO_MANAGER', 'LD_LIBRARY_PATH', 'MHI2Q_CP_CONFIG')}
        env.update(H=str(home), IPL_CONFIG_DIR=str(home))
        failed = subprocess.run(['sh', str(script)], env=env, text=True, capture_output=True)
        assert failed.returncode == 78 and 'configuration unavailable' in failed.stderr
        for name in ('framework.json', 'dio_manager.json'):
            (home / name).write_text('{}\n')
        ready = subprocess.run(['sh', str(script)], env=env, text=True, capture_output=True, check=True)
        lines = ready.stdout.splitlines()
        assert lines[:2] == [str(home), str(home)] and '/eso/lib' in lines[2]
        custom = home / 'custom'
        custom.mkdir(); (custom / 'dio_manager.json').write_text('{}\n')
        env.update(IPL_CONFIG_DIR_DIO_MANAGER=str(custom), LD_LIBRARY_PATH='/explicit/libs')
        ready = subprocess.run(['sh', str(script)], env=env, text=True, capture_output=True, check=True)
        assert ready.stdout.splitlines() == [str(home), str(custom), '/explicit/libs']
    print('supervisor environment: factory config, inherited overrides and fail-before-exec passed')


if __name__ == '__main__':
    main()
