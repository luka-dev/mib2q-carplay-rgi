#!/bin/bash
# Deploy the hook, APAC codec and selected HSSR model over ssh, then restart dio_manager.
# Development loop only -- the SD-card install is deploy/card/ (see its README).
# The LD_PRELOAD into dio_manager is already declared on-device (RGD hook uses it),
# so this is a clean binary swap. Reconnect the iPhone after running.
set -e

DEV="root@10.173.189.1"
PASS="harman_f"
OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oConnectTimeout=10"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK="${1:-${PROJECT_DIR}/build/libcarplay_hook.so}"
APAC="${2:-${PROJECT_DIR}/build/libcarplay_apac.so}"
MODEL="${3:-${PROJECT_DIR}/build/hssr-selection-v11/hssr_model.bin}"
MODEL_SHA256="ab01463e9359f013c4b40f311d630bd20af48d761ad361563cb5246f6bd686ae"
DST="/mnt/app/root/hooks/libcarplay_hook.so"
APAC_DST="/mnt/app/root/hooks/libcarplay_apac.so"
MODEL_DST="/mnt/app/root/hooks/hssr_model.bin"
STAGE="${DST}.new"
APAC_STAGE="${APAC_DST}.new"
MODEL_STAGE="${MODEL_DST}.new"

[ -f "$HOOK" ] || { echo "missing hook: $HOOK (run ./scripts/build_hook.sh first)"; exit 1; }
[ -f "$APAC" ] || { echo "missing APAC codec: $APAC (run ./scripts/build_hook.sh first)"; exit 1; }
[ -f "$MODEL" ] || { echo "missing HSSR model: $MODEL"; exit 1; }
ACTUAL_MODEL_SHA256="$(shasum -a 256 "$MODEL" | awk '{print $1}')"
[ "$ACTUAL_MODEL_SHA256" = "$MODEL_SHA256" ] || {
    echo "wrong HSSR model hash: $ACTUAL_MODEL_SHA256"
    echo "expected: $MODEL_SHA256"
    exit 1
}
echo "Deploying hook $(ls -la "$HOOK" | awk '{print $5}') bytes -> $DST"
echo "Deploying codec $(ls -la "$APAC" | awk '{print $5}') bytes -> $APAC_DST"
echo "Deploying HSSR V11 model $(ls -la "$MODEL" | awk '{print $5}') bytes -> $MODEL_DST"

# 1. Stage and rename. QNX rejects a direct scp overwrite while dio_manager has
# the shared object mapped (ETXTBSY surfaces only as "dest open: Failure"). A
# new inode can be copied and atomically renamed over the live pathname; the
# current process retains its mapped inode until step 2, and its replacement
# opens the new one. No backup copies accumulate: each deploy reuses `.new` and
# the hook remains reproducible with scripts/build_hook.sh.
sshpass -p "$PASS" ssh $OPTS "$DEV" 'mount -uw /mnt/app; echo rw'
sshpass -p "$PASS" scp $OPTS "$HOOK" "$DEV:$STAGE"
sshpass -p "$PASS" scp $OPTS "$APAC" "$DEV:$APAC_STAGE"
sshpass -p "$PASS" scp $OPTS "$MODEL" "$DEV:$MODEL_STAGE"
sshpass -p "$PASS" ssh $OPTS "$DEV" \
    'chmod 755 '"$STAGE $APAC_STAGE"' && chmod 644 '"$MODEL_STAGE"' && mv '"$STAGE $DST"' && mv '"$APAC_STAGE $APAC_DST"' && mv '"$MODEL_STAGE $MODEL_DST"'; sync; ls -la '"$DST $APAC_DST $MODEL_DST"

# 2. kill dio_manager so it respawns with the new hook on next CarPlay connect
sshpass -p "$PASS" ssh $OPTS "$DEV" 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in *dio_manager*) pid=${p#/proc/}; echo "killing dio_manager pid=$pid"; kill $pid 2>/dev/null;; esac; done; echo done'

echo "Deployed. Reconnect the iPhone (CarPlay)."
echo "Normal build: tail -f /tmp/carplay_hook.log"
echo "State-trace build: ./scripts/carplay_state_trace.sh tail"
