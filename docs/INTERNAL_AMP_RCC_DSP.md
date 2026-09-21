# RCC (DRA6xx / C674x) DSP image — 6-channel input analysis

Firmware: `MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`, NOR `MU1316-RCC_fs0.bin`, image at 0x3D00000 (813348 B).
Work dir: `scratchpad/rcc_dsp/` (parsers, unpacked sections, ELFs, IDA .i64).
Tags: **PROVEN** = read from code/data with addresses; **INFERRED** = consistent evidence, no direct proof.

## 1. Container: Harman "BGZ boot table" (magic 0xC6000ABC) — PROVEN

Not COFF, not ELF. Detector = IPL `sub_40309304` (rcc_ipl.elf, base 0x40300000 = NOR offset 0, `c674x_ipl_T10 (May 6 2015)`):
`word[0]==0xC6000ABC && (word[9]&0xFFFFFF00)==0xC6C6C600` → image type 11 "BGZ boot table format". Same magic + plain COFF at +32 → type 8 "DSP COFF image".

Header, 32 bytes LE (labels from IPL dump `sub_40309394`, strings 0x403123a4..0x40312424):

| off | value | meaning |
|---|---|---|
| 0x00 | 0xC6000ABC | magic |
| 0x04 | 0x000C6924 | image size = whole file (also used as NOR-scan step) |
| 0x08 | 0x0011AE70 | "original" (unpacked) size — informational (records sum to 0x11AD30) |
| 0x0C | 0x11800100 | never read by IPL or DSP stub (looks like C674x L2 address) |
| 0x10 | 0x2817145A | "datecode"; low 24 bits printed as `ver.%X` |
| 0x14 | 0x00000000 | subtype / core index 0 = "DSP1" (table 0x403122b8) |
| 0x18 | 0x11111111 | filler, never read |
| 0x1C | 0x22222222 | filler, never read |
| 0x20 | 0x87629000 | entry point (DSP `start`); stub stores it at `*B4` |

Records from 0x24: `{u32 tag, u32 size, u32 dst, u8 data[size], pad to 4}`.
`tag 0xC6C6C601` = LZO1X-compressed (" LZO"), `0xC6C6C600` = raw copy ("copy"). Terminator = record with `size==0` (here at 0xC6918: `01c6c6c6 00000000 27e4b3ad`). The last word (0xADB3E427 LE) is in the terminator's `dst` slot and is **never read** by any loader path found (§3).

Decompression: `python-lzo lzo.decompress(blob, False, bufsize)` round-trips every record (38 records). Parser: `rcc_dsp/bgz_unpack.py` → `sections/NN_<dst>.bin`, `layout.md`, `rcc_dsp.elf` (EM_TI_C6000, one PT_LOAD per record). Repacker: `rcc_dsp/bgz_pack.py` (self-checks by re-unpacking).

## 2. Section layout (38 records; full table in `rcc_dsp/layout.md`)

| # | dst (C674x addr) | unpacked | content |
|---|---|---|---|
| 3 | 0x87500180 | 0xE7C40 | `.text` (DSP/BIOS app; B14 data page = 0x87624CE0) |
| 10 | 0x875E7DC0 | 0x1C9F4 | `.const` (all strings) |
| 4 | 0x87612900 | 0x68EC | `.cinit` ROM-model init stream (795 records → .bss 0x8760xxxx/0x8762xxxx; terminator words padded, parse must skip zero words) |
| 0,18,20,5,21,9,23,7,25,12,19,24,6,11,13 | 0x87619200..0x876284BC | — | .switch/.data/.far/.sysmem etc. |
| 16 | 0x87629000 | 0xC0 | `.sysinit`/`start` (entry; sets B15 stack 0x876207FC, B14=0x87624CE0, jumps to c_int00 0x8760CE14) |
| 17 | 0x87E00000 | 0xBC | version block: `DRX6XX_DSPBIOS-RELEASE.A024_MIB2.DSPDUMP.No_Audio_Decoder.1`, `IF_SAMPLE_RATE` |
| 14,15,26..37 | 0x0081C400..0x0081FE20 | — | C674x L2 RAM: fast code/vectors (0x81C400 0xF00, 0x81D300 0xEC0) and small data |

