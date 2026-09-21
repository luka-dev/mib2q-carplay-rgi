# B9 speaker workbook — external evidence, not DSP calibration

Source: `docs/B9 S4 B&O speaker data.xlsx`, SHA256
`e118a86e50ed9467cabc96c3f9209fa3b669985d4fc955d3972d09722703b586`.
Read-only extraction: `output/upmix-speaker-workbook-20260920/evidence.json`.
The workbook identifies Bobby Kinstle as author and attributes its wiring image
to AudiWorld user Jorge GS. Its `My Car` tab describes the author's aftermarket
changes; it is **not** evidence of the user's installed speakers or amplifier.
No workbook cells, model filters or production settings were changed from it.

## Useful stock-system observations

`System overview!A3:H19` lists 19 physical drivers. The frequency ranges are
described in A19 as microphone-sweep observations with isolation limitations,
not measured digital filter coefficients or complete speaker-to-ear transfer
functions. Nominal power labels do not establish amplifier output power (A18).

| Group | Workbook range | Observation reported by author |
|---|---|---|
| Front woofers | `Front Door Woofer!A2:H6` | 8 ohm, reported 58–260 Hz; amplifier channel said to extend to44 Hz |
| Front mids | `Front door Mid!A4:H5` | 4 ohm, reported150–4500 Hz |
| Rear doors | `Rear door!A2:H7` | Mid/tweeter parallel pair; reported160–5000 Hz for stock woofer |
| Subwoofer | `Rear Deck Subwoofer!A3:G4` | Two8-ohm coils, reported30–90 Hz |
| Upper A-pillar / side dash | `Dash Speakers!A3:J6` | Marked3D-effect drivers; reported2–7 kHz /150–2000 Hz |
| Centre | `Dash Speakers!A7:J8` | Separate mid and tweeter; author measured both together |
| Rear shelf surrounds | `Surround speakers!A1:I4` | Reported120–5000 Hz; role described as surround audio |

Do not fit rectangular passbands to these ranges or apply them after the existing
electronic EQ as another invented filter bank. They can help cross-check physical
group hypotheses, but cannot recover a B spectral mask, current selector, phase,
delay, sensitivity, protection law or cabin impulse response.

## Connector chart

The chart is an embedded **image**, not spreadsheet cell data:
`Amp Wiring Chart`, drawing `xl/drawings/drawing3.xml`, relationship `rId2`,
member `xl/media/image1.png`. It was read directly because the worksheet renderer
did not display the embedded pictures. The chart depicts these speaker pin pairs;
polarity and fit to the user's particular harness are **not** asserted:

| Chart label | Pins |
|---|---|
| Left front door Mid-Tw |11 /12|
| Right front door Mid-Tw |3 /15|
| Left front door Midbass |7 /8|
| Right front door Midbass |9 /10|
| Left rear door Mid-Tw |4 /5|
| Right rear door Mid-Tw |16 /17|
| Left pillar Tweeter |23 /31|
| Right pillar Tweeter |13 /14|
| Dashboard Left Midrange |25 /33|
| Dashboard Right Midrange |26 /34|
| Center Midrange |6 /18|
| Center Tweeter |24 /32|
| Rear shelf/trunk left Midrange |27 /35|
| Rear shelf/trunk right Midrange |28 /36|
| Subwoofer Coil1 |19 /20|
| Subwoofer Coil2 |21 /22|

These16 electrical pairs are consistent with19 physical drivers when the four
door mid/tweeter pairs share channels and the one subwoofer has two driven coils.
This is a consistency check **within the workbook**, not an independent pinout
verification. The missing link is still DSP config/output index→hardware pin.
No permutation of the16 model outputs is chosen from this chart alone.

## Conflicts retained rather than repaired

- `System overview!D10` contains `=#REF!` for the rear tweeter range.
- `Dash Speakers!A1` says the lower A-pillar tweeter plays8–12.5 kHz, whereas
  `Dash Speakers!E5` lists2–12 kHz. Neither is silently preferred as calibration.
- The workbook mixes stock observations, subjective upgrade advice and modified
  hardware. Replacement-driver rows and `My Car` do not become factory data.
- Linked external T/S/ZMA files are not included in this read-only extraction.
  They were not fetched, and no measured acoustic model is claimed from links.

For the B spectral binding itself, only the hash-verified firmware/common-header,
effective configuration, pointer maps and decoded consumers are coefficient
sources. This workbook stays separately labelled external speaker evidence.
