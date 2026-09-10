# CarPlay call/Siri priority over buffered media

2026-09-10. Implemented and host-tested; **not yet verified with a live HU call**.
No change to voice codecs, microphone PCM, upmix, LFE or loudness processing.

## Evidence and scope

The local iOS 26.1 AirPlaySender dump constructs `type=100` streams with
`audioType=telephony`, optional `vocoderInfo` and the `input` flag. Its
`carPlayAudioFormat_getFormatsKeyForAudioType` selects `audioInputFormats` for
main telephony/speechRecognition, independently of media output capabilities.
See `AirPlaySender_01.mm` around line 4070 and `AirPlaySender_13.mm` around 9479
under `Refferences/iPhone18-3_26.1_23B85_Restore/.../AirPlaySender/`.

Pioneer SPHERA 01030010 `libSPF_CP_PlugInWrapper.so`, `AudioStreamStart`
at `0x14d210`, starts main input when requested and main output separately
from buffered 103 and alternate 101. Type 102 is not a prerequisite for calls;
100 is not a synonym for music, and 101 is not a synonym for Siri.

MU1316 MHI2Q P5087 stock `eso/lib/libairplay.so`, verified in IDA:

| Function | Address | Relevant behavior |
| --- | --- | --- |
| `get_audio_path` | `0x49398` | Telephony 3 → `tel2_dio_phone`; speech 4 → `ann4_dio_voice`; media → `mpl5_dio_ent`. |
| `AudioStreamPrepare` | `0x4a9d0` | Reads 32-bit streamType at object byte offset 108 and audioType at 112. Main 100 declares audioType, waits for HMI ACK, then opens QSA. Input uses `mic2_dio`. |
| `AudioStreamStop` | `0x4a2f8` | Synchronously joins workers and calls teardown callbacks before returning; also closes QSA objects. |
| `_AudioStreamCopyProperty` | `0x49a90` | Requires prepared state and does not return audioType; unsuitable for pre-Prepare classification. |
| `AudioStreamStart` | `0x4a73c` | On failure calls `AudioStreamStop` for cleanup. |

The stock ELF has `R_ARM_JUMP_SLOT` relocations for both Prepare and Stop;
the hook exports both entry points via `carplay_hook.exports.map`.
Private fields are read only after the existing reviewed AirPlay 210.81
firmware fingerprint passes; offsets are part of that profile, not generic
assumptions about every AirPlay implementation.

MU1316 `dio_manager`: `s_audioTypeCallback` at `0x154e58` queues the global
main-audio type; `s_audioTornDownCallback` at `0x154dbc` queues -1. Neither is
scoped to a QSA handle. The Java `CarPlayDSIManager.updateMainAudioType` replaces
the active audio resource, so an out-of-order hook Media request can interfere
with telephony even though the underlying PCM devices are separate.

## Fixed race

Previously a retained media QSA handle scheduled a Media declaration 50 ms
after stock close. The timer did not know whether phone/Siri had started, and
ran before checking whether 103 was playing. A paused, still-live 103 could
therefore overwrite a newer Telephony declaration. Hook teardown also cleared
its ownership flag under a lock but called global tornDown after unlocking.

The new ownership policy in `hook/audio/media_route.h` is:

1. Before **every main-100 Prepare**, register the stock object and relinquish
   hook ownership without sending tornDown. All main purposes take precedence,
   not only the currently known phone/Siri values. Alternate 101 is unaffected.
2. Keep the object registered throughout stock activity and Stop, including its
   worker joins and teardown callback. An older media Stop cannot release a
   newer phone or Siri object. A failed new Prepare removes only its own new
   registration; a duplicate failed Prepare preserves the existing owner.
3. Cold bootstrap and the detached 103 writer cannot claim Media while any
   stock main object is registered. The detached pump also yields instead of
   consuming PCM during this interval.
4. After the last stock Stop, apply the existing 50-ms settle interval. Reclaim
   Media only with PLAYING, primed, full-period buffered PCM. A living paused
   receiver never forces playback or route acquisition. A stock media reopen
   takes ownership normally instead.
5. Check hook ownership and issue any global release under the same `ioLock`
   as the stock-begin transition. Never send a hook release after ownership
   has transferred to stock. Never hold this lock over real Prepare/Stop:
   their HMI wait, QSA re-entry and worker joins must run without it.

Tracking uses eight fixed slots, no new thread and no heap allocation. Capacity
overflow fails closed for hook route claims for the process lifetime, leaving
stock calls operational. The type-103 decoder/queue and stock media QSA bridge
are otherwise unchanged. The supported lifecycle assumes stock serializes
operations on one AudioStream object; distinct overlapping objects are tracked
independently.

## Validation and next vehicle check

- `scripts/run_tests.sh`: PASS, including `media_route_test` and existing
  buffered lifecycle, PCM, soundcheck and real APAC capture regression tests.
- New tests cover music→call→resume, cancelled/failed Prepare, duplicate
  Prepare, phone/Siri overlap, cold 103 during a call, pause, insufficient PCM,
  teardown103 during a call, capacity overflow and 20,000 concurrent stock /
  buffered ownership iterations per thread.
- GCC 8.5 QNX/ARM build: `LOG=0 AUDIO_STATE_TRACE=1`; exact-export and loader
  safety checks pass. These are host/state-machine and binary checks, not a
  substitute for a live phone/HU audio test.

On HU, test incoming answer and reject, outgoing call, remote/local hangup,
Siri, phone foreground/background and reconnect during a call. Check both
microphone and remote voice, then music resume (including deliberately paused
music). Use `AUDIO_ROUTE action=stock_begin phase=prepare audio_type=3/4` and
the matching `stock_end phase=stop_done` events. Between them there must be no
hook `action=request` or `action=release`. After hangup, expect either stock
main media ownership or a hook request after buffered PLAYING resumes.

No deployment or audible call verification was performed as part of this change.
