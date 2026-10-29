"""Serialize local profiling commands so QEMU runs do not compete with each other."""
from __future__ import annotations
import fcntl
import hashlib
import json
import os
import signal
from pathlib import Path
import subprocess
import sys
import time

LOCK = Path('/tmp/audi2-wifi-perf.lock')
RECORDS = Path(__file__).resolve().parents[1] / 'build/wifi-perf-coordinator-20261023'


def host_snapshot():
    # Process names only: do not collect command arguments or environment.
    ps = subprocess.run(['ps', '-axo', 'pid,ppid,pcpu,comm'], capture_output=True, text=True, timeout=5)
    return {'process_cpu': ps.stdout,
            'logical_cpus': os.cpu_count(), 'time_ns': time.time_ns()}


def main():
    args = sys.argv[1:]
    if args[:1] == ['--']:
        args = args[1:]
    if not args:
        raise SystemExit('Usage: python3 -m wireless.perf_guard -- COMMAND [ARGS]')
    with LOCK.open('a+') as handle:
        print(f'Waiting for benchmark slot: {args[0]}', flush=True)
        fcntl.flock(handle.fileno(), fcntl.LOCK_EX)
        print(f'Benchmark slot acquired; monotonic={time.monotonic():.3f}', flush=True)
        record = {'command': args, 'before': host_snapshot(),
                  'guard_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
        start = time.monotonic()
        process = subprocess.Popen(args, start_new_session=True)
        record['owned_process_group'] = process.pid
        def interrupted(signum, frame):
            raise KeyboardInterrupt(f'Benchmark interrupted by signal {signum}')
        handlers = {sig: signal.signal(sig, interrupted) for sig in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP)}
        try:
            code = process.wait()
        except BaseException:
            # Only the group created for our own benchmark is terminated.
            # Unrelated QEMU processes are never selected by name or killed.
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
            raise
        finally:
            for sig, handler in handlers.items():
                signal.signal(sig, handler)
        record.update(returncode=code, elapsed_seconds=time.monotonic()-start,
                      after=host_snapshot())
        RECORDS.mkdir(parents=True, exist_ok=True)
        path = RECORDS / f'guard-{record["before"]["time_ns"]}-{os.getpid()}.json'
        path.write_text(json.dumps(record, indent=2) + '\n')
        print(f'Benchmark environment receipt: {path}', flush=True)
        raise SystemExit(code)


if __name__ == '__main__':
    main()
