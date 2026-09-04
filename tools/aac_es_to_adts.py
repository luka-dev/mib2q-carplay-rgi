#!/usr/bin/env python3
"""Wrap the hook's raw AAC access-unit dump in ADTS so ordinary tools can decode it.

The hook writes /tmp/spatial_aac_es.bin as [4-byte LE length][access unit]... under a
type-103 index it cannot decode itself.  Those units carry no ADTS header, so nothing off
the shelf will read them; the parameters come from the stream's magic cookie instead, an
MPEG-4 ES_Descriptor whose DecSpecificInfo is the AudioSpecificConfig.

    ./aac_es_to_adts.py spatial_aac_es.bin out.adts [--asc 1190]

ASC 1190 = AAC-LC / 48000 / stereo, which is what index 23 carries.
"""
import struct, sys

def parse_asc(hexstr):
    b = bytes.fromhex(hexstr)
    bits = "".join(f"{x:08b}" for x in b)
    aot = int(bits[0:5], 2)
    freq = int(bits[5:9], 2)
    chan = int(bits[9:13], 2)
    return aot, freq, chan

def main(argv):
    if len(argv) < 3:
        print(__doc__); return 2
    asc = "1190"
    if "--asc" in argv:
        asc = argv[argv.index("--asc") + 1]
    aot, freq, chan = parse_asc(asc)
    src = open(argv[1], "rb").read()
    out = bytearray()
    i = n = 0
    while i + 4 <= len(src):
        (ln,) = struct.unpack_from("<I", src, i); i += 4
        if ln == 0 or ln > 8192 or i + ln > len(src):
            break                                   # a partial trailing record
        au = src[i:i+ln]; i += ln; n += 1
        fl = ln + 7
        h = bytearray(7)
        h[0], h[1] = 0xFF, 0xF1
        h[2] = (((aot - 1) & 3) << 6) | ((freq & 0xF) << 2) | ((chan >> 2) & 1)
        h[3] = ((chan & 3) << 6) | ((fl >> 11) & 3)
        h[4] = (fl >> 3) & 0xFF
        h[5] = ((fl & 7) << 5) | 0x1F
        h[6] = 0xFC
        out += h + au
    open(argv[2], "wb").write(bytes(out))
    rate = [96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000][freq]
    print("%d access units, %.2f s at %d Hz, %d channels -> %s"
          % (n, n * 1024 / rate, rate, chan, argv[2]))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
