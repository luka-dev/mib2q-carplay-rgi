#!/bin/sh
# Thin smartphone_integrator child wrapper.
# Publish this generation, start the renderer monitor, then immediately become
# dio_manager so SI retains exact PID/watchdog ownership.

DIODIR=${DIODIR:-/mnt/app/eso/bin/apps}
H=${H:-/mnt/app/root/hooks}
WLOG=${WLOG:-/tmp/carplay_wrapper.log}
OWNER_FILE=${OWNER_FILE:-/tmp/carplay_supervisor.owner}
DIO_PID=$$
MONITOR_PID=

startup_fail()
{
    echo "[startup] abort pid=$DIO_PID" >> "$WLOG"
    [ -n "$MONITOR_PID" ] && kill -15 "$MONITOR_PID" 2>/dev/null
    exit 127
}
trap 'startup_fail' 1 2 15

[ -x "$DIODIR/dio_manager" ] || {
    echo "[startup] missing $DIODIR/dio_manager" >> "$WLOG"
    exit 127
}
[ -r "$H/libcarplay_hook.so" ] || {
    echo "[startup] missing $H/libcarplay_hook.so" >> "$WLOG"
    exit 127
}
[ -x "$H/carplay_monitor.sh" ] || {
    echo "[startup] missing $H/carplay_monitor.sh" >> "$WLOG"
    exit 127
}
[ -r "$H/carplay_processes.sh" ] || {
    echo "[startup] missing $H/carplay_processes.sh" >> "$WLOG"
    exit 127
}
cd "$DIODIR" 2>/dev/null || startup_fail

# Never expose a partially-written owner file to an older monitor.
OWNER_STAGE=${OWNER_FILE}.${DIO_PID}
echo "$DIO_PID" > "$OWNER_STAGE" || startup_fail
mv "$OWNER_STAGE" "$OWNER_FILE" || startup_fail
echo "===== CarPlay generation pid=$DIO_PID ppid=${PPID:-unknown} =====" >> "$WLOG"

# The NCM generation this session starts on, for carplay_ncm_cleanup.sh (SI's
# otgStackCleanupScript): only that carplay0 may be destroyed after this session.
NCM_GEN=
[ -r /tmp/ncm_generation_carplay0 ] && read NCM_GEN < /tmp/ncm_generation_carplay0
if [ -n "$NCM_GEN" ]; then
    echo "$NCM_GEN" > /tmp/ncm_session_generation.$DIO_PID && mv -f /tmp/ncm_session_generation.$DIO_PID /tmp/ncm_session_generation
else
    rm -f /tmp/ncm_session_generation
fi

LD_PRELOAD= "$H/carplay_monitor.sh" "$DIO_PID" </dev/null >>"$WLOG" 2>&1 &
MONITOR_PID=$!

# Only dio_manager receives the hook. The monitor and both renderers explicitly
# clear LD_PRELOAD.
export LD_PRELOAD="$H/libcarplay_hook.so"

# Native cluster altScreen (stream 111).
export ALTSCREEN_HANDLE_ALT=1
export ALTSCREEN_ADVERT_DISPLAYS=1

