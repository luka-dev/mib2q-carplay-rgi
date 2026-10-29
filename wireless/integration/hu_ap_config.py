#!/usr/bin/env python3
"""Read a private MU1316 persistence snapshot; verify AP credentials without logging them."""
from __future__ import annotations

import argparse
import hashlib
import hmac
import ipaddress
import os
from pathlib import Path
import re
import shlex
import sqlite3

PARTITION = 0x01B800E1


def read_ap(baseline: Path) -> dict[str, str]:
    database = baseline / 'persistence.sqlite'
    # URI read-only mode must never create an empty DB at a mistyped path.
    with sqlite3.connect(database.resolve().as_uri() + '?mode=ro', uri=True) as db:
        def string(key: int) -> str:
            row = db.execute('''select d.type,d.value from "persistence-data" d
                join "persistence-partitions" p on p.id=d.partition
                where p.name=? and d.key=?''', (PARTITION, key)).fetchone()
            if not row or row[0] != 2 or not isinstance(row[1], bytes):
                raise ValueError(f'missing or unsupported AP record {key}')
            data = row[1]
            # Observed type-2 string: zero prefix, BE16 byte length, UTF-8 data.
            if len(data) < 3 or data[0] != 0 or int.from_bytes(data[1:3], 'big') != len(data) - 3:
                raise ValueError(f'invalid AP record framing {key}')
            value = data[3:].decode('utf-8')
            if '\0' in value:
                raise ValueError(f'NUL in AP record {key}')
            return value
        password, address = string(39), string(62)
    config = (baseline / 'tmp/uaputl.cfg').read_text()

    def field(key: str) -> str:
        matches = re.findall(r'^\s*' + key + r'\s*=\s*(.*?)\s*$', config, re.MULTILINE)
        if len(matches) != 1:
            raise ValueError(f'missing or duplicate AP field {key}')
        words = shlex.split(matches[0], comments=True)
        if len(words) != 1:
            raise ValueError(f'invalid AP field {key}')
        return words[0]

    ssid = field('SSID')
    if not 1 <= len(ssid.encode()) <= 32 or not 8 <= len(password.encode()) <= 63:
        raise ValueError('unsupported AP credential length')
    ip = ipaddress.IPv4Address(address)
    if ip.is_unspecified or ip.is_loopback or ip.is_multicast:
        raise ValueError('invalid AP address')
    psk = field('PSK')
    if not re.fullmatch(r'[0-9a-fA-F]{64}', psk):
        raise ValueError('expected generated WPA2 PSK')
    derived = hashlib.pbkdf2_hmac('sha1', password.encode(), ssid.encode(), 4096, 32).hex()
    if not hmac.compare_digest(derived, psk.lower()):
        raise ValueError('persistence password does not match generated WPA2 PSK')
    channel = field('Channel')
    if channel not in ('1', '6', '11'):
        raise ValueError('first test requires a reviewed 2.4 GHz channel')
    return {'MHI2Q_CP_SSID': ssid, 'MHI2Q_CP_PASSPHRASE': password,
            'MHI2Q_CP_CHANNEL': channel, 'MHI2Q_CP_IP_ADDRESS': str(ip)}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    values = read_ap(args.baseline)
    # Output is deliberately a private file, never stdout or a command argument.
    fd = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as stream:
        stream.write(''.join(f'export {key}={shlex.quote(value)}\n' for key, value in values.items()))
    print('Private AP environment written; persistence credentials match generated PSK.')


if __name__ == '__main__':
    main()
