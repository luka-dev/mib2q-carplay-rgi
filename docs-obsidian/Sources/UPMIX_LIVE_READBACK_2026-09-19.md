# J525 B-state readback attempt — 2026-09-19 EDT

Sep20 local follow-up: common-input EQ and mode5 scenario integration is now
available (see [input-path audit](SHARC_INPUT_PATH_2026-09-20.md)), but the
external-J525 transport prerequisite below is still unmet. This follow-up made
no new vehicle connection or diagnostic request. Historical SSH observations
below are not a fresh live-state check or confirmation of the active EQ bank.

Read-only session, approximately 2026-09-20 02:10–02:20 UTC. No deployment,
settings changes, diagnostic writes, process restarts or memory pokes.

## Result

MMX SSH and RCC qnet access work. **J525 SHARC state was NOT read.**
The previous proposal to read C4ADD/BC9EC omitted an implementation prerequisite:
there is no verified ready-to-use external-amplifier memory-read transport in
the inspected project/tools. SSH access to the head unit is not that transport.
Neither live B=0 nor the active Bank1/profile/mixer row is established here.

## Observed on the vehicle

- `uname -a`: QNX mmx 6.5.0, ARM little-endian.
- RCC `AudioProcess` command line includes `--amptype=EXT` and
  `/etc/MIB_AudioTable_AU_G22.xml`. This is the HU configuration, not J525 RAM.
- Startup script AND the current `dio_manager` environment contain:

  ```text
  AUDIO_STEREO_UPMIX=on
  AUDIO_STEREO_LFE=assist
  AUDIO_STEREO_LFE_GAIN=0
  AUDIO_STEREO_FRONT_BASS_DB=-3
  AUDIO_STEREO_SURROUND=0.40
  AUDIO_HSSR_MODEL=/mnt/app/root/hooks/hssr_model.bin
  AUDIO_SOUNDCHECK=1
  AUDIO_SOUNDCHECK_FALLBACK=off
  AUDIO_SOUNDCHECK_LIMITER=off
  ```

  Read via `pidin -p dio_manager env`, filtered locally to `AUDIO_*` only.
  This verifies process environment, not independently the computed DSP gain
  or the MMI Bass/Subwoofer controls. The earlier PID3186823 disappeared during
  inspection; refreshed PID18030727 was observed. The agent did not restart it;
  the reason for that lifecycle change was not investigated.
- `/tmp/carplay_hook.log` contains repeated `source=2ch target=6ch`, including
  log time00:11:48.681, writes37888, underrunFrames0 and shortWrites0; a later
  generation starts with writes1 at00:12:15.410. These timestamps are the
  unit's log times, not trusted wall-clock timestamps. This is HU output
  evidence, not a J525 Bank1 readback or proof of nonzero music samples.
- On-disk `/mnt/app/root/hooks/libcarplay_hook.so`:674618 bytes,
  SHA-256 `6c0baa3baaec17f34fe27c860cc252898063f2c13db376be8494f790d4b6dd5e`.
  Read over SSH, marker-stripped, ELF magic checked, hashed locally at
  2026-09-20T02:16:31Z. This is file identity, not a hash of mapped process pages.
  Thus the old handoff table's build/size/sha is no longer current.
- Existing RCC `sloginfo` output had no matches for the selected J525/SoundAmp/
  Symphoria/3D/SoundSettings/readback terms. Trace settings were not changed;
  no matches are not evidence that processing is absent.

## Why reading has stopped short of J525

RCC exposes `/dev/io-amp` and `/dev/dspipc`, but these route to its internal
TI DRA6xx/C6000 DSP. They are not the external J525 ADSP-2148x SHARC memory.
See [internal amplifier distinction](INTERNAL_AMP_PATH_KNOWLEDGE.md#15-what-the-internal-amplifier-physically-is-rcc).

The external amplifier has an identified matrix-row read handler `D39B6`,
but its outer service identifier/transport remains unverified in
[the matrix audit](SHARC_INPUT_OUTPUT_MATRIX.md). Even exposing that handler
would read matrix data, not automatically arbitrary SHARC state. No guessed
MOST/UDS payload was sent; no diagnostic/programming session was entered.

## Next prerequisite

Recover a read-only route all the way from a supported MOST/diagnostic request
through V850 to SHARC memory and its response, including allowed addresses,
word addressing and byte order. If the firmware has no such exposed route,
use an independently authorized passive V850→SHARC capture or hardware readout;
do not invent a head-unit memory address alias for the external amplifier.

Only after establishing that transport collect C4ADD (including1292D8 tracing
indirection), B95E2, BC9EC, B89BA, B89B9, object+1128, actual mixer row and
ideally B audio during known nonzero input. User confirmation of playback and
MMI Bass/Subwoofer positions was requested but was not available for this report.

Static finding remains: in callback state3 BC9EC==1 clears; !=1 calls library.
Host20001 selects processing;20003 selects copy, not silence. These facts
do not prove the current vehicle state. See
[activation audit](SHARC_REMAINING_BLOCKS_2026-09-19.md#activation-audit-b0-is-not-established-playback-state).
