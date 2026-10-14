import json,re,collections,importlib.util
spec=importlib.util.spec_from_file_location("apactypes","/tmp/apacwork/types.py")
T=importlib.util.module_from_spec(spec); spec.loader.exec_module(T)
D=json.load(open("/tmp/apacwork/out/final.json"))
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '../../../..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
OUT=_REPO + "/apac_decoder/reference/decomp"

SZ={"_BYTE":1,"char":1,"unsigned __int8":1,"__int8":1,"bool":1,"_BOOL1":1,
    "_WORD":2,"__int16":2,"unsigned __int16":2,"_BOOL2":2,
    "_DWORD":4,"int":4,"unsigned int":4,"float":4,"_BOOL4":4,"__int32":4,"unsigned __int32":4,
    "_QWORD":8,"__int64":8,"unsigned __int64":8,"double":8,"_OWORD":16,"__int128":16}
TY={1:"u8",2:"u16",4:"u32",8:"u64",16:"u128"}

R1=re.compile(r'\*\(\((?:const )?([A-Za-z_0-9 ]+?) \*\)(\w+) \+ (\d+)\)')
R2=re.compile(r'\*\((?:const )?([A-Za-z_0-9 ]+?) \*\)\(\(char \*\)(\w+) \+ (\d+)\)')
R3=re.compile(r'\*\((?:const )?([A-Za-z_0-9 ]+?) \*\)(\w+)(?![\w \+])')
PROTO=re.compile(r'^\s*(?:const )?(\w+) \*(\w+)[,)]', re.M)

# param name -> class, per function
def proto_map(p):
    # header runs from function name '(' to matching ')'
    i=p.index("(")
    depth=0; j=i
    for j in range(i,len(p)):
        if p[j]=="(": depth+=1
        elif p[j]==")":
            depth-=1
            if depth==0: break
    hdr=p[i+1:j]
    m={}
    for a in T.split_top(hdr, ","):
        a=a.strip()
        mm=re.match(r'^(?:const )?([A-Za-z_][\w]*) \*+(\w+)$', a)
        if mm: m[mm.group(2)]=mm.group(1)
    return m

acc=collections.defaultdict(lambda: collections.defaultdict(collections.Counter))  # cls -> off -> width Counter
newsz=collections.defaultdict(collections.Counter)
for r in D["text"]:
    p=r.get("pseudo")
    if not p: continue
    try: pm=proto_map(p)
    except Exception: continue
    if not pm: continue
    for rx,idxmul in ((R1,True),(R2,False)):
        for m in rx.finditer(p):
            ty,var,idx=m.group(1),m.group(2),int(m.group(3))
            if var not in pm: continue
            w=SZ.get(ty.strip())
            if not w: continue
            off = idx*w if idxmul else idx
            acc[pm[var]][off][w]+=1
    for m in R3.finditer(p):
        ty,var=m.group(1),m.group(2)
        if var not in pm: continue
        w=SZ.get(ty.strip())
        if w: acc[pm[var]][0][w]+=1
    # operator new sizes inside this function -> attribute to classes named in it
    for m in re.finditer(r'operator new\((0x[0-9A-Fa-f]+|\d+)u?L?L?\)', p):
        n=int(m.group(1),16) if m.group(1).startswith("0x") else int(m.group(1))
        newsz[r["_g"] if "_g" in r else ""][n]+=1

# hierarchy
ti={t["cls"]:t for t in D["typeinfo"]}
base={}
for cls,t in ti.items():
    if t["size"]==24 and len(t["words"])>2:
        b=t["words"][2]["name"] or ""
        if b.startswith("`typeinfo for'"): base[cls]=b[len("`typeinfo for'"):]
    elif t["size"]>24:
        bs=[]
        for w in t["words"][3:]:
            n=w["name"] or ""
            if n.startswith("`typeinfo for'"): bs.append(n[len("`typeinfo for'"):])
        if bs: base[cls]="(multiple) "+", ".join(bs)

vt={v["cls"]:v for v in D["vtables"]}

# methods per class
NSL=["apac::scene","apac::drc","apac::chan","apac::lbr","apac","std","boost","__gnu_cxx","mpddrc","CarPlay"]
def cls_of(dem):
    q=T.qualified(dem); comps=[T.strip_targs(c) for c in T.split_top(q,"::")]
    return T.class_of_scope(comps[:-1])
methods=collections.defaultdict(list)
free=[]
for r in D["text"]:
    c=cls_of(r["aliases"][0])
    if c: methods[c].append(r)
    else: free.append(r)