# Buffered CarPlay audio policy.
# 1 = this unit (external J525 amp, 6-channel MOST route). The internal-amp thread's policy is
# AUDIO_SPATIAL=0 unless E2P 0xFC reads 02 (docs/audio/DISCRETE_5_1_ROUTING.md); that gate belongs to the
# installer, not to this unit's startup file.
export AUDIO_SPATIAL=1
export AUDIO_SPATIAL_ATMOS=0
export AUDIO_SPATIAL_ATMOS_MODE=downmix
export AUDIO_STEREO_UPMIX=on
export AUDIO_HSSR_MODEL="$H/hssr_model.bin"
export AUDIO_STEREO_SURROUND=0.40
export AUDIO_STEREO_LFE=assist
# Sub level, dB re the calibrated assist (-24..+12). 0 = 2026-09-07 calibration; the 18.09 build ran at +9.5 and boomed.
export AUDIO_STEREO_LFE_GAIN=0
# Front-only bass trim (dB, -12..0): our Bass knob for the 5.1 bank; sub and rear PCM untouched
# (in the amp model front bass still moves the sub by about +3 dB).
export AUDIO_STEREO_FRONT_BASS_DB=-3
# Scene bed (2026-09-21): keep half of each second-scene object in front and decorrelate the
# bed with per-band delays.  The attack detector (owner/front) and the HF retention taper that
# rides on it were turned OFF on 2026-09-25: on 12 MoisesDB songs the per-hit collapse was the
# largest jump generator of every stem (flicker -30..40 %, back-and-forth -20..30 % without it)
# and the taper added another +0.07.  docs/audio/UPMIX_STABILITY_2026-09-25.md.
export AUDIO_STEREO_FRONT_RETENTION=0.5
export AUDIO_HSSR_BED_DELAYS=on
export AUDIO_HSSR_BED_ATTACK=off
# Vocal gate (2026-09-25): the model calls a voice in every band, but three slow-release
# memories let a band that just belonged to a guitar send the next syllable's top to the rear
# (vocal 2.5-10 kHz was 34-38 % rear, flickering at syllable rate).  The gate applies the
# model's per-band vocal veto to the rendered object instantly, held ~0.35 s, per bin by
# comodulation with the voice body.  0 = the 22.09 behaviour.  0.95 was the setting without
# the mask net; with it the gate is the "soft veto" and 0.5 is the operating point measured on
# the 40 DEV songs (vocals 4 % rear, guitars 7 %, 2/40 swinging; 0.95 halves the guitars).
export AUDIO_HSSR_VOCAL_GATE=0.5
# Mask net (2026-09-27, Phase 1 of docs/audio/UPMIX_SOURCE_SEPARATION_PLAN.md): a per-cell
# rear-eligibility student (models/mask-full-probe-v21a) run on the control worker; the object
# path becomes sqrt(eligibility) * drive with the bin's own balance as its pan, the vocal gate
# stays on top; three late controls hand the object back to the heuristic path.  DRIVE 0 = off
# (the heuristic object path), 1.0 = the measured operating point, range 0..2.
export AUDIO_HSSR_MASK_MODEL="$H/hssr_mask.bin"
# 1.0 measured "confident" on the PC but thin in the car (guitars 7 % rear vs 17 % in the 22.09
# build the ear remembers); 1.7 brings the 22.09 fullness back together with its voice leak
# (10 %); 1.5 = guitars 17 %, voice 8 % (as 22.09, on half as many songs above 15 %), guitars'
# swing a quarter smaller than 22.09 at equal level (car + 40-song harness, 2026-09-27 evening).  AUDIO_HSSR_MASK_TREBLE_DB and
# AUDIO_HSSR_MASK_GAMMA exist (0 / 1.0 = off); both pull drums and voice back faster than
# guitars on the harness, so they stay off.
export AUDIO_HSSR_MASK_DRIVE=1.5
# Rear air bed (2026-09-25): the input above ~2.5 kHz copied into the scene bed at a constant
# level (rides the bed delays, never subtracted from the front).  Gives the rears back the
# treble the gate and the attack-off removed (-12 dB = the 22.09 rear treble level on 12
# MoisesDB songs) without the per-hit decisions that made it jump.  Unset = off.
# -9 since 2026-09-27: the mask keeps instrument bodies and leaves the rear ~1.6 dB darker above
# 2.5 kHz than 22.09; 3 dB more static air brings the brightness back without per-hit decisions.
export AUDIO_STEREO_REAR_AIR_DB=-9
# Front-only treble trim (dB, -12..0, high shelf @4 kHz), unset = off.  The static lever if the
# front sounds too bright (21.09 complaint): -3 lands near the 22.09 front balance without touching
# the object decisions.  Retention and the taper darken the front too, but they flicker.
# export AUDIO_STEREO_FRONT_TREBLE_DB=-3
# HSSR upmix also for the stock realtime media stream (type 100, audioType=media: Spotify and
# every app without buffered type 103). Own DSP state and controller; plain L/R while a
# phone/Siri/alert stream is tracked or while type 103 owns the rail. off = zero rears as before.
export AUDIO_REALTIME_UPMIX=on
export AUDIO_SOUNDCHECK=1
export AUDIO_SOUNDCHECK_FALLBACK=off
export AUDIO_SOUNDCHECK_LIMITER=off
export AUDIO_BUFFERED_THREAD_PRIO=18
export AUDIO_SPATIAL_CODEC_LIB="$H/libcarplay_apac.so"
export SPATIAL_103_EXTRA_INDEX=23

echo "[startup] exec dio_manager pid=$DIO_PID monitor=$MONITOR_PID" >> "$WLOG"
exec "$DIODIR/dio_manager" "$@"

# Preflight makes this reachable only for an exec/runtime loader failure.
STARTUP_EXEC_RC=$?
echo "[startup] exec dio_manager failed rc=$STARTUP_EXEC_RC" >> "$WLOG"
startup_fail
