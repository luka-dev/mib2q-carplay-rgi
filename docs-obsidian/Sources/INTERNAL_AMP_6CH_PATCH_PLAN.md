# Discrete 5.1 to the INTERNAL amplifier (non-B&O Audi) — concrete patch plan

Date: 2026-09-19. Static RE only; nothing built, nothing flashed, no car touched.
**Verdict: FEASIBLE, no hard wall.** All edits are on the **RCC only** — two binaries (MIBRoot +
the DSP image), 4 edit sites, exactly ONE of which is code. No MMX change (the MMX already emits 6
channels; see site 5). Nothing on the RCC has a CRC or signature, so a modified image loads. Can
only be validated on a car coded `Sound_System`=1 (internal amp) — our B&O car cannot test it (its
5.1 goes MMX→MOST→external amp and never touches the RCC DSP).

Source reports: `INTERNAL_AMP_MIBROOT_ROUTING.md`, `INTERNAL_AMP_AUDIOPROCESS_SINKS.md`,
`INTERNAL_AMP_RCC_DSP.md`, `INTERNAL_AMP_PATH_KNOWLEDGE.md`. Byte/offset provenance noted per site.

Why it works: the internal-amp DSP mixer (`SetupZone0Mixers` 0x8755a050) is a real N×M matrix whose
output bus is already ~6-wide (FL FR C LFE Ls Rs, C/LFE at −6 dB). 6 discrete channels have somewhere
to land; they are not synthesized from stereo. MHI2Q only gets stereo because the MMX→DSP link
(McASP3) is configured as 2-slot I2S. The AU audio table already defines the multichannel connection.

---

## Binaries and files

