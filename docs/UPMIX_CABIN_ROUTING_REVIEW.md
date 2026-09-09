# Cabin-aware routing and gain review

2026-09-08. Read-only review of production code and supplied workbook. No model,
runtime, factory parameterization or workbook was changed. Calculations concern
the recovered profile-0/bank-1 input stage, not a complete acoustic simulation.

## Decision

Redesign the separation between source estimation, spatial placement and gain
control before training a larger network. The existing rear signal combines
estimated objects with synthetic spatial material, but the front subtraction
treats the whole result as an extracted source. A fixed rear shelf then changes
both rear reproduction and the known factory feed back into the front buses.

The next candidate should preserve front anchors, relocate only confident source
estimates, add genuine ambience separately, and use bounded per-band rear energy.
Do not use equal energy per physical driver, a target percentage of rear energy,
or an inverse of the S4 microphone plots as the design objective.

## Workbook evidence and limits

Read all ten worksheet cell grids and visually inspected embedded stock-response
plots for front woofer/midrange, rear door woofer/tweeter, surround and rear-deck
subwoofer. Most of the workbook's other images show hardware/upgrades rather than
factory response data. The workbook was not edited or exported.

| Evidence in `B9 S4 B&O speaker data.xlsx` | Implication |
|---|---|
| `System overview!A18:H19`: microphone sweeps; some drivers hard to isolate; power labels/estimates | Neither anechoic driver response nor calibrated amp power/seat transfer function |
| `Rear door!A1:I6`: shared parallel feed; stock lower driver listed 160–5000 Hz | Lower rear door is a significant midrange source, not an additional subwoofer |
| `Rear door`, drawing named `image45.png` at I1: “Rear Door Tweeter 2k–20KHz” | Tweeter can contribute in the vocal-presence region, not only above 8 kHz |
| `Surround speakers!A3:I4`: stock driver listed 120–5000 Hz; response image at A14 | Surround has useful midrange; extra treble is not a substitute for selected musical body |
| `Front door Mid!A4:H5`: 150–4500 Hz; `Front Door Woofer!A5:H6`: 58–260 Hz | Protect the main front midrange/body while relocating supporting material |
| `My Car!A5:C8`: replacement tweeters, 8 µF cap, replacement woofer/inductor | Those crossover choices are modifications, not Audi factory crossover values |
| `System overview!D10` contains literal formula/value `#REF!` | Do not infer stock rear tweeter bandwidth from this broken summary cell |
| `Dash Speakers!A1` versus `E5` describe different lower-A-pillar ranges | Do not treat every tabular range as a precise measured crossover |

The rear woofer image also marks a narrower central region than the 160–5000 Hz
table entry. Its large room/position-dependent ripple cannot support a precise
software inverse EQ. The rear tweeter and surround plots have different level
scales/measurement isolation; do not derive driver sensitivity ratios from them.

Author's [original method description](https://www.audizine.com/threads/complete-guide-of-speakers-in-the-b9-b-amp-o-system.858483/)
confirms hobby measurements with limited isolation. It also distinguishes later
hardware modifications. S4 rear-deck subwoofer measurements do not establish the
response of this S5 Sportback's luggage-compartment subwoofer enclosure.

## Physical and digital constraints

- The car has 19 drivers on 16 amplified circuits, not 19 independently routed
  PCM channels. Rear door R14+R15 and R16+R17 each share an electrical output.
  Separate R209/R210 surround/effect drivers are in the Sportback C-pillar trim.
- Our six PCM channels cannot independently command rear-door woofer, door
  tweeter and C-pillar surround. Their individual responses constrain the common
  signal, but do not justify separate neural output channels for each driver.
- MMI 3D OFF is an operating target, not complete J525 processing bypass. The
  complex signal-dependent block and downstream mixing remain incompletely
  modelled; Sound Focus All/minimum Surround have not been read back live.
- Documentary equal-power door/surround fan-out is a test assumption, not factory
  calibration. Likewise recovered candidate rear-output HP records near 100 Hz
  and the S4's approximately 160 Hz observation describe different evidence, not
  a contradiction to resolve by simply selecting one cutoff.

