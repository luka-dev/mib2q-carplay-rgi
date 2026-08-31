# The amp advertises an 8-channel sink — `Entertainment_7.1`

Found 2026-08-30 while checking whether chasing 5.1.2 over CarPlay is worth anything downstream.
Static read of the already-dumped B&O amp image; nothing was re-extracted and no amp document was
modified — amp-side RE is owned elsewhere. This file records only what bears on the CarPlay audio
project.

## What is actually there

`Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin`, V850 image, base 0.

Strings live at `0x950FC–0x95220`, each stored as `0x01` + ASCII + NUL. The leading `0x01` is the MOST
string charset tag, so these are **advertised on the bus**, not internal debug names.

A descriptor table at `0x95224` holds 18 `{u32 ptr, u32 length}` entries (length counts tag + text +
NUL). It is referenced from code at `0xC9032` and `0xC9100`.

| idx | label | idx | label |
|--:|---|--:|---|
| 0 | `Entertainment_2.0` | 9 | `Entertainment_2.0_SAD` |
| 1 | `Announcement1_Mono` | 10 | `Entertainment_5.1` |
| 2 | `Announcement2_Mono` | 11 | `Entertainment_5.1_SAD` |
| 3 | `Announcement3_Mono` | 12–15 | `-` (placeholder) |
| 4 | `Entertainment_5.1` | 16 | `Entertainment_7.1` |
| 5 | `Entertainment_5.1_SAD` | 17 | `Entertainment_7.1_SAD` |
| 6 | `Entertainment_2.0_SAD` | | |
| 7 | `Entertainment_7.1` | | |
| 8 | `Entertainment_7.1_SAD` | | |

Three entertainment families — 2.0, 5.1, **7.1** — each in a plain and a `_SAD` variant, plus three
mono announcement sinks and four `-` placeholders. Entries repeat across the table, so it is indexed
by something with more than one axis (configuration × slot is the obvious guess, unverified).

Adjacent at `0x95360` is a 49-entry list of `{u16 id, u16 = 3}` records whose ids fall in the MOST
function-id ranges — `0x000/0x001/0x011`, `0x100–0x125`, `0x202/0x203`, `0x402–0x467`,
`0xC02–0xCFF`, `0xD10–0xD29`. Consistent with an FBlock's advertised function list. Not decoded.

## Why this matters here

`HANDOFF_SPATIAL_AUDIO.md` §5 previously asserted that 5.1.2 / height is impossible on this platform
because the amplifier is a 5.1 system. That was sourced from B&O marketing material — the doc says so
itself, "Not in the firmware; sourced externally" — and `Entertainment_7.1` is in the firmware.
Firmware wins. The §5 claim has been retracted.

The practical consequence is narrower than it first looks: the amp accepting eight channels does not
make eight channels reachable, because **the blocker is on the HU side**, and all of it is ours:

- `deva_mib2_channel_map` @ `0x54ACC` switches on `{1, 2, 3, 6}` only.
- No device in deva's 31-slot capability table exceeds 6 channels.
- `deva_csd_start_csd_session_playback` @ `0x24F28` hardcodes 6 in its `voices > 2` branch.
- Output nodes are `mono_`, `stereo_` and `six_channel_speakerp`. There is **no eight-channel node** —
  and unlike the other three, that is a missing mixer-topology object, not a constant to patch.
- Buffer sizing would need recomputing again: 768 frames × 8 ch = 12288 B per transfer, against the
  0x4800 (18432 B) `max_fragsize` the 6-channel patch already installed — that one still fits, but
  `max_buffer` and the caps entry must be rechecked, not assumed.

Grepping the whole extracted MHI2Q tree finds **no** `Entertainment_*` label anywhere, so the HU does
not reference these sinks by name; selection is presumably numeric or handled inside the MOST stack.

## Follow-up: what the V850 actually decides (Ghidra, V850:LE:32, base 0)

Imported the CodeFlash image and read the two call sites that touch the descriptor table.

**`FUN_000C7694` — the MOST property that returns a sink name.** Request must carry a 1-byte
selector. It is validated against a **7-entry table at `0x96E2C`**:

```
0x96E2C:  21 22 23 24 26 2A 32        selectors  = 33 34 35 36 38 42 50
0x96E35:  02 04 06 08 08 08 00        parallel   =  2  4  6  8  8  8  0
0x96E3C:  10 10 10 00 00 00 00        parallel   = 16 16 16  0  0  0  0
```

The index is `selector - 0x21`, computed at `0x0C7746` (`addi -0x21,r10,r10`). An unlisted selector
takes the error path (`0x00099EBC`, "parameter wrong"); a listed one returns the string.

The parallel row at `0x96E35` reads as a **channel count per selector: 2, 4, 6, 8, 8, 8** — three
distinct eight-channel configurations. This is independent corroboration of the `Entertainment_7.1`
labels: the amp does not merely carry the string, it carries per-mode channel counts that include 8.
Similar byte runs with `02` / `06 06` / `08 08` / `10` / `18` continue to `0x96E9x`, unparsed.

**`FUN_000C8F0C`** — a second property, sub-index `0..11`, three bytes per entry, base `0x952B1`,
with index 0 returning a 30-byte blob from `0x952B4`. Twelve entries is suggestive next to the twelve
coefficient groups in `DAT_00097028`, but the records did not resolve into anything readable and are
recorded here only as a pointer for whoever picks this up.

