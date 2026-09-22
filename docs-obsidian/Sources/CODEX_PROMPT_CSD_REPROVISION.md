# Task: re-provision the CSD session when a six-voice channel attaches to an already-routed timer

## Your diagnosis is confirmed on the car — this is the follow-through

The `deva-ctrl-qc-csdlive.so` patch was built and run on the unit. It redirects
`0x54a14` in `deva_update_session_info_from_list` to a helper that prefers the live PCM
channel (timer `+0x11c` -> channel, `+0x10` voices, `+0x94` fragment) and falls back to the
record minima when no channel is attached. Assembled with the QNX toolchain and verified by
disassembling it back out of the image; see `deploy/deva-ctrl-qc/patch_deva_csd_live.sh`.

The car still ticks, and the unit's own slog says exactly why:

    00:00:38  session_csd_setup: Setting up session while DEVICE_ID = <4> is being attached
    00:00:38  session_csd_setup: pcm format => sample_rate=48000, channels=2, bit_per_sample=24
    00:00:39  session_deva_timer_start: ENTER ... LEAVE (rc=0x0)
    00:00:52  [hook] audio sink: setup actual=6ch/48000Hz/9216-byte fragment

The route is attached and the timer is already **running** fourteen seconds before the PCM
channel exists. So the helper legitimately takes its fallback, the session is built at
`channels=2` / `0x1800`, and nothing rebuilds it when the six-voice channel arrives. This is
the route-before-open ordering your audit predicted, now measured rather than assumed.

Your analysis is therefore right and complete about the cause. What is missing is the
transition.

## What to work out

`deva_mib2_aquire @0x56060` stores the channel pointer at timer `+0x11c` (at `0x56504`) and
the active fragment at channel `+0x94`. At that moment the attached CSD session still carries
the old geometry at session `+0x84` / `+0x6c`.

The driver exposes `session_csd_teardown`, `session_csd_setup`, `session_csd_attach_device`,
`session_csd_detach_device`, `session_csd_stop`, `session_csd_start`, plus
`session_deva_timer_start`. Determine:

1. **The safe sequence** to rebuild an attached, already-started session in place: which of
   stop / teardown / setup / start must run, in what order, and whether the timer must be
   stopped around it. Your audit already warned that overwriting session `+0x84` and `+0x6c`
   alone is unsafe because the stream, context and buffer pool were created with the old
   geometry — so name the calls that actually recreate them.
2. **The right hook point.** Candidates: the tail of `deva_mib2_aquire`, `deva_mib2_prepare`
   @0x55394 (currently a logging no-op, so it is cheap to take over), or
   `deva_mib2_trigger` @0x5598c on start. Which one runs after the channel pointer is stored
   and before audio flows, and which one can block long enough to rebuild a session safely?
3. **The guard.** The rebuild must fire only when the attached session's `+0x84` disagrees
   with the live `channel->voices`, so that stereo — which is measured excellent today — takes
   no new code path at all.
4. **The register and stack state** at the chosen site, and a cave large enough for the
   sequence. `0x8d090..0x8d0df` is taken by the current helper; `0x8f2c8` by the older
   fragment helper. Both live in truncated `__FILE__` strings, which is the established
   technique here.

## Deliverable

The exact call sequence with addresses, the chosen hook site with its register state, the
guard condition, and the assembly to install — or, if rebuilding in place is not safe from any
of those sites, say so plainly and name what would be (for example forcing a detach/attach
cycle from the PCM path).

Do not hand-encode instructions: this project assembles patches with the QNX toolchain and
disassembles them back out of the patched image to check. Do not modify the hook. State
clearly what remains unproven.

## Context you already own

`docs-obsidian/Sources/ROUTER_INPUT_WIDTH_AUDIT.md` (cause), `docs-obsidian/Sources/AUDIO_5_1_STATE.md` (whole picture),
`deploy/deva-ctrl-qc/README.md` (both existing patches and why the minima must not be widened:
that would make every stereo open fail).
