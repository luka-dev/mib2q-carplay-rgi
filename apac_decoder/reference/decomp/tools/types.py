import re, json

def split_top(s, sep):
    out=[]; depth=0; cur=""; i=0
    while i < len(s):
        c=s[i]
        if c in '<([': depth+=1
        elif c in '>)]': depth-=1
        if depth==0 and s.startswith(sep, i):
            out.append(cur); cur=""; i+=len(sep); continue
        cur+=c; i+=1
    out.append(cur); return out

def arglist(d):
    depth=0; start=None
    for i,c in enumerate(d):
        if c in '<([':
            if c=='(' and depth==0 and start is None: start=i
            depth+=1
        elif c in '>)]':
            depth-=1
            if c==')' and depth==0 and start is not None:
                return d[start+1:i], d[i+1:]
    return None, ""

def strip_args(s):
    depth=0
    for i,c in enumerate(s):
        if c in '<([':
            if c=='(' and depth==0: return s[:i]
            depth+=1
        elif c in '>)]': depth-=1
    return s

THUNKS=("virtual thunk to ","non-virtual thunk to ","covariant return thunk to ")
def qualified(dem):
    s=dem
    for p in THUNKS:
        if s.startswith(p): s=s[len(p):]
    s=strip_args(s).strip()
    parts=split_top(s," ")
    return parts[-1] if parts else s

PRIM = {
 "unsigned int":"unsigned int","int":"int","bool":"bool","float":"float","double":"double",
 "short":"__int16","unsigned short":"unsigned __int16","char":"char",
 "signed char":"__int8","unsigned char":"unsigned __int8",
 "long":"__int64","unsigned long":"unsigned __int64",
 "long long":"__int64","unsigned long long":"unsigned __int64","void":"void",
}
ENUMS = {"apac::FrameType","apac::ASCType","apac::CodingType","apac::AudioDataType","apac::Profile",
         "apac::ProfileLevel","mpddrc::HeaderPayloadType","mpddrc::UniDrcDomain","apac::ASCChannelRange",
         "apac::WindowSequence","apac::WindowShape"}

classes = set()   # C++ names needing a forward struct decl

def cident(n):
    return re.sub(r'[^A-Za-z0-9_]','_', n)

def map_type(t, depth=0):
    """C++ type string -> IDA C type string."""
    t = t.strip()
    if t.endswith("&&"): return ptr_of(t[:-2])
    if t.endswith("&"):  return ptr_of(t[:-1])
    if t.endswith("*"):
        inner = t[:-1].strip()
        return ptr_of(inner)
    if t.startswith("const "): t = t[6:].strip()
    if t.endswith(" const"): t = t[:-6].strip()
    if t in PRIM: return PRIM[t]
    if t in ENUMS: return "int"
    if t.startswith("std::vector<"): return "std_vector"
    if t.startswith("std::unique_ptr<") or t.startswith("std::shared_ptr<"): return "void *"
    if t.startswith("__gnu_cxx::__normal_iterator<"): return "void *"
    if t.startswith("apac::observer_ptr<"): return "void *"
    if t.startswith("std::tuple<") or t.startswith("std::pair<"): return "void"
    m = re.match(r'apac::aligned_array<\s*([^,]+),\s*\d+ul?\s*>$', t)
    if m: return map_type(m.group(1))
    if t.startswith("apac::ConstParamArray<"): return "std_vector"
    if t == "std::type_info": return "void"
    if t == "boost::any": return "void"
    if t.startswith("TBitstreamReader<"): return "TBitstreamReader_u32"
    if t=="DSPSplitComplex": return "DSPSplitComplex"
    if "<" in t:
        t = strip_targs(t)
    if re.match(r'^[A-Za-z_][\w:]*$', t):
        classes.add(t); return cident(t)
    return "void"

def ptr_of(t):
    t=t.strip()
    const = False
    if t.endswith(" const"): t=t[:-6].strip(); const=True
    if t.startswith("const "): t=t[6:].strip(); const=True
    if t.endswith("*"):
        return ptr_of(t[:-1]) + " *"
    base = map_type(t)
    if base.endswith("*"): return base + "*"
    return ("const " if const else "") + base + " *"

NAMEHINT = [
 ("TBitstreamReader","bs"), ("APACICSInfo","ics"), ("APACScaleFactorData","sfData"),
 ("apac_SFBOffsets","sfbOffsets"), ("APACSpectralData","spectrum"),
 ("APACIndividualChannelStream","ics_stream"), ("apac_GlobalConfig","globalCfg"),
 ("apac_ParamList","paramList"), ("apac_FrameOutRecord","outRec"),
 ("APACSyntacticElement","element"), ("APACInstanceConfig","instCfg"),
 ("apac_AncillaryConfig","ancCfg"), ("apac_drc__DynRangeCompressor","drc"),
 ("std_vector","vec"), ("SC_AudioScenes","scenes"), ("SC_SceneComposition","sceneComp"),
 ("DRCConfiguration","drcCfg"), ("apac_TCEConfig","tceCfg"),
]

def argname(ctype, i, used):
    base=None
    for k,v in NAMEHINT:
        if ctype.replace("const ","").strip().startswith(k): base=v; break
    if base is None:
        if ctype in ("unsigned int","int"): base="n"
        elif ctype=="bool": base="flag"
        elif ctype.startswith("float"): base="buf" if "*" in ctype else "f"
        elif ctype.startswith("unsigned __int16") or ctype=="__int16": base="v"
        else: base="a"
    n=base; k=2
    while n in used:
        n="%s%d"%(base,k); k+=1
    used.add(n); return n

NS = ["apac::scene","apac::drc","apac::chan","apac::lbr","apac::detail",
      "apac","std","boost","__gnu_cxx","mpddrc","CarPlay"]
def class_of_scope(scope):
    """Return the C++ class name owning the method, or None for a free function."""
    sc = [strip_targs(c) for c in scope]
    for ns in NS:
        parts = ns.split("::")
        if sc[:len(parts)] == parts:
            rest = sc[len(parts):]
            return "::".join(sc) if rest else None
    return "::".join(sc) if sc else None

def build_decl(dem, mangled, funcname, rettype):
    """Return (decl_string, nargs) or None."""
    args_s, tail = arglist(dem)
    if args_s is None: return None
    q = qualified(dem)
    comps = split_top(q,"::")
    scope = comps[:-1]
    meth  = comps[-1]
    params=[]
    used=set(["this"])
    cls_clean = class_of_scope(scope)
    if cls_clean:
        if cls_clean == "std::vector": ctn = "std_vector"
        elif cls_clean.startswith("TBitstreamReader"): ctn = "TBitstreamReader_u32"
        else:
            classes.add(cls_clean); ctn = cident(cls_clean)
        params.append("%s *this" % ctn)
    for t in split_top(args_s, ", "):
        t=t.strip()
        if not t or t=="void": continue
        ct = map_type(t)
        if ct=="void": ct="void *"
        if ct=="std_vector": ct="std_vector *"   # by value never happens for vectors here
        nm = argname(ct, len(params), used)
        params.append("%s %s" % (ct, nm))
    if not params: params=["void"]
    return "%s __fastcall %s(%s)" % (rettype, funcname, ", ".join(params))

def strip_targs(s):
    depth=0; out=""
    for c in s:
        if c=='<': depth+=1; continue
        if c=='>': depth-=1; continue
        if depth==0: out+=c
    return out