DSP window seen from QNX: `dev-dspipc -ddra6xx,int=77,intprio=20,base=0x87000000,memsize=0x1000000,suppress,clockenable=1` (from `collectConfigData` → `/tmp/startArgs_dev_dspipc`); `suppress` = dev-dspipc does **not** download the DSP; the IPL does (below). dev-dspipc's COFF path (`load_coff_file`@0x2404, `dump_coff_file`@0x1EF8 in dspipc-dra6xx.so) is only for `filename=*.out` and is unused here.

## 3. Boot chain and integrity — PROVEN

1. **IPL** (NOR 0x0, runs at 0x40300000) console script runs `sboot` → `sub_40305C98`: scans NOR (`sub_40304548` → `sub_40304418`: steps through the device window, `sub_40308274` classifies each candidate header via `sub_40309304`), accepts type 11 for core mask, `sub_40305730` appends loadargs tags (3: I=id L=label; 4/5/6: S=src,len D=dst,len T=type; 8: P=prio), `sub_40305B28` loads the embedded C6000 **DSP-loader stub** (COFF at IPL file 0xDDEC, entry 0xE00000, .text 0x2700 B — extracted to `rcc_dsploader.elf`) into the DSP and releases reset.
2. **DSP-loader stub** (`rcc_dsploader.elf` 0xE00544..0xE006F0, read via IDA): `LDW *A10` == 0xC6000ABC → `A10+=32`; `entry=*A10++ → *B4`; loop: `tag=*A10++; size=*A10++; if size==0 → return 0; dst=*A10++`; address translate (`loc_E00314`, threshold globals B14+0x20/0x28), L1D writeback (0x01845000) + region switch (`loc_E00488`), then `if tag==0xC6C6C601 → LZO decompress (loc_E02020)(src,size,dst)`, rc≠0 → status = rc−5000 (negative) → abort; else word copy; `src += (size+3)&~3`. **No CRC, no signature** (no 0x04C11DB7/0xEDB88320/table constants anywhere in the stub; loop has no other checks).
3. **QNX startup** (ifs1 0x83000108, `sub_830009A0`) polls loadargs status (negative → "DSP loader failed: %d", timeout → "DSP loader timed out"); `dsploader` (ifs2 `/bin/dsploader`) waits for image numbers and mounts IFS images from `/dev/mem`.
4. **"BG CRC32"** (`sub_403025EC`, table 0x4030CEC0, poly 0x04C11DB7, MSB-first, register preloaded with ~first 4 bytes, no final flush, result inverted) is used only by: `crc32` console cmd (`sub_40302690`), IPL self-check (`sub_40309894`: "IPL header/program CRC mismatch"), memboot copy check (`sub_4030717C`), and the **verbose dump** of a BGZ header (`sub_40309394`, prints `CRC: 0x%08X` over (image, size) — informational). Exhaustive xref list of `sub_403025EC` = these 5 sites; none is in the image boot path. Neither the trailer word nor any header word matches this CRC over any slice tested; it is not checked.

**Consequence for flashing a modified image**: keep magic, `word[1]` = exact file size, `word[8]` = entry, valid record chain (`0xC6C6C601`+LZO1X or `0xC6C6C600`+raw — raw needs no compressor), size-0 terminator. Sizes in the NOR window must not collide with the next image (scan steps by `word[1]`). No key/signature exists. (Type 8 "header + plain COFF" is also detected, but the automatic `sboot` callback only handles type 11; type 8 is served by the manual `dsp` command `sub_40305204`.)

## 4. A2I (ARM → DSP PCM stream) format, from `liba2itodspipc.so.1` — PROVEN

