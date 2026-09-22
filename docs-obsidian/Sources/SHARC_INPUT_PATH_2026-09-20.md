# J525 input controls and C4A54 delay — 2026-09-20

> [Sep20 matrix correction](SHARC_MATRIX_CONTROL_2026-09-20.md): common-input
> findings stand, but old v5/v6 downstream music/bass figures below used a transposed
> A07 matrix. [Corrected v8 music figures](UPMIX_MATRIX_RERENDER_2026-09-20.md)
> now supersede the historical table below. Current405 tests; no car-state inference.

Latest static-only follow-up: [level control provenance](SHARC_LEVEL_CONTROL_2026-09-20.md)
traces gp−77F8 to a smoothed source-level request, including normal/override
sources, trim writers, override-mode entry, mute gates and selector commit.
First-B API25 uses the unsmoothed selected target, with deferred SHARC apply;
its three linear-gain curves are zero in loader/B01 (not a B=0 claim).
All9 branch-gain slots now bound; B01 has nonzero curves for18/19/22/27.
API18/19 scalar destinations, delay selectors and zero-gain EQ rebuilds now bound;
API17 four-bank replacement/replay also bound, distinct from matrix bank0..2.
API16/20 effective-config pointer selection now preserves apply-order dependence.
Post crossfeed1161 is bound; four pointer consumers remain unresolved.
Extended FIFO canonical indexed writes/source-RAM mutation are now bound as
effect plans; [generic outer A21 forwarding and API18's dedicated sender](SHARC_OUTER_CONTROL_2026-09-20.md)
are now bound, not actual request delivery. API18 source zero→0; its additional
coefficient paths and UI name remain open.392 tests pass. This supersedes the
earlier unknown-level-source and live-readback next steps below; only local
firmware/offline work is authorized.

Static writer bindings and a **settled-state** input reference are now available.
These are not the car's active RAM values. No vehicle access, firmware edits,
production-hook changes or default AmpChain changes were made.

## Main finding for the bass investigation

C4AB7 is a level-selected,16-bank EQ **before the A/B split**. Its three peak
templates are55Hz (q8),10kHz (q7),4.5kHz (q8). Bank15 requests +20.5/0/+8dB;
bank7 requests0/0/0. This control is separate from the Bass tone position and
the host upmixer's LFE gain. **Bass0/LFE0 does not establish a flat input path.**

The graph proves this is not restricted to B: C4AED's A07 matrix receives
processed ports24,23,22,21,50,51,36,37; C4AD9 B also receives24,23,22.
Therefore setting B=0 in a scenario does not eliminate this upstream EQ.

Explicit settled examples (tone gains all0, delay0, calibrated tables):

| Effective EQ bank | 30Hz | 55Hz | 120Hz |
|---:|---:|---:|---:|
| 0 | approximately0dB | −0.001dB | −0.006dB |
| 7 | 0dB | 0dB | 0dB |
| 8 | +1.085dB | +2.000dB | +0.799dB |
| 10 | +3.885dB | +6.001dB | +3.073dB |
| 13 | +11.039dB | +14.003dB | +9.705dB |
| 15 | +17.400dB | +20.505dB | +15.971dB |

These are **electronic input transfer functions**, not driver responses or
measured bass gain in the cabin. No effective vehicle bank was read. The finding
is a concrete candidate mechanism, not an established diagnosis.

## Writer bindings

`tools/audit_amp_input_path.py` exports both calibrated profile scenarios into
`remaining-blocks-20260919/audit.json` under `input_path_profiles`.

| Block | Parameter source / writer | Meaning |
|---|---|---|
| C4A54 | A9D80→E000C | same requested delay for8 lanes |
| C4A6C | AB6A6→table68A70→E0038 | one shared biquad, fixed-calibration record gp−1285C; type0 in this dump |
| C4A7B | AC0E2→C0090 from68B90 | eight target gains1; current/ramp state is not read back |
| C4AA6 | A8F36 or A9BDE→E0068 | five shared sections, tone or graphic family; current E004E is smoothed |
| C4AB7 | A8904→C016A/C015A | sixteen banks of three sections, separate bank gain |
| C4AB7 selectors | A8B26→E0082 | eight copies of the effective selector |

Fixed calibration comes from the B64E0 copy102438→gp−1330C. The dump passes
its signature check; the fallback8E53C is a different scenario and is not
silently substituted. Profile data comes from C23C0's100034+profile*370 copy.
Profile index is not the MOST matrix-bank number.

### Shared tone/graphic EQ

A76C2 dispatches events0..4 to tone writerA8F36 when gp−3E42==A5, otherwise
to graphic writerA9BDE.

Tone sections0..2 use templates at gp−129B0:125Hz/type26,650Hz/type1,
6kHz/type27. Each gain is the corresponding19-entry table (gp−12B64,
index9+signed tone position gp−3E28) plus signed addition gp−3E25.
Neutral position/addition gives0dB. Section3 uses profile template gp−12A34
(90Hz/type26 here) and signed gain gp−3E22. Section4 is bypass (type byte0).

Graphic templates at gp−12944 are63/250/1000/4000/16000Hz type1. A9BDE
clears the design gain if gp−3DDF==5A. Both families address the same targets;
delivery order and interpolation state matter during changes.

Type26 low-shelf math was added to `design_to_sos`, from AB850:
gain G>=1 uses b=[K+G,G−K,0],a=[K+1,1−K,0]; otherwise
b=[K+1,1−K,0],a=[K+1/G,1/G−K,0]. All divide by a0. This expands the
designer, but does not change existing default output-EQ scenarios.

### Bank construction and selector

Templates are gp−1291C (CodeFlash102E28), with16×3 signed tenths-dB values at
gp−12904 (102E40). A8904 builds every bank. Each section stores
`[-a2/a0,-a1/a0,b2/b0,b1/b0]`; bank gain is the product of the three `b0/a0`
values. The12618C kernel's implicit b0=1 is therefore intentional, not a
missing normalization. Loader zeros at C015A/C016A are not usable playback EQ.

B0066 derives a candidate selector from signed32 gp−77F8 divided by2^24,
truncated toward zero. Thresholds at gp−128A4 (102EA0):

```text
12,9,6,3,-3,-10,-12,-16,-22,-28,-33,-38,-44,-50,-60,-60
```

Select the first threshold<=converted level, saturating to15. Examples:0→4,
−20→8,−30→10,−50→13,−60→14,below−60→15. This is not a mapping from MMI
volume percentages. The follow-up above identifies the word as a smoothed
source-level control; actual external values and event ordering remain conditional.

The commit gate is gp−7844==A5. The enable value comes from gp−456C if
gp−4573==A5, otherwise gp−3C2E. Changed enable/index values are stored to
gp−3E2A/gp−3E29 and event17 is queued. A8B26 sends index to all8 selectors
only when gp−3E2A==A5; otherwise it sends0. **Disabled means bank0, not flat
bank7** (bank0 cuts10kHz by3.5dB and4.5kHz by3dB). Startup index is0.

## C4A54: writer, state machine and DMA

Event19 calls A9D80. The request record is gp−3DDC+4*u8(gp−3D94): byte0
enable and uint16 at+2 milliseconds. Startup clears both fields; message
writers C9F28/CA0A6 may change them. The writer calculates per-output alignment
candidates from calibration mappings and signed delay tables, preserving its
uint16 wrap and index8/9 accesses (do not silently clamp them to0..7).
The minimum, with initial cap2880, is1719 samples in both audited profiles.

```text
request_samples = enabled ? 48*request_ms : 0
delay = min(9600, max(0, request_samples − (1719+1248)))
write [delay]*8 to E000C
```

Thus100ms requests1833samples,62ms requests9,61ms requests0. This is alignment
compensation, not a hidden bass amplifier. Current delays E0004 can lag targets.
Descriptor13 capacity is76800 (=8*9600); ring length descriptor12 is76864.

The recovered state machine at124517 uses dispatch B93D4:

- State0 allocates8*frames scratch, advances to1 or fails to−1.
- State1 clears scratch/external ring, queues clear DMA and advances to2.
- State2 clears outputs while waiting. Callback1244DF expects2 and sets3.
- State3 copies target delays to current, validates nonnegative values and
  total capacity. Changed valid delays reinitialize; unchanged values process.
- Audio DMA is pending in state4; callback1244FB expects4 and restores3.
  Entering the processing scheduler before completion reaches invalid-state
  fallback: set−1 and copy corresponding inputs, not permanent mute.
- Global B93DD must equal2. Without that, descriptor state0 becomes−1 and
  copies once; other nonzero states leave output memory unchanged.

Steady audio is channel-independent integer delay. For d=0,1257B2 copies input
directly. For0<d<N, the CPU concatenates the last d old scratch samples and
the first N−d current input samples. For d>=N, it uses the fetched scratch block.
It then copies current input into scratch for a13-word delay-line DMA TCB:
write N to the external ring, then read N at the **advanced** write index plus
tap `−max(d,N)` into scratch for the next callback.

Critical delayed-branch detail:12477A's GE branch executes12477F `MAX(d,0)`
in its delay slot before12478E negates d. Ignoring that slot would invent a
zero tap and a wrong long-delay model. The13-word TCB and write-then-read
semantics match local ADI Hardware Reference1.1 §4-138..141 and §3 TCB layout.

An independent test simulates the shared76864-word ring, DMA writes/reads,
per-channel offsets and scratch-prefix handling. It matches sample delays
for0/1/17/63/64/65/127/128/129/1833/9600, including ring wrap. Startup clears,
DMA timing/failures and control transitions remain outside the settled renderer.

## Model and validation

`BInputPathFixedReference` in `amp_b_upstream.py` composes delay→prefilter→
input gain→shared EQ→bank EQ. All choices are explicit: calibrated profile,
tone/graphic family and effective gains, effective bank, settled delay. It
accepts graph0..7 PCM (8 columns), not an assumed raw MOST6 mapping. Output
columns0..2 are the B boundary lanes; all8 also feed the A07 matrix.

32 static response scenarios (2 profiles×16 banks) are reproducible with:

```sh
python3 tools/soundamp-virtual-cabin/audit_amp_input_scenarios.py \
  assets/soundamp/remaining-blocks-20260919/audit.json \
  --output assets/soundamp/remaining-blocks-20260919/input_scenarios.json
```

277 tests pass:81 extraction/decoder and196 model/report.127 bounded SHARC
ranges have UNKNOWN0. Tests cover coefficient packing against independent SOS,
all-channel isolation, shelf branches, selector thresholds, delay DMA versus
sample delay, streaming and frequency response versus a long impulse.

CodeFlash: V850 LE32 raw base0,size180000h,
SHA256 `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
SHARC loader SHA256 `9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
V850 addresses are byte offsets; SHARC PCs are16-bit shortword addresses.
Listings: `input_path_writers.asm`, `input_delay_*.asm` in the audit output folder.

## Common input integration and music smoke test

AmpChain now accepts `upstream_scenario`, which supplies all of:

```python
upstream = {
    "audit_path": "assets/soundamp/remaining-blocks-20260919/audit.json",
    "graph_input_sources": [0, 1, 2, 3, 4, 5, None, None],
    "eq_family": "tone",
    "shared_gains_tenths_db": [0, 0, 0, 0],
    "bank_index": 7,
    "delay_samples": 0,
}
chain = AmpChain(upstream_scenario=upstream)
```

This mapping is an **explicit hypothesis**, not recovered physical MOST wiring.
Each of8 graph inputs selects a PCM6 index or None (silence). All8 processed
lanes feed the full A07 matrix. The first3 also feed optional mode5 B:

```python
# mode5_reference must be a BMode5FixedReference constructed with explicit
# spectral/tail/finish parameters, histories and input_mix_gain.
# extra_wrapper_delay_samples must be supplied; no live default is inferred.
chain = AmpChain(upstream_scenario=upstream, first_b_scenario={
    "reference": mode5_reference,
    "wrapper_delay_samples": extra_wrapper_delay_samples,
    "label": "description-of-the-supplied-parameter-snapshot",
})
```

The wrapper delay is ADDITIONAL to delays inside the B reference. B's8 outputs
feed first-mixer slots0,1,4,5,6,7,8,9; center/LFE slots2/3 are zero. Mixer gains
apply once. Injected boundary `first_b` bypasses the input filter and cannot be
combined with internal first-B rendering. Each offline call restores the given
B snapshot and resets other filters/delays; partial512-sample blocks are padded
and trimmed, with no delayed-tail flush. Metadata records controls, mapping,
parameter/history hashes and unverified-state warnings. Nonlinear B scenarios
must render PCM; a level-independent full frequency response is rejected.

The default remains `bank1-known-electronics-v5`; common-input opt-in reports
`bank1-electronics-v6-explicit-common-input`. CLI `amp_music_report.py` accepts
`--upstream-scenario FILE.json`; its first B still defaults to zero. JSON audit
paths resolve relative to the scenario file. No calibrated full-B preset is
silently synthesized from loader zeros or synthetic test fixtures.

Two existing authored PCM6 clips were rendered for12s and measured over2..10s.
Controls: tone gains0, input delay0, mapping above, bank10 versus flat bank7;
B1/B2=0, C4D17 identity, no additional runtime gain/limiter scenario. The source
files were not rerendered with a new hook and are not current vehicle captures.
Historical v6 candidate driver-group power changes (dB); superseded by the
[v8 rerender](UPMIX_MATRIX_RERENDER_2026-09-20.md#common-input-scenarios-after-correction):

| Clip | Front woofers20–60Hz | Sub20–60Hz | Front woofers60–120Hz | Sub60–120Hz |
|---|---:|---:|---:|---:|
| Boost Up | +5.980 | +5.938 | +5.251 | +5.663 |
| Crazy Train | +5.953 | +5.869 | +4.850 | +5.695 |

These are electronic scenario differences, not acoustic SPL, a stock-versus-
upmix comparison, evidence of active bank10, or proof of the reported bass cause.
`bands_re_dry_L` in this particular report references authored PCM6 channel0,
not the unavailable original stereo dry signal; scenario deltas use group power.
Source hashes, settings and complete results are saved in
`assets/soundamp/remaining-blocks-20260919/common-input-music-smoke.json`.
Reproduce with existing source files:

```sh
python3 tools/soundamp-virtual-cabin/audit_amp_common_input_music.py \
  --scenario assets/soundamp/remaining-blocks-20260919/common-input-bank7-hypothesis.json \
  --scenario assets/soundamp/remaining-blocks-20260919/common-input-bank10-hypothesis.json \
  --out assets/soundamp/remaining-blocks-20260919/common-input-music-smoke.json \
  '/tmp/upmix-bank1-audit.BR9uxY/Boost Up-gain4096_fb3.wav' \
  '/tmp/upmix-bank1-audit.BR9uxY/Crazy Train-gain4096_fb3.wav'
```

Current regression:287 tests pass (81 extraction +206 model). Ten added
integration tests cover common A/B routing, correct mixer slots/weights,
independent composition, reset/copy, short blocks, mapping, response/delay and
rejection of conflicting scenarios. They use explicitly synthetic B parameters,
not evidence of the real B spectrum or lifecycle.

Next: establish effective level/flags/tone/request values through a verified
external-J525 readback transport; bind graph0..7 physically; obtain effective B
parameters and wrapper timing. Local tool review found no verified transport;
RCC /dev/io-amp is the internal TI DSP, not J525 SHARC. No new vehicle contact,
diagnostic payload, deployment or settings change was made. Do not replace
defaults with assumed bank15 or claim bank selection caused the reported bass.
