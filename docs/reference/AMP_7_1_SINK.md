# The amp's 8-channel sink is rear 7.1, not 5.1.2 — settled

Short note. The authoritative write-up is
`Firmwares/SoundAmp/AMP_ENTERTAINMENT_7_1_MOST.md` (amp-side RE, owned elsewhere); this file keeps
only what bears on the CarPlay audio project, plus corrections to what we got wrong here on the way.

## The answer

`SinkInfo.Get(0x32)` on the amplifier's FBlock `0x22`, FktID `0x0111`, returns:

```text
32          SinkNr                      = Entertainment_7.1_SAD
00 1A       BlockWidth                  = 26  ( = 2 DTCP + 8 ch * 24 bit / 8 )
00          TransmissionClass           = synchronous
02          ContentProtection           = MOST-DTCP
10          ContentType                 = GenericPCM
08          AudioChannels               = 8
01 18 02 18 03 18 04 18 05 18 06 18 13 18 14 18     (AudioChannelName, bits) x 8
```

Stream positions 7 and 8 carry `AudioChannelName` **`0x13` / `0x14` = Back Left / Right of Center**.
Height would be `0x0D` / `0x0F` (Top Front Left / Right) in the MOST 3.0 GeneralFBlock catalog, and
neither code appears in **any** of the 18 sink profiles at `0x96E99`. The EEPROM carries no
alternative profile either.

**So discrete 5.1.2 over `Entertainment_7.1` is impossible, and the bus contract alone settles it —
no bench tone needed.** Putting a height layer in positions 7/8 would hand height samples to inputs
declared as rear surround. The amp may still synthesize height for its 3D drivers, but that is
Symphoria upmixing, not transport of a discrete height layer.

## Selectable sinks, for reference

The seven selectable `SinkNr` values are `0x21 0x22 0x23 0x24 0x26 0x2A 0x32`, addressing sparse
indices 0,1,2,3,5,9,17 of the 18-entry profile tables. Note the plain `Entertainment_5.1` and
`Entertainment_7.1` rows are **not** selectable — only the `_SAD` variants are:

| SinkNr | profile | BlockWidth | protection | format |
|---:|---|---:|---:|---|
| `0x21` | `Entertainment_2.0` | 4 | none | 2 x 16-bit |
| `0x22`–`0x24` | `Announcement1..3_Mono` | 2 | none | 1 x 16-bit |
| `0x26` | `Entertainment_5.1_SAD` | 14 | MOST-DTCP | **6 x 16-bit**, IDs 1..6 |
| `0x2A` | `Entertainment_2.0_SAD` | 8 | MOST-DTCP | 2 x 24-bit |
| `0x32` | `Entertainment_7.1_SAD` | 26 | MOST-DTCP | 8 x 24-bit, IDs 1..6,19,20 |

Worth carrying into the 5.1 work: the only selectable six-channel sink is `0x26`, **16-bit**, and it
is DTCP-protected. Our target `L R C LFE Ls Rs` matches IDs 1..6 exactly, so the layout is right; the
sample width and the protected path are the parts to confirm when the type-103 audio actually reaches
DIO5.

## What we got wrong here, corrected

Recorded so the errors do not get re-derived from this file's history:

- The byte runs I read at `0x96E35` / `0x96E3C` as "per-mode channel counts 2, 4, 6, 8, 8, 8" are
  **misaligned views** that cross array boundaries. The real per-profile arrays are `0x96E3F`
  TransmissionClass, `0x96E51` ContentProtection, `0x96E63` ContentType, `0x96E75` AudioChannels,
  `0x96E87` bits-per-sample, `0x96E99` the 18 x 8 channel-name rows, read by `FUN_000C7424` under
  FktID `0x0111`. The arrays that code actually references near `0x96E33`/`37`/`3B` are four-entry
  and clamped to indices 0..3, i.e. the basic and announcement sinks only. The conclusion "the amp
  carries per-mode counts including 8" happened to be true, but not from those bytes.
- `FUN_000C7694` (FktID `0x0116`) computing `selector - 0x21` into `r6` and then never using it is
  **real compiled behaviour**, not lost Ghidra analysis: the handler echoes any valid selector and
  then always returns descriptor 0, `Entertainment_2.0`. My hedge about a missing index expression
  was wrong.
