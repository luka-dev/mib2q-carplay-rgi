"""PRIVATE: build and validate the exact staged handout sources on the host."""
from pathlib import Path
import json
import os
import queue
import signal
import socket
import struct
import subprocess
import threading
import time

P=Path(__file__).resolve().parent
ROOT=P.parents[2]
S=ROOT/'output/share/altscreen-renderer-share'
FLAGS=['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
def run(args):
    r=subprocess.run([str(x) for x in args],capture_output=True,text=True,timeout=30)
    if r.returncode: raise RuntimeError(r.stdout+r.stderr)
    return r
run(FLAGS+['-pedantic',S/'renderer/main.c',S/'renderer/sink_capture.c',S/'altscreen/frame_io.c','-pthread','-o',P/'video_receiver'])
run(FLAGS+['-I',ROOT/'toolchain/qnx65-abi/include/OMX','-I',S/'renderer',P/'check_decoder.c',S/'renderer/sink_omx.c','-pthread','-o',P/'check_decoder'])
mock=run([P/'check_decoder'])
(P/'decoder-test.log').write_text(mock.stdout+mock.stderr)

with socket.socket() as s:
    s.bind(('127.0.0.1',0)); port=s.getsockname()[1]
capture=P/'receiver-test.h264'
env=dict(os.environ,VIDEO_CAPTURE_FILE=str(capture))
p=subprocess.Popen([str(P/'video_receiver'),str(port)],env=env,stderr=subprocess.PIPE,stdout=subprocess.DEVNULL,text=True)
lines=queue.Queue(); log=[]
def collect():
    for line in p.stderr: log.append(line); lines.put(line)
t=threading.Thread(target=collect,daemon=True); t.start()
def wait_for(fragment):
    deadline=time.monotonic()+5
    while time.monotonic()<deadline:
        try: line=lines.get(timeout=max(0.001,deadline-time.monotonic()))
        except queue.Empty: break
        if fragment in line: return
    raise AssertionError('missing log: '+fragment+'\n'+''.join(log))
def send(payload,fragmented=False):
    with socket.create_connection(('127.0.0.1',port),timeout=2) as s:
        if fragmented:
            for i in range(0,len(payload),3): s.sendall(payload[i:i+3])
        else: s.sendall(payload)
        s.shutdown(socket.SHUT_WR)
def record(payload): return struct.pack('!I',len(payload))+payload
try:
    wait_for('listening on loopback')
    config=b'\x00\x00\x00\x01\x67abc\x00\x00\x00\x01\x68def'
    picture=b'\x00\x00\x00\x01\x65xyz'
    expected=config+picture+picture
    send(record(config)+record(b'')+record(picture)+record(picture),True)
    wait_for(f'session end messages=3 bytes={len(expected)} error=0')
    assert capture.read_bytes()==expected
    send(record(config)+record(picture))
    wait_for(f'session end messages=2 bytes={len(config+picture)} error=0')
    expected+=config+picture
    assert capture.read_bytes()==expected
    send(struct.pack('!I',4*1024*1024+1))
    wait_for('record reader ended status=-2')
    wait_for('session end messages=0 bytes=0 error=0')
    send(struct.pack('!I',20)+b'partial')
    wait_for('record reader ended status=-1')
    wait_for('session end messages=0 bytes=0 error=0')
    assert capture.read_bytes()==expected
    # Stop during a live, idle session: closing sockets must unblock reception.
    with socket.create_connection(('127.0.0.1',port),timeout=2):
        wait_for('decoder generation 5 begin')
        p.send_signal(signal.SIGTERM)
        assert p.wait(timeout=4)==0
    t.join(timeout=2)
    assert not any(x in ''.join(log) for x in ('AddressSanitizer','runtime error:'))
finally:
    if p.poll() is None: p.kill(); p.wait()
    t.join(timeout=2)
    (P/'receiver-test.log').write_text(''.join(log))

result={'strict_host_build':'passed','host_asan_ubsan':'passed',
        'framed_receive_fragmentation_empty_records':'passed','two_sessions_capture_exact_bytes':'passed',
        'oversized_header_partial_body_rejected':'passed','shutdown_with_idle_connected_client':'passed',
        'decoder_mock_asan_ubsan':'passed','decoder_mock_checks':mock.stdout.strip(),
        'hardware_decode_display':'not tested; no hardware backend supplied'}
(P/'test_results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
