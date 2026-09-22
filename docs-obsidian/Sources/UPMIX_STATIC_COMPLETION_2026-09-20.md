# Static full-path model: additional closure and explicit limits

STATIC/OFFLINE ONLY. No vehicle connection, capture, deployment, production
source/default change or commit. This extends the retained-B model; it does
**not** certify that all static unknowns or every dynamic state are resolved.

## New source-derived behavior

1. [Library math](SHARC_B_LIBRARY_MATH_2026-09-20.md): actual124393/12D80B
   log10 and1257E6 pow10 table/polynomial paths replace host log/pow in an
   explicit numerical scenario. The latter uses anchor/correction tables,
   sixteenth splits and a reconstruction polynomial, not a substituted exp.
   FIX rounding is explicitly `nearest` or `floor`; neither is inferred live.
2. `amp_b_fft.py`: the internal512-complex transform uses the stored E4754
   table with **complex-index stride2**, as passed by the1024-real wrapper.
   DIF stages double table stride; the final exact radix4 does not read a
   twiddle table. Both forward and inverse real wrappers use this unscaled
   forward complex transform. No normalization or added listening gain.
3. [Publication](SHARC_B_PUBLICATION_2026-09-20.md): retained bank copy,
   descriptor/control replay, full829-word target, prior current/step state,
   raw publication flag and explicit coefficient advances can be composed.
   Exactly31 initial entries interpolate; flag **equal to1**, not any odd
   number, copies the whole target. A target bank alone is not current state.
4. [Mode5 access schedule](SHARC_B_MODE5_SCHEDULE_2026-09-20.md): one successful
   fixed-mode5 invocation processes512 samples and performs313 coefficient
   update calls in the retained bank1 path. This includes the below-base
   object97 bypass. Each of the31 initially interpolated fields is visited
   once; EQ coefficient visitation is reversed within each five-word section.
5. [Pointer/filter ownership](SHARC_B_FILTER_ALIAS_DOMAINS_2026-09-20.md):
   decoded allocation and traversal checks distinguish filter-owned arrays
   from B-object pointer cells. Retained section counts are checked against
   actual allocation capacities. This narrows alias uncertainty; it is not
   a whole-program proof that1162..1165 have no consumers.
   A final [spectral-pointer provenance review](SHARC_B_SPECTRAL_POINTER_DOMAINS_2026-09-20.md)
   conditionally excludes three more reads: the two allocated history-pointer
   cells and the ramp's preceding output sample inside its separate workspace.
   Nested-allpass ring provenance and whole-program alias closure remain open.

The complex FFT is a float64 algorithmic reference, not a SIMD instruction
interpreter. ROM RECIPS/RSQRTS seeds,40-bit arithmetic/flags, intermediate stores
and exceptional math behavior are not reproduced. Log/pow have bounded domains
and fail closed outside them; they must not silently fall back to host math.

## How to select the new numerical scenario

```python
make_test_scenario(
    input_eq_bank=7,                 # explicit hypothesis, NOT MOST BANK1
    first_b_spectral='retained_calibration',
    first_b_numerics='firmware_library_nearest',
)
```

`firmware_library_floor` is the separate rounding hypothesis. Existing
`host_math` and old neutral/retained scenarios preserve their numerical choices.
Factory metadata version is now `explicit-e2e-diagnostic-v3`.

The existing whole-upmix C→PCM6→DSP runner accepts these additional names for
each ROUNDING=`nearest` or `floor`:

| Scenario suffix after `full_static_ROUNDING_` | Independent aux probes | C4D17 adaptation |
|---|---|---|
| `zero_aux` | zero |0|
| `aux_fixed` | tones |0|
| `zero_aux_adaptive` | zero |1|
| `aux_probe` | tones |1|

