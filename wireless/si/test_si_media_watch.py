#!/usr/bin/env python3
"""Execute the real watchdog client with malformed/healthy/ended/busy peers."""
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading

ROOT = Path(__file__).resolve().parent


def main():
    errors = []
    with socket.socket() as listener, tempfile.TemporaryDirectory() as directory:
        listener.bind(('127.0.0.1', 0))
        listener.listen(2)
        listener.settimeout(4)
        port = listener.getsockname()[1]

        def exchange(operation, reply):
            with listener.accept()[0] as connection:
                connection.settimeout(2)
                actual = b''
                while len(actual) < 32:
                    chunk = connection.recv(32-len(actual))
                    if not chunk:
                        raise AssertionError('short CPRX request')
                    actual += chunk
                expected = bytearray(32)
                expected[:4] = b'CPRX'
                expected[4:6] = bytes([3, operation])
                struct.pack_into('<IIQI', expected, 8, 7, 5000, 42, 9)
                assert actual == expected, (actual, expected)
                if reply:
                    connection.sendall(reply)

        def server():
            try:
                ok = b'CPRA\x02\0\0\0'
                exchange(7, ok + bytes([7, 0, 0, 0]))  # Healthy: no STOP.
                exchange(7, ok + bytes([4, 1, 0, 0]))  # Invalid reserved byte.
                exchange(7, ok + bytes([4, 0, 0, 0]))
                exchange(2, b'CPRA\x02\x03\0\0')     # New SETUP won: BUSY.
                exchange(7, ok + bytes([4, 0, 0, 0]))
                exchange(2, ok)                        # Idle-stop verified.
                exchange(7, None)                      # Lost reply != Ended.
            except BaseException as error:
                errors.append(error)

        helper = Path(directory)/'probe.c'
        helper.write_text('''#include "si_media_watch.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
static unsigned descriptors(void){unsigned n=0;for(int i=0;i<256;++i)if(fcntl(i,F_GETFD)>=0)++n;return n;}
int main(void){struct si_media_target t={42,1,9,7,100};unsigned before=descriptors();
for(unsigned i=0;i<5;++i)assert(si_media_watch(&t)==(i==3));
assert(descriptors()==before);puts("SI media watchdog: healthy, malformed, BUSY race, native idle-stop, EOF and fd lifetime PASS");}
''')
        binary = Path(directory)/'probe'
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c99',
                        '-D_POSIX_C_SOURCE=200809L', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=address,undefined', f'-DSI_MEDIA_WATCH_PORT={port}',
                        '-I', str(ROOT), str(helper), str(ROOT/'si_media_watch.c'),
                        '-o', str(binary)], check=True)
        thread = threading.Thread(target=server)
        thread.start()
        try:
            subprocess.run([str(binary)], check=True, timeout=10)
        finally:
            thread.join(timeout=5)
        assert not thread.is_alive()
        if errors:
            raise errors[0]


if __name__ == '__main__':
    main()