L=[]
A=L.append
A("# APAC decoder — recovered class layout\n")
A("Source: `libSPF_CP_PlugInWrapper.so`, Pioneer SPHERA 1.030010 (AArch64 Linux ELF, image base 0).")
A("Recovered with IDA Pro 9.3 / Hex-Rays from the intact `.dynsym` Itanium symbols, the")
A("`_ZTV`/`_ZTI`/`_ZTS` RTTI records, and the decompiled member accesses.\n")
A("Everything in this file is derived from the binary. Where a value could not be derived it")
A("says so; nothing here is inferred from how other AAC-family codecs are built.\n")
A("---\n")
A("## 1. The bitstream reader\n")
A("`TBitstreamReader<unsigned int>` (typedef'd `BitstreamReader` in Apple's source, per the")
A("`__PRETTY_FUNCTION__` string at `0x30ea90`) is a **real class with out-of-line copies of every")
A("method**, and it is *also* inlined at almost every call site. Both forms are in the binary.")
A("Out-of-line at `0x000b1430`–`0x000b19a8`, decompiled in `TBitstreamReader.c`.\n")
A("Layout (24 bytes), derived from `FillCacheFrom`, `GetBits`, `BitsRemaining` and the ctor:\n")
A("```c")
A("struct TBitstreamReader_u32 {   /* sizeof == 24 */")
A("    const uint8_t *cur;      /* +0   next 32-bit word to pull into the cache        */")
A("    const uint8_t *end;      /* +8   one past the last byte of the buffer           */")
A("    uint32_t       cache;    /* +16  MSB-aligned; bits are taken from the top       */")
A("    int32_t        bitsLeft; /* +20  signed; < 0 means the cache must be refilled   */")
A("};")
A("```\n")
A("Semantics, exactly as decompiled:\n")
A("* `FillCacheFrom(p)` — if `end - p > 3`, `cache = bswap32(*(uint32_t*)p)` (a **big-endian**")
A("  32-bit load). Otherwise it shifts in the `end - p` bytes that remain, MSB first, and pads")
A("  the rest of the word with `0xFF` bytes. Note the tail padding is `0xFF`, not zero.")
A("* `GetBits(n)` — returns 0 for `n == 0` or `n > 32`. Otherwise")
A("  `v = cache >> (32 - n); bitsLeft -= n;` and if `bitsLeft` went negative it refills:")
A("  `v |= newcache >> (32 + bitsLeft); cache = newcache << (-bitsLeft); cur += 4;")
A("  bitsLeft += 32;` with `bitsLeft == -32` special-cased to `cache = 0`.")
A("  All shifts rely on AArch64 `LSR`/`LSL` taking the shift amount modulo 32.")
A("* `GetBit()` — the `n == 1` specialisation of the above.")
A("* `SkipBits(n)`, `AlignToNextByte()` — advance without producing a value;")
A("  `AlignToNextByte` discards `bitsLeft & 7` bits.")
A("* `BitsRemaining()` — `bitsLeft + 8 * (uint32_t)(end - cur)`. The pointer difference is")
A("  **truncated to 32 bits** before the multiply; that is what the code does.")
A("* `PutBack(n)` — rewinds: `bitsLeft = (bitsLeft + n) & 31; cur -= 4 * ((bitsLeft + n) >> 5);`")
A("  then refills from `cur - 4`, or zeroes the cache when the new `bitsLeft` is 0.")
A("* The constructor takes `(const uint8_t *buf, uint32_t len)`, sets `cur = buf`, `end = buf+len`,")
A("  and if `buf` is **not 4-byte aligned** it pre-loads the leading 1–3 bytes into the cache and")
A("  advances `cur` to the next 4-byte boundary. A port must reproduce this, because every")
A("  subsequent refill assumes `cur` is word aligned.\n")
A("`apac::GetEscBits<ESCBits, ESC2Bits>(BitstreamReader&, uint32_t numBits)` is instantiated three")
A("times — `<16,20>` at `0x78150`, `<8,16>` at `0x96630`, `<6,12>` at `0x99220`. It reads")
A("`numBits`; if the value is all-ones it reads a further `ESCBits` and adds; if *that* escape is")
A("all-ones it reads `ESC2Bits` and adds again. Asserts `numBits <= 32`")
A("(`APACStructs.h:198`, from the assert string at `0x30eb50`).\n")
A("---\n")
A("## 2. Class hierarchy (from `_ZTI` records)\n")
A("`__class_type_info` (16 bytes) = no base; `__si_class_type_info` (24 bytes) = one public base")
A("at word 2; 32 bytes = `__vmi_class_type_info`.\n")
A("```")
kids=collections.defaultdict(list)
roots=[]
for c in sorted(ti):
    b=base.get(c)
    if b and not b.startswith("(multiple)"): kids[b].append(c)
    else: roots.append(c)
