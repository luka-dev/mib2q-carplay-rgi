# LZOZ — eso compressed QNX imagefs (MU1439's `mifs-stage2`)

## TL;DR

- File: `MMX2/mifs-stage2/{50,70}/default/mifs-stage2.img` (48 MB; canonical compressed payload also at `extracted/app70/img_restore/main_stage2.ifs.lzo` = 29.6 MB).
- Container magic: ASCII `"LZOZ"` at offset 0.
- Algorithm: **lzo1z** (NOT lzo1x). The "LZOZ" header is also recognized by stage1's `"found lzo1z header"` string.
- 37 **independent** lzo1z streams (one per 2 MB output block, last is the partial remainder).
- Compressed blocks are stored 512-byte aligned in the file (padding between them).
- Blocks 1–36 are decoded with **block 0's own decompressed output as the preset dictionary** (`lzo1z_decompress_dict_safe(..., dict=block0_out, dict_len=0x200000)`).

## Container layout

```
offset    field                                  bytes
------    -----                                  -----
0x000     magic "LZOZ"                           4
0x004     u32  blocksize = 0x00200000 (2 MiB)    4
0x008..   u32[37] table[k] = compressed length   4×37 = 148
                  of block k
0x09c..   zero-pad to 0x800                      remainder
0x800..   compressed stream block 0              ((tbl[0]+0x1ff) & ~0x1ff) bytes (incl pad)
…         block 1, block 2, …                    each (tbl[k]+0x1ff) & ~0x1ff
```

So block-start offsets in the file:
```
fb[0]   = 0x800
fb[k+1] = fb[k] + ((tbl[k] + 0x1ff) & ~0x1ff)
```

Final decompressed size = `image_size` field of the resulting QNX imagefs header = **0x04883B28 = 76 036 904 B**.

## Decoding (the correct recipe)

Pseudocode (Python-ish):

```python
fb = 0x800
tbl = struct.unpack_from("<37I", data, 8)
out = bytearray()
block0 = None
for k, comp_len in enumerate(tbl):
    src = data[fb : fb + comp_len]
    dst_len = 0x200000  # last block: actual remainder; lzo1z reports actual
    if k == 0:
        out_k = lzo1z_decompress_safe(src, comp_len, dst_len)         # no dict
        block0 = bytes(out_k)
    else:
        out_k = lzo1z_decompress_dict_safe(src, comp_len, dst_len,
                                            dict=block0, dict_len=0x200000)
    out.extend(out_k)
    fb += (comp_len + 0x1ff) & ~0x1ff
assert len(out) == 0x04883B28
open("mifs-stage2.full.imagefs", "wb").write(out)
```

Genuine `lzo1z_decompress_safe` / `lzo1z_decompress_dict_safe` are in QNX's `liblzo.a` (`/usr/lib/liblzo.a` on the QNX VM; headers `/usr/include/lzo1z.h`). The Linux/brew `liblzo2` exposes lzo1z too but the QNX build is what was actually used to produce these images.

## Practical workflow that succeeded

1. On the QNX VM (has `liblzo.a` + `lzo1z.h` + gcc):
   - Compile a tiny C wrapper invoking `lzo1z_decompress_safe` for block 0 and `lzo1z_decompress_dict_safe` for blocks 1–36, advancing `fb` with the 512-pad rule.
   - Output → `mifs-stage2.full.imagefs`.
2. Validate with **native** QNX `/bin/dumpifs` (the macOS port chokes on the unwrapped-but-still-funny imagefs).
3. `lsd.jxe` carving: it's NOT nested. The imagefs has it directly at `ifs/lsd.jxe`, with header offset **0x878000** and size **0x376FFBE** (~58 MB). Carve by offset or extract via dumpifs.
4. `python3 jxe2jar.py lsd.jxe MU1439-lsd.jar` (rt.classes default skip-list).

## Why my earlier theories were wrong

This is documented in `_mhi2/memory/mhi2_mu1439_lzoz_format.md` for posterity. Three false leads burned a lot of effort:

1. **"It's lzo1x"** — the algorithm appeared correct because my hand-ported lzo1x decoder happened to reach the expected output size (≈76 MB), but the content was garbage (corrupt ELF headers, no parseable directory). The real algorithm is lzo1z. Lesson: validate by content (a real dirent / "imagefs" header) not by size.
2. **"Preset dictionary is in stage1.full"** — I located a template imagefs at stage1.full:0x1a110 (which IS the right kind of data) and produced a 6-byte dirent match by aligning dict_END=0x22460. But that match was *coincidental* (it's a small repeated pattern); dumpifs still couldn't parse the directory because the real algorithm is lzo1z and the real dict is block-0's output, not a stage1 blob.
3. **"There's a block-36 framing desync"** — also an artifact of the wrong lzo1x decoder. With correct lzo1z + 512-aligned blocks + block0-as-dict, all 37 blocks decode cleanly to exactly 0x04883B28.

What I DID correctly establish (and is still useful):
- `mifs-stage1.img` is **zlib-compressed** (raw `78 da` stream at file offset 0x800, behind the "A.D.O.D" outer header). The 7.1 MB ARM startup binary inside contains the lzo1z+dict decompressor that mounts stage2 at boot.
- `swdlclient` does NOT decompress stage2 (it uses zlib only and copies stage2 verbatim to flash).

## References

- `_mhi2/memory/mhi2_mu1439_lzoz_format.md` — full history including the wrong leads
- `_mhi2/paths.md` — where the clean imagefs and `MU1439-lsd.jar` live
