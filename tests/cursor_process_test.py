"""Exercise the shipping process/socket/session with a fake Screen backend."""
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager

binary, fixture = sys.argv[1:]
base = bytearray(open(fixture, 'rb').read())

def packet(mode, seq, token=0):
    p = bytearray(base)
    p[1] = mode
    struct.pack_into('>II', p, 4, seq, token)
    return p

def ack(s, seq, status):
    p = bytearray()
    while len(p) < 32:
        chunk = s.recv(32-len(p))
        assert chunk, 'EOF during ACK'
        p.extend(chunk)
    got, result, token, ttl = struct.unpack('>IiII', p[:16])
    assert (got, result, ttl) == (seq, status, 500), (got, result, seq, status)
    return token

def connect(port):
    s = socket.create_connection(('127.0.0.1', port), timeout=1)
    s.settimeout(2)
    return s

@contextmanager
def service(mode='normal'):
    with socket.socket() as reserve:
        reserve.bind(('127.0.0.1', 0))
        port = reserve.getsockname()[1]
    with tempfile.TemporaryFile() as log:
        env = dict(os.environ, CURSOR_TEST_PLANE=mode)
        proc = subprocess.Popen([binary, '--port', str(port)], env=env, stdout=log, stderr=log)
        try:
            deadline = time.monotonic()+3
            while True:
                assert proc.poll() is None, 'service exited before listen'
                try:
                    s = connect(port)
                    break
                except OSError:
                    assert time.monotonic() < deadline
                    time.sleep(.01)
            yield proc, s, port, log
            s.close()
        finally:
            if proc.poll() is None:
                proc.terminate()
            proc.wait(timeout=3)

with service() as (proc, first, port, log):
    p = packet(1, 1)
    first.sendall(p[:5]); time.sleep(.03); first.sendall(p[5:])
    token = ack(first, 1, 1)
    first.sendall(packet(2, 2, token)); token = ack(first, 2, 0)
    # Replacement immediately revokes old connection's plane and lease.
    with connect(port) as second:
        assert first.recv(1) == b''
        second.sendall(packet(2, 3, token)); ack(second, 3, -2)
        second.sendall(packet(1, 4)); token = ack(second, 4, 1)
        second.sendall(packet(2, 5, token)); token = ack(second, 5, 0)
        time.sleep(.6)
        log.seek(0); assert log.read().splitlines()[-1] == b'CLOSE'
        second.sendall(packet(2, 6, token)); ack(second, 6, -2)
        second.sendall(packet(1, 7)); token = ack(second, 7, 1)
        second.sendall(packet(2, 8, token)); ack(second, 8, 0)
        second.sendall(packet(0, 9)); ack(second, 9, 0)
        bad = packet(2, 10, token); bad[3] = 16
        second.sendall(bad); assert second.recv(1) == b''
    # A stalled partial packet cannot keep a connection indefinitely.
    with connect(port) as third:
        third.sendall(packet(1, 11)[:8]); assert third.recv(1) == b''
    assert proc.poll() is None

with service('adopting') as (proc, s, port, log):
    s.sendall(packet(1, 1)); token = ack(s, 1, 1)
    s.sendall(packet(2, 2, token)); time.sleep(.03)
    s.sendall(packet(0, 3)); ack(s, 3, 0)
    log.seek(0); data = log.read()
    assert b'OPEN' in data and b'CLOSE' in data and b'DRAW' not in data

with service() as (proc, s, port, log):
    s.sendall(packet(1, 1)); token = ack(s, 1, 1)
    p = packet(3, 2, token)
    struct.pack_into('>HH', p, 24, 65000, 1)  # elapsed time, beyond viewport bounds
    s.sendall(p); token = ack(s, 2, 0)
    p = packet(3, 3, token); p[28] = 1       # video ready; native owns the reveal
    s.sendall(p); token = ack(s, 3, 0)
    s.sendall(packet(2, 4, token)); ack(s, 4, 0)  # same plane becomes a cursor
    s.sendall(packet(0, 5)); ack(s, 5, 0)

with service('fail') as (proc, s, port, log):
    s.sendall(packet(1, 1)); token = ack(s, 1, 1)
    s.sendall(packet(2, 2, token)); ack(s, 2, -1)
    assert proc.poll() is None

with service('hang') as (proc, s, port, log):
    s.sendall(packet(1, 1)); token = ack(s, 1, 1)
    s.sendall(packet(2, 2, token))
    assert proc.wait(timeout=3) == 70
    assert s.recv(1) == b''

print('cursor_process_test: fragmentation, replacement, stale lease, hide, malformed input, adoption and isolated Screen watchdog PASS')
