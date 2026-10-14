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

# Vehicle data to the phone and phone data to the cluster over iAP2 (docs/connect-and-usb/VEHICLE_DATA_TO_PHONE.md).
# IAP2_VEHICLE_STATUS: VehicleStatusUpdate 0xA101 (range, outside temperature, low-range warning) plus the Identify
#   components VehicleInformation (20) and VehicleStatus (21); needs the 0xA100-0xA102 IDs in dio_manager.json (installer).
# IAP2_VEHICLE_ENGINE: engine type advertised in VehicleInformationComponent: gasoline | diesel | electric | cng.
# IAP2_VEHICLE_SPEED: $PASCD wheel-speed sentences (0xFFFB) once the phone asks for them; needs "nmea":"0x000B"
#   in dio_manager.json (installer) so dio advertises VehicleSpeedData.
# IAP2_PHONE_BATTERY: iPhone battery and charging state (PowerUpdate 0xAE01) to the cluster phone tab.
# Values are read by value (on/off), never by presence.
export IAP2_VEHICLE_STATUS=on
export IAP2_VEHICLE_ENGINE=gasoline
export IAP2_VEHICLE_SPEED=on
export IAP2_PHONE_BATTERY=on

# Buffered CarPlay audio policy.
# 1 = the 6-channel path (external MOST amp: B&O J525 on this unit). The installer reads the amplifier
# coding (RCC E2P 0xFC) and rewrites this line to 0 unless it is 02 (external): the internal amp has no
# 6-channel route (docs/audio/INTERNAL_AMP_PATH.md).
export AUDIO_SPATIAL=1
export AUDIO_SPATIAL_ATMOS=0
export AUDIO_SPATIAL_ATMOS_MODE=downmix
export AUDIO_STEREO_UPMIX=on
export AUDIO_STEREO_LFE=assist
# Sub level, dB re the calibrated assist (-24..+12). 0 = 2026-09-07 calibration; the 18.09 build ran at +9.5 and boomed.
export AUDIO_STEREO_LFE_GAIN=0
# Scene renderer (2026-10-09, docs/build-and-deploy/SCENE_BACKEND_DEPLOY.md): the stereo-to-5.1 scene
# renderer of docs/audio/UPMIX_SCENE_ALGORITHM_2026-10-08.md (sections 14-18), variant J (rear delay 0,
# full-rear shaping), the only renderer since the Universal retirement of 2026-10-09
# (docs/audio/UPMIX_CODE_RETIREMENT_2026-10-09.md).  The rears and fronts leave at the engine's own
# levels: only AUDIO_STEREO_LFE / LFE_GAIN and the cabin guard apply on top.  The blob is the fine mask
# net c ('SMSKNET1', 32,544 B, models/scene-masknet-c/); comment the BLOB line out for the spatial-only
# A/B (no instrument transfer, no duck; the hook logs "SCENE: mask net" / "running without a mask" once).
# The seven knobs below are the engine defaults, spelled out so the running config can be read here.
# The net (1.37 M ARMv7 instructions per 512-frame control, NEON) runs on its own thread
# (scene_mask_worker.h, SCHED_RR 17): the audio thread pays only the hand-off (~1 k instructions per
# period), a net slower than realtime skips controls instead of stalling the audio ("scene mask worker
# stopped rows=A/B skipped=N" at stream stop; N = controls refused, 0 when it keeps up), and its rows
# apply one control (10.7 ms) late.  AUDIO_SCENE_MASK_SYNC=1 puts the net back on the audio thread
# (the port tests' exact timing, ~3.1 M instructions per 768-frame period under the 12 ms guard): an
# A/B lever, not the default.
export AUDIO_SCENE_MASK_BLOB="$H/scene_masknet_c.bin"
export AUDIO_SCENE_REAR_DELAY_MS=0
export AUDIO_SCENE_INST_GAMMA=1
export AUDIO_SCENE_INST_ALPHA=0.9
export AUDIO_SCENE_DUCK_DEPTH=0.25
export AUDIO_SCENE_ALPHA=0.7
export AUDIO_SCENE_SIDE_LP_HZ=7000
export AUDIO_SCENE_BLEED_CANCEL=0
# The scene upmix also for the stock realtime media stream (type 100, audioType=media: Spotify and
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
