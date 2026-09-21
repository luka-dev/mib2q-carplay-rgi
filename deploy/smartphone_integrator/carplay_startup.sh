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

LD_PRELOAD= "$H/carplay_monitor.sh" "$DIO_PID" </dev/null >>"$WLOG" 2>&1 &
MONITOR_PID=$!

# Only dio_manager receives the hook. The monitor and both renderers explicitly
# clear LD_PRELOAD.
export LD_PRELOAD="$H/libcarplay_hook.so"

# Native cluster altScreen (stream 111).
export ALTSCREEN_HANDLE_ALT=1
export ALTSCREEN_ADVERT_DISPLAYS=1

# Buffered CarPlay audio policy.
export AUDIO_SPATIAL=0
export AUDIO_SPATIAL_ATMOS=0
export AUDIO_SPATIAL_ATMOS_MODE=downmix
export AUDIO_STEREO_UPMIX=on
export AUDIO_HSSR_MODEL="$H/hssr_model.bin"
export AUDIO_STEREO_SURROUND=0.40
export AUDIO_STEREO_LFE=assist
# Sub level, dB re the calibrated assist (-24..+12). 0 = 2026-09-07 calibration; the 18.09 build ran at +9.5 and boomed.
export AUDIO_STEREO_LFE_GAIN=0
# Front-only bass trim (dB, -12..0): our Bass knob for the 5.1 bank; sub and rear untouched.
export AUDIO_STEREO_FRONT_BASS_DB=-3
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