| # | binary | where it lives | how to open it |
|---|---|---|---|
| A | **MIBRoot** (RCC) | RCC IFS3 `usr/apps/MIBRoot` (LZO imagefs, flash region #3 @0xba0000) | `dumpifs`/`mkifs` (`qnx65-armv7-toolchain`); or patch the extracted `MIBRoot_RCC` then repack |
| B | **RCC DSP image** | RCC NOR flash type-4 @0x3d00000 ("BGZ boot table", LZO1X records, no CRC) | `scratchpad/rcc_dsp/bgz_unpack.py` → sections; edit; `bgz_pack.py` → image (round-trip verified) |

Both binaries are on the RCC. No MMX-side edit (site 5).

MIBRoot record bytes verified in `MIBRoot_RCC`:
`0xCE05A4: 78 00 00 00 | 6e 04 00 00 | 01 00 00 00 | 6f 04 00 00 | 02 00 00 00`
= {resource 0x78, int_src 0x46e, int_snk 1, ext_src 0x46f, ext_snk 2}.

---

## SITE 1 — MIBRoot: route internal DIO media to the multichannel sink  (DATA, 8 bytes, PROVEN)

Resource record 0x78, internal pair. Flip internal `{src,snk}` from stereo to the MC pair (same
values the external branch already uses).

| file offset (in `MIBRoot_RCC`) | VA | field | before | after |
|---|---|---|---|---|
| `0xCE05A8` | `0xDE05A8` | internal src | `6e 04 00 00` (0x46e `eSRC_ENT_AMP_DIO_MEDIA`) | `6f 04 00 00` (0x46f `…_MC`) |
| `0xCE05AC` | `0xDE05AC` | internal snk | `01 00 00 00` (`eSNK_SPEAKERS`) | `02 00 00 00` (`eSNK_DTCP51`) |

Result: connection 162 on an internal-amp car (`ExternalAmplifierCoded==0`) resolves to
`{eSRC_ENT_AMP_DIO_MEDIA_MC, eSNK_DTCP51}` = the `MULTICHANNEL` connection, which on the internal
`CMainAmp` map (sub_146B78) is DSI sink **6** "Entertainment Multichannel", width 26 (6ch).
Nothing else in MIBRoot changes. Keep `Sound_System`=1 (coding =2 → `--amptype=EXT` → internal area
dropped → no audio). No AudioProcess change, no AU XML change.

> On-unit sanity check before trusting the byte: `audioprocess-re` proved AM10 (`MULTICHANNEL`/
> eSNK_DTCP51) → internal DSI sink 6 width 26, and AM13 (`SPEAKERS_MC`) is only an alias of stereo
> sink 1. So the target is eSNK_DTCP51 (2), NOT eSNK_SPEAKERS_MC (31). Confirm once from the live
> AudioDiag sink log.

---

## SITE 2 — RCC DSP: McASP3 receive → 8-slot TDM  (DATA / .cinit, PROVEN values)

The MMX→DSP link (McASP3) QC block is 2-slot I2S. The nvidia-vs-QC selector is SOFT (branch on
bit31 of a config value, no hardware detect), so editing the QC block in place forces TDM8 no matter
which branch is taken.

QC McASP3 rx cfg block: **file 0x876153A0** (unpacked DSP section), run addr 0x8760D098.
Block layout `{sermask, RFMT, AFSRCTL, RTDM, 0, ACLK, 0, pinctl, x, 0}`, 40 bytes, 32-bit words.

| word | run addr | field | before | after |
|---|---|---|---|---|
| [2] | 0x8760D0A0 | AFSRCTL | `0x00000113` (I2S, 2-slot) | `0x00000412` (8-slot TDM) |
| [3] | 0x8760D0A4 | RTDM slot mask | `0x00000003` (2 slots) | `0x000000FF` (8 slots) |

Also the 84-byte channel descriptor word[9] channel-count: `0x00000002` → `0x00000008`
(QC descriptor run ≈ 0x8760D044). Do the same on the tx pair block if the amp path is bidirectional.
Buffer-size word is already 0x1000 (same as nvidia) — no resize.

Values are little-endian in the image (C6000 LE): e.g. AFSRCTL byte pattern `13 01 00 00` → `12 04 00 00`.
File offsets: the one PROVEN anchor is rx block file 0x876153A0 ↔ run 0x8760D098. Resolve the exact
file offset of each word from `bgz_unpack.py`'s section map before writing; do not assume a fixed
run→file delta across records.

Alternative (equivalent): NOP the `[B0]` predicate at DSP 0x875334E4 so the selector always picks the
nvidia/TDM8 head 0x8760CF68 (1-instruction patch). Site 2's in-place data edit is cleaner.

---

## SITE 3 — RCC DSP: reader channel count 2 → 6  (CODE — the only code edit, PROVEN location)

The McASP3 ARC push-mode reader instance is created with a hard-coded channel count. Change the
immediate 2 → 6 so buffers/taps are sized for six channels.

- Function: McASP3 setup `0x87532d30..0x87533730` (`ARC_McASP_IF_createPushmodeReaderInstance` path).
- The channel constant is an `MVK 2` immediate stored into the ARC device struct at `0x87532a2c..
  0x87532a78` (the code also contains the `MVK 6` pattern for reference). Patch the MVK operand 2→6.
- `arcInputDev_ReaderSettings` (0x875317e4) and `arcWriterDistribution` (0x87532104) size taps from
  this count; confirm they follow the constant (they read the device-struct field, so one constant
  should suffice — verify on the rendered C6x).

This is the single genuine code touch in the whole plan. Resolve its file offset via `bgz_unpack.py`.

---

## SITE 4 — RCC DSP: MainMix maps 1:1 + PreDownMixer identity  (DATA / .cinit, substantial)

Make the six discrete input channels land on the six amp outputs and stop the pre-mix fold.

- **MainMix source map** `0x8760c108` [7×20]: rows currently `{0,0,1,2,3,4,5}`. Set so the new
  discrete DIO-MC source's 6 channels map 1:1 to mixer inputs 0..5.
- **MainMix channel map** `0x8760c238` (variant >14; also 0x8760c2c8 =13, 0x8760c358 ≤14): rows
  reference input channels 0..7 (e.g. row3 `4 5 6 7 1 1`). Set the DIO-MC rows to identity 0..5 →
  outputs 0..5. Leave the C/LFE −6 dB output gains at `0x87602648` ([4]=[5]=0.5) unless you want
  full-scale C/LFE.
- **PreDownMixer** channel-offset table at `0x876225a0` (Harman ALG object, created in
  `CreatePreMixChain` 0x87559650): set to identity so a 6-ch source is passed through, not folded to
  speaker geometry.

These are `.cinit` tables but the edit spans the mixer config (158-entry), so it needs the rendered
mixer semantics, not a blind byte poke. This is the most careful part of the DSP work.

---

## SITE 5 — MMX transmit side: NO PATCH NEEDED  (localized, PROVEN)

Resolved: the MMX→RCC entertainment link is **MOST, not a direct wire**. Path:
MMX deva → LPASS/AFE `TDM_OUT_MPL` (CSD dev 78, static 6-slot×24-bit, frame header type 2 =
ENTERTAINMENT_MOST) → MMX INIC → MOST150 ring → RCC INIC → McASP3 → RCC DSP. The MMX AFE frame and
RCC McASP3 sit on OPPOSITE sides of two INICs + the ring, so 6×24 vs 8×32 never have to match — the
INICs reframe; only the MOST **BlockWidth** (26 bytes for the MC source) crosses the ring.

Consequences:
- The MMX already emits 6 channels (TDM_OUT_MPL is statically 6-slot; deva is never consulted for
  width). Our deployed `deploy/deva-ctrl-qc` 6-voice widen + hook `ASAD=0xfc` already cover the
  CarPlay case; the `mpl1` media path is proven 6ch in a real-vehicle slog.
- The 26B(6ch)/4B(stereo) source width lives in MIBRoot's `MmxSourcePresCtrl` table (src7 srcID 0x34
  BW26 vs src1 srcID 0x3a BW4) and is chosen by MIBRoot's logical-source pick = **site 1**. So site 5
  is gated entirely by site 1; there is nothing to edit on the MMX (no deva/ACDB/AFE change).
