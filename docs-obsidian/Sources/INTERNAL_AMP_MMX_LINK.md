# Site 5 — the MMX transmit side of the internal-amp entertainment link

Date: 2026-09-19. Static RE only; nothing built/flashed, no car touched.
Scope: `deva-ctrl-qc.so` (MMX/APQ8064 QNX audio driver) + the three PROVEN prior
audits (`AFE_TDM_MOST_SLOT_AUDIT.md`, `MOST_SOURCE_BLOCKWIDTH_AUDIT.md`,
`INTERNAL_AMP_RCC_DSP.md`, `INTERNAL_AMP_AUDIOPROCESS_SINKS.md`).

Tags: **PROVEN** = read from code/data with addresses. **INFERRED** = consistent
evidence, no single direct proof.

---

## Topology (MMX AFE → link → RCC DSP)

```
MMX (APQ8064)                                        RCC (DRA6xx / C674x)
─────────────                                        ────────────────────
deva-ctrl-qc.so  ── 6ch PCM ──►  LPASS/AFE
  mpl5_dio_ent (CarPlay)          TDM_OUT_MPL = CSD dev 78
  mpl1_int_ent (media)            grouped-I2S TDM frame, 32 B @48 kHz:
                                    bytes 0..1  custom header TYPE 2 = ENTERTAINMENT_MOST
                                    bytes 2..19 six 24-bit media slots (2,5,8,11,14,17)
                                    bytes 26..31 TEL/ANN1/ANN2
                                          │
                                          ▼
                                    MMX MOST INIC (OS81xxx, board HW — NOT in any HU binary)
                                          │
                                    ┌─────┴───────── MOST150 synchronous ring ─────────┐
                                    │  synchronous channel, BlockWidth allocated by      │
                                    │  MIBRoot: MC source 7 = srcID 0x34 → 26 B (6ch),   │
                                    │  stereo source 1 = srcID 0x3a → 4 B                 │
                                    └─────┬──────────────────────────────────────────────┘
                                          ▼
                                    RCC MOST INIC ──► RCC McASP3 ──► C674x DSP
                                      SPC: srcID 0x34(26B) → DSP input 9
                                           srcID 0x3a(4B)  → DSP input 10
                                      PreDownMixer → Zone0 MainMix → EQ/limiter → McASP1 (8-slot TDM) → DAC → internal amp
```

The MMX AFE frame (6 slots × 24-bit) and the RCC McASP3 frame (2- or 8-slot × 32-bit)
sit on **opposite sides of two INICs and the MOST ring**. They are not the same wire
and do not have to agree slot-for-slot; the INICs frame/reframe, and the only quantity
carried end-to-end is the MOST synchronous **BlockWidth** (26 B for the MC source).

---

## Answers to the four questions

### Q1 — What does TDM_OUT_MPL physically connect to: MOST INIC or a direct TDM/I2S wire to the RCC?

**A MOST INIC. PROVEN (by the AFE frame format) + INFERRED (the INIC part is HW).**

- `deva-ctrl-qc.so` contains **zero** occurrences of `MOST`, `INIC`, `MediaLB`, `MLB`,
  `McASP`, `SLIMBUS`, `MI2S` (IDA string search, session e2b2ac5e). It only speaks
  Qualcomm AFE: `TDM_OUT_MPL`, `TDM_OUT_ANN_*`, `TDM_OUT_TEL`, `TDM_IN_ENT_*`,
  `TDM mode = %d not supported`. So the MMX driver's world ends at the AFE TDM port;
  everything past it is board-level and not in deva. **PROVEN.**
- The AFE frame for dev 78 is not a bare I2S stream: it is a **grouped-I2S TDM frame
  with custom header TYPE 2**, and Qualcomm's header-type 2 is literally
  `ENTERTAINMENT_MOST` (`AFE_TDM_MOST_SLOT_AUDIT.md` §5, from
  `csd_acdb_set_dev_info_tdm_sad` in `audio_service`). The AFE is explicitly formatting
  its output *for a MOST formatter*, i.e. an INIC. **PROVEN (header type is MOST).**
- The whole entertainment audio path is carried as a **MOST synchronous channel** with a
  Connect/BlockWidth handshake (`MOST_SOURCE_BLOCKWIDTH_AUDIT.md`): MIBRoot allocates
  `MmxSourcePresCtrl/7` = srcID 52 (0x34), BW 26; the `car` MOST NetworkManager holds
  matching NAL sync resources of BW 26. On the RCC, AudioProcess **SPC** maps MOST
  srcID 0x34 → DSP input 9 (`INTERNAL_AMP_AUDIOPROCESS_SINKS.md`). A MOST srcID→DSP-input
  map is a MOST-INIC concept, not a raw-wire concept. **PROVEN.**

Topology: **MMX AFE (ENTERTAINMENT_MOST framed) → MMX INIC → MOST ring → RCC INIC →
McASP3 → RCC DSP.** The McASP3 in `INTERNAL_AMP_RCC_DSP.md` §5 is the RCC-INIC-to-DSP
link on the RCC board, *downstream* of the MOST ring — not a wire back to the APQ8064.

### Q2 — MOST-synchronous (INIC framed) or a direct McASP/TDM wire?

