# Full conditional model — musical check, 2026-09-21

Offline only. No vehicle contact, production/source/default edits or deployment.
**All56 calculations and storage checks pass, but this is not an unconditional
sound-quality or vehicle-headroom pass.** Front bass0 produces a relative bass/
mid tilt; positive electrical-model peaks remain even with the tested trims.

## Exactly what was tested

Seven local music clips, **first6 seconds +0.25s zero tail**,48kHz. Not complete
tracks: Boost Up, Crazy Train, Deep Down Low, Heart-Shaped Box, The Blue-Eyed
Devil, UP. UP. UP., take your vibes and go.

Actual hash-pinned native C renderer →PCM6 →AmpChain with
`full_static_nearest_zero_aux`. Retained B01 bank1 first-B controls, real bands,
spatial initialization, firmware-derived window, packing/internal FFT tables
and log/pow algorithms. Input-EQ hypotheses7 and10 are separate runs; neither
is asserted to be the current vehicle selector or MOST bank number.

| Variant | C mode | Surround | Front bass,dB | LFE,dB | Front retention |
|---|---|---:|---:|---:|---:|
| `zero_trim` | S16 |0.40|0|0|0|
| `startup` | S16 |0.40|−3|0|0|
| `retain75_bass1` | float→S16 |0.30|−1|0|0.75|

Every case also has `dry_bank1`: L/R-only PCM6 with the same512-frame reference
delay through the **same BANK1 model**, not stock stereo/bank0. All LFE settings
are assist mode,0dB calibration offset, not an LFE mute. Thus7tracks×2EQ×4rows=56.

`startup` matches local startup **upmix settings at unity programme gain**.
The script also enables SoundCheck metadata gain, which is not supplied here;
absent float/retention exports imply S16/0 only without inherited overrides.
This is not an exact replay of the running startup environment. The float
candidate changes four controls at once; comparisons do not isolate retention.

## Results

Bass/mid means the change in candidate front-woofer60–120Hz power relative to
front-mid/treble500–2000Hz, compared with dry through the identical model and
time window. Values below are arithmetic means across seven clips, not matched
loudness, cabin SPL or a listener preference score. Physical group names remain
unverified config-output candidates.

| Variant | Mean bass/mid tilt EQ7/EQ10,dB | Maximum peak EQ7/EQ10,dBFS | Clips above0dBFS EQ7/EQ10 |
|---|---:|---:|---:|
| dry BANK1 |0/0|+4.168/+5.872|3/4|
| front bass0 |+1.855/+1.847|+3.751/+4.850|3/4|
| startup front bass−3 |−1.009/−1.031|+3.651/+4.594|3/4|
| float candidate |−0.461/−0.465|+3.282/+4.216|3/4|

The zero-trim tilt varies from approximately+0.24 to+3.31dB between clips;
startup from−2.63 to+0.50; candidate from−0.91 to+0.005. These bounded cases
show a bass/mid shift at front bass0, not proof of excess cabin bass at MMI0.

Worst upmix peaks occur in cfg0/1, not just in the model's low-frequency group.
Deep Down Low produces the worst zero-trim/startup peaks and the EQ7 candidate
peak. The EQ10 candidate maximum is The Blue-Eyed Devil/cfg0.

Trimming the C front channels is **not** isolated to a physical front woofer
after downstream mixing. Model sub20–60Hz relative-to-dry mean changes from
−1.34/−1.37dB at zero-trim to−4.78/−4.83dB at startup. The candidate gives
−2.49/−2.53dB. The startup comment claiming sub/rear are untouched should not
be read as an accurate description of this full conditional downstream model.
No configuration/comment was edited as part of this diagnostic request.

## PCM and protection checks

- All42 upmix rows have finite PCM6 and **zero near-S16-rail samples**, zero
  near-rail runs, and digital C exactlyzero. Final16-channel arrays are finite.
  Digital C zero does not imply a silent physical centre after DSP mixing.
- Dry contains884 near-rail samples per EQ set, inherited from original source
  prefixes: Deep Down Low476, The Blue-Eyed Devil376, take your vibes and go32.
  The longest dry near-rail run is9 samples. Each dry WAV was checked exactly
  against the original prefix delayed512frames; these samples were not created
  by the upmixer. Near-rail counts alone do not prove master clipping.
- Float guard activity is observable: minimum gain0.56804281 on Deep Down Low
  (−4.91dB;22436/300000frames attenuated),0.6014033 on The Blue-Eyed Devil
  (19840frames),0.82136577 on take your vibes and go (1378frames). Other tested
  clips have minimum1.0 and zero attenuated frames. Counts include the tail.
- S16 does not emit these float guard counters. Missing telemetry is not zero
  limiter activity. Clean PCM6 can result from successful internal protection.

## Why the peaks are not a hardware clipping diagnosis

This scenario supplies pre/post gains1 and two synthetic limiter banks with
threshold100 (+40dBFS), initialgain1. Actual MMI volume, SoundCheck gain, limiter
parameters and conversion headroom are not recovered. Positive model peaks
therefore flag the assumed electrical level budget, not proven DAC saturation.
They also occur in **dry**, so they cannot be attributed solely to upmix.

C4D17 uses retained INITIAL FIR/profile0, adaptation0 and zero independent
auxiliary input. Second-B is present but receives zero input/zero histories;
its nonzero real sources are not tested here. Fresh histories, settled controls,
on-time wrapper callbacks and nearest FIX remain explicit assumptions. No
dynamic transitions, complete pointer-alias proof, exact SHARC arithmetic,
physical-output mapping or measured cabin/ear transfer is established.

Conclusion: execution/storage integrity is good for these selected tests;
zero-trim tonal balance and the assumed output headroom are not an all-clear.
Do not convert these observations into a production gain change without a
separate decision about the missing level/state evidence. This pass changes no
production settings and does not certify subjective sound.

## Artifacts and verification

- `output/upmix-full-model-check-eq7-20260921/report.json`:28/28,
  SHA256`e67a81d555d27bfdbc6f41020e7cbf423ee300c50671604f10526800d855c615`.
- `output/upmix-full-model-check-eq10-20260921/report.json`:28/28,
  SHA256`2867342588a9fd607c1a7a5453ee26d41258a79a6992ef3194626b89fd27a671`.
- `output/upmix-full-model-check-20260921/review.json`:56 verified rows,
  SHA256`a1cc478439103d87a74ff7da4322f3409196048c202c609bd654487bf3c866ac`.
- `rows.csv` and reproducible `review.py` are in that review directory.

All309 unique referenced file fingerprints were checked. All56 saved16ch
FLOAT32 WAV array hashes and every stored metric were recomputed. Exact grid
coverage and original shard equality were checked; PCM6 hashes match across
the two EQ runs, and saved auxiliary inputs are exactlyzero. Per-row output
WAVs retain overs without normalization. Every source prefix and dry reference
was additionally checked against the corresponding original input samples.