- The RCC McASP3 8-slot enablement is **site 2** (RCC-side), past both INICs — not an MMX concern.

Only open item (INFERRED, not patchable in any HU binary): whether the OS81xxx INIC physically
forwards all 6 active MOST slots for the 26B BlockWidth. This is a live-capture check on the unit,
the same boundary flagged in `AFE_TDM_MOST_SLOT_AUDIT.md` §8 — not a patch.

---

## Build & flash mechanics (PROVEN — MIB "More Incredible Bash" already does exactly this)

The RCC-patch pipeline is a solved problem in `Refferences/M.I.B._More-Incredible-Bash` (the toolset
that flashes modified RCC images from the MMX). Reuse its exact flow:

RCC access: the RCC is a qnet node reachable from the MMX as `/net/rcc/...`; RCC NOR flash =
`/net/rcc/dev/fs0`; remote-exec on the RCC via `on -f rcc <cmd>`. Shipped binaries in
`M.I.B.../apps/sbin/`: `flashunlock`, `flashlock`, `flashit`, `dumb_persistence_reader`,
`updatePersistence`; `modifyE2P` lives on the RCC (`/net/rcc/usr/apps/modifyE2P`).

1. **Dump** the RCC to know offsets: `rccd -d` (`cat /net/rcc/dev/fs0`, ~64 MB) or `backupplus`
   per-region. `backupplus` already carves the DSP image as **`AUDI_MIB_DSP.bin.bgz`** at
   NOR block 15616 (= **0x3D00000**), 768 blocks — this is our BGZ DSP image (matches Flashlock's
   DSP region), so it dumps and reflashes by the same mechanism.
2. **Carve the stage-2 IFS** (contains MIBRoot): `patch_prep` finds OFFSETPART2 from the startup
   magic `EB7E FF00 0100 08` (our **0x00BA0000**) and STAGE2SIZE (dword at header+0x24 = C005F400),
   producing `<ver>-ifs-root-stage2-0x00BA0000-<size>.ifs`.
3. **Patch offline**:
   - MIBRoot IFS3: it is a QNX startup image with an LZO imagefs — decompress → patch the 8 bytes
     (site 1) → recompress → fix startup `stored_size` + imagefs checksum. Tools:
     `qnx65-armv7-toolchain` (`dumpifs`/`mkifs`); `Tools/mmi-ifs` decompresses (lzod).
   - DSP image: "BGZ boot table" (magic 0xC6000ABC, LZO1X records, entry 0x87629000), no internal
     CRC/signature. Edit sites 2-4, repack with `scratchpad/rcc_dsp/bgz_pack.py` (round-trip verified).
