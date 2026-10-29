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

# Video decode feed tuning (hook module "decoder_optimize"; reversed facts in
# dio_asm_to_c/_legacy_attempt/libairplay-mhi2q/reversed/04_video_decoder_optimization.md).
# The hook automatically reduces the decode-feed and present queues from 4 to 2
# after verifying the stock libairplay and both patch signatures.
# Optional thread priority, read natively by stock libairplay
# (OmxVideoImpl::initialize); default 17.
# export SCREEN_RECEIVER_THREAD_PRIO=32

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
#
# Baked-in runtime defaults (no env needed):
#   - SCENE stereo-to-5.1 upmixer armed on both buffered (type 103) and realtime (type 100) media;
#     user-facing bypass and level (OFF / NORMAL / INTENSE) controlled from MMI.
#   - Phase-coherent LFE assist (11585 Q15) + 45-65 Hz front-door woofer relief + J525 cabin guard.
#   - Variant J scene defaults (alpha=0.7, inst_alpha=0.9, inst_gamma=1, duck_depth=0.25,
#     side_lp_hz=7000, rear_delay_ms=0, bleed_cancel=0, front_keep_db=1.0) on the SCHED_RR 17 worker.
#   - Apple SoundCheck loudness metadata enabled; voice/nav announcement gain +5.0 dB;
#     type-103 secondary format index 23 (AAC-LC/48k/stereo); APAP receiver thread SCHED_RR 18.
#   - Optional ring/diagnostic overrides in code: AUDIO_RING_MS (500), AUDIO_HIGH_MS (250),
#     AUDIO_REPRIME_MS (160), AUDIO_SPATIAL_TAP.
export AUDIO_SPATIAL=1
export AUDIO_SPATIAL_ATMOS=0
export AUDIO_SPATIAL_ATMOS_MODE=downmix
export AUDIO_SCENE_MASK_BLOB="$H/scene_masknet_c.bin"
export AUDIO_SPATIAL_CODEC_LIB="$H/libcarplay_apac.so"

echo "[startup] exec dio_manager pid=$DIO_PID monitor=$MONITOR_PID" >> "$WLOG"
exec "$DIODIR/dio_manager" "$@"

# Preflight makes this reachable only for an exec/runtime loader failure.
STARTUP_EXEC_RC=$?
echo "[startup] exec dio_manager failed rc=$STARTUP_EXEC_RC" >> "$WLOG"
startup_fail
