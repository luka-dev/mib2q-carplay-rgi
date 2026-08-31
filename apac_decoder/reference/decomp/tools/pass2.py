import idapro, json, re, sys, importlib.util
idapro.enable_console_messages(False)
spec=importlib.util.spec_from_file_location("apactypes","/tmp/apacwork/types.py")
T=importlib.util.module_from_spec(spec); spec.loader.exec_module(T)

F=json.load(open("/tmp/apacwork/out/apac_funcs.json"))
FUNCS={int(k,16):v for k,v in F.items()}

# ---- pre-compute decls (populates T.classes) ----
decls={}
for ea,lst in FUNCS.items():
    s=lst[0]
    d=T.build_decl(s[7], s[3], "F", "__int64")
    if d: decls[ea]=(s,d)
extra=["std::vector"]
clsdecls=[]
for c in sorted(T.classes):
    if c in ("std::vector","TBitstreamReader","DSPSplitComplex"): continue
    clsdecls.append("struct %s;" % T.cident(c))

idapro.open_database("/tmp/apacwork/lib.so", run_auto_analysis=True)
import ida_auto, ida_hexrays, ida_funcs, ida_bytes, ida_name, idautils, ida_segment, ida_typeinf, idc
ida_auto.auto_wait(); ida_hexrays.init_hexrays_plugin()

def decl(s):
    r=ida_typeinf.idc_parse_types(s, ida_typeinf.PT_SIL)
    return r

base=["struct std_vector { void *begin; void *end; void *cap; };",
      "struct DSPSplitComplex { float *realp; float *imagp; };",
      "struct TBitstreamReader_u32 { const unsigned __int8 *cur; const unsigned __int8 *end; unsigned int cache; int bitsLeft; };"]
for d in base+clsdecls:
    decl(d)
print("declared %d class fwds" % len(clsdecls), file=sys.stderr)

# helper: bitstream reader helper prototypes
ida_typeinf.apply_cdecl(None, 0xb1600, "void __fastcall _ZN16TBitstreamReaderIjE13FillCacheFromEPKh(TBitstreamReader_u32 *this, const unsigned __int8 *p);")

# ---- pass 1: guess return types ----
rets={}
for ea in decls:
    try:
        c=ida_hexrays.decompile(ea)
        rt=c.type.get_rettype()
        rets[ea]=str(rt) if rt else "__int64"
    except Exception:
        rets[ea]="__int64"
print("pass1 rettypes", len(rets), file=sys.stderr)

RETOVR={}
applied=0; failed=[]
for ea,(s,_) in decls.items():
    rt = rets.get(ea,"__int64")
    if rt in ("", "?"): rt="__int64"
    # explicit return type from demangling (templates only)
    q=T.qualified(s[7]); pre=s[7][:s[7].index(q)].strip() if q and q in s[7] else ""
    if pre and pre not in ("virtual","const"):
        mt=T.map_type(pre)
        if mt not in ("void",) and "::" not in pre: rt=mt
    name=ida_name.get_name(ea) or ("sub_%X"%ea)
    d=T.build_decl(s[7], s[3], name, rt)
    if not d: continue
    ok=ida_typeinf.apply_cdecl(None, ea, d+";")
    if ok: applied+=1
    else:
        failed.append((hex(ea), d))
print("applied",applied,"failed",len(failed), file=sys.stderr)
for x in failed[:20]: print("FAIL",x, file=sys.stderr)

# ---- pass 2: final decompile ----
out={"text":[], "failed_protos":failed}
for ea,lst in sorted(FUNCS.items()):
    f=ida_funcs.get_func(ea)
    drefs=set(); ins=0
    if f:
        for head in idautils.Heads(f.start_ea, f.end_ea):
            ins+=1
            for d in idautils.DataRefsFrom(head): drefs.add(d)
    rec={"ea":ea,"size":(lst[0][2] or ((f.end_ea-f.start_ea) if f else 0)),"aliases":[x[7] for x in lst],
         "mangled":[x[3] for x in lst],"drefs":sorted(drefs),"insns":ins,
         "func_start": f.start_ea if f else None, "func_end": f.end_ea if f else None}
    try:
        c=ida_hexrays.decompile(ea)
        rec["pseudo"]=str(c) if c else None
    except Exception as e:
        rec["pseudo"]=None; rec["err"]=str(e)
    if not rec["pseudo"] and f:
        rec["disasm"]="\n".join("%08X  %s"%(h, idc.generate_disasm_line(h,0)) for h in idautils.Heads(f.start_ea,f.end_ea))
    out["text"].append(rec)

# ---- vtables / typeinfo / rodata ----
SYMS="/Users/luka/Desktop/AUDI_2/Patches/mhi2-carplay/docs/reference/apac_symbols_pioneer.txt"
rows=[]
for line in open(SYMS):
    m=re.match(r'^(0x[0-9a-f]+)\s+(\S+)\s+(\d+)\s+(.*)$', line.rstrip("\n"))
    if m: rows.append((int(m.group(1),16),m.group(2),int(m.group(3)),m.group(4)))
def nm(ea):
    if not ea: return None
    n=ida_name.get_name(ea)
    if n: return ida_name.demangle_name(n,0) or n
    f=ida_funcs.get_func(ea)
    return ("sub_%X"%f.start_ea) if f else ("0x%X"%ea)
out["vtables"]=[];out["typeinfo"]=[];out["rodata_syms"]=[]
for ea,sect,size,dem in rows:
    if sect=='.data.rel.ro' and dem.startswith("vtable for "):
        out["vtables"].append({"ea":ea,"size":size,"cls":dem[11:],
            "slots":[{"i":i,"raw":ida_bytes.get_qword(ea+i*8),"name":nm(ida_bytes.get_qword(ea+i*8))} for i in range(size//8)]})
    elif sect=='.data.rel.ro' and dem.startswith("typeinfo for "):
        out["typeinfo"].append({"ea":ea,"size":size,"cls":dem[13:],
            "words":[{"raw":ida_bytes.get_qword(ea+i*8),"name":nm(ida_bytes.get_qword(ea+i*8))} for i in range(size//8)]})
    elif sect in ('.rodata','.data'):
        b=ida_bytes.get_bytes(ea,size) or b""
        out["rodata_syms"].append({"ea":ea,"sect":sect,"size":size,"name":dem,"bytes":b.hex()})

allrefs=set()
for fa in idautils.Functions():
    f=ida_funcs.get_func(fa)
    if not f: continue
    for h in idautils.Heads(f.start_ea,f.end_ea):
        for d in idautils.DataRefsFrom(h): allrefs.add(d)
out["all_data_refs"]=sorted(allrefs)
out["segments"]=[{"name":ida_segment.get_segm_name(ida_segment.getnseg(i)),
                  "start":ida_segment.getnseg(i).start_ea,"end":ida_segment.getnseg(i).end_ea}
                 for i in range(ida_segment.get_segm_qty())]
json.dump(out, open("/tmp/apacwork/out/final.json","w"))
print("done", file=sys.stderr)
idapro.close_database(False)