`A2I_Client_Open(handle*, params*)` → `check_parameter`@0x4C1C: DevType==0; GapMode==1; uNumberOfPackages 1..256; uSizeOfPackage = power of two; **uNumChannels 1..10**; uSizeOfSample 2|4 (Open additionally requires **==2 → 16-bit only**); uSampleRate Hz or flag 0x900000xx (8000..48000 table @0x5630..0x57D8); MuxType 0 `A2I_INTERLEAVED` / 1 `A2I_NON_INTERLEAVED`; eBufferFormat 0 `A2I_FORMAT_MONO` / 1 `A2I_FORMAT_DUAL_MONO`; audioDataBitFormat 0..6; writeTrigger 0x0ABABABA (by client) / 0x05555555 (by library); pDevIdentifier `"/dev/dspipc/dra6xx.0"`, `"<hexchan>"` or `".../stream_<hex>"`.
Transport: `dspipc_openstreamchannel_prio(chan, …, prio 19)`, `dspipc_alloc(nPackages × ((uSizeOfPackage+35)&~3))`; each package = **32-byte header + PCM** (`sub_31C4`/`sub_3004`/`sub_30EC`):

```
00 00 01 BA 80 00 | size(BE,4) | 01 89 C3 F8 50 00 00 01 | nch | bits(16/24/32,0xFF) | rate Hz (BE,4) | 00×8
```
So the ARM library is not the channel limiter (≤10 ch, 16-bit, ≤48 kHz).

DSP side: the A2I streams are the ARC "WAVPlayer1..3" devices (`ipc_WavePlayer{1,2,3}_Handle`, SetupAudioRouterCtrl 0x875344E0..0x87534B10) = announcement/chime players. I found **no** code that reads header byte 18 (nch) relative to a packet base (scan of all `LDBU *+R[18]` sites: 2 hits, neither in a header parser) → INFERRED: the DSP uses its fixed device configuration, not the header's channel count.

## 5. Audio path geometry inside the DSP app

McASP hardware configs live in `.cinit` (ROM model), 40-byte rx/tx blocks `{sermask, RFMT, AFSRCTL, RTDM, 0, ACLK 0x1FF, 0, pinctl, x, 0}` + 84-byte channel descriptors `{.., bufsize, .., dir(1=rx), cfg*, 2, ser<<16|4, ..}` + per-McASP heads. Mapping (heads with index 0/1/2 follow their groups; McASP3 heads `{2,2,3}`):

| McASP | rx/tx cfg (run addr) | AFSRCTL | RTDM | slots | notes |
|---|---|---|---|---|---|
| 0 | 0x8760CA5C / 0x8760CA84 | 0x113 | 0x3 | 2 (I2S) | ser tx5/rx3, buf 0xA00/0x600 |
| 1 | 0x8760CBF0 / 0x8760CC18 | 0x413 | 0xFF | **8 (TDM)** | ser 1, buf 0x800 — INFERRED: DAC → internal amp |
| 2 | 0x8760CD84 / 0x8760CDAC | 0x111 | 0x3 | 2 | ser 1, buf 0x200 |
| 3 "nvidia" (`mcasp3_rcv_cfg`) | 0x8760CF18 / 0x8760CF40, head 0x8760CF68 | 0x412 | 0xFF | **8 (TDM)** | ser 2, buf 0x1000 |
| 3 "QC" (`mcasp3_rcv_cfg_QC`) | 0x8760D098 / 0x8760D0C0, head 0x8760D0E8 | 0x113 | 0x3 | **2 (I2S)** | ser 2, buf 0x1000, word9=0x8000 |

