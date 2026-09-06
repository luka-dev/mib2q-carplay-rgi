# Audi B&O 9VS virtual-cabin evidence boundary

This note records what the no-recording SoundAmp model can prove from Audi
documents and firmware static analysis. It deliberately keeps recovered facts,
architectural inference, and unknown calibration separate.

Target state: Audi A5/S5 F5 Sportback, PR code 9VS, J525 `8W0035465`, software
`0420`, hardware `H11`, with Symphoria/3D disabled.

## Proven physical output circuits

Audi Current Flow Diagram No. 45 applies to the Bang & Olufsen 9VS system from
July 2016. The J525 connector-assignment page applies to Coupe/Sportback up to
September 2019. Together they prove these sixteen amplified T38 circuits:

| Circuit | T38 pins | Connected loudspeaker(s) |
|---:|---:|---|
| 1 | 3, 15 | R219 Centre loudspeaker 2 |
| 2 | 4, 5 | R14 rear-left treble + R15 rear-left bass |
| 3 | 6, 18 | R208 Centre loudspeaker |
| 4 | 7, 8 | R21 front-left bass |
| 5 | 9, 10 | R23 front-right bass |
| 6 | 11, 12 | R20 front-left treble + R103 front-left mid-range |
| 7 | 13, 14 | R22 front-right treble + R104 front-right mid-range |
| 8 | 16, 17 | R16 rear-right treble + R17 rear-right bass |
| 9 | 19, 20 | R211 subwoofer, pair/winding A |
| 10 | 21, 22 | R211 subwoofer, pair/winding B |
| 11 | 23, 31 | R220 front-left treble 2 |
| 12 | 24, 32 | R221 front-right treble 2 |
| 13 | 25, 33 | R276 front-left mid-range 2 |
| 14 | 26, 34 | R277 front-right mid-range 2 |
| 15 | 27, 35 | R209 left effect loudspeaker |
| 16 | 28, 36 | R210 right effect loudspeaker |

This accounts for all nineteen loudspeakers. Four output pairs feed two named
drivers each, and the R211 subwoofer occupies two output pairs. The diagrams do
not assign `+`/`-` polarity labels suitable for an acoustic model, so pin order
must not be treated as recovered speaker polarity.

Local source:

- `/Users/luka/Documents/Audi/S5_B9_2018_by_category/S5_B9_2018_4_Electrical_MIB2_Wiring.pdf`
- PDF pages 540–548: Current Flow Diagram No. 45/1–45/9
- PDF page 1694: J525 T38 connector assignment

The 19 physical fitting locations are also listed in:

- `/Users/luka/Documents/Audi/Audi A5 (2016 - 2019) [F5,F53,F57,F5A] - Repair manuals/D4B8067F819-Communication.pdf`
- PDF pages 31–33 / printed pages 27–29

`M53V20_SCH.pdf` is intentionally excluded: it is not documentation for this
vehicle/amplifier target.

## Proven firmware layer

The V850 CodeFlash image SHA-256 is
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Static analysis recovered the profile-0/bank-1 eight-input/eight-logical-bus
matrix at CodeFlash offset `0x101BB4`. The V850 writer sends its 64 gains to
SHARC parameter range `0xC022A..0xC0269`.

The exact matrix and its orientation are documented in
[`SHARC_INPUT_OUTPUT_MATRIX.md`](SHARC_INPUT_OUTPUT_MATRIX.md). Recovered
output-tuning tables are catalogued in
[`AMP_AUDIO_DSP_TUNING.md`](AMP_AUDIO_DSP_TUNING.md): 33 crossover-frequency
values, 23 reconstructed EQ biquads, and 16 per-channel gain selectors.

Those tuning values are not applied in the virtual-cabin renderer because the
available image has not yet yielded a defensible filter/gain-to-T38 assignment.

### Probable internal output-channel numbering

The V850 service tables group all sixteen output IDs as:

```text
[1,2,4,3] [5,6,8,7] [9,10,12,11] [13,14] [16,15]
```

An official Audi MIB3 self-study publishes a later J525 convention in which
3/4 are the two centres, 9/10 are left/right effect, 11/12 are front bass,
13/14 are rear left/right, and 15 is subwoofer. Combining that convention with
the exact A5 T38 topology yields this useful but **not directly proven** MIB2
hypothesis:

| Inferred channel | A5 T38 circuit |
|---:|---|
| 1, 2 | front-left/right shared mid+treble pairs |
| 3, 4 | R219 and R208 centre circuits |
| 5, 6 | R220/R221 front-left/right treble 2 |
| 7, 8 | R276/R277 front-left/right mid-range 2 |
| 9, 10 | R209/R210 left/right effect |
| 11, 12 | R21/R23 front-left/right bass |
| 13, 14 | rear-left/right shared treble+bass pairs |
| 15, 16 | two R211 subwoofer pairs |

Confidence is strongest for channels 3/4 and 9–15, weaker for 1/2 and 5–8,
and the ordering of the two subwoofer pairs is arbitrary. This map is suitable
for prioritising future static xrefs. It is not yet suitable for applying
per-channel gain or crossover values as OEM truth.

Source for the later published J525 convention:

- Audi self-study, *Third-generation modular infotainment matrix*:
  https://static.nhtsa.gov/odi/tsbs/2021/MC-10189329-0001.pdf

## Explicit inference used by `circuits` mode

For topology tests only, the renderer maps each semantic bus to the circuits in
the corresponding physical zone:

| Logical bus | Documentary destinations |
|---|---|
| Bus_L | four front-left circuits |
| Bus_R | four front-right circuits |
| Bus_C | two centre circuits |
| Bus_LFE | two R211 subwoofer circuits |
| Bus_Ls | rear-left door pair and R209 left effect |
| Bus_Rs | rear-right door pair and R210 right effect |
| Bus_7, Bus_8 | unused in the recovered 5.1 bank |

This zone assignment is a strong architectural inference, not a recovered
internal SHARC binding. Each bus is divided by the square root of its number of
destinations, preserving aggregate test energy. That normalization is not an
OEM gain estimate.

## Public architecture corroboration

Official Audi/Bang & Olufsen material for the shared B9 architecture describes
a 19-loudspeaker, 16-channel, greater-than-700-watt 3D system and identifies the
additional elevated front speakers. It corroborates the system topology but
does not provide A5 MY2018 per-channel calibration:

- Audi of America self-study, *2017 Audi A4 Infotainment and Audi connect*:
  https://static.nhtsa.gov/odi/tsbs/2015/MC-10118335-9999.pdf
- Bang & Olufsen press release, *Bang & Olufsen 3D Sound System for Audi Q7 and
  A4* (2015):
  https://investor.bang-olufsen.com/static-files/eb127e9f-cdfa-45ca-885d-300c4c65e66d
- Audi MediaCenter, *The Audi A5: new look and new technologies* (2019 facelift):
  https://www.audi-mediacenter.com/en/the-audi-a5-new-look-and-new-technologies-12622/download

Power figures from another B9 body style or later model year are therefore
reference context, not exact A5 MY2018 calibration.

## What remains unknowable without more static artifacts

- exact internal SHARC logical-bus-to-output index binding;
- filter, gain, delay, phase and limiter configuration per T38 circuit;
- loudspeaker impedance curves, passive crossover transfer functions and
  directivity;
- cabin impulse response at each seat;
- the exact runtime effects of Surround and Sound Focus control values.

The documentary model can compare HSSR profiles at the exact J525 input matrix
and documented electrical topology. It cannot reproduce the acoustic sound of
the real cabin from public documents alone.
