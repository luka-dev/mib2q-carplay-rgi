#!/bin/bash
# Deploy build/libcarplay_hook.so to the MHI2Q head unit and restart dio_manager.
# The LD_PRELOAD into dio_manager is already declared on-device (RGD hook uses it),
# so this is a clean binary swap. Reconnect the iPhone after running.
set -e

DEV="root@10.173.189.1"
PASS="harman_f"
OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oConnectTimeout=10"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK="${1:-${PROJECT_DIR}/build/libcarplay_hook.so}"
DST="/mnt/app/root/hooks/libcarplay_hook.so"

[ -f "$HOOK" ] || { echo "missing hook: $HOOK (run ./scripts/build_hook.sh first)"; exit 1; }
echo "Deploying $(ls -la "$HOOK" | awk '{print $5}') bytes -> $DST"

# 1. No on-device backup: git holds every previous hook, and stale .bak copies in
# /mnt/app/root/hooks only cost flash and confuse the next deploy.
sshpass -p "$PASS" ssh $OPTS "$DEV" 'mount -uw /mnt/app; echo rw'
sshpass -p "$PASS" scp $OPTS "$HOOK" "$DEV:$DST"
sshpass -p "$PASS" ssh $OPTS "$DEV" 'chmod 755 '"$DST"'; ls -la '"$DST"

# 2. kill dio_manager so it respawns with the new hook on next CarPlay connect
sshpass -p "$PASS" ssh $OPTS "$DEV" 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in *dio_manager*) pid=${p#/proc/}; echo "killing dio_manager pid=$pid"; kill $pid 2>/dev/null;; esac; done; echo done'

echo "Deployed. Reconnect the iPhone (CarPlay), then: tail -f /tmp/carplay_hook.log"