See `SHARC_INPUT_OUTPUT_MATRIX.md`, `SHARC_OUTPUT_BINDING_AUDIT.md`,
`SHARC_VISA_3D_OFF_AUDIT.md`, and `UPMIX_STATIC_READINESS_AND_SOUNDCHECK.md`.

## Current transfer mathematics

Ignoring approximately -99 dB floor paths, the known input-stage equations are:

```
Bus_L  = .794328 L + .398107 C + .398107 LFE + .707946 Ls
Bus_R  = .794328 R + .398107 C + .398107 LFE + .707946 Rs
Bus_C  = .501187 (L+R) + .707946 C + .316228 LFE
Bus_Ls = 1.995262 Ls + .199526 LFE
Bus_Rs = 1.995262 Rs + .199526 LFE
Bus_LFE = 1.412538 LFE
```

Thus PCM C=0 does not mute the physical center path, and PCM LFE affects more
than the subwoofer. PCM rear amplitude is not post-matrix rear amplitude.

Let Q_L/Q_R be the rear signal AFTER our configured rear gain but BEFORE the
high shelf; G is dry-front gain; H is the rear high-shelf filter. Current float
synthesis is:

```
L = G*dry_L - 2.5*Q_L; R = G*dry_R - 2.5*Q_R
C = 0; Ls = H(Q_L); Rs = H(Q_R)
```

The known front bus consequently changes by `-.794328*2.5*Q + .707946*H(Q)`.
At low/mid frequencies where H is approximately unity this is **-1.277875 Q**.
The center changes by **-1.252968 (Q_L+Q_R)**. These are absolute coefficient
changes relative to the same dry-front gain, not a percentage loss of music or
measured sound pressure at a seat.

The coefficient that cancels just the same-side front-bus rear feed is
`.707946/.794328 = .891251` applied to the actual post-shelf signal. It is NOT a
replacement recommendation for 2.5: it neither performs desired relocation nor
preserves the center bus. Those are separate constraints.

An exact linear preservation solution using L/R/C exists at this input stage,
but is costly: injecting Ls=1, Rs=0, LFE=0 requires approximately
delta[L,R,C]=[-1.9802,-1.0890,+2.1728] to preserve all three front buses.
The front 3×3 condition number is about 11.87. This demonstrates headroom cost;
do not implement an unconstrained matrix inverse as an acoustic correction.

### What deserves changing

1. **Subtraction source.** `hssr_dsp.c` sums a hard-steered object, a side-based
   ambience component, and a phase-rotated mid-derived Universal bed. Then
   `stereo_upmix.c` subtracts the complete pre-shelf result from the dry fronts.
   The synthetic/steered output is not the original source image. Maintain a
   separate source-aligned estimate for subtraction and a separate rear rendering.
   Synthetic ambience must not be treated as a source removed from the original.
2. **Fixed transfer factor.** Replace the implicit combination of front-feed
   compensation and musical relocation with two explicit policies. Use bounded
   source-confidence-dependent removal; evaluate center preservation separately.
3. **Forced rear activity.** Universal fallback makeup uses
   `sqrt(.32/(rear_power/input_power))`, clamped 1…3.5. It can reinforce uncertain
   synthetic material when object activity is weak. This is not a target of 32%
   physical rear energy. Quiet rear passages should be allowed when evidence is
   insufficient; ambience has a separate confidence/energy budget.
4. **High shelf.** The current +3 dB/4 kHz shelf is +1.5 dB at 4 kHz, +2.865 dB
   at 8 kHz, with about 13.94 degrees phase at 4 kHz. It affects the door pair,
   surround feed and factory front-bus return, not tweeters alone. Test neutral
   shelf as the next baseline; retain the existing shelf as a labelled A/B option.
   Do not compensate microphone notches with large narrow boosts.
5. **Extreme steering.** Object output pan is forced into roughly ±0.78…0.95,
   while fallback pan alternates ±0.72 across broad frequency groups. This can
   make output separation stronger than the source image. Test confidence-scaled
   steering and spectral grouping so a single instrument is not split into
   unrelated left/right frequency fragments.

## Proposed content policy

These are initial experiment regions with smooth boundaries, not recovered OEM
crossovers or fixed decisions based only on frequency.