- `FUN_000C8F0C` is FktID `0x0D1A`. Its twelve subindices have no relationship to the twelve
  coefficient groups in `DAT_00097028`; I flagged that resemblance as suggestive and it is
  coincidence.
- The table at `0x96DE8` is a function-dispatch table into MOST network-service code, not audio
  profile data.

Two earlier findings stand: A07 is EQ/filter design data with no routing matrix in it, and no SHARC
image exists anywhere in the 1.5 MB CodeFlash dump (blank at `0x0F0000-0x0FFFFF`,
`0x110000-0x11FFFF` and all of `0x130000-0x17FFFF`; `0x000000-0x01FFFF` is mirrored at `0x020000`).
The amp note adds that the selected profile index at `gp-3592` is never read by the DSP service code,
so no register-write delta between 5.1 and 7.1 can be derived from the V850 at all.

## Update: the branch reopens conditionally (2026-08-31)

The amp note now carries a patch plan — `AMP_ENTERTAINMENT_7_1_MOST.md`, "Patch plan: replace
selectable `0x32` with 5.1.2". It repurposes the apparently unused `Entertainment_7.1_SAD` profile
rather than adding a sink, and the V850 half is an exact two-byte delta in profile row 17 at
`0x96F21`:

```
0x96F27:  13 -> 0D    BCL -> Top Front Left
0x96F28:  14 -> 0F    BCR -> Top Front Right
```

Transport is untouched — still protected GenericPCM, 8 x 24-bit, BlockWidth `0x001A`, same Connect
message. So **"5.1.2 is not transportable" was true of the stock amp, not of a patched one**, and my
line below claiming the branch was closed on evidence is retracted: it is closed on *cost*, and the
cost is now itemised rather than unknown.

Two things from this side sharpen that cost.

**The two bytes are inside the CRC-covered region.** `AMP_AUDIO_DSP_TUNING.md` §10a establishes that
`AD301FLASH0` block-integrity footers cover `0x0-0xFFFFF`, the V850 code, and that the DSP region
`0x100000+` has none. `0x96F27` sits inside the covered range, so this is not a free two-byte poke:
it needs the main-flash integrity value resolved on the MCU that also holds component protection.
The amp plan already lists this as step 2; noting the boundary explicitly because "two bytes" reads
cheaper than it is.

**The HU has no MOST audio stack in its filesystem.** Grepping the whole extracted MHI2Q tree for
`NetServices`, `Mns`, `MostNs`, `AudioAmplifier`, `SyncSink`, `Allocate`, `ConnectionLabel`, `SinkNr`,
`BlockWidth` and `FBlock` returns **zero files for every one of them**, alongside the earlier zero
hits for `Entertainment_*`. Absence of strings is not proof — the code could be numeric, or live in a
companion controller outside this image — but ten independent MOST terms all missing is strong. The
Connect that would select `SinkNr 0x32` is therefore not issued by anything we can see, let alone
patch, which matters for the amp plan's "check whether stock ever connects 0x32" step and for any
notion of us issuing that Connect ourselves.

**And the payoff is narrower than the plan's framing suggests.** Its SHARC step 6 — bypass the
synthesized Symphoria height for this profile so it is not summed with discrete TFL/TFR — is the tell:
those height speakers already play today, driven by Symphoria from the bed. The delta being bought is
**discrete height versus synthesized height**, not height versus none.

## Consequence for the CarPlay plan

Unchanged: 5.1 is the target. The full 5.1.2 chain is seven serial steps — working type-103 receiver,
a decodable 5.1.2 format, deva 6->8 including a mixer-topology node that does not exist, an 8-channel
MOST allocation from a stack we cannot find, the V850 two bytes plus CRC, a chip-off read and matrix
patch of the SHARC boot flash, and the Symphoria bypass. Four of those are open research or hardware
access. The 5.1 path needs only the first, which is the actual project and is no longer blocked now
that the five-second teardown is fixed.

So the amp plan is well-scoped and worth keeping, and it should sit behind working 5.1 audio.
5.1.2 content still gets folded to 5.1 in the meantime.

The implementation note stands anyway and costs nothing: when index 40 (AAC-LC 5.1.2) decodes, keep
the PCM8 stage in its own buffer and make the 8->6 fold the last step. Not for a future 8-channel
path — that is now dead — but because an in-place fold makes the downmix coefficients impossible to
revisit.
