#!/usr/bin/env python3
# ldzip.py - self-contained lang_data.zip patcher for the Audi MHI2Q unit.
# Runs on micropython OR python3. NO external deps: pure-python CRC32 + a
# minimal zip reader/writer. gzip/zip/unzip are NOT needed on the unit.
#
# The zip path is hardcoded (ZIP). Override with env LDZIP or "-z <path>".
#
#   micropython ldzip.py langs
#   micropython ldzip.py list HMIInfoEvoHigh en_US
#   micropython ldzip.py set  HMIInfoEvoHigh en_US 5 "New text"
#   micropython ldzip.py add  HMIInfoEvoHigh en_US=Hi es_MX=Hola fr_CA=Salut
#
# Only edited .data entries (STORED) get a fresh CRC32; every other entry is
# copied through byte-for-byte with its original compression, so no inflate/
# deflate is ever needed. HMISpeechEvoHigh/*.data are unsupported.
import struct, sys, os

# some micropython ports have no os.environ (then only "-z <path>" overrides)
ZIP = getattr(os, "environ", {}).get("LDZIP", "/mnt/app/eso/hmi/lsd/jars/lang_data.zip")

# ---- CRC32 (zlib poly 0xEDB88320), pure python ----
_T = []
def crc32(data):
    if not _T:
        for n in range(256):
            c = n
            for _ in range(8):
                c = (0xEDB88320 ^ (c >> 1)) if (c & 1) else (c >> 1)
            _T.append(c & 0xFFFFFFFF)
    c = 0xFFFFFFFF
    for byte in data:
        c = _T[(c ^ byte) & 0xFF] ^ (c >> 8)
    return c ^ 0xFFFFFFFF

# ---- pure-python RAW DEFLATE inflate (RFC1951) - to read compressed .data ----
def inflate(data):
    out = bytearray(); pos = 0; bitbuf = 0; bitcnt = 0
    def getbit():
        nonlocal pos, bitbuf, bitcnt
        if bitcnt == 0:
            bitbuf = data[pos]; pos += 1; bitcnt = 8
        b = bitbuf & 1; bitbuf >>= 1; bitcnt -= 1; return b
    def getbits(n):
        v = 0
        for i in range(n): v |= getbit() << i
        return v
    def build(lengths):
        maxlen = max(lengths) if lengths else 0
        blcount = [0] * (maxlen + 1)
        for l in lengths:
            if l: blcount[l] += 1
        nextcode = [0] * (maxlen + 1); code = 0
        for bits in range(1, maxlen + 1):
            code = (code + blcount[bits - 1]) << 1; nextcode[bits] = code
        table = {}
        for sym, l in enumerate(lengths):
            if l:
                table[(l, nextcode[l])] = sym; nextcode[l] += 1
        def decode():
            l = 0; code = 0
            while True:
                code = (code << 1) | getbit(); l += 1
                s = table.get((l, code))
                if s is not None: return s
        return decode
    LBASE=[3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258]
    LEXT=[0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0]
    DBASE=[1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577]
    DEXT=[0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13]
    while True:
        final = getbit(); btype = getbits(2)
        if btype == 0:
            bitcnt = 0
            ln = data[pos] | (data[pos + 1] << 8); pos += 4
            out += data[pos:pos + ln]; pos += ln
        else:
            if btype == 1:
                declit = build([8]*144 + [9]*112 + [7]*24 + [8]*8); decdist = build([5]*30)
            else:
                hlit = getbits(5) + 257; hdist = getbits(5) + 1; hclen = getbits(4) + 4
                order = [16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15]
                cl = [0]*19
                for i in range(hclen): cl[order[i]] = getbits(3)
                deccl = build(cl); lens = []
                while len(lens) < hlit + hdist:
                    s = deccl()
                    if s < 16: lens.append(s)
                    elif s == 16: lens += [lens[-1]] * (getbits(2) + 3)
                    elif s == 17: lens += [0] * (getbits(3) + 3)
                    else: lens += [0] * (getbits(7) + 11)
                declit = build(lens[:hlit]); decdist = build(lens[hlit:])
            while True:
                s = declit()
                if s == 256: break
                if s < 256: out.append(s)
                else:
                    s -= 257; length = LBASE[s] + getbits(LEXT[s])
                    d = decdist(); dist = DBASE[d] + getbits(DEXT[d])
                    start = len(out) - dist
                    for i in range(length): out.append(out[start + i])
        if final: break
    return bytes(out)

