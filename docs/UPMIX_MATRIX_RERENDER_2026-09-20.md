# Corrected matrix: offline music rerender — 2026-09-20

Static/offline only. No vehicle contact, deployment, settings, production hook
or source PCM changes. This supersedes old music numbers derived from the
[extra matrix transpose](SHARC_MATRIX_CONTROL_2026-09-20.md).

## Scope and reproducibility

Seven cached 60-second stereo clips and their 21 authored PCM6 variants have
the **same 28 SHA256 hashes** as the historical v2 seven-track report. Tracks:
Boost Up, Crazy Train, Deep Down Low, Heart-Shaped Box, The Blue-Eyed Devil,
UP. UP. UP., take your vibes and go. No new upmixer build/render was used.

Results live in `output/upmix-matrix-corrected-20260920/`:

- `music-v7/report.json`: seven tracks, three variants plus dry BANK1.
- `controls.json`: historical comparison and isolated controls on two tracks.
- `common-input-v8.json`: two common-input EQ hypotheses on two tracks.

v7: profile0, pair row1, ASAD FC, B1/B2=0, C4D17 identity; no common-input
stage or extra runtime gain/limiter. These are explicit hypotheses, **not
verified vehicle state**. Physical graph/channel binding remains conditional.
The reference is L/R-only PCM6 through BANK1, **not stock stereo/bank0**.
All 60 seconds are processed; source window5..55s is measured with known
per-output A delay plus512 upmix frames compensated. Group power is a linear
mean, then paired dB changes are averaged across tracks; not cabin SPL or an
acoustic sum of drivers. Welch32768/50% as in the original report.

Variant names encode unchanged cached settings: surround0.4 throughout,
`gain4096_fb0` = front bass0dB/LFE gain0dB;
`gain12288_fb0` = front bass0dB/LFE gain+9.542425094dB;
`gain4096_fb3` = front bass−3dB/LFE gain0dB.
LFE gain0 means the default assist coefficient4096, **not zero LFE samples**.

## Seven-track result

Mean paired changes versus dry BANK1, dB:

| Candidate group / band | LFE0, front0 | LFE+9.542, front0 | LFE0, front−3 |
|---|---:|---:|---:|
| Front woofers20–60Hz | −0.374 | −0.374 | −3.357 |
| Front woofers60–120Hz | −0.380 | −0.380 | −3.278 |
| Sub20–60Hz | −1.290 | −2.746 | −4.715 |
| Sub60–120Hz | −1.197 | −3.038 | −4.459 |
| Front mid/treble500–2000Hz | −4.684 | −4.684 | −4.703 |
| Centre mid500–2000Hz | −4.812 | −4.812 | −4.840 |
| Rear doors500–2000Hz | +0.125 | +0.125 | +0.105 |

Thus baseline front woofer bass60–120Hz is almost retained while front
midrange500–2000Hz drops: the **cross-group electrical bass/mid balance changes
by +4.303dB** (per-track range+1.285..+7.159). This is a diagnostic proxy, not
a perceptual loudness metric. Front bass−3 changes that proxy to+1.425dB
(range−1.602..+4.274); it does not fix the midrange loss.

Absolute sub20–60Hz relative to the original input L: dry+0.567dB,
baseline−0.723dB, LFE+9.542 variant−2.179dB, front−3 variant−4.148dB.
The old “sub only LFE / no LFE means silent sub” model conclusion is false.
Unclipped model peaks can exceed0dBFS (baseline front mid/treble+4.727dBFS).
This calls for gain/headroom analysis, not a claim of actual hardware clipping.

## Isolated controls

`audit_amp_matrix_music_control.py` verifies hashes, settings, windows and
default model metadata. It changes only the matrix orientation or zeroes only
PCM channel3 in memory; original WAVs remain untouched.

On Boost Up and Crazy Train, reinserting just the old transpose reproduces
historical v2 group-band powers with maximum absolute difference **0.0dB**.
This isolates the matrix error for those two controls; it is not an emulation
of all previous model versions or a seven-track isolated-control claim.

