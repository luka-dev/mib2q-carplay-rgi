# Stock AirPlay feature 37 patch, offline

_MHI2Q wireless CarPlay — historical investigation, integrated 2026-09-29._

---

[Wireless overview](../README.md) · [Current stock210 lifecycle](../STOCK210_RECEIVER_LIFECYCLE.md)

Historical investigation; earlier limitations and commands describe that experiment. The current implementation and validation boundary are in the section overview.

Implementation and local diagnostic paths in this record refer to the [original source directory](../../../wireless/airplay_feature_patch/). Run `wireless/...` commands from the repository root; moving this note does not move binaries or logs.

## 📊 Experiment overview

```mermaid
flowchart LR
    accTitle: Historical feature bit experiment
    accDescr: The experiment changes a capability declaration and measures it in QEMU; it does not supply the pairing and media security later added by the hook.
    stock["Original receiver copy"]
    bit["Add feature bit 37"]
    probe["Observe features in QEMU"]
    missing["Pairing and media still absent"]
    stock --> bit --> probe --> missing
```

## 📋 Implementation and evidence

`build_offline.py` checks the exact MU1316 stock `libairplay.so` SHA-256,
then changes one ARM instruction in `AirPlayGetFeatures` at virtual address
`0x20D74`: `MOV R3,#1` becomes `MOV R3,#0x21`. The upper 32-bit feature word
therefore gains bit 5, global feature bit 37 (`CarPlayControl`). The lower
word, platform-supplied bits, exports and ELF layout remain byte-identical.
The script writes a separate output and records both hashes; it never changes
the firmware image.

The MHI2Q 210.81 library does not contain the CarPlay Control client. The
wireless daemon supplies that client separately. This patch only makes the
AirPlay receiver advertise the capability consistently with that client.
The patched library has run in the offline stock-`dio_manager` QEMU mesh.
The real captured manager started the receiver; `GET /info` on its TCP 5000
listener returned a binary plist with `features=0x0000002144440B80` (bit 37
set). No phone has connected and the stock server has not bound TCP 7000.
Reproduce from the repository root with:

```sh
python3 tests/carplay_lab/qnx/qemu_capture.py --mesh --start-experiment \
  --seed-display --skip-hid-qemu --feature37
```

The display seed and HID bypass are QEMU diagnostics only.

An earlier offline variant changed `0x20D38`, which passes an argument to the
platform property getter; a later `MOV R3,#1` at `0x20D74` overwrites it.
That earlier variant did **not** add feature bit 37 to the return value.

`qemu_verify.sh` builds two temporary QNX 6.5 IFS images using the local
Docker SDK and firmware dependencies. Its QEMU-only copies replace the call
to `CFObjectGetPropertyInt64Sync` at `0x20D6C` with `MOV R0,#0`, leaving
`R1=0`, so the no-server probe can exercise the rest of the function. This
extra test patch is never written to the deliverable library. It validates
the feature arithmetic in real QNX/ARM execution. The receiver mesh
experiment above separately validates the full `/info` path.
The verified values are stock `0x0000000144440B80` and patched
`0x0000002144440B80`; both probe processes exit with status 0.
