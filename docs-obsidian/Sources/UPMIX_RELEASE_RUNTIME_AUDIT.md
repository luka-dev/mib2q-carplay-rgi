# Current release runtime audit

2026-09-16. **Runtime infrastructure is available; no current model or complete
car path is approved.** This audit did not install anything, change factory
settings, open an audio device, train a model or alter the dirty production hook.
It distinguishes current execution from historical measurements and documentary
cabin assumptions. It does not replace [release acceptance](UPMIX_RELEASE_ACCEPTANCE.md).

## Current verified evidence

[Machine-readable audit](../output/release-runtime-audit-a/report.json), SHA256
`7514be85e2cd1dfa84bb76216854a337c147b1ed9e4c8192aa551a6af143bfdf`,
binds the current inspected source hashes, exact archives, commands/logs, compiler
images, fresh build and guest proof. The bounded local run took54.529s and wrote
approximately39.65MB before its report. `complete:true` means the audit finished;
`release_ready:false` and `car_approved:false` remain explicit.

- Current Mac: Darwin25.5.0 arm64; Clang21.0.0; Docker29.6.2 daemon reachable;
  QEMU11.0.3. Installed QNX GCC8.5.0 and binutils2.19.1 actually execute in the
  local linux/amd64 image, without network access or downloads. The8.5 image ID
  is `d3a1485fa996bb1473e7f614b85540d8bd1c38947f9c53f1f7b300e143b20776`.
- A fresh ARM32 little-endian EABI5/softfp build succeeded in
  [native-qnx-8.5.hLWIuB](../build/upmix-p1-control/native-qnx-8.5.hLWIuB/build-manifest.json).
  Flags include `-O2 -mfpu=neon -mfloat-abi=softfp -ffp-contract=off`; default
  libm, no fast-math or experimental SiLU. These newly built binaries were
  **not** the binaries executed below; this is separately a current compile proof.
- All36 base-ZIP members and16 addon-ZIP members passed independent size,
  manifest SHA, `SHA256SUMS` and CRC readback, with exact base/addon dependency
  bindings. No extraction, repackaging or installer. Archives remain SHA
  `a15ef9f05999830d1ad7a21d0ce06a4d98db5edac814c8f8b9ed2bc239028838`
  and `1e1f16e255e66b53ba1c6166eccd2193e5d75918d688be3eb755cdf6bd8de028`.
-21 focused host tests passed in1.463s:7 new archive/export admission tests plus
  the14 existing native/callback tests. The exporter explicitly rejects both
  current Q1 and a subclass of the otherwise exact P1 model.
- The **exact previously packaged** core `557bb012…6d9cb7` and callback
  `e52f4edd…59e78c` were executed again in an isolated QNX guest, not just found
  on disk. [Fresh proof](../output/release-runtime-audit-a/callback-proof/report.json)
  SHA `e91efda2a5fe2e15448018efa8d99ce8f0e3400a5aa55431a7b0f0bf1be5b187`:
  six existing TRAIN fixtures, max normalized PCM error1.19209289551e-7;
  host ASan/UBSan max8.94069671631e-8, unchanged rtol3e-5/atol3e-7. Exact external
  chunk/reset replay, delayed bypass, seven fault boundaries, numerical overflow,
  input sanitization, generation reset and transactional invalid arguments pass.
  LeakSanitizer was disabled. This is not a two-hour stress or current-model
  quality test. QEMU used Cortex-A15 TCG, no network and no PCM device.

The sealed P1 model still failed its musical qualification. Passing its runtime
tests neither re-admits that model nor exports Q1, Q2 or another new architecture.

## Exact boundaries and dangerous unit differences

| Boundary | Rate / layout / amplitude | Present status |
|---|---|---|
| Frozen P1 core |48kHz normalized FLOAT32 stereo → `FL FR C LFE SL SR`; C/LFE exactly0 | Native core and guest parity verified |
| P1 callback | Same FLOAT layout, accepts/emits exactlyN samples; fixed1024-sample delay | Diagnostic only; default delayed-dry |
| Separate P1 cabin helper | NormalizedFLOAT; common gain, known input-bus rail, finalS16 | Electrical proxy, not measured cabin |
| Current hook FLOAT route |48kHz, six slots; FLOAT values are in **S16 amplitude units** | Source inspected, installed state not read back |
| Current QSA sink | Interleaved `S16_LE`, `SPA_SR=48000`, negotiated channel count | Device setup source only, not exercised |
| Current Q1 Python reference |44.1/48kHz normalizedFLOAT, four family stereo outputs; residual separate | CPU reference only, not a six-slot renderer/native ABI |

