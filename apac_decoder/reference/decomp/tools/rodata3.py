import json,struct,os,collections,bisect,math,shutil
OUT="/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay/apac_decoder/reference/decomp"
shutil.rmtree(OUT+"/rodata", ignore_errors=True); os.makedirs(OUT+"/rodata")
f=open("/tmp/apacwork/lib.so","rb").read()
D=json.load(open("/tmp/apacwork/out/final.json")); DR=json.load(open("/tmp/apacwork/out/drr.json"))
REACH=json.load(open("/tmp/apacwork/out/reach.json"))
e_phoff=struct.unpack_from("<Q",f,0x20)[0]; phes,phn=struct.unpack_from("<HH",f,0x36)
PH=[]
for i in range(phn):
    t,fl,off,va,pa,fsz,msz,al=struct.unpack_from("<IIQQQQQQ",f,e_phoff+i*phes)
    if t==1: PH.append((va,va+fsz,off))
def rd(va,n):
    for a,b,off in PH:
        if a<=va<b: return f[off+va-a:off+va-a+min(n,b-va)]
    return b""
segs={s["name"]:(s["start"],s["end"]) for s in D["segments"]}
RO=segs[".rodata"];DA=segs[".data"];DRR=segs[".data.rel.ro"]
byea={r["ea"]:r for r in D["text"]}
drr={r[0]:r[1] for r in DR["drr"]}
B=set(D["all_data_refs"])
for a,v,s,n in DR["drr"]:
    if v: B.add(v)
for s in D["rodata_syms"]: B.add(s["ea"])
B=sorted(B)
def ext(a):
    seg = RO if RO[0]<=a<RO[1] else DA
    i=bisect.bisect_right(B,a); nxt=B[i] if i<len(B) else None
    return min(nxt if nxt else seg[1], seg[1])-a
named={s["ea"]:s for s in D["rodata_syms"]}
def is_str(b):
    if len(b)<4: return False
    t=b.split(b"\0")[0]
    return len(t)>=4 and all(32<=c<127 or c in (9,10) for c in t)
