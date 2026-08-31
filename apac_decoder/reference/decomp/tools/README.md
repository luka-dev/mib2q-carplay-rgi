# How this directory was generated

Scripts that drive IDA Pro 9.3 headless through the `idapro` idalib Python module. They are
here so the extraction can be re-run — with different Hex-Rays settings, a different function
set, or after a correction — rather than being a one-off.

They expect a **copy** of the library at `/tmp/apacwork/lib.so` (copy it, so the shared `.i64`
is not contended) and write intermediates to `/tmp/apacwork/out/`.

Run in order:

| script | does |
|---|---|
| `pass2.py` | declares the C types, rebuilds every prototype from the Itanium mangling, applies them, decompiles all functions → `out/final.json` |
| `pass3.py` | dumps `.data.rel.ro` with relocations applied → `out/drr.json` |
| `pass4.py` | disassembles the high-risk functions → `out/disasm.json` |
| `pass5.py` | closes the function set over the call graph, to catch code that is not named after APAC |
| `emit.py` | writes the per-class `.c` files |
| `classes.py` | writes `CLASSES.md` |
| `rodata3.py` | walks the pointer graph, writes `RODATA.md` and `rodata/` |
| `types.py` | the C++ → IDA type mapper the others share |

`out/apac_funcs.json` (the selected function set) is built by the enumeration step described in
`../NOTES.md`; `pass5.py` extends it.