RFMT 0x180F0 everywhere = 32-bit slots, delay 1 bit (I2S/TDM), DMA/bus select. Selection code (0x875334AC..0x87533560): `A6 = (flag bit31) ? 0x8760D0E8 : 0x8760CF68` → `ARC_McASP_IF_init_HW(&h3, A6)`, then prints "Frontground chip is nvidia, mcasp3_rcv_cfg!" / "…QC, mcasp3_rcv_cfg_QC!". (The MVKH halves live in compact packets IDA did not decode; the two constants are exactly the two group heads → PROVEN-by-data, branch↔chip mapping INFERRED from branch order = string order = declaration order.)
McASP3 is the link from the front-end SoC ("Frontground chip" = MMX, QC on MHI2Q). ARC input reader settings (`arcInputDev_ReaderSettings`, 0x875317C8..0x875318F0) store the constant **2** into the channel fields (with 64, 42, 8, 4, 9) — INFERRED 2 = channels per input device.
Processing chain (`MIB2_AudioProcessingChain_create`, 0x87531310..0x87535F04): `hPreDownMixer` (downmixer with per-channel offsets, 0x87559640) → `hZone0_MainMix`/`hZone0_refMix` → delay (`dly_setCurrentTimeDelayChannels`), dynComp, EQ, limiter, gainboost, Gala-f, TCL/TLAM, `cmixer0` (TUNER/BT), MOST-mute → writer devices. An explicit **down**mixer before the main mix means multi-channel inputs are folded to the mix bus; no evidence of a discrete 6-in→6-out path.

## 6. Answers

**(a) 6-channel input mode into the speaker chain?** Interface level: the app *contains* an 8-slot TDM receive config for McASP3 (the MMX link) but on MHI2Q (QC front-end) the 2-slot I2S variant is selected → the DSP receives **stereo** from the MMX. PROVEN (configs, selection code); QC↔2-slot mapping INFERRED. Chain level: no discrete multichannel route found; chain has a PreDownMixer → INFERRED no.
**(b) What must change for FL FR RL RR C SUB:** (1) DSP McASP3 QC rx cfg → TDM8: `.cinit` record at file 0x876153A0 (run 0x8760D098): AFSRCTL 0x113→0x412, RTDM 0x3→0xFF (pure data patch), or make the branch use group D; (2) the McASP3 ARC reader device channel count 2→6/8 (code constants in the McASP2/3 setup function 0x87532D30..0x87533730 / SetupAudioRouterCtrl) — code patch; (3) Xbar/mixer routing so the 6 inputs bypass the downmixer and land on the 6 amp outputs ("Loading mixer configuration"/"Setting xbar settings" tables reached from 0x8755A628/0x8755A7E8) — code+table patch, unmapped; (4) the **MMX must transmit TDM8** on that I2S link (Qualcomm side, not analysed) and MIBRoot/AudioProcess must use a multichannel sink (`eSNK_SPEAKERS_MC` exists in `ifs3/etc/MIB_AudioTable_AU_G22.xml`, gzip'd).
**(c) Formats:** A2I: 16-bit only, 8–48 kHz, ≤10 ch. McASP: 32-bit slots, TDM8 slot mask 0xFF, system rate = `IF_SAMPLE_RATE` startup param (48 kHz INFERRED; `dly_setSampleFreq zone0/zone2`).

## 7. Verdict

**NEEDS DSP PATCH** (and an MMX-side change). The MHI2Q DSP image is wired for a stereo I2S feed from the MMX; an 8-slot TDM feed is already present in the image (nvidia variant) so the *input interface* is a config-level change, but 6-channel discrete routing through the chain is **not demonstrated** (UNKNOWN; likely code/table work in the ARC Xbar/mixer). Flashing is unguarded: no CRC/signature on the BGZ image; only magic/size/tag/LZO validity matter.

## Tools (scratchpad/rcc_dsp)
`bgz_unpack.py` (image → sections + ELF), `bgz_pack.py` (sections → image, round-trip check), `mkelf.py`, `c6x_consts.py` (MVKL/MVKH constant xrefs), `c6x_funcs.py` (function bounds from `B B3`), `c6x_dp.py` (B14-relative data refs). IDA sessions: `rcc_dsp` (tms320c6; MCP quirk: `undefine` range → per-word `define_code` → `search_text` regex "." to render), `rcc_ipl` (ARM, Hex-Rays), `rcc_dsploader`.