def tree(c,ind=0):
    A("%s%s" % ("    "*ind, c))
    for k in sorted(kids.get(c,[])): tree(k,ind+1)
seen=set()
for r in sorted(roots):
    tree(r); seen.add(r)
A("```\n")
A("Classes with a `_ZTI` record but **no** `_ZTV` (abstract-with-no-vtable, or POD with RTTI only):")
novt=[c for c in sorted(ti) if c not in vt]
A("`" + "`, `".join(novt) + "`\n")
A("---\n")
A("## 3. Vtables\n")
A("Slot 0 is offset-to-top, slot 1 is the `_ZTI` pointer; virtual function *index n* is at")
A("slot `n + 2` (byte offset `16 + 8n`). Order is the compiler's, i.e. declaration order.\n")
for c in sorted(vt):
    v=vt[c]
    A("### `%s`  — vtable at `0x%08x`, %d bytes, %d virtuals" % (c, v["ea"], v["size"], max(0,v["size"]//8-2)))
    if base.get(c): A("base: `%s`" % base[c])
    A("")
    A("| slot | vidx | target |")
    A("|---:|---:|---|")
    for s in v["slots"]:
        vi = s["i"]-2
        nm = s["name"] or "0"
        if s["i"]==0: nm="(offset-to-top = %d)" % (s["raw"] if s["raw"] < 2**63 else s["raw"]-2**64)
        elif s["i"]==1: nm="(typeinfo) %s" % nm
        A("| %d | %s | `%s` |" % (s["i"], vi if vi>=0 else "-", nm))
    A("")
A("---\n")
A("## 4. Member layout, harvested from the decompilation\n")
A("Every access through a typed pointer (`this` or a typed parameter) was collected and grouped by")
A("byte offset. **These are observed accesses, not a declaration.** A field that is never touched")
A("by the code in this library does not appear; padding is invisible; and where the same offset is")
A("read at two widths both are listed (usually a `uint8` inside a word, or a decompiler artefact).")
A("`n=` is how many distinct read/write sites were seen. Object *size* is only given where an")
A("`operator new` in a constructor or factory pins it.\n")
order=sorted(acc, key=lambda c:-sum(sum(w.values()) for w in acc[c].values()))
for c in order:
    offs=acc[c]
    if not offs: continue
    mx=max(o+max(w) for o,w in offs.items())
    A("### `%s`" % c)
    A("observed extent: `0` .. `0x%x` (%d bytes), %d distinct offsets" % (mx,mx,len(offs)))
    A("")
    A("| offset | width(s) | n |")
    A("|---:|---|---:|")
    for o in sorted(offs):
        ws=offs[o]
        A("| +0x%03x (%d) | %s | %d |" % (o,o, ", ".join("%s"%TY.get(w,w) for w in sorted(ws)), sum(ws.values())))
    A("")
A("---\n")
A("## 5. Method list\n")
tot=0
for c in sorted(methods):
    rs=sorted(methods[c], key=lambda r:r["ea"]); tot+=len(rs)
    A("### `%s` — %d functions, %d bytes" % (c,len(rs),sum(r["size"] for r in rs)))
    A("")
    A("| addr | size | signature |")
    A("|---|---:|---|")
    for r in rs:
        A("| `0x%08x` | %d | `%s` |" % (r["ea"], r["size"], r["aliases"][0]))
        for a in r["aliases"][1:]:
            A("| | | `%s` (alias, same code) |" % a)
    A("")
A("### free functions / C API — %d" % len(free))
A("")
A("| addr | size | signature |")
A("|---|---:|---|")
for r in sorted(free,key=lambda r:r["ea"]):
    A("| `0x%08x` | %d | `%s` |" % (r["ea"], r["size"], r["aliases"][0]))
A("")
open(OUT+"/CLASSES.md","w").write("\n".join(L)+"\n")
print("wrote CLASSES.md", len(L), "lines; classes with layout:", len(acc), "; methods classes:", len(methods), "free:", len(free))
