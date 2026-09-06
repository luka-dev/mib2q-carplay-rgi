# SoundAmp Virtual Cabin

Host-side documentary/static model of the known Audi J525/B&O 9VS signal path
for evaluating HSSR and authored 5.1 without changing or recording the car.

The checked-in target is the user's current listening state: **Symphoria/3D
Off**. It deliberately separates facts from the parts that still require a
different missing static artifact or an acoustic measurement:

- proven: 48 kHz, six input positions, V850-selected SHARC profile-0/bank-1
  8x8 input matrix, gain-floor behaviour, all 19 service-manual speakers and
  all 16 J525/T38 amplified output circuits;
- inferred for the documentary render: which semantic logical bus fans out to
  which front/centre/sub/rear circuit;
- unknown: the internal SHARC output index, OEM gain/delay/crossover/EQ per
  circuit, speaker aim/polarity, cabin transfer functions, and the current
  Surround/Sound Focus control values.

The `circuits` mode makes the inferred boundary explicit. It uses equal-power
fan-out only as a neutral test normalization; it never labels that fan-out as
recovered Audi calibration. The result is useful for electrical topology and
rear/front energy auditing, but is not a speaker-perfect acoustic simulation.

## Requirements

Python 3 and NumPy. Input and response files are 48 kHz signed 16-bit PCM WAV.
The input order is `L R C LFE Ls Rs`, matching `stereo_upmix_render`.

## Inspect the target

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py \
  inspect --speakers --circuits
```

The JSON target is
[`audi_a5_f5_9vs_3d_off.json`](audi_a5_f5_9vs_3d_off.json). Its normalized
speaker coordinates are schematic drawing coordinates only. The service
manual proves the named locations, but it does not provide acoustic distances
or driver aim angles.

## Run an HSSR file through the proven J525 matrix

First create a six-channel render, for example:

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror \
  -Ihook/audio \
  tools/stereo-upmix-render/stereo_upmix_render.c \
  hook/audio/stereo_upmix.c hook/audio/hssr_dsp.c hook/audio/hssr_neon.c \
  -lm -o build/stereo_upmix_render

build/stereo_upmix_render \
  --mode hssr-object --model build/hssr/hssr_model.bin \
  --lfe assist --surround 0.25 \
  input-stereo.wav build/hssr-object.wav
```

Then apply the exact amplifier input matrix:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py matrix \
  build/hssr-object.wav build/hssr-object-j525-buses.wav
```

This creates:

- an eight-channel PCM file ordered as
  `Bus_L Bus_R Bus_C Bus_LFE Bus_Ls Bus_Rs Bus_7 Bus_8`;
- `build/hssr-object-j525-buses.wav.report.json`, containing pre-clipping peak,
  RMS, rear energy fraction, rear/front ratio and per-bus clipping counts.

The eight-channel WAV has channel mask zero on purpose: Bus 7/8 have no honest
standard speaker label in the current 5.1 target.

## Expand to the documented 16 J525/T38 output circuits

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py circuits \
  build/hssr-object.wav build/hssr-object-j525-circuits.wav
```

This applies two deliberately separate layers:

1. The exact 8x8 matrix recovered from the V850 firmware.
2. A labelled architectural inference from each logical bus to the speaker
   circuits in its matching front/centre/sub/rear zone.

The 16 output channels follow the `output_circuits` order in the JSON config.
Every T38 pin pair and connected `Rxxx` loudspeaker is taken from Audi Current
Flow Diagram No. 45. Shared electrical pairs and the two R211 subwoofer pairs
are preserved. The report includes per-circuit RMS/peak, grouped front/rear
energy and every evidence limitation.

Each circuit also carries an `inferred_amplifier_channel` from 1 through 16.
That numbering combines V850 service-table groups with Audi's published
later-generation J525 channel convention. It is a useful static-analysis
hypothesis, explicitly not a direct proof for this MIB2 amplifier.

The inferred fan-out divides each logical bus by `sqrt(number_of_destinations)`.
That preserves aggregate electrical test energy and prevents a four-way front
fan-out from looking 6 dB louder simply because it was duplicated. It is not
an estimate of Audi's individual loudspeaker gains.

## Optional measured acoustic extension

Generate the sequential six-channel measurement file:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py probe \
  build/cabin-driver/probe-6ch.wav
```

The default is deliberately restrained at -30 dBFS and the command refuses
anything above -18 dBFS. Play it only while parked, starting with low vehicle
volume. Record the driver's-head position as unprocessed 48 kHz/16-bit stereo
with a binaural microphone or calibrated two-microphone fixture. Keep every
car setting fixed. Most importantly:

- Symphoria/3D Off;
- fixed Surround value;
- fixed Sound Focus;
- neutral Bass/Mid/Treble;
- fixed volume and microphone geometry.

Convert the unedited recording into six end-to-end responses:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py deconvolve \
  build/cabin-driver/driver-capture.wav build/cabin-driver/responses \
  --probe-manifest build/cabin-driver/probe-6ch.wav.probe.json
```

The sync chirp removes arbitrary recording start offset. All responses share
one storage scale, so their relative channel gains and path delays are kept;
the generated `brir.json` restores that common scale. Do not individually
normalize the response WAVs. Fill the actual seat, Surround, Sound Focus and
microphone calibration into the generated manifest before treating it as a
repeatable measurement.

For `domain: "most_inputs"`, each stereo response is end-to-end and already
contains the J525 matrix and every downstream speaker path. Therefore the
renderer does **not** apply the matrix a second time:

```sh
python3 tools/soundamp-virtual-cabin/soundamp_virtual_cabin.py binaural \
  build/hssr-object.wav build/hssr-object-driver-headphones.wav \
  --brir-manifest build/cabin-driver/responses/brir.json \
  --gain-db -6
```

The convolution is block FFT overlap-add. It works on full tracks without
loading the programme audio into memory. The output report records the IR
domain, FFT size, peak and clipping count.

`domain: "logical_buses"` is also supported for future direct bus-response
measurements. In that mode the tool first applies the recovered 8x8 matrix and
then convolves all eight logical buses.

## What the documentary model can and cannot validate

It can compare `music-stage`, `hsr-td-active`, `hssr-active` and
`hssr-object` under the same exact input matrix, expose rear-bus level and
headroom, and produce a repeatable headphone audition once measured responses
exist.

It cannot claim speaker-perfect acoustics from documentation alone. Static
evidence still does not bind every recovered crossover/EQ/gain value to a T38
circuit, and public documents do not contain the speaker directivity or cabin
impulse responses. Consequently no synthetic headphone preview is presented
as the sound of the real cabin.

## Regression tests

```sh
python3 -m unittest \
  tools/soundamp-virtual-cabin/test_soundamp_virtual_cabin.py
```

The tests lock the 3D-Off constraint, 19-speaker inventory, all 16 unique T38
pin pairs, recovered matrix orientation, 6-to-8 and 6-to-16 renders,
equal-power documentary fan-out, and the rule that an end-to-end MOST-input
BRIR must not receive the OEM matrix twice.