4. **Image-internal checksums only** — there is no per-region flash signature to satisfy:
   - stage-2 IFS (MIBRoot): fix the QNX startup-header + imagefs checksum on repack (`mkifs` /
     the "CP" = checksum-patched step in MIB's prebuilt images). This is the one integrity
     requirement for the IFS.

     **IFS repack — you do NOT need to rebuild the whole tree.** MIBRoot's 8 bytes sit inside the
     LZO-compressed imagefs (stored 0xF405C0 vs uncompressed 0x2A94A1C; a raw byte-poke on the flash
     is impossible — verified: the record-0x78 bytes do not appear in the `.ifs`). The imagefs is a
     sequence of independently-compressed LZO blocks, so the minimal path is **surgical, one block**:
     1. decompress only the LZO block covering the MIBRoot target offset (mmi-ifs already walks the
        blocks with lzod);
     2. patch the 8 bytes;
     3. recompress that block (LZO1X);
     4. fix its length prefix + the startup `stored_size` + imagefs checksum. If the recompressed
        block is ≤ the original, pad to the original length so nothing downstream shifts; otherwise
        rewrite the following blocks' length prefixes and `stored_size`.
     Full `dumpifs → mkifs` rebuild is the alternative (needs a reconstructed build script) and is
     unnecessary if the surgical path holds. Uncompressed rebuild is impossible: 44.6 MB does not fit
     the ~16 MB flash region. Either way, `flashit` rewrites the whole 0xBA0000 region on write — the
     *build* is one block, the *flash* is the region.
     TODO before building: confirm the target bytes fall in a single block and that it recompresses
     to ≤ its original size.
   - DSP BGZ image: no internal CRC or signature (rcc-dsp-re); `bgz_pack.py` produces a valid image.
   - No whole-flash or per-region signature check was found: Flashlock is write-protection only
     (cleared by `flashunlock`), and the IPL scans NOR by header type. The `SHA1` in MIB's PATCH_DB
     is MIB's own check of the patch file, not a unit-side verification. Confirm at boot.
   - **FEC is NOT involved.** `FecContainer.fec` (`/net/rcc/mnt/efs-persist/FEC/`, ~243 B, signed)
     is the **Feature Enabling Code** container: per-vehicle feature licenses + VIN binding.
     `addfec`/`addFecs.txt` merge feature codes a patch needs; the entries are feature IDs, not flash
     offsets. Our audio-route patch enables no licensed feature, so FecContainer.fec is untouched.
     (Datasets, a separate thing, are CRC16-protected — `apps/subwoofer`, `apps/amp`.)
5. **Flash**: `flashunlock`, then `flashit -v -d -x -a <OFFSET> -p /net/rcc/dev/fs0 -f <img>`
   (OFFSET = 0x00BA0000 for the IFS, 0x3D00000 for the DSP), then reboot. Flashlock shows IFS + DSP
   LOCKED; `flashunlock` clears it. This is the proven MIB write path.

So both our targets ride an existing, exercised RCC-flash pipeline; the only integrity step is the
IFS's own internal checksum on repack. No FEC work for this patch.

## Recommended order (when a test car exists)
1. Confirm the eSNK_DTCP51 sink target from the live AudioDiag log (site 1 note).
2. DSP: sites 2 (McASP3 TDM8) + 3 (reader count) + 4 (MainMix identity + PreDownMixer identity).
3. MIBRoot: site 1 (8-byte source/sink flip).
4. Bring up together; watch SPC "number of channels does not match" and the AudioDiag sink log; if
   only front L/R come out, check the INIC forwards all 6 MOST slots (site 5 open item, live only).
No MMX edit at any step.

## Risk
All edits are unguarded (no signature) so a bad image is recoverable by reflashing stock. The real
cost is that debugging (SPC channel-match, AudioDiag sink log, actual 6-speaker output) needs a car
with the internal amplifier; none of it is observable on our B&O unit.
