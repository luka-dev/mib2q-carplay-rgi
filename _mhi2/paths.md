# Artifact paths

Large artifacts and reference firmware locations — not copied here, point in place.

## MU1439 (target firmware) — Audi MHI2 US AUG22 G22

| What | Path |
|---|---|
| Flash image root | `/Users/luka/Desktop/AUDI_2/Firmwares/MHI2_US_AUG22_K3347_MU1439` |
| Extracted `/app` (where `dio_manager`, `swdlclient`, libs live) | `…/MU1439/extracted/app70/` and `app50/` |
| **Clean decompressed imagefs** (76 MB; QNX-dumpifs-validated) | `…/MU1439/extracted/mifs-stage2.full.imagefs` |
| **Carved `lsd.jxe`** (~58 MB, PK/ZIP-style) | `…/MU1439/extracted/ifs/lsd.jxe` (offset 0x878000 of the imagefs) |
| **`MU1439-lsd.jar`** (123 MB, 28 528 classes) — jxe2jar output | `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/MU1439-lsd.jar` |

## Key binaries inside MU1439 `/app`

| Binary | Path | Role |
|---|---|---|
| `mm-ipod` | `extracted/app70/armle/usr/sbin/mm-ipod` | iAP2 daemon (loads driver via dlopen); RGI hook host |
| `ipod-drvr-iap2.so` | `extracted/app70/armle/lib/dll/ipod-drvr-iap2.so` | iAP2 protocol stack (DT_SYMBOLIC); binary-detour target for RGI |
| `dio_manager` | `extracted/app70/eso/bin/apps/dio_manager` | CarPlay/DSI app; cover-art hook host (LD_PRELOAD) |
| `libiap2client.so.1` | `extracted/app70/armle/usr/lib/libiap2client.so.1` | client API (NOT stripped); cover-art calls `iap2_nowplay_getartwork` here |
| `libairplay.so` | `extracted/app70/eso/lib/libairplay.so` | srcvers 210.81; same as MHI2Q (AltScreen still impossible) |
| `swdlclient` | `extracted/app70/eso/bin/apps/swdlclient` | software-download/flashing; uses zlib (not LZO) — not relevant to runtime hooks |
| `mifs-stage1.img` | `MMX2/mifs-stage1/50/default/mifs-stage1.img` | zlib-compressed ARM QNX startup (from offset 0x800); contains the lzo1z+dict decompressor that mounts stage2 at boot |

## Sibling firmware references

| Firmware | Generation | Use |
|---|---|---|
| `Firmwares/MHI2Q_US_AUG22_P5087_MU1316/extracted/...` | MHI2Q (patch baseline) | original target's `dio_manager`/`libairplay`/`lsd.jxe` |
| `Firmwares/MU1367-MHI2_ER_VWG13_K4525/` | MHI2 VW (already-extracted, no LZOZ) | proxy for sibling MHI2 layout; contains `system/etc/scripts/start_usb_device_stack.sh` + `system/etc/mm/iap2.cfg` template |
| `Firmwares/MU1326-MHI2_ER_AU37x_...` | MHI2 ER Audi | closest MU1439 proxy on the Java side (stock classes identical to MU1316) |

## Already-converted lsd.jar collection

`/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/out/`
- `MU1316-lsd.jar` (128 MB) — MHI2Q, patch's compile baseline (also `build_java.sh` → `../jxe2jar/out/lsd.jar` is a symlink to this)
- `MU1326-lsd.jar` (129 MB) — MHI2 ER (proxy)
- `MU1339-lsd.jar`, `MU1340-lsd.jar` (130 MB each) — MHI2 sibling builds
- `MU1439-lsd.jar` (123 MB) — **the new target**

`MU1326-lsd-vf/` next to it = full cfr-decompiled MU1326 (30 998 .java) — used for the `carplay_ref_mu1326/` package extract.

## Other reference

| | |
|---|---|
| QNX VM (SDP 6.5 + liblzo + libucl + arm-eabi-gcc) | `bash /Users/luka/Desktop/AUDI_2/qnx_ssh.sh` (root@192.168.64.16 / root) |
| jxe2jar tool | `/Users/luka/Desktop/AUDI_2/Tools/jxe2jar/src/jxe2jar.py` |
| dumpifs (macOS port) | `/Users/luka/Desktop/AUDI_2/Tools/dumpifs-linux/dumpifs_macos` |
| cfr decompiler | `/Users/luka/Desktop/AUDI_2/Tools/cfr.jar` |
| LZO 2.10 (brew) | `/opt/homebrew/Cellar/lzo/2.10/` |
| python-lzo 1.15 | brewed python (install: `pip3 install --break-system-packages python-lzo`) |
| Existing CarPlay-class reference (1170 .java) | `…/MU1439/carplay_ref_mu1326/` (from MU1326-lsd-vf) |
