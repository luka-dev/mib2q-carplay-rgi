# Electronic/acoustic bridge and legacy matrix repair — 2026-09-20

Static/offline only. No car I/O, recording, deployment, production settings,
firmware writes or commit. No acoustic measurements were invented.

## Actual stale-renderer bug repaired

The current AmpChain v7/v8 was already corrected, but the separate documentary
`soundamp_virtual_cabin.py` and its JSON still said A07 rows were inputs and
used `samples @ gains`. This repeated the withdrawn transpose in its matrix,
documentary-circuit and logical-bus-BRIR modes. Its old orientation test
repeated the same mistake rather than checking the DSP consumer.

Now A07 rows are outputs, columns are inputs, and the operation is
`samples @ gains.T`. Configs carrying the old convention are rejected. Display
headings are corrected; new reports declare `matrix_semantics_version:2`
and the storage order, and binaural reports state whether a matrix was applied.
All64 impulse coefficients cross-check AmpChain's
extracted M8; independent asymmetric checks pin Ls→bus3 at−14dB and the
reverse LFE→bus4 at the−99dB input-matrix floor. The floor itself was correct
and is unchanged. [Instruction evidence](SHARC_MATRIX_CONTROL_2026-09-20.md):
A903C RAM[8*out+in]→C022A[8*in+out], consumed by126A02/126A07 as in*8+out.

This changes old documentary matrix-domain renders. It does **not** change
the corrected105-render tuning sweep, the C upmix, AmpChain v7/v8 defaults,
or end-to-end `most_inputs` BRIR rendering, which bypasses the matrix.
The documentary equal-power fan-out remains inferred, not recovered calibration.

The even older `tools/stereo-upmix-render/cabin_model.py` also retains a
withdrawn transposed six-bus matrix and is not the complete DSP. It is now
fail-closed for normal CLI/API use. Historical reproduction alone requires
`--allow-obsolete-transposed-model` (or explicit API opt-in); output is labelled
obsolete/not-for-tuning. Its old code/data were preserved, not silently relabelled
as corrected results. Two regression tests enforce refusal before rendering
or output-directory creation. Use AmpChain/amp_music_report for current work.

## New bridge: amp_cabin_render.py

Two mutually exclusive acoustic boundaries are supported:

| Manifest domain | Response set | Processing order |
|---|---|---|
|`most_inputs`|6 stereo IRs: L,R,C,LFE,Ls,Rs|PCM6→end-to-end IRs→ears; AmpChain forbidden|
|`post_amp_config_outputs`|16 stereo IRs: cfg00..cfg15|PCM6→explicit AmpChain→post-output IRs→ears|

The second set must describe the transfer **after** the modeled electronic
output boundary, in exact cfg order, including the relevant physical drivers
and acoustic path. It cannot reuse a MOST-to-ear capture: that would apply
the electronic filtering twice. Candidate speaker names/pins are not a proof
of cfg-order measurement correspondence.

Each response WAV is48kHz, two channels (left/right ear), finite and nonempty,
with its SHA256 recorded in the manifest. Paths are relative to that manifest.
Per-response `gain_db` is mandatory and bounded to±120dB for validation; this
is a file-format bound, not a recommended acoustic gain. Relative signs,
timing and phase are retained. Ear signals are coherently summed, not combined
as powers. No per-speaker or final peak normalization is performed.

Complete acoustic FIR tails are retained. Post-amp mode additionally requires
an explicit number of zero-padded electronic frames; this finite duration
does not prove all IIR/adaptive histories have settled. Programmatic callers
can supply a configured AmpChain and explicit first-B/second-B/complex auxiliary
arrays, including padded tail frames. Their shapes and hashes are reported.

Output WAV is float32, reread-verified, with over-range samples preserved rather
than clipped. **Do not play an unreviewed over-range report at normal volume.**
Its scale is whatever the declared measurement reference defines, not calibrated
SPL by default. Reports carry full AmpChain warnings and acoustic provenance.

### Manifest contract

Required fields:

- `schema_version:1`, `sample_rate:48000`, supported `domain`.
- `channels`: exact ordered6/16 identifiers above.
- `responses`: exactly those identifiers, each with `file`, `sha256`, `gain_db`.
- `provenance`: `kind` is `measured` or `synthetic`, plus a description.
- `capture`: nonempty `seat`, `state`, `microphone_calibration`, `level_reference`.

`measured` is a declaration, not independent verification. `hardware_verified`
remains false. Synthetic test IRs never become a calibrated cabin. The companion
[readiness audit](UPMIX_MODEL_READINESS_2026-09-20.md) reopens the actual IRs and
separately checks evidence references; hashes establish identity, not truth.
Legacy BRIR manifests without these fields need an explicit provenance/hash
upgrade, not automatic promotion to measured evidence.

### Usage with actual supplied transfer data

```sh
python3 tools/soundamp-virtual-cabin/amp_cabin_render.py \
  candidate.wav ears.wav --manifest end-to-end-most-ir.json

python3 tools/soundamp-virtual-cabin/amp_cabin_render.py \
  candidate.wav ears.wav --manifest post-amp-cfg-ir.json \
  --electronics-hypothesis --electronic-tail-frames 48000
```

The second command explicitly accepts default B0/C4D17 identity/omitted runtime
stages; optional `--upstream-scenario` supplies a common-input scenario without
certifying its live state.48000 padded frames is an example, not a proved tail
bound. Full custom B/complex/runtime scenarios use the Python API.

## Verification artifacts

`output/upmix-cabin-bridge-20260920/{most_inputs,post_amp_config_outputs}/`
contains **synthetic test-only** manifests, impulses and CLI-generated float
outputs/reports. These prove file flow and signal algebra, not Audi acoustics.
Ten bridge tests cover coherent addition/cancellation, sign, relative delay,
full FIR tail, cfg order, explicit electronic padding, no double DSP, changed
hash rejection, malformed inputs, output-overwrite refusal and float round-trip.
Eight legacy cabin tests pass, including two added orientation regressions.

To claim real-cabin prediction still requires actual responses, cfg mapping
(if using the post-amp domain), documented capture state/level and validation
against independent recordings at the intended listening position. A single
fixed linear IR set cannot certify every volume, adaptive state or seat.