**MOST-synchronous, INIC-framed. PROVEN.** The three independent facts above
(ENTERTAINMENT_MOST header type, MOST BlockWidth Connect protocol, MOST-srcID→DSP-input
SPC map, `car` NAL sync resources) all describe a MOST synchronous stream. It is **not**
a direct APQ8064→RCC McASP wire.

Consequence: the MMX side just has to (a) present 6 real PCM channels into the AFE and
(b) allocate the 26-byte synchronous source when MIBRoot requests the MC connection.
The RCC McASP3 slot count (2 vs 8) is on the far side of both INICs and is **not driven
by the MMX** — that is site 2, an RCC-DSP-side edit. The apparent "6-slot/24 vs 8-slot/32"
contradiction is therefore not a contradiction: the two frames never touch.

### Q3 — Where is the 26-byte (6ch) vs 4-byte (stereo) source width applied on the MMX, and does selecting it need anything beyond MIBRoot's choice (site 1)?

**In MIBRoot's `MmxSourcePresCtrl` source table — NOT in deva.** PROVEN
(`MOST_SOURCE_BLOCKWIDTH_AUDIT.md`):

| source # | srcID | BlockWidth | logical source |
|---:|---:|---:|---|
| 1 | 58 (0x3a) | **4** | `eSRC_ENT_AMP_DIO_MEDIA` (0x46e), stereo |
| 7 | 52 (0x34) | **26** | `eSRC_ENT_AMP_DIO_MEDIA_MC` (0x46f), 6ch |

Built at MIBRoot VA `0x00207dd4` (`0x1a`→r8 at `0x00207e0c`; source-7 record at
`0x00207ee4`). Selected purely by which logical source MIBRoot's route map picks — which
is exactly **site 1** (0x46e→0x46f). deva/CSD is never consulted for this width; the AFE
always packs six 24-bit slots into dev 78 regardless (static ACDB property `0x1319f`).

For the CarPlay input specifically (`mpl5_dio_ent`), the MMX just needs to *emit 6 real
PCM voices with a 6-active SAD declaration*. Both are **already covered by what we deploy**:
- deva `deploy/deva-ctrl-qc` widens `mpl5_dio_ent` to 6 voices;
- the hook writes `ASAD=0xfc` (6 active) via `DCMD_PCM_SET_SAD_METADATA`, overriding the
  stale stereo `0xc0` that stock AirPlay leaves in the shared MPL session
  (`AFE_TDM_MOST_SLOT_AUDIT.md` §5–7). The clean-boot deva default is already
  `ASAD=0xfc`, header-enabled, 24-bit (deva `deva_init_platform_mib` @0x57af0, stores
  0x5a734..0x5a7d0), and the mpl1 media path already ships six channels this way in a
  real vehicle slog — so 6ch emission on TDM_OUT_MPL is a proven, working MMX capability.

Nothing beyond MIBRoot's connection choice is required on the MMX to *select* the 26-byte
source.

### Q4 — Bottom line for site 5

**(i) NO new MMX binary or config patch is needed.**

- The MMX AFE `TDM_OUT_MPL` (CSD dev 78) is **statically 6-slot × 24-bit** and already
  frames for MOST (`ENTERTAINMENT_MOST` header). There is no MMX "2→8 slot" edit to make
  — the 8-slot concern in the plan lives entirely on the **RCC McASP3 INIC↔DSP link
  (site 2)**, past both INICs.
- The 6ch-vs-stereo choice on the MMX is a MOST source-width property owned by **MIBRoot
  (site 1)**, not by deva.
- CarPlay 6-channel emission is already handled by the **already-deployed** deva
  `mpl5_dio_ent` 6-voice widen + hook `ASAD=0xfc`.

**Remaining open boundary (unchanged from prior audits, INFERRED-only):** whether the
physical **INIC** (OS81xxx, MMX board HW / `car` NAL config) actually forwards all six
active MOST slots — i.e. whether it honours the 26-byte BlockWidth and the ASAD mask —
is not in any analysable HU binary. This is the same "INIC/MLB/MOST formatter"
boundary flagged in `AFE_TDM_MOST_SLOT_AUDIT.md` §8, resolvable only by a live capture,
not by an MMX patch. It is *not* a code edit we can prepare; it is validation.

---

## One-line site-5 verdict for the patch plan

> **Site 5 = NO MMX patch.** The MMX→RCC entertainment link is MOST-synchronous (INIC-framed,
> `ENTERTAINMENT_MOST` header), so the MMX AFE's static 6×24-bit `TDM_OUT_MPL` is decoupled
> from RCC McASP3's slot count; 6ch is selected by MIBRoot (site 1) and already emitted by
> our deployed deva 6-voice widen + hook `ASAD=0xfc`. The "8-slot TDM" belongs to site 2 on
> the RCC. Only live INIC-forwarding of the 6 active MOST slots remains to be *verified*, not
> patched.

## Provenance
- `deva-ctrl-qc.so` SHA-256 `58dddcabaa94c2fecf989855a6f810ec219926f49669a0ed9482ed629fdc64b9`
  (IDA session e2b2ac5e). String search: no MOST/INIC/McASP/MediaLB/SLIMBUS present.
- Cross-refs to PROVEN audits: `AFE_TDM_MOST_SLOT_AUDIT.md`,
  `MOST_SOURCE_BLOCKWIDTH_AUDIT.md`, `INTERNAL_AMP_RCC_DSP.md`,
  `INTERNAL_AMP_AUDIOPROCESS_SINKS.md`.