| Region | Main responsibility | Rear policy |
|---|---|---|
| Below ~120 Hz | Front bass plus existing deep-bass assist | No semantic object relocation; do not try to load rear-door drivers with sub-bass |
| ~120–250 Hz | Preserve warmth, bass/kick body | Very conservative supporting material; no blanket boost to make lower rear drivers busy |
| ~250–800 Hz | Body of guitar, keys, pads and low vocal formants | Admit confident supporting instruments together with their upper harmonics; protect vocal/body stability |
| ~800 Hz–4 kHz | Main localization, intelligibility, instrument definition | Principal useful rear-object region, with strongest vocal/lead protection; do not broadly boost |
| ~4–8 kHz | Attacks, cymbals, articulation and sibilance | Controlled continuation of accepted objects/percussion; explicit sibilance rejection |
| Above ~8 kHz | Air, reverb, upper harmonics, codec noise | Modest ambience/detail; no minimum activity requirement or automatic shelf boost |

Frequency is not semantic identity. A snare attack or lead guitar can be the
front anchor; an instrument is not automatically appropriate for relocation
because it is non-vocal. Preserve lead vocal, lead melody, bass and rhythmic
foundation, allow supporting objects and natural ambience to surround them.
Backing vocals/vocal reverb need explicit labels before relaxing vocal protection.

Source estimation should preserve object continuity across bands and time. The
render should keep three distinct branches: source-aligned relocatable object,
genuine diffuse ambience, and optional synthetic fallback. Only the first branch
can justify removing the corresponding estimated source from the front image.

## Bass, center and headroom

Keep the existing 48 Hz LR4 assist as a reference, not a proven hardware crossover.
Its measured code response is -6.02 dB at 48 Hz, -10.73 at 60 Hz and -18.81 at
80 Hz. The S4 workbook's sub range 30–90 Hz does not justify expanding this S5
assist to 90 Hz or adding a broad bass boost; phase/level interaction with front
woofers and the S5 enclosure is unknown. Likewise adding a protective subsonic
filter requires a defined hardware/measurement basis, not an invented limit.

Enabling assist changes dry-front gain from 32670/32768 to 31392/32768,
approximately -0.347 dB over the whole spectrum, not just bass. Keep this visible
in level-matched comparisons. The adaptive anti-phase bass folding and small
dynamic gain also need mono/anti-phase and transition regressions.

Initially keep native APAC multichannel untouched and C=0 for the stereo
candidate. Evaluate the resulting Bus_C explicitly: introducing a center feed
without rebalancing L/R/C can duplicate vocals. Do not promise center preservation
merely because the PCM C slot is zero.

Retain float synthesis, uniform post-render Sound Check, known-matrix/rail linked
guard, and one final S16 conversion. Add branch/band budgets before the emergency
guard so a bright rear transient is less likely to attenuate the whole programme.
Do not fix that pumping with independent final-channel clipping. The existing
guard is sample-peak safety, not full intersample/true-peak or speaker-excursion
protection. Test input-level robustness separately from Sound Check post-gain.

## Next implementation and promotion checks

1. Expose diagnostic source-object, ambience and fallback branches in an offline
   candidate; preserve the V11 reference and production defaults.
2. Compare one change at a time: neutral shelf; confidence-gated fallback; bounded
   source-aligned subtraction; calibrated steering. Then combine winners.
3. On mixture-conditioned masks, measure vocal/lead leakage, useful instrumental
   rear body, front/center damage, mono/fold-back consistency, band energy, mask
   modulation and global-guard attenuation. Listening and true target CPU checks
   remain mandatory before promotion.
4. Use the known matrix for exact digital tests; use S4 bandwidth observations as
   uncertain response scenarios only. Vary door/surround share rather than claiming
   equal-power fan-out is factory truth. Do not invent complete per-seat impulse
   responses or chain the unknown complex block as identity.
5. Fix teacher rate/gain/provenance and stateful training per
   `HSSR_DATASET_TRAINING_AUDIT.md`. Teach source role/confidence and continuity,
   not louder rear output. Compare the corrected existing 2×160 model before
   attributing improvements to a wider or frequency-aware graph.

Evidence calculations: `build/cabin-routing-audit/transfer-math.json` and
`transfer_math.py`. Spreadsheet extraction and original embedded plots remain
under that ignored local directory. No private audio or workbook was uploaded.