| Clip | Corrected minus transposed sub20–60Hz | LFE zeroed minus baseline sub20–60Hz | LFE zeroed minus baseline sub60–120Hz |
|---|---:|---:|---:|
| Boost Up | +14.976 | +1.267 | +0.903 |
| Crazy Train | +14.664 | +1.255 | +0.752 |

Zeroing LFE **increases**, rather than removes, modeled sub bass in these
clips. In this fixed linear chain that establishes net destructive summation
between LFE and the other contributions in these bands. It does not establish
a constant180-degree phase offset. Likewise the louder LFE variant reduces
seven-track mean sub20–60Hz by1.457dB versus baseline. Therefore decreasing
LFE is not a justified standalone bass fix. Other groups have only floor-level
LFE leakage; their effects round to0.000dB in the two ablations.

## Common-input scenarios after correction

Same two PCM6 files and same scenario-file hashes as the historical v6 report.
Both use `gain4096_fb3`, first12s rendered/window2..10s, tone0, delay0,
mapping[0,1,2,3,4,5,None,None], B1/B2=0 and C4D17 identity. Bank10 minus
bank7 group-power deltas under v8, dB:

| Clip | Woofers20–60Hz | Sub20–60Hz | Woofers60–120Hz | Sub60–120Hz |
|---|---:|---:|---:|---:|
| Boost Up | +5.980 | +5.951 | +5.265 | +5.553 |
| Crazy Train | +5.953 | +5.901 | +4.868 | +5.455 |

The common-input EQ can change bass strongly **in these scenarios**. Neither
bank selection nor B=0 nor the physical input mapping is proven in the car.
These bank numbers are common-input EQ banks, not the5.1 matrix-bank number.

## Reproduce / next

Use `amp_music_report.py --rendered-dir /tmp/upmix-bank1-audit.BR9uxY`
with the seven original stereo WAVs, `--start 5 --end 55`, and explicit variants
`gain12288_fb0:0.4:0:9.542425094`, `gain4096_fb0:0.4:0:0`,
`gain4096_fb3:0.4:-3:0`. Keep historical reports intact; use a new output path.
The cache is temporary; JSON retains paths/hashes, not the PCM contents.

```sh
python3 tools/soundamp-virtual-cabin/audit_amp_matrix_music_control.py \
  --report output/upmix-matrix-corrected-20260920/music-v7/report.json \
  --legacy-report output/upmix-bank1-model-v2-20260919/report.json \
  --out output/upmix-matrix-corrected-20260920/controls.json \
  'Boost Up' 'Crazy Train'

python3 tools/soundamp-virtual-cabin/audit_amp_common_input_music.py \
  --scenario output/r2/soundamp/remaining-blocks-20260919/common-input-bank7-hypothesis.json \
  --scenario output/r2/soundamp/remaining-blocks-20260919/common-input-bank10-hypothesis.json \
  --out output/upmix-matrix-corrected-20260920/common-input-v8.json \
  '/tmp/upmix-bank1-audit.BR9uxY/Boost Up-gain4096_fb3.wav' \
  '/tmp/upmix-bank1-audit.BR9uxY/Crazy Train-gain4096_fb3.wav'
```

405 tests pass:121 extraction+284 model. New controls test dB differences and
fail-closed comparison of changed PCM, variants, track sets and windows.

Follow-up completed: [front/centre per-input attribution and diagnostic recovery](UPMIX_MIDRANGE_ATTRIBUTION_2026-09-20.md).
It isolates upstream front loss and changed centre L/R summation, with seven-track
float counterfactuals. Current C guard topology remains stale and must be corrected
before accepting a new guarded candidate.411 tests now pass.
Do not choose a production bass/LFE correction from the old transpose results.
Static gaps remain in outer/UI bindings, B effective-state composition and
physical mapping; this rerender does not resolve them or authorize vehicle I/O.
