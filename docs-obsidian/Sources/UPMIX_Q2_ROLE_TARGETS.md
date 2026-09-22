# Q2 instance-role target compiler — prospective, not training admission

This NEW pure module compiles verified, interval-resolved human evidence into
disjoint reference accounting before the twelve identity-family aggregation.
It does not read original files, invent reviews, train, infer roles, render rear
audio or grant permissions. Its qualification uses authored arrays and one
actual synthetic packet/journal lifecycle, not music or human judgments.

## Supported interface and trust boundary

`q2_role_targets.compile_targets(canonical_audio, proof, bindings, resolutions,
*, start_sample, end_sample, expected_packet_manifest_sha256,
expected_case_proof_sha256, expected_bindings_sha256,
expected_resolutions_sha256, expected_journal_head_sha256,
synthetic_fixture=False)`.

The caller must first verify a complete retained packet with `PacketEvidence`,
open its separate journal using an externally retained acknowledged head, and
resolve each original instance for exactly the proposed loss-support interval.
Freeze the ordered bindings and resolved-results SHA in a separate target plan.
Do not derive an allegedly trusted expected SHA from untrusted input on demand.

Inputs are the packet's canonical unit-gain float32 `[N, 1+M, 2]` array (mixture,
then every original in its saved order), core proof, ordered `binding()` results,
and ordered `resolve()` results. Original/body SHA, packet context, decoded
canonical-array/mixture/solo hashes, preview hashes and container hashes retain
their distinct meanings. The compiler verifies the actual array and per-column
context/core/preview hashes, proof/binding/result SHA pins, exact interval,
48-kHz geometry, source order, TRAIN/fixture scope and journal head.

No filesystem verification is duplicated inside this math helper. Original
file and NPY/WAV container provenance is inherited from the caller's verified
packet; a hash chain is not reviewer authentication or proof of listening.
Resolved summaries alone cannot reconstruct or authenticate a journal: the
external resolved-results pin and acknowledged-head check remain mandatory.
Using real retained PCM is real TRAIN exposure even without original file I/O;
metadata says `original_source_files_opened=false`, `input_pcm_hashed=true`,
`source_provenance_inherited=true`, not “no music accessed.”

## F / S / U semantics

Every original contributes once, in original order, to float64 sums:

- **F — foreground:** confirmed `lead`, `co_lead`, `lead_double`, `vocal_hook`,
  `primary` or `foundation` with admitted evidence.
- **S — support:** confirmed `backing`, `secondary`, `support_percussion` or
  `background_texture` with admitted evidence and no hard-policy conflict.
- **U — unknown accounting:** everything else, including incomplete/conflicting
  reviews, low/medium/unknown confidence, contamination unknown/present, or an
  asserted support role that conflicts with the separate hard front policy.

“Admitted evidence” here means one fully covered, confirmed, high-confidence
review with protected contamination explicitly **absent**. High confidence is
a fixed first-compiler eligibility rule, not a calibrated probability or a
new journal rule. The original review—including uncertainty—is preserved.

For foreground sources, protected contamination means unintended *other*
protected content/bleed relative to the reviewed instance; the intended lead
waveform itself is not automatically labelled contamination. The compiler does
not manufacture absence in either case. A known primary role with contamination
UNKNOWN therefore still cannot provide a clean dense target in this version.

The separate conservative front-only policy covers `lead_vocal` and `bass`
identities and any confirmed foreground role. This is a policy veto, not role
truth inferred from identity: an unreviewed bass remains U, and a bass labelled
secondary remains an asserted secondary with a recorded policy conflict—not
an invented confirmed foreground label. Identity never grants support/rear
permission. A backing-labelled principal hook can be reviewed as foreground.

The compiler returns `targets[window,3,2]`, copied original PCM, an explicit
`rounding_residual = mixture - sum(original instances)`, and owned JSON metadata.
No source clipping, per-source gain, mask restriction or normalization occurs;
support can exceed mixture amplitude through cancellation. The residual is
numerical accounting, not clean foreground/support. Float32 packet closure and
float64 grouped accounting are checked separately. Inputs are not modified.

## Dense-loss eligibility and time boundaries

Dense F/S waveform and semantic-presence masks are true only when **all original
contributors** have admitted, policy-compatible reviews over the entire actual
loss-support window. U has no semantic-class or dense-waveform training mask.
There is no energy exemption: even an exactly zero unreviewed original prevents
complete admission. Missing role evidence is not absence of support.

The interval must lie wholly inside the packet's labelled core and contain at
least 1024 samples. For an STFT loss the caller resolves the whole actual
1024-sample window, not its center or end. A four-hop descriptor target needs
the union of all four contributing windows covered (1792 samples at hop256).
The compiler does not infer these masks from arbitrary frame centers or resize
reviews. Role changes require separately covered windows; crossing/partial
windows abstain. Context supplies original-only warmup, not labelled loss time.
Do not create hard waveform cuts at annotation edges and call their transients
source truth. No annotation enters normal model inference.

The current Q2 objective accepts per-case rather than arbitrary time-varying
semantic completeness. A future adapter can first make fixed-role, fully
covered cases with explicit full-support masks; it must not silently mark an
entire mixed/partially labelled case known. Target candidates and masks are not
an implemented new objective, model or authorization to run training.

## Minimal next experiment and risks

First obtain actual acknowledged TRAIN instance/interval reviews. Compare the
reference F/S scene against all-front and deliberately naive family routing:
protected leakage and retained desired support must both be measured. All-front
has zero rear leakage by construction, so leakage alone is a mute shortcut.
Only after the reference is useful should a frozen learnability protocol add a
role-conditioned decoder and independently calibrated protected-content risk.

Do not append overlapping F/S targets to the twelve identity heads under the
same `direct_power` normalization: its K axis assumes a disjoint partition.
Identity auxiliary reconstruction would need a separately defined branch and
normalization. U records missing knowledge; it is not an acoustic “unknown”
class that the network can infer from annotation incompleteness. New role
outputs also are not a drop-in replacement for the frozen twelve-head native
provider/permission ABI.

Mixture, absent-file and solo controls need separate provenance. Removing or
isolating a part changes musical context and may promote support into a lead.
Do not automatically carry a human role label across the changed context SHA.
Known-component cancellation/absence probes can measure mathematical source
passage, but do not become human-approved role examples. Principal/co-lead,
double and hook safeguards must be exercised in mixtures, including quiet
protected content; isolated-source suppression alone is insufficient.

Missing evidence remains concrete: this qualification supplies no real human
reviews or acknowledged real journal head, no complete F/S-labelled core, no
validated active replacements/additions for the four quiet source contexts,
and no independently role-reviewed artist-disjoint evaluation or listener
scene preference. The original five/59 queue and quiet cases remain unchanged.
No genre-generalization, runtime-risk, role/rear/training/car pass follows.

## Authored qualification

Twelve tests pass, including same-family foreground/support separation, full
loss-window role boundaries, partial/conflicting evidence, digital-zero unknown,
known role with contamination UNKNOWN, policy conflicts, altered bindings,
wrong partition/gain, finite/closure/cancellation and unmodified inputs. One
test uses the existing synthetic packet writer and actual journal resolver,
then forbids file opens during compilation. This is not real music admission.
