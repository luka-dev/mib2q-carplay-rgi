#!/usr/bin/env python3
"""Build one complete patched MU1316 Wi-Fi driver from the pinned stock .so.

The full patch combines the three original fixes with native RX age/lazy-timer
and early TX runtime hooks. Input files are never modified. Installation is separate.

  python3 -m wireless.driver_re.build_patched_so STOCK.so OUT.PATCHED.so
"""
from __future__ import annotations
import argparse, hashlib, tempfile, json, os
from pathlib import Path


STOCK_SHA  = "d7fbf49b297665cbb8c6ba71b41ac892ccd062e0dcbd8339f3286555db1f394a"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("stock", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument('--io-pkt',type=Path,default=Path(__file__).resolve().parents[2]/'build/wifi-driver-re-20261018/io-pkt-v6-hc')
    ap.add_argument('--build-dir',type=Path)
    a = ap.parse_args()
    if a.stock.resolve()==a.out.resolve():raise SystemExit('output must differ from stock input')
    if a.io_pkt.resolve()==a.out.resolve():raise SystemExit('output must differ from io-pkt input')
    data = bytearray(a.stock.read_bytes())
    if hashlib.sha256(data).hexdigest() != STOCK_SHA:
        raise SystemExit("input is not the fingerprinted MU1316 stock driver")
    from .latency_payload import build_latency
    build_dir=a.build_dir or a.out.parent/(a.out.name+'.build')
    result,manifest=build_latency(bytes(data),a.io_pkt.read_bytes(),build_dir)
    a.out.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=a.out.parent,prefix=a.out.name+'.',delete=False) as f:
        temp=Path(f.name);f.write(result)
    temp.chmod(a.stock.stat().st_mode & 0o777)
    os.replace(temp,a.out)
    a.out.with_name(a.out.name+'.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'latency: {a.out}, {len(result)} bytes, sha256 {manifest["sha256"]}')
    print(f'Build receipt: {a.out.name}.json; io-sdiorm is not modified')
    return

if __name__ == "__main__":
    main()
