#!/usr/bin/env python3
"""Credential framing and generated-PSK consistency checks with synthetic data."""
import hashlib
from pathlib import Path
import sqlite3
import tempfile
from hu_ap_config import PARTITION, read_ap


def main():
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        (root / 'tmp').mkdir()
        ssid, password = 'Test AP', "test'password"
        psk = hashlib.pbkdf2_hmac('sha1', password.encode(), ssid.encode(), 4096, 32).hex()
        with sqlite3.connect(root / 'persistence.sqlite') as db:
            db.executescript('''create table "persistence-partitions"(id,name);
                create table "persistence-data"(partition,key,type,value);''')
            db.execute('insert into "persistence-partitions" values(1,?)', (PARTITION,))
            for key, value in ((62, '10.173.189.1'), (39, password)):
                encoded = value.encode()
                db.execute('insert into "persistence-data" values(1,?,2,?)',
                           (key, b'\0' + len(encoded).to_bytes(2, 'big') + encoded))
        config = root / 'tmp/uaputl.cfg'
        valid = f'SSID="{ssid}"\nPSK={psk}\nChannel=11\n'
        config.write_text(valid)
        assert read_ap(root)['MHI2Q_CP_PASSPHRASE'] == password
        for invalid in (valid.replace(psk, '0' * 64), valid.replace(ssid, 'Other AP'),
                        valid + 'Channel=6\n', valid.replace('Channel=11', 'Channel=165')):
            config.write_text(invalid)
            try:
                read_ap(root)
            except ValueError:
                pass
            else:
                raise AssertionError('inconsistent AP accepted')
        config.write_text(valid)
        with sqlite3.connect(root / 'persistence.sqlite') as db:
            db.execute('update "persistence-data" set value=? where key=39', (b'\0\0\x05shorter',))
        try:
            read_ap(root)
        except ValueError:
            pass
        else:
            raise AssertionError('incorrect string length accepted')
    print('HU AP: string framing, exact SSID, duplicate fields and WPA2 PSK verification passed')


if __name__ == '__main__':
    main()
