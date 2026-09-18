# MHI2 (non-Q) port — consolidated knowledge

This folder collects everything established about porting this CarPlay patch from **MHI2Q (MU1316, the patch's baseline)** to **plain MHI2 (target: MU1439, Audi US AUG22 G22 — also applies to sibling MU1326/MU1339/MU1340)**.

## TL;DR

| Layer | MHI2Q (current) | MHI2 (target) | Effort |
|---|---|---|---|
| iAP2 stack | **Cinemo** in `dio_manager` | **Apple ref stack** in `mm-ipod` (loads `ipod-drvr-iap2.so`) | architectural change |
| Hook process | dio_manager only | **mm-ipod (RGI) + dio_manager (cover-art)** | 2 hooks |
| RGI enable | runtime Identify patch + `dio_manager.json` msg-IDs | **`/etc/mm/iap2.cfg [ident] recvmsgs/sendmsgs`** (no binary patch) | trivial config edit |
| RGI capture | LD_PRELOAD interpose Cinemo `NmeIAP2Message::Decode` | **binary DETOUR `link_handle_iap2pkt@0x12e00`** in `ipod-drvr-iap2.so` (driver is DT_SYMBOLIC → LD_PRELOAD interpose does NOT work for driver-internal calls) | the main C-hook work |
| Cover art | chunked-JPEG reassembly from raw stream | **LD_PRELOAD dio_manager + call `iap2_nowplay_getartwork`** (clean libiap2client export) | simpler than MHI2Q |
| AltScreen | impossible | impossible (same Cinemo-pre-2020/libairplay-210.81 gates) | dropped from scope |
| Java side | works against MU1316 stock | cover-art class **identical**; ClusterService needs rebuild vs new `Util.formatDistance/getInvalidDistance` sigs; AppConnectorNavi NOT replaced by patch (internal-only changes) | bounded recompile + 3 small fixes |

Focus features for this port: **RGI + cover art** (user-narrowed scope).

## Folder layout

- [`memory/`](memory/) — durable knowledge memos (snapshots of the project memory):
  - `mhi2_mu1439_port_analysis.md` — top-level architecture deltas (Apple iAP2 vs Cinemo)
  - `mhi2_mu1439_rgi_path.md` — RGI hook path (mm-ipod, iap2.cfg, link_handle_iap2pkt)
  - `mhi2_mu1439_coverart_path.md` — cover-art path (iap2_nowplay_getartwork)
  - `mhi2_mu1439_lzoz_format.md` — eso LZOZ = lzo1z + block0-as-dict + 512-aligned
  - `mhi2_mu1439_java_port_lowrisk.md` — Java diffs MU1316↔MU1439 (only ClusterService + small fixes needed)
- [`hook_port_plan.md`](hook_port_plan.md) — **the main deliverable**: concrete C-hook port plan with mechanism + symbols + open RE
- [`paths.md`](paths.md) — pointers to firmware files, jar, imagefs, decompiled refs
- [`re/symbol_offsets.md`](re/symbol_offsets.md) — known offsets/symbols in ipod-drvr-iap2.so, dio_manager, libiap2client.so.1
- [`lzoz/README.md`](lzoz/README.md) — how the LZOZ container actually works (lzo1z, 37 blocks, dict=block0)
- [`java/MU1316/`, `MU1326/`, `MU1439/`](java/) — decompiled (cfr) versions of the 4 key port-target classes, same decompiler for clean diff
- [`java/diffs/`](java/diffs/) — unified diffs MU1316 vs MU1439 of those 4 classes (AppConnectorTerminalMode = 0 B = identical)

## Status

✅ Done: LZOZ cracked (lsd.jxe → `MU1439-lsd.jar`, 123 MB, 28528 classes), Java diffs done, hook mechanism decided (binary detour vs LD_PRELOAD), all symbols/offsets located.

🔜 Next concrete step (per user direction): RE of `link_handle_iap2pkt@0x12e00` prologue → design the ARM detour (entry overwrite + trampoline + lazy `dlsym` to the LD_PRELOAD'd handler). See [hook_port_plan.md](hook_port_plan.md) §"Next RE step".
