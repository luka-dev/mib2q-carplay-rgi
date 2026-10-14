import json, re, os, collections, importlib.util
spec=importlib.util.spec_from_file_location("apactypes","/tmp/apacwork/types.py")
T=importlib.util.module_from_spec(spec); spec.loader.exec_module(T)
import os as _os; _REPO = _os.path.normpath(_os.path.join(_os.path.dirname(_os.path.abspath(__file__)), '../../../..')); _WS = _os.environ.get('AUDI_WORKSPACE', _os.path.expanduser('~/Desktop/AUDI_2'))  # sibling firmware/data dirs
OUT=_REPO + "/apac_decoder/reference/decomp"
D=json.load(open("/tmp/apacwork/out/final.json"))
DIS={int(k,16):v for k,v in json.load(open("/tmp/apacwork/out/disasm.json")).items()}

NS=["apac::scene","apac::drc","apac::chan","apac::lbr","apac","std","boost","__gnu_cxx","mpddrc","CarPlay"]
def group_of(dem):
    q=T.qualified(dem); comps=[T.strip_targs(c) for c in T.split_top(q,"::")]
    scope=comps[:-1]
    if not scope: return "_free_functions"
    if scope[0] in ("std","boost","__gnu_cxx"): return scope[0]
    if scope[0]=="apac":
        return "apac::"+scope[1] if len(scope)>1 else "apac"
    return scope[0]
def fname(g): return re.sub(r'[^A-Za-z0-9_]','_',g.replace("::","_"))

STACKCLASH=re.compile(r'^\s*\*\(&\w+ - \d+\) = 0;\n', re.M)
def clean(p):
    p=STACKCLASH.sub('', p)
    return p

groups=collections.defaultdict(list)
for r in D["text"]:
    r["_g"]=group_of(r["aliases"][0])
    groups[r["_g"]].append(r)

SKIP_NOTE = {"apac::scene","apac::AudioSceneConfig","apac::APACAudioSceneDecoder","SC_SceneComposition",
  "SC_SceneController","SC_SceneProcessor","SC_AudioScenes","SC_Extension","SC_Tag","SC_PresetDescription",
  "SC_CompositionSelectionGroup","SC_LanguageSelectionSet","SC_LanguageSpecificItem","SC_NonLanguageItem",
  "SceneCompositionSettings","SC_ComponentModifier","APACCoreLBRDecoder","apac::lbr","CD_AudioSceneControlData"}

manifest=[]
for g,recs in sorted(groups.items()):
    recs.sort(key=lambda r:r["ea"])
    fn=os.path.join(OUT, fname(g)+".c")
    tot=sum(r["size"] for r in recs)
    L=[]
    L.append("/* ============================================================================")
    L.append(" * %s" % g)
    L.append(" *")
    L.append(" * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into")
    L.append(" * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).")
    L.append(" * %d function(s), %d bytes of .text." % (len(recs), tot))
    L.append(" *")
    L.append(" * Addresses are file/virtual offsets in that .so (image base 0).")
    L.append(" * This is REFERENCE MATERIAL for a hand port - it does not compile.")
    L.append(" * Types: parameter types come from the Itanium mangling and are exact;")
    L.append(" *        class structs are forward-declared only, so member accesses appear")
    L.append(" *        as raw offsets off `this`. See CLASSES.md for the offset tables.")
    if g in SKIP_NOTE:
        L.append(" *")
        L.append(" * NOTE: not required for the CarPlay 5.1 / 5.1.2 target (Ambisonics scene")
        L.append(" *       or low-bitrate path). Included for completeness only.")
    L.append(" * ========================================================================== */")
    L.append("")
    L.append("/* ---- index -------------------------------------------------------------- */")
    for r in recs:
        L.append("/* 0x%08x %6d  %s */" % (r["ea"], r["size"], r["aliases"][0]))
    L.append("")
    for r in recs:
        L.append("/* ------------------------------------------------------------------------ */")
        L.append("/* 0x%08x  size %d bytes  %d insns" % (r["ea"], r["size"], r["insns"]))
        for a in r["aliases"]:
            L.append(" *   %s" % a)
        for mg in r["mangled"]:
            L.append(" *   [%s]" % mg)
        if r["drefs"]:
            L.append(" *   data refs: %s" % ", ".join("0x%x"%x for x in r["drefs"][:24]))
        L.append(" */")
        if r.get("pseudo"):
            if "output may be wrong" in r["pseudo"]:
                L.append("/* !! Hex-Rays reported 'local variable allocation has failed' for this")
                L.append(" * !! function. Treat the pseudocode below as a sketch, not as source.")
                L.append(" * !! The disassembly appended after it is the ground truth. */")
            L.append(clean(r["pseudo"]))
            if r["ea"] in DIS:
                L.append("")
                L.append("#if 0 /* disassembly of 0x%08x, for verification */" % r["ea"])
                L.append(DIS[r["ea"]])
                L.append("#endif")
        else:
            L.append("/* DECOMPILATION FAILED (%s) - disassembly follows */" % r.get("err"))
            L.append("#if 0\n"+ (r.get("disasm") or "") +"\n#endif")
        L.append("")
    open(fn,"w").write("\n".join(L)+"\n")
    manifest.append((g, fname(g)+".c", len(recs), tot))

manifest.sort(key=lambda m:-m[3])
print("%-42s %-40s %4s %8s" % ("class/group","file","fns","bytes"))
for g,f,n,b in manifest: print("%-42s %-40s %4d %8d" % (g,f,n,b))
print("TOTAL", sum(m[2] for m in manifest), sum(m[3] for m in manifest))
json.dump(manifest, open("/tmp/apacwork/out/manifest.json","w"))
