import idapro, json, sys
idapro.enable_console_messages(False)
D=json.load(open("/tmp/apacwork/out/final.json"))
warn=[r["ea"] for r in D["text"] if r.get("pseudo") and ("output may be wrong" in r["pseudo"])]
grp=set()
for r in D["text"]:
    g=r.get("_g")
want=set(warn)
for r in D["text"]:
    a=r["aliases"][0]
    if a.startswith("APACLRVQDecoder") or a.startswith("APACSpectralData") or a.startswith("APACScaleFactorData") \
       or a.startswith("TBitstreamReader") or a.startswith("APACICSInfo"):
        want.add(r["ea"])
print("disasm targets:",len(want), "warned:",len(warn), file=sys.stderr)
idapro.open_database("/tmp/apacwork/lib.so", run_auto_analysis=True)
import ida_auto, ida_funcs, idautils, idc, ida_lines, ida_bytes
ida_auto.auto_wait()
out={}
for ea in sorted(want):
    f=ida_funcs.get_func(ea)
    if not f: continue
    L=[]
    for h in idautils.Heads(f.start_ea, f.end_ea):
        b=ida_bytes.get_bytes(h,4) or b""
        L.append("%08X  %s  %s" % (h, b.hex(), idc.generate_disasm_line(h, 0)))
    out[ea]="\n".join(L)
json.dump({hex(k):v for k,v in out.items()}, open("/tmp/apacwork/out/disasm.json","w"))
print("dumped",len(out), file=sys.stderr)
idapro.close_database(False)