# ---- minimal zip read: keep method/crc/usize + raw (possibly compressed) bytes ----
def zip_read(b):
    i = b.rfind(b"PK\x05\x06")
    cd_off = struct.unpack("<I", b[i+16:i+20])[0]
    cnt = struct.unpack("<H", b[i+10:i+12])[0]
    names = []; meta = {}; raw = {}
    p = cd_off
    for _ in range(cnt):
        method = struct.unpack("<H", b[p+10:p+12])[0]
        crc    = struct.unpack("<I", b[p+16:p+20])[0]
        csize  = struct.unpack("<I", b[p+20:p+24])[0]
        usize  = struct.unpack("<I", b[p+24:p+28])[0]
        nlen   = struct.unpack("<H", b[p+28:p+30])[0]
        elen   = struct.unpack("<H", b[p+30:p+32])[0]
        clen   = struct.unpack("<H", b[p+32:p+34])[0]
        lho    = struct.unpack("<I", b[p+42:p+46])[0]
        name = b[p+46:p+46+nlen].decode("utf-8")
        lnlen = struct.unpack("<H", b[lho+26:lho+28])[0]
        lelen = struct.unpack("<H", b[lho+28:lho+30])[0]
        ds = lho + 30 + lnlen + lelen
        names.append(name); meta[name] = (method, crc, usize); raw[name] = b[ds:ds+csize]
        p += 46 + nlen + elen + clen
    return names, meta, raw

# ---- minimal zip write: preserve each entry's method/crc/size ----
def zip_write(names, meta, raw):
    out = b""; central = b""; off = 0
    for name in names:
        method, crc, usize = meta[name]; body = raw[name]; nm = name.encode("utf-8"); csize = len(body)
        lh = (b"PK\x03\x04" + struct.pack("<HHHHH", 20, 0, method, 0, 0)
              + struct.pack("<III", crc, csize, usize) + struct.pack("<HH", len(nm), 0) + nm)
        out += lh + body
        central += (b"PK\x01\x02" + struct.pack("<HHHHHH", 20, 20, 0, method, 0, 0)
                    + struct.pack("<III", crc, csize, usize)
                    + struct.pack("<HHHHH", len(nm), 0, 0, 0, 0)
                    + struct.pack("<II", 0, off) + nm)
        off += len(lh) + csize
    eocd = (b"PK\x05\x06" + struct.pack("<HH", 0, 0) + struct.pack("<HH", len(names), len(names))
            + struct.pack("<I", len(central)) + struct.pack("<I", off) + struct.pack("<H", 0))
    return out + central + eocd

# ---- .data string-table codec (big-endian) ----
def d_unpack(b):
    n = struct.unpack(">I", b[0:4])[0]
    offs = [struct.unpack(">I", b[4+4*i:8+4*i])[0] for i in range(n)]
    p = 4 + 4*n; L = struct.unpack(">I", b[p:p+4])[0]; p += 4
    blob = b[p:p+L]; p += L; flags = list(b[p:p+n]); ends = offs[1:] + [L]
    return [bytes(blob[offs[i]:ends[i]]).decode("utf-8") for i in range(n)], flags
def d_pack(strs, flags):
    blob = b""; offs = []
    for s in strs:
        offs.append(len(blob)); blob += s.encode("utf-8")
    out = struct.pack(">I", len(strs))
    for o in offs:
        out += struct.pack(">I", o)
    return out + struct.pack(">I", len(blob)) + blob + bytes(flags)

def main():
    a = sys.argv[1:]
    zp = ZIP
    if a and a[0] == "-z":
        zp = a[1]; a = a[2:]
    cmd = a[0]
    b = open(zp, "rb").read(); names, meta, raw = zip_read(b)
    def get(k):
        m, _c, _u = meta[k]
        return d_unpack(inflate(raw[k]) if m == 8 else raw[k])
    def put(k, d): raw[k] = d; meta[k] = (0, crc32(d), len(d))   # edited entry is STORED
    def save(): open(zp, "wb").write(zip_write(names, meta, raw))

    if cmd == "langs":
        scr = []; loc = set()
        for n in names:
            if n.startswith("resources/") and n.endswith(".data"):
                parts = n.split("/")
                if parts[1] not in scr: scr.append(parts[1])
                loc.add(parts[2][len("strings_"):-len(".data")])
        print("locales:", ", ".join(sorted(loc)))
        for s in scr: print("  ", s)
    elif cmd == "list":
        strs, _ = get("resources/%s/strings_%s.data" % (a[1], a[2]))
        for i in range(len(strs)):
            if strs[i]: print("[%d] %r" % (i, strs[i]))
    elif cmd == "set":
        screen, loc, idn, text = a[1], a[2], int(a[3]), a[4]
        key = "resources/%s/strings_%s.data" % (screen, loc)
        strs, flags = get(key); strs[idn] = text; put(key, d_pack(strs, flags)); save()
        print("set %s [%d] = %r" % (loc, idn, text))
    elif cmd == "add":
        screen = a[1]; kv = dict(x.split("=", 1) for x in a[2:]); newid = None
        pre = "resources/%s/strings_" % screen
        for n in names:
            if n.startswith(pre) and n.endswith(".data"):
                loc = n[len(pre):-len(".data")]
                strs, flags = get(n); strs.append(kv.get(loc, "")); flags.append(8)
                newid = len(strs) - 1; put(n, d_pack(strs, flags))
        save(); print("added [%s] to %s: %r" % (newid, screen, kv))
    else:
        print(__doc__)

if __name__ == "__main__":
    main()
