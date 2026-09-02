#!/bin/bash
# Deploy the hook plus APAC codec to the MHI2Q head unit over ssh and restart dio_manager.
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
DST="/mnt/app/root/hooks/libcarplay_hook.so"
APAC_DST="/mnt/app/root/hooks/libcarplay_apac.so"

[ -f "$HOOK" ] || { echo "missing hook: $HOOK (run ./scripts/build_hook.sh first)"; exit 1; }
[ -f "$APAC" ] || { echo "missing APAC codec: $APAC (run ./scripts/build_hook.sh first)"; exit 1; }
echo "Deploying hook $(ls -la "$HOOK" | awk '{print $5}') bytes -> $DST"
echo "Deploying codec $(ls -la "$APAC" | awk '{print $5}') bytes -> $APAC_DST"

# 1. Straight overwrite. dio_manager is killed two lines down and re-execs from
# the new file, so there is nothing to stage through: the rename dance only ever
# protected a process we are about to end anyway. No on-device backup either --
# the hook is reproducible with scripts/build_hook.sh.
sshpass -p "$PASS" ssh $OPTS "$DEV" 'mount -uw /mnt/app; echo rw'
sshpass -p "$PASS" scp $OPTS "$HOOK" "$DEV:$DST"
sshpass -p "$PASS" scp $OPTS "$APAC" "$DEV:$APAC_DST"
sshpass -p "$PASS" ssh $OPTS "$DEV" 'chmod 755 '"$DST $APAC_DST"'; sync; ls -la '"$DST $APAC_DST"

# 2. kill dio_manager so it respawns with the new hook on next CarPlay connect
sshpass -p "$PASS" ssh $OPTS "$DEV" 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in *dio_manager*) pid=${p#/proc/}; echo "killing dio_manager pid=$pid"; kill $pid 2>/dev/null;; esac; done; echo done'

echo "Deployed. Reconnect the iPhone (CarPlay), then: tail -f /tmp/carplay_hook.log"