def classify(b):
    n=len(b)
    if is_str(b): return "cstring"
    if n>=16 and n%4==0:
        fl=struct.unpack("<%df"%(n//4),b)
        if sum(1 for x in fl if x==0.0 or (1e-25<abs(x)<1e12 and math.isfinite(x)))>=0.95*len(fl): return "float32"
        u=struct.unpack("<%dI"%(n//4),b)
        if all(x<0x200000 for x in u): return "uint32"
    if n>=4 and n%2==0:
        h=struct.unpack("<%dH"%(n//2),b)
        if all(x<0x4000 for x in h): return "uint16"
    return "bytes"
rows=[]
for k,path in REACH.items():
    a=int(k,16); n=ext(a); b=rd(a,n)
    rows.append(dict(ea=a,size=n,bytes=b,kind=classify(b),path=path,sym=named.get(a,{}).get("name")))
def cn(r): return "apac_rodata_%08x"%r["ea"]
def pathstr(r):
    L=[]
    for kind,x in r["path"]:
        if kind=="code":
            e=int(x,16); L.append("code 0x%08x %s"%(e, byea[e]["aliases"][0] if e in byea else "?"))
        else: L.append("via .data.rel.ro slot %s"%x)
    return L
def emit(r):
    b=r["bytes"];n=len(b);L=[]
    L.append("/* %s"%(r["sym"] or "unnamed table"))
    L.append(" * libSPF_CP_PlugInWrapper.so (Pioneer SPHERA 1.030010) @ 0x%08x, %d bytes."%(r["ea"],n))
    L.append(" * reached by:")
    for s in pathstr(r): L.append(" *   %s"%s)
    L.append(" * Size is an UPPER BOUND (distance to the next referenced address in the image).")
    L.append(" * The byte array is exact; the typed view is inferred (%s) and may be wrong."%r["kind"])
    L.append(" */")
    L.append("static const unsigned char %s_raw[%d] = {"%(cn(r),n))
    for i in range(0,n,16): L.append("    "+" ".join("0x%02x,"%c for c in b[i:i+16]))
    L.append("};")
    if r["kind"]=="float32":
        v=struct.unpack("<%df"%(n//4),b);L.append("")
        L.append("static const float %s_f32[%d] = {"%(cn(r),len(v)))
        for i in range(0,len(v),6): L.append("    "+" ".join("%.9gf,"%x for x in v[i:i+6]))
        L.append("};")
    elif r["kind"]=="uint32":
        v=struct.unpack("<%dI"%(n//4),b);L.append("")
        L.append("static const unsigned int %s_u32[%d] = {"%(cn(r),len(v)))
        for i in range(0,len(v),8): L.append("    "+" ".join("%uu,"%x for x in v[i:i+8]))
        L.append("};")
    elif r["kind"]=="uint16":
        v=struct.unpack("<%dH"%(n//2),b);L.append("")
        L.append("static const unsigned short %s_u16[%d] = {"%(cn(r),len(v)))
        for i in range(0,len(v),12): L.append("    "+" ".join("%u,"%x for x in v[i:i+12]))
        L.append("};")
    return "\n".join(L)+"\n"
DATA=sorted([r for r in rows if r["kind"]!="cstring"], key=lambda r:-r["size"])
STR=sorted([r for r in rows if r["kind"]=="cstring"], key=lambda r:r["ea"])
for r in DATA:
    open(OUT+"/rodata/%s.h"%cn(r),"w").write(emit(r))
    open(OUT+"/rodata/%s.bin"%cn(r),"wb").write(r["bytes"])

# ---------- structural dumps ----------
def q(a): return drr.get(a)
SFB=[]
for i in range(12):
    base=0x4258f8+40*i
    SFB.append([q(base),q(base+8),q(base+16),q(base+24),q(base+32)])
HUF=[q(0x424d98+8*i) for i in range(12)]
def hufdesc(p):
    return (q(p), q(p+8))

M=[];A=M.append
A("# APAC decoder — constant tables\n")
A("Source: `libSPF_CP_PlugInWrapper.so`, Pioneer SPHERA 1.030010 (AArch64 Linux ELF, image base 0).\n")
A("## Two corrections to the brief\n")
A("**1. The 48 `.rodata` symbols are not the codec's tables.** 45 of the 48 entries in")
A("`apac_symbols_pioneer.txt` marked `.rodata` are `typeinfo name for ...` strings (`_ZTS`, RTTI")
A("type names). Only three are data, and they total 30 bytes:\n")
A("| symbol | addr | size | bytes |")
A("|---|---|---:|---|")
for s in sorted(D["rodata_syms"], key=lambda s:s["ea"]):
    if not s["name"].startswith("typeinfo name for"):
        A("| `%s` | `0x%08x` | %d | `%s` |"%(s["name"],s["ea"],s["size"]," ".join("%02x"%x for x in bytes.fromhex(s["bytes"]))))
A("")
A("**2. The real tables are unnamed, and most are reached through pointer arrays in")
A("`.data.rel.ro`, not by a direct `ADRP` from code.** `apac::GetHuffmanTable`,")
A("`apac::GetSCFHuffmanTable` and `apac::GetSFBOffsetTable` are three-instruction accessors that")
A("index a relocated pointer array; the arrays in turn point at the `.rodata` payloads. A scan")
A("that only follows `ADRP` out of `.text` finds the accessor's array and stops.\n")
A("So the table set here was built by: taking every `.rodata`/`.data`/`.data.rel.ro` address")
A("referenced by any of the 334 APAC functions (143 addresses, 61 of them in `.data.rel.ro`),")
A("then walking the `.data.rel.ro` pointer graph transitively. That reaches **%d distinct" % len(rows))
A("objects, %d of them data (%d bytes) and %d C strings**.\n" % (len(DATA), sum(r['size'] for r in DATA), len(STR)))
A("### Extents are upper bounds\n")
A("Only a table's base is ever referenced — the code indexes off it — so the start address is")
A("exact and the length is the distance to the next address referenced from anywhere in the image.")
A("Where several tables sit back to back and only the first is addressed, they merge into one")
A("extent. Two lengths *are* pinned exactly, by identifying the generating function (see below).\n")
A("Each data object is written byte-for-byte to `rodata/apac_rodata_<addr>.h` (a C array plus a")
A("typed view) and `rodata/apac_rodata_<addr>.bin`. **The byte array is authoritative.** The")
A("element type in the typed view is inferred from the byte pattern and is not proof.\n")
A("---\n")
A("## The scalefactor-band offset table — fully recovered\n")
A("`apac::GetSFBOffsetTable(uint32 sampleRate, uint32 frameLength)` at `0x0009d9d0`:")
A("returns `NULL` unless `frameLength == 1024`, then linearly searches a 12-entry table at")
A("`0x004258f8` for a matching sample rate and returns the record. 40-byte records:\n")
A("```c")
A("struct SFBOffsetRecord {          /* 40 bytes, 12 records at 0x004258f8 */")
A("    uint64_t        sampleRate;   /* +0                                   */")
A("    uint64_t        numSfbLong;   /* +8                                   */")
A("    const uint16_t *sfbOffsetLong;/* +16  numSfbLong + 1 entries          */")
A("    uint64_t        numSfbShort;  /* +24                                  */")
A("    const uint16_t *sfbOffsetShort;/*+32  numSfbShort + 1 entries         */")
A("};")
A("```")
A("The `+1` is confirmed by arithmetic: for 48 kHz, `sfbOffsetLong = 0x330280` with")
A("`numSfbLong = 49`, and `0x330280 + (49+1)*2 = 0x3302e4`, immediately before the next")
A("referenced address `0x3302e8`. Same for every other record.\n")
A("| # | sample rate | numSfbLong | sfbOffsetLong | numSfbShort | sfbOffsetShort |")
A("|---:|---:|---:|---|---:|---|")
for i,r in enumerate(SFB):
    A("| %d | %d | %d | `0x%08x` | %d | `0x%08x` |"%(i,r[0],r[1],r[2],r[3],r[4]))
A("")
A("Records 3 (48000) and 5.1/5.1.2 CarPlay use `numSfbLong = 49`, `numSfbShort = 14`.")
A("The actual offset arrays, decoded:\n")
seen=set()
for i,r in enumerate(SFB):
    for (cnt,ptr,lbl) in ((r[1],r[2],"long"),(r[3],r[4],"short")):
        if ptr in seen: continue
        seen.add(ptr)
        b=rd(ptr,(cnt+1)*2)
        v=struct.unpack("<%dH"%(cnt+1),b)
        A("* `0x%08x` — %s, %d+1 entries (rate %d): `%s`" % (ptr,lbl,cnt,r[0], ", ".join(str(x) for x in v)))
A("")
A("---\n")
A("## The Huffman codebooks\n")
A("`apac::GetHuffmanTable(uint32 n)` at `0x00099dc0` is `return off_424D98[n];` — a 12-entry")
A("pointer array at `0x00424d98`. `apac::GetSCFHuffmanTable()` at `0x00099e10` returns")
A("`&off_424DF8`, which is entry 0 of the same array, so **codebook 0 is the scalefactor book**.\n")
A("Each entry points at a two-word descriptor followed by a run of 16-byte records:\n")
A("```c")
A("struct HuffTable {                /* 16 bytes */")
A("    const HuffEntry *entries;     /* +0  */")
A("    uint64_t         n;           /* +8  see caveat below */")
A("};")
A("struct HuffEntry {                /* 16 bytes */")
A("    uint64_t    a;                /* +0  a small number, 6..31, ascending down the array */")
A("    const void *payload;          /* +8  -> .rodata; payload sizes halve as `a` grows    */")
A("};")
A("```")
A("**Caveat — the field semantics are not established.** `APACScaleFactorData::Deserialize`")
A("computes `0x80000000u >> tbl->n`, so `n` is used as a *bit count*, not only as a length; and")
A("`a` ascends while the payload sizes descend by powers of two, which is the shape of a canonical")
A("Huffman \"symbols per code length\" table but is not proof of it. Do not port these from a")
A("guessed structure — decode them against the running library.\n")
A("| idx | descriptor | entries | n | first entry `a` | last entry `a` |")
A("|---:|---|---|---:|---:|---:|")
for i,p in enumerate(HUF):
    e,n=hufdesc(p)
    if e is None: continue
    fa=q(e); la=q(e+16*(n-1)) if n and n<4096 else None
    A("| %d%s | `0x%08x` | `0x%08x` | %s | %s | %s |"%(i," (SCF)" if i==0 else "",p,e,n,fa,la))
A("")
A("Full entry lists:\n")
for i,p in enumerate(HUF):
    e,n=hufdesc(p)
    if e is None or not n or n>4096: continue
    A("* codebook %d — descriptor `0x%08x`, %d entries at `0x%08x`:"%(i,p,n,e))
    parts=[]
    for j in range(n):
        parts.append("(%d, 0x%08x)"%(q(e+16*j), q(e+16*j+8) or 0))
    A("  `" + " ".join(parts) + "`")
A("")
A("---\n")
A("## Tables whose contents are identified exactly\n")
A("* **`0x003308c0`, 4096 bytes, `float[1024]`** — read by `APACSpectralData::Deserialize`.")
A("  Entry `i` equals `i^(4/3)` to the last bit of float precision for all 1024 entries")
A("  (checked exhaustively). This is the inverse-quantisation power table. The extent reported")
A("  by the boundary scan is 4272 bytes; the last 176 bytes are a different object, because")
A("  entries past index 1023 do not fit `i^(4/3)`. **Use 1024 entries.**")
A("* **`0x0030f3b8`, uint32** — sample-rate table read by `apac::GlobalConfig::Deserialize`:")
A("  `96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, ...` — the same 12 rates, in the")
A("  same order, as the SFB record table above.")
A("* **`0x00330280` / `0x00330260`** — the 48 kHz long / short scalefactor band offsets, above.\n")
A("## Tables whose purpose is placed but whose contents are not decoded\n")
A("* **`0x00332ff8`, ~65536 bytes** — the single largest object, addressed from")
A("  `apac::LSFTools::Reset()`. LSF/LPC codebook data.")
A("* **`0x00317e90`, `0x0031be90`, `0x0031de90`, `0x0031fe90`, `0x00323e90`, `0x00327e90`,")
A("  `0x0032be90`** — seven bases, all addressed from")
A("  `APACLRVQDecoder::lsfsToMdctSpectralEnvelope`, 0x2000/0x4000 apart, forming one contiguous")
A("  ~0x16000-byte block of float32. Some hold ascending Hz-like values (654.57, 1219.46,")
A("  2382.69, ...), others signed coefficient-like values. Which base is selected by what is in")
A("  that function's index arithmetic — read it there, do not assume.")
A("* **`0x0030ec80`, 280 bytes, float32** — 70 ascending values from 0.00012 upward, addressed")
A("  from `APACBWE2Decoder::DecodeFrame`.")
A("* **`0x00330080`, `apac::kSupportedProfileLevels`** — the symbol claims 4 bytes; the")
A("  referenced extent is 864. Read by `apac::ProfileLevelConfig::ValidateProfileLevelData`.\n")
A("---\n")
A("## The MDCT window tables — fully identified\n")
A("`GetSineWindowCoefficientPtr(n)` at `0x00105590` and `GetKBDWindowCoefficientPtr(n)` at")
A("`0x00105600` each accept only `n == 128` or `n == 1024` and return `NULL` otherwise, so the")
A("decoder has exactly two block sizes. Each table stores the **rising half** of a symmetric")
A("window of length `2n`; the falling half is its mirror.\n")
A("| table | addr | entries | bytes | window |")
A("|---|---|---:|---:|---|")
A("| sine, short | `0x00345e00` | 128 | 512 | `w[i] = sinf(pi * (i + 0.5) / 256)` |")
A("| sine, long | `0x00347200` | 1024 | 4096 | `w[i] = sinf(pi * (i + 0.5) / 2048)` |")
A("| KBD, short | `0x00346000` | 128 | 512 | Kaiser-Bessel derived; alpha not determined |")
A("| KBD, long | `0x00346200` | 1024 | 4096 | Kaiser-Bessel derived; alpha not determined |")
A("")
A("Both sine tables were verified against `sin(pi*(i+0.5)/(2N))` over every entry; the worst")
A("absolute error is 7.3e-08, i.e. float rounding. **They can be generated rather than embedded.**")
A("The two KBD tables are monotone rising, run from 4.38e-05 to 1.0 (short) and 2.93e-04 to")
A("1.0 (long), and are definitely not sine. The Kaiser alpha was not solved for, so **copy the KBD")
A("bytes; do not try to regenerate them**.\n")
A("---\n")
A("## Complete data-object index\n")
A("| addr | bytes | inferred | reached by | file |")
A("|---|---:|---|---|---|")
for r in DATA:
    p=pathstr(r)
    s=p[0] if p else ""
    if len(p)>1: s+= " (+%d more paths)"%(len(p)-1)
    s=s.replace("|","\\|")
    A("| `0x%08x` | %d | %s | %s | `rodata/%s.h` |"%(r["ea"],r["size"],r["kind"],s[:110],cn(r)))
A("")
A("## Strings referenced by APAC code\n")
A("| addr | text |")
A("|---|---|")
for r in STR:
    A("| `0x%08x` | `%s` |"%(r["ea"], r["bytes"].split(b"\0")[0].decode("utf8","replace").replace("|","\\|")[:140]))
A("")
A("---\n")
A("## The 48 `.rodata` / 4 `.data` symbol-table entries, verbatim\n")
A("| symbol | addr | size | contents |")
A("|---|---|---:|---|")
for s in sorted(D["rodata_syms"], key=lambda s:s["ea"]):
    b=bytes.fromhex(s["bytes"])
    c=("`"+b.split(b"\0")[0].decode("utf8","replace")+"`") if s["name"].startswith("typeinfo name for") else ("`"+" ".join("%02x"%x for x in b)+"`")
    A("| %s | `0x%08x` | %d | %s |"%(s["name"],s["ea"],s["size"],c))
A("")
open(OUT+"/RODATA.md","w").write("\n".join(M)+"\n")
print("RODATA.md:",len(M),"lines; data objs",len(DATA),"strings",len(STR))