For example use `--scenario full_static_nearest_zero_aux` with the arguments
in [the retained music recipe](UPMIX_RETAINED_B_2026-09-20.md#run-it). The prior
76 musical rows remain historical retained/host-math results; they are **not**
silently relabelled as this new numerical version.

## Independent offline state grid

```sh
OPENBLAS_NUM_THREADS=1 python3 tools/soundamp-virtual-cabin/audit_amp_retained_state_grid.py \
  --frames 4096 --jobs 4 --out output/upmix-static-state-grid-20260920
```

This declares512 rows:16 input-EQ selectors ×2 pointer maps ×2 FIX hypotheses
×2 adaptation states ×2 complex-aux states ×2 second-B-aux states. Each row
starts from fresh declared state. Six music-input tones and independent auxiliary
tones run for3072 frames, followed by1024 zero frames (4096 total at48kHz).
This is a short deterministic graph/state test, not another full-track musical
test, a measured cabin, an exhaustive history space or a worst-case bound.

The report records per-stage float64 hashes/peaks/RMS, sources and scenario
bindings. It checks auxiliary/adaptation changes do not leak upstream, checks
second-B independence, and at both extreme axis combinations compares separate
boundary injection and exact reset replay. Each context also compares the
zero-aux/adaptation0 output with retained host math. Context metadata is the
base adaptive0 template; each row explicitly records its adaptation override.

Completed artifact: `output/upmix-static-state-grid-20260920/report.json`,
**512/512 rows,64 contexts**, SHA256:
`7a458caa8c741962dbd9e950fb37e6df7366e8b199b9a4cd22e18ff7f3df8887`.
All68 unique referenced file hashes and exact six-axis coverage were checked.
Every context passes the independence checks and two extreme boundary/reset
replays (128 such replay pairs). The context with the largest observed numerical
difference was rerun afresh: all eight rows and replay checks match exactly.

On these low-amplitude synthetic signals, the largest zero-aux/adaptation0
output difference from retained host math is `1.2520329217835524e-9` linear
(bank15, pointer20:3, nearest); its RMS difference is`5.827636577172746e-11`.
The largest final-output sample across all512 rows is`0.029727865207672066`.
Neither observation bounds music peaks, sound quality or hardware error.

Full Python validation: **704 tests (583 model/cabin/runner +121 extraction)**.
Existing C S16/float sanitizer and NEON binaries were rerun successfully. No new
target build or vehicle execution is implied.

A dispatcher regression also verifies the whole successful mode5 route:
`131591→1315D3` prepares the third input, then `131624→131594` rejoins the
spectral call at`1315B5→12E80E`. The initial branch must not be mistaken for
a spectral bypass by stopping disassembly before its backward continuation.

## Fresh musical integration check

`output/upmix-e2e-static-math-20260920/report.json` is complete **4/4**:
Deep Down Low,6-second prefix +0.25s tail, input-EQ hypothesis10, dry BANK1
and float retention75/surround0.30/front-bass−1/LFE0, each through retained
host math and `full_static_nearest_zero_aux`. Report SHA256:
`49e54a0be40bdfacd3d5335a2ecd8e0b3d958bd3f09e76fc4e04ae9c2f599ac4`.

All83 unique referenced file fingerprints were checked. All four saved16ch
FLOAT32 WAV hashes were verified, and every stored metric was recomputed
exactly. PCM6,16-output array hashes and metrics for the old retained mode match
the previous retained-aux report exactly, confirming unchanged old numerical
behavior rather than relabelling historical results.

New-versus-old FLOAT32 maximum absolute sample difference is
`1.1920928955078125e-7` for both dry and candidate; RMS differences are
`4.194467995329864e-9` and`3.7082539201277237e-9`. Sample peaks remain
**+5.871566869 /+4.139370052 dBFS** respectively. This tiny observed numerical
change does not remove the electrical headroom warning, establish a hardware
clipping verdict or justify changing the production bass/LFE setting.

## Remaining work that must not be disguised as completed

- **Static alias closure:** consumers1162..1165 remain unresolved. Partial
  heap/count proofs do not prove acoustic neutrality, dead fields, or complete
  absence of other incoming pointers/callers.
- **Dynamic audio transitions:** publication/access scheduling now advances
  coefficient state. Rendering a changing delay/filter/mode while preserving
  the exact audio histories and per-consumer access timing is not implemented
  merely by applying its end-of-frame snapshot to the fixed renderer.
- **Instruction precision:** the numerical limitations above remain. There
  is no hardware bit-exact or exceptional/failed-allocation execution claim.
- **Externally chosen state:** active bank, input selectors, runtime gain and
  limiter settings, auxiliary physical signals and evolved histories cannot
  be identified from retained images alone.
- **Physical/acoustic response:** pin-to-config mapping, actual transducer
  behavior and speaker-to-ear transfer functions are not generated from the
  speaker workbook. Its topology information is separate evidence.

Thus the new tools support stronger **conditional full signal-path tests**;
they do not justify the statement “all static unknowns are closed” or an
automatic production bass correction. The state-grid report keeps that claim
false even when every declared test row passes.