Do not pass normalized output directly into the S16-scaled hook guard, or apply
the32768 conversion twice. Q1's44.1k option does not make the current48k sink a
44.1k transport. Rate conversion, if required, must be explicit and qualified.
The native APAC5.1 decoder's `C L R Ls Rs LFE` order is separately remapped
`[1,2,0,5,3,4]`; native multichannel input must not be re-upmixed. Experimental
eight-slot/height paths are outside this audit.

Evidence: [native ABI](../tools/stereo-upmix-render/p1_native.h),
[callback contract](../tools/stereo-upmix-render/p1_native_callback.h),
[strict exporter](../tools/stereo-upmix-render/export_p1_native.py),
[hook](../../hook/audio/audio_hook.c) at69,2799–2820,3295–3392,5038–5105,
and [SoundCheck implementation](../../hook/audio/soundcheck.c).

Current hook source supports FLOAT render → one uniform SoundCheck programme
gain → linked known-matrix/PCM guard → final S16. It also retains a legacy S16
route and feature gates; source support is not proof which route is installed.
Its existing error branch immediately widens the current stereo input. That is
**not latency-aligned fallback** for a new1024-sample model, and cannot be reused
unchanged for that purpose. The hook also holds an outer renderer mutex: the
isolated allocation-/I/O-/lock-free core does not prove a lock-free whole callback.

## Reusable safety and remaining integration work

The frozen core provides caller-owned, sized/aligned model/state/work buffers,
finite and endpoint checks, fixed tensor layout, bounded processing, transactional
failure, required FE_TONEAREST, CRC-protected load and monotonically increasing
model generations. SHA validation belongs outside the audio callback; CRC and
SHA are integrity checks, **not cryptographic signatures or trust policy**.
Failed model loads preserve an already valid model. The exporter admits only the
exact baseline type/config and validated named FLOAT32 tensors.

The addon supplies a separate delayed-dry ring, fixed1024-sample latency for both
wet and dry, numerical-fault latching and channel-wise nonfinite sanitization
without dropping/duplicating timeline samples. Reset discards the old tail and
starts1024 zeros; seek/song/rate/reload ownership must explicitly invoke it.
Pause without timeline advance is different from reset. Serialized load/reset is
mandatory: an illicit reload is detected at the next hop, not necessarily before
already queued output. Wet→dry is time-aligned but **not proven click-free**.

Missing release work includes a model-independent dry owner for a missing/corrupt
first model (the addon currently requires a valid model even for initialization),
click-free bounded transition policy, actual hardware callback registration,
codec/FLUSH/reset lifecycle integration, current model native export/parity,
full-song and two-hour stress, worker stall/late/drop tests at integration level,
unsupported-format handling, and downstream gain/safety on every fallback path.
Huge finite dry input is not limited by the pre-cabin callback itself. No new
architecture may inherit P1's memory sizes or native proof.

## CPU, memory and latency budget

Historical actual APQ8064/Krait upmix measurements were per1024 PCM samples at48k,
period21.333ms: HSSR-NEON mean4.283/P956.043/P998.057/max9.065ms; Object
mean5.022/max13.093ms. Loaded Object P99 was21.15ms; later contention exceeded
the period. Timer quantization was approximately1.007ms. These standalone
upmix measurements exclude parts of decode/transport/QSA and are not current
full-chain measurements. They do not establish75–80% spare CPU.

QEMU instruction units must not be confused: V12 float/safety6.612581M is per
1024 samples; P1 default7.738215M is per256-sample hop, or30.952860M per1024,
excluding cabin processing in that timed region. No conversion to ARM ms is
justified. The new current guest run above measures correctness, not new cost.
See [runtime budget and harnesses](UPMIX_R1_RUNTIME_BUDGET.md), whose historical
sources are hash-bound by this audit.

The prospective release contract chooses, rather than measures, ≤40ms algorithmic
latency, ≤32MiB task state/weights/work, loaded full-path P99≤80% of the **actual**
callback period, maximum≤100%, and zero missed deadlines/underruns during≥2h
competing load. For a1024-sample48k callback that is P99≤17.067ms/max≤21.333ms;
other callback sizes require their own period. P1's21.333ms alignment delay is
not CPU allowance, and transport/device buffering is additional latency.

