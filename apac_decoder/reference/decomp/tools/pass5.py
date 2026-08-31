import idapro, json, sys, re
idapro.enable_console_messages(False)
have={int(k,16) for k in json.load(open("/tmp/apacwork/out/apac_funcs.json"))}
syms=json.load(open("/tmp/apacwork/out/dynsym.json"))
dem={}
for s in syms:
    if s[1]=='.text' and s[4]==2: dem.setdefault(s[0],[]).append(s)
idapro.open_database("/tmp/apacwork/lib.so", run_auto_analysis=True)
import ida_auto, ida_funcs, idautils, ida_name
ida_auto.auto_wait()
EXCL=re.compile(r'^(?:[\w\s\*&<>,:~\[\]]+ )?(?:mpddrc|CarPlay|DFTHandle|PlugInWrapperLogFile|std|boost|__gnu_cxx|__cxx|operator)\b|^(?:_|__)|^vDSP_|^cblas|^mem(cpy|set|move)|^operator ')
frontier=set(have); closure=set(have); added={}
for _ in range(4):
    nxt=set()
    for ea in frontier:
        f=ida_funcs.get_func(ea)
        if not f: continue
        for h in idautils.Heads(f.start_ea,f.end_ea):
            for c in idautils.CodeRefsFrom(h,0):
                cf=ida_funcs.get_func(c)
                if not cf or cf.start_ea in closure: continue
                n=ida_name.get_name(cf.start_ea) or ""
                d=ida_name.demangle_name(n,0) or n
                if EXCL.search(d): continue
                if not n: continue
                nxt.add(cf.start_ea); added[cf.start_ea]=d
    if not nxt: break
    closure |= nxt; frontier = nxt
new=sorted(added)
print("NEW callees:", len(new), file=sys.stderr)
for ea in new:
    print("0x%08x %s" % (ea, added[ea]), file=sys.stderr)
json.dump({hex(k):added[k] for k in new}, open("/tmp/apacwork/out/newcallees.json","w"))
idapro.close_database(False)
