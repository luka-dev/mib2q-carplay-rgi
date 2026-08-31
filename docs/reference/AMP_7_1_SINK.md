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