Current Q1's approximately25.25M denseMAC/s at48k excludes FFT, features, four
source inverse transforms/WOLA, renderer, validation, copies and scheduling.
It is not a measured Krait budget. Existing HSSR async infrastructure moves only
its recurrent controller to a two-entry SPSC worker; FFT/masks/WOLA stay on the
audio thread. Priority17 is requested, not verified; `pthread_setschedparam`
return is ignored. Three missed control periods trigger a rear failsafe. A new
full-source decoder requires its own queue, generation and sample-deadline
contract; offloading is not free CPU.

## Cabin constraints are still mandatory

The documentary9VS topology has19 drivers/16 amplifier circuits. Rear door lower
driver+tweeter share a feed per side (leftT38/4,5; right16,17); independent
C-pillarR209/R210 amplifier outputs do not become four independent rear inputs
in our six-slot transport. No independent “send this to rear tweeter only” claim.
See [speaker evidence](SOUNDAMP_9VS_SPEAKER_PARTS_RESEARCH.md) and
[routing review](UPMIX_CABIN_ROUTING_REVIEW.md).

Keep C/LFE PCM zero under the present policy; that does not mute the physical
center or remove bass in other buses. With C/LFE zero the recovered electrical
proxy includes `frontL=.794328L+.707946SL`, `center=.501187(L+R)` and
`rearL=1.995262SL` (right analogously). Generic downmix closure is therefore not
physical front/center preservation. C1/C2 renderer math is an offline electrical
tradeoff, not an inverse cabin IR or established sound quality.

3D OFF is not DSP bypass. Surround minimum and Sound Focus All are operating
assumptions pending live readback/measurement; document the actual active state.
No extra bass/LFE/treble boost is admitted by wiring or S4 hobby measurements.
One SoundCheck programme gain, linked electrical safety and S16 bounds cannot
prove true-peak, excursion, SPL, role protection or subjective bass balance.

## Actual target access and required next procedure

Established project endpoint is `root@10.173.189.1`, from
[deploy script](../scripts/deploy_over_ssh.sh):8 and
[trace script](../../scripts/carplay_state_trace.sh):5. A prior HU investigation also
records `100.127.17.86` ([historical evidence](reference/ALTSCREEN_LATENCY_AUDIT_2026_09_14.md)).
These are distinct from NAS100.85.38.96 and the old build VM192.168.64.16.

One bounded read-only SSH attempt per established HU address on2026-09-16 timed
out connecting to port22, before authentication or remote execution. Strict host
checking stayed enabled; no passwords were printed and no new key accepted.
10.173.189.1 currently routes through default/en0 gateway192.168.0.1, whereas
100.127.17.86 has an utun12 route. The first address has an existing known-host
entry; the Tailscale alias does not. This proves neither that hardware is absent
nor that the stored address remains current.

Required external action: power/connect the HU, give this Mac its established
Audi-network route or restore the HU's Tailscale reachability, confirm the current
address, and verify identity/host key for any new alias. Then:

1. Read-only identify QNX/CPU/runtime, installed binary/model hashes and active
   configuration; inspect existing logs without truncation/restart. Record
   3D/Surround/Focus, sample format/rate and clock resolution. Never run the deploy
   script merely to test access—it remounts/writes and restarts `dio_manager`.
2. After a useful model/native path is frozen, authorize an isolated temporary
   diagnostic staging directory outside factory/installed paths, hash readback,
   and no-audio target correctness/cost tests. Present audit has not staged this.
3. Separately authorize controlled playback/integration with backup, rollback,
   safe level and recovery plan. Obtain full-chain ≥2h loaded timing/underrun
   evidence and actual front/rear/bass/listening measurements. No candidate
   installation follows automatically from this document or a QEMU pass.

## Reproduce the isolated local audit

Run from repository root with a **fresh** output path:

```sh
env OPENBLAS_NUM_THREADS=2 OMP_NUM_THREADS=2 MKL_NUM_THREADS=2 \
  PYTHONDONTWRITEBYTECODE=1 /opt/homebrew/opt/python@3.14/bin/python3.14 \
  tools/stereo-upmix-render/audit_upmix_release_runtime.py \
  --output output/release-runtime-audit-NEXT \
  --test-native --build-core --verify-guest
```

The new scaffold has no SSH/install/audio-device path. It creates fresh outputs,
checks a10GiB disk reserve, bounds archive reads, retains incomplete evidence,
binds source hashes before/after and never upgrades `car_approved`. The existing
legacy HSSR benchmark scripts replace their dedicated build directory; do not
run them over old evidence without first making a separate frozen source owner.
