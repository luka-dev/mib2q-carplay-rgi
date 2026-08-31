import idapro, json, sys
idapro.enable_console_messages(False)
idapro.open_database("/tmp/apacwork/lib.so", run_auto_analysis=True)
import ida_auto, ida_bytes, ida_name, ida_segment, idautils, ida_funcs
ida_auto.auto_wait()
segs={}
for i in range(ida_segment.get_segm_qty()):
    s=ida_segment.getnseg(i); segs.setdefault(ida_segment.get_segm_name(s), (s.start_ea,s.end_ea))
DRR=segs[".data.rel.ro"]; RO=segs[".rodata"]; DA=segs[".data"]; TX=segs[".text"]; BSS=segs[".bss"]
out={"drr":[], "segs":{k:list(v) for k,v in segs.items()}}
ea=DRR[0]
while ea < DRR[1]:
    q=ida_bytes.get_qword(ea)
    nm=ida_name.get_name(ea)
    tgt=None
    for k,(a,b) in segs.items():
        if a<=q<b: tgt=k; break
    out["drr"].append([ea,q,tgt,ida_name.demangle_name(nm,0) or nm if nm else None])
    ea+=8
json.dump(out, open("/tmp/apacwork/out/drr.json","w"))
print("drr qwords:",len(out["drr"]), file=sys.stderr)
idapro.close_database(False)