**FktID tables.** A `{u32 ptr, u32 id}` table at `0x96DE8` carries MOST function ids `0x0A00`,
`0x0A01`, `0x0A02`, `0x0A03`, `0x0A10`, terminated `0xFFFF`. Separately, the 49-entry
`{u16 id, u16 = 3}` list at `0x95360` spans `0x000/001/011`, `0x100-0x125`, `0x202/203`,
`0x402-0x467`, `0xC02-0xCFF`, `0xD10-0xD29` — the FBlock's advertised function list.

## The routing matrix is not in this dump — stop looking for it here

The channel-to-speaker assignment is **not answerable from `CodeFlash_*.bin`**, and no further static
work on that file will produce it. The amp document's §4a already establishes why: there is **no
SHARC code in the dump**. The V850 is the control MCU — it selects a mode, gates per-channel gain and
enable, and pushes a configuration; the actual mixing/routing executes on the ADSP-2148x from the
`AD301A07` download stream, and all we have of A07 is `param_addr -> final_value` with no parameter
semantics (`AMP_DSP_A07_cells.csv`). The V850 tables that *are* decoded — `DAT_00097028`
coefficient-block indices, `DAT_000970B0` reorder, per-channel gain/limit/enable — are all
**output-side**: they describe what happens to a speaker channel, never which source channel feeds it.

So the gating question has exactly two honest routes, and neither is more static analysis of this
binary:

1. **Bench measurement.** Put the amp in `Entertainment_7.1`, drive source channel 7 alone with a
   tone, and find which driver moves. One afternoon, unambiguous, and it also settles the `_SAD`
   variants. This is the cheap answer.
2. **Obtain the SHARC image or an A07 parameter map.** Both halves of this were tried; see below.
   Neither survives, and the surviving half needs hardware access anyway.

### Route 2 was followed and it dead-ends

**2a — the A07 parameter map is tuning, not routing.** The stream at `0x100038` parses as records
carrying a small id, an audio-band frequency and a signed gain. The first records read 987 Hz/-10,
7000/+20, 2000/-10, 250/+5, 135/+20, 1170/-20, 64/+10, 8000/-2 — roughly 69% of the parsed fields sit
in 20..20000 Hz and -30..+30 dB, so the stream is not uniform 8-byte records, but what is legible is
unambiguous EQ/filter *design* data of the same nature as the B01 catalog. There is no matrix in it:
a routing matrix would show as a block of near-0.0/1.0 cells, and a scan of
`AMP_DSP_A07_cells.csv` finds no contiguous run of 16 or more parameters restricted to {0.0, 1.0}
anywhere. Decoding A07 further yields better EQ knowledge and nothing about channel assignment.

**2b — the SHARC firmware is not in this flash at all.** Occupancy of the 1.5 MB image, counting
bytes that are neither 0x00 nor 0xFF:

```
000000-0EFFFF   V850 code, dense          (000000-01FFFF is mirrored at 020000-03FFFF)
0F0000-0FFFFF   blank except the AD301FLASH0 footer at 0FFFF0
100000-10FFFF   AD301A07   27389 bytes
110000-11FFFF   blank (16 bytes)
120000-127FF0   AD301B01    4969 bytes
130000-17FFFF   blank, entirely 0xFF
```

320 KB at the top plus two more blank windows, and no 48-bit SHARC instruction image anywhere —
confirming the amp document's §4a from the layout side rather than by search. The ADSP-2148x
therefore boots from its own memory, not from the V850, so its code lives on a **separate device on
the amplifier PCB**. The only other dump we hold is `S-25C640A@TMSOP8.BIN`, an 8 KB EEPROM — three
orders of magnitude too small to be SHARC firmware, and already accounted for as the tuning dataset.

Recovering it is therefore a hardware job: identify and read the SHARC's boot flash off the board.
That is strictly more work than route 1, and route 1 answers the same question.

## The one question that gates everything

**In `Entertainment_7.1`, where do source channels 7 and 8 physically land?**

7.1 and 5.1.2 both carry eight channels but mean different things: 7.1 puts channels 7/8 at
side/rear surround positions, 5.1.2 puts them overhead. The B9 does have two dedicated 3D/height
drivers (`AMP_AUDIO_DSP_TUNING.md` §7 lists "two dash/A-pillar height speakers (the Symphoria 3D
layer)"), so a favourable routing is plausible — but if channels 7/8 feed rear surrounds instead,
sending a height layer there puts overhead content in the wrong drivers and is worse than an honest
downmix.

That answer lives in the DSP routing, specifically the per-channel coefficient-block table
`DAT_00097028` — whose `0xF0`/`0xFF` entries the amp doc already reads as "special/unused slots for
smaller speaker configs", i.e. the routing is config-dependent, which is what we would expect if
multiple `Entertainment_*` modes exist. It is open item 4 in that document's §8. It is not ours.

## What this does and does not change for the CarPlay plan

Does not change the build order. 5.1 remains the target and the milestone: six channels are already
proven end to end (device 1 was observed live at `48000 / 6ch / 24-bit` in stock, channel order is
already Apple-canonical `L R C LFE Ls Rs`, and the amp has `Entertainment_5.1`). The type-103
receiver is the work; the amp is not the constraint.

Does change one argument. "Decoding eight channels is pointless because the amp is 5.1 anyway" is no
longer a valid reason to prefer APAC index 65 over AAC-LC index 40. Index 40 stays the right target
on decodability grounds and now also on headroom grounds.

Implementation note that keeps the option open at near-zero cost: when index 40 lands, keep the
decoded **PCM8 stage as its own buffer** and make the 8→6 fold the last step, rather than folding in
place. Then a future 8-channel path is a routing choice instead of a rewrite, with the working 5.1
fold one flag away as fallback.
