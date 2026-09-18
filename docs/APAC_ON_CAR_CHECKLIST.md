# APAC on the car: what to check, and what each failure looks like

> **Blocked before deployment, 2026-09-02:** do not start this checklist with
> `spatial-5.1` on the current deva binary. Direct QSA playback proved MPL5
> clean at two voices and broken at six. Its DEVA timer/DMA side is correctly
> six-channel, but the attached CSD stream remains stereo/`0x1800`; see
> [`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md).
>
> **FIXED 2026-09-02, confirmed on the car.** Not by rebuilding the session late -- the route
> attaches fourteen seconds before the PCM channel exists -- but by making mpl5's caps record
> mirror mpl1's, so the session is built six-channel at attach time. `AUDIO_SPATIAL=1` is the
> default again and stereo still works. See `deploy/deva-ctrl-qc/BUILD_CHAIN.md`.

> Current source status: the decoder ships separately as `libcarplay_apac.so`, and the hook drains
> its PCM ring into the existing `mpl5_dio_ent` QSA handle through the stock
> `snd_pcm_plugin_params/setup/write` cadence. The index-65 decode chain is bench-verified; the QSA
> bridge and the complete index-66 chain still require live vehicle validation.

The index-65 decoder is finished and proven on the bench. Index 66 is accepted as eight-channel
APAC, validates the known 5.1.2 layout and reaches the implemented `downmix`, `drop`, or
`passthrough` stage, but no complete real index-66 APAP capture has yet been differential-tested.
Nothing in this document has been done on the vehicle yet, because deployment is the owner's call
and the head unit is never restarted from here.

What "proven on the bench" means, precisely, so the on-car result can be judged against it:

* the port decodes the live capture and differs from Apple's own decoder on **1 S16 LSB at 0.005 %
  of 24 434 688 samples**, entirely because of an FFT difference that cannot be removed — the
  library wraps Apple's closed `vDSP_DFT_Execute`, which does not exist on the head unit;
* with the port's own transform substituted into the original decoder, the difference is **exactly
  zero** on every sample, so everything above the FFT is bit-identical;
* `scripts/run_tests.sh` drives the hook's own chain — framer, decrypt, TLV split, decode,
  channel permutation — over the whole capture: 3980 frames authenticate, the 3977 APAC packets
  extracted are byte-identical to the reference, and the permutation is checked on real audio;
* `scripts/build_hook.sh` builds `libcarplay_hook.so` and optional `libcarplay_apac.so` separately;
  both pass `emutls=0 exceptions=0 rtti=0 stl=0 libstdc++=0`, and the main hook has no hard codec
  dependency.

## Before deploying

`scripts/deploy_over_ssh.sh` now installs the complete packaged release through
`install_MoreIncredibleBash/mod/custom.sh`: native artifacts, runtime scripts,
Java, SI child configuration and persistent PF rules. Disconnect CarPlay first;
the installer refuses a running `dio_manager` and never stops it automatically.
Reboot manually after installation. See the
[installer guide](../install_MoreIncredibleBash/README_CARPLAY.md).

The profile is set by `scripts/set_spatial_profile.sh`, which edits the exact
`AUDIO_SPATIAL*` policy exports. For now use only `fallback-stereo`; the former
instruction to start with `spatial-5.1` is suspended until the CSD-width fix is
implemented. A profile change applies to the next `dio_manager` generation; no
reboot.

## The first question: does it decode at all

Grep the slog for `buffer103: APAC decoder up`. On a healthy start it carries the geometry the
cookie declared. Index 65 must report six channels; index 66 must report eight:

```
buffer103: APAC decoder up id=<cookie> 48000 Hz 6 ch 1024 frames/packet
buffer103: APAC decoder up id=<cookie> 48000 Hz 8 ch 1024 frames/packet
```

If that line never appears, the render state says why, and each state is a different problem:

| state | meaning | where to look |
| --- | --- | --- |
| `idle` | type 103 never became active | the profile, private AirPlay profile, and whether iOS selected index 65 or 66 |
| `waiting-for-cookie` | key in hand, no usable magic cookie yet | `SETMAGICCOOKIE` handling; the stream may have torn down first |
| `no-key` | SETUP carried no usable `shk` | the SETUP descriptor path, not the decoder |
| `unsupported-format` | this rail received an index other than 65 or 66 | advertisement/selection policy |
| `unsupported-layout` | cookie geometry/tag does not match the selected 5.1 or 5.1.2 contract | inspect the cookie before adding a mapping |
| `cookie-failed` | the cookie would not configure a decoder | compare the cookie against the bench one |
| `no-memory` | allocation failed inside `dio_manager` | allocation size and available process memory |
| `decoder-unavailable` | the optional codec could not be loaded or rejected ABI v1 | deployed path, permissions, loader diagnostics, and codec build |

For index 66 the hook accepts only CoreAudio tag `0x00c20008`, whose defined order is
`L R C LFE Ls Rs Ltm Rtm`. Any other eight-channel cookie remains a deliberate refusal: it must be
captured and mapped explicitly rather than guessed. `downmix` folds `Ltm/Rtm` into `Ls/Rs` at
-6 dB, `drop` discards them, and `passthrough` is forbidden until both the HU PCM path and amplifier
really accept eight channels.

## The second question: does it sound right

Correct decoding and correct routing are different failures and they present differently.

* **Wrong channel order** — dialogue from a surround, bass from the wrong speaker, or a channel
  that should be low-passed carrying full-range content. The permutation is
  `spa_reorder_5_1_decoder_to_declared_s16`, and it is `C L R Ls Rs LFE` in, `L R C LFE Ls Rs` out.
  That the decoder emits the first is established three ways: measured on 74.6 s of audio, a
  locally encoded round trip, and the stream's own layout tag `0x007c0006`, which CoreAudio
  resolves as exactly that. Where it disagreed with Pioneer's format-index table, the cookie won.
* **Dropouts or judder** — a real-time problem, not a decode problem. Decoding replaced the
  receiver's artificial one-packet-per-frame-period sleep with real consumption, so backpressure
  now comes from the sink. If the ring underruns, the decoder is not keeping up. See the timing
  note below.
* **Silence with the decoder up** — the ring is filling but nothing drains it, i.e. the DIO5 side,
  not the codec.

## The third question: does QSA drain the ring

The decoder-up line proves only APAC -> PCM. The existing Audi output handle must also be captured,
widened, and written successfully. Expect these lines in order for the first index-65 run:

```
audio sink: captured mpl5_dio_ent playback handle=<ptr>
audio sink: params source=2ch target=6ch format=<S16LE> result=0 why=0
audio sink: setup actual=6ch/<bytes>-byte fragment -> stock=2ch/<bytes>-byte fragment
audio sink: writes=<n> buffered=<n> underrunFrames=<n> shortRingFallbacks=<n> avoidedSilenceFrames=<n> lastReady=<n> source=2ch target=6ch ringRead=<n>/<n>
```

`buffered` must increase after the decoder enters `running`, and `ringRead` must become non-zero.
`underrunFrames` must remain zero: the bridge now forbids partial buffered periods and never
zero-pads their tails.  A `shortRingFallbacks` increment is acceptable at a lock or codec-switch
boundary when `avoidedSilenceFrames` records the missing tail and all QSA/device underrun counters
remain zero.  A counter that continuously races upward still means the producer, timing, or sink
cadence is not stable. If the decoder is running but no
`audio sink: captured` line appears, the remaining fault is the QSA handle/name or call seam rather
than APAC.

## What the bench could not answer

1. **Real-time headroom on the actual silicon.** The stream is 46.9 packets per second of
   six- or eight-channel 48 kHz, and the decoder runs inside `dio_manager` alongside everything
   else. Any figure produced under emulation is a bound with assumptions attached, not a
   measurement of a Krait. The honest test is the car.
2. **Behaviour across a pause and resume.** An Audio Synchronization Packet is the only legal entry
   point into a running stream, so it is also the only legal resume point after a `FLUSHBUFFERED`.
   The whole captured stream is ASPs, which is why this matters and why it was ported rather than
   treated as a formality — but a real pause/resume cycle has never been driven.
3. **A long session.** The capture is 84.8 seconds. Nothing has run for an hour.
4. **The complete real index-66 path.** Cookie/layout validation and PCM8 -> PCM6 helpers are tested,
   but a real eight-channel APAP corpus has not been decoded and compared with an Apple/Pioneer
   oracle end to end.
5. **Anything the captured 5.1 content never did.** BWE synthesis and LRVQ are now ported and wired,
   but the live index-65 capture enters neither active mode. A non-joint channel pair, a non-empty
   channel remap and more than one codec config are also implemented but not exercised by that
   capture. A stream that uses any of them is new territory until it gains a differential fixture.
6. **Exact render timeline behaviour.** The hook acknowledges and stores rate/anchor/network-time
   controls, while PCM is currently drained FIFO at the stock type-100 QSA cadence. Hardware must
   prove that this is sufficient; persistent start offset or drift requires anchor-aware prebuffer
   and scheduling rather than codec changes.

## If it goes wrong

`scripts/set_spatial_profile.sh fallback-stereo` disables both Spatial requests. The type-100 stream
is the runtime fallback while Spatial is enabled; before the APAC decoder reaches RUNNING, the QSA
bridge maps its L/R samples into the configured PCM6/PCM8 frame and zeros the remaining channels.
