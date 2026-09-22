# Coefficient side paths and matrix-orientation correction — 2026-09-20

Static-only continuation of [outer-control audit](SHARC_OUTER_CONTROL_2026-09-20.md).
No vehicle access, recording, request, deploy, settings change or firmware patch.
V850 addresses are byte/file offsets; SHARC addresses are shortword PCs or DM/PM
word addresses as labelled. Same CodeFlash SHA256
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.

## Critical correction: the old model transposed the matrix

V850 A903C loops j=0..7 and i=0..7, loading signed RAM[8*i+j] atgp−15904,
converting it through the gain table and sending it to SHARC C022A[8*j+i].
The source read is A90A2..A90AE/A9122..A9132; destination is A907E..A908A;
two adjacent eight-word groups are sent per16-word record. A8C6C serializes
words in order, not with a matrix transpose. Host record1296B9..1296E2 writes
successive payload words to successive destination offsets.

SHARC1269BC obtains input/output counts from descriptor+5/+7. R7 loops inputs,
R3 loops outputs.1269F3 chooses output buffer at inputs+R3;126A10 chooses input
R7.126A02/126A04/126A07 read coefficient **R7*outputs+R3**.

Therefore:

```
RAM[8*output+input] -> SHARC[8*input+output] -> gain(output,input)
```

RAM/A07 rows are **outputs**, columns are **inputs**. The previous CSV/extractor,
documentation and AmpChain v5/v6 treated them as input rows and transposed again.
Earlier checks compared model/table values but did not compose this consumer's
index arithmetic; they did not detect the error. This is corrected, not silently
preserved as a second plausible preset.

For profile0/bank1 and zero coordinate, output3(zero-based) receives inputs0/1/2/3/4/5
at −8/−8/−10/+3/−14/−14dB before later filtering. Conversely input3 reaches
outputs0/1/2/4/5 only at the −99dB floor. This reverses significant parts of the
old interpretation. Channel names and physical speaker grouping remain separate
assumptions; the numeric matrix proof does not identify raw MOST pin order.

The earlier **model-based** “sub only from LFE” and “front woofers sum L/Ls/C/LFE”
claims are withdrawn, as are speaker-group bass figures from the old orientation.
This does not invalidate independent physical measurements or prove the actual
cause of the listening complaint. Older music figures are superseded by the
[corrected offline rerender](UPMIX_MATRIX_RERENDER_2026-09-20.md).

## Three coordinate tables

gp−15840 is the already clamped signed coordinate−9..9. Each lookup uses index9+x.
The fixed calibrated copy B64E0 maps102438→gp−1330C; these are file-backed source
tables, not asserted runtime RAM values or fallback calibration.

| Path | RAM relative | File bytes | 19 signed values, tenths dB |
|---|---|---|---|
| AA644 odd pair gains | −12ACC | 102C78 | all0 |
| A903C input-coordinate branch | −12AA6 | 102C9E | all0 |
| A903C output-coordinate branch | −12A80 | 102CC4 | −90,−80,…,0,…,+80,+90 |

A903C applies the following priority, where i is the **DSP output**, j the input:

1. If ASAD(gp−15905)&15!=0 and j>=4, add the input table's value.
2. Otherwise, if ASAD==C0, gp−17771==5A and i>=4, add the output table's value.
3. Otherwise add0.

Every result is clamped to−990..200 tenths and looked up at69398−4*result.
Thus the nonzero stored table affects **output rows4..7**, not input columns4..7,
under the second condition. With ASAD FC the first table is zero, so the
coordinate has no direct matrix correction. At coordinate0 all three tables
return0; this is not a flat or identity-matrix claim.

## AA644 20-word pair mixer and transition

Selected row is level+1 only if gp−17771 and gp−17756 both equalA5, else0.
The five20-byte source rows come from gp−135F8(profile0 file10033C, profile1
file1006AC). The current20-byte row is gp−16804. State bytegp−16784 controls it:

- 5A: copy selected source row.
- AA: move each signed current byte by one toward target. Change state to5A
  only on a call where no byte moves; reaching target on this call is not enough.
- FF: copy even entries; force odd entries to−100.
- Other values: retain prior current row.

For row>0, ASAD C0 and odd indices, add the first table and clamp−990..200.
Other indices use10*current_byte without that clamp. Consequently a zero-valued
table does **not** make the branch inert: −100dB(exact lookup zero) becomes
−99dB(1.1220185e−5 linear) on corrected odd entries. This is floor leakage, not
a +9.5dB boost. E0584 is consumed by12694C as
out[j]=in[j]*gain[2*j]+in[10+j]*gain[2*j+1].

API18 itself is sent after this block. Event22 also sends the matrix; event368
does not include that additional A903C call. Actual scheduling/state remain unknown.

## Model and tests

- `matrix_control.json` exports exact tables, lookup, source rows and consumer hashes.
- `amp_matrix_control.py` models one explicit row-state step and coefficient targets;
  it does not invent current flags, packet delivery or audio B values.
- AmpChain default is now **v7**; explicit common-input variant is **v8**.
  Both use the corrected matrix orientation. Old schema1 artifacts are rejected.
  Default ASAD FC/zero-coordinate matrix scenario and B=0 hypothesis are retained,
  not verified as current vehicle state. C0 coefficient scenarios remain isolated
  helpers, not silently integrated into the default music report.
- Matrix CSV/extractor and default electronic audit were regenerated. Production
  hooks, upmix levels and firmware were not changed.

403 tests pass:121 extraction +282 model.168 bounded SHARC listings UNKNOWN0;
43 V850 listings byte-checked. New bounded V850 execution compares380 complete
matrix-writer cases(actual and asymmetric synthetic correction tables),288 pair
writer/state cases, all64 SHARC address-slice combinations and all64 AmpChain
matrix routes. Transport calls use explicit capture stand-ins; these are not
full CPU, SPI, concurrent-task or bit-exact SIMD emulators.

`bank1_model_v7_check.json` independently recomposes the downstream stages:
maximum numeric difference7.13e−15. This checks consistency, not cabin SPL.

Offline rerender completed: [seven tracks and isolated controls](UPMIX_MATRIX_RERENDER_2026-09-20.md).
Next isolate midrange loss and candidate correction/headroom; preserve explicit
B/common-EQ alternatives. Remaining
static gaps include pointer1162..1165, API29/UI/outermost request binding,
nonzero EQ designer, full state/target composition and physical input mapping.
