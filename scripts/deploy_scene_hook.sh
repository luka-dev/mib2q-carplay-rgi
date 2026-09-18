#!/bin/bash
# Hot-swap the second-scene hook build on the HU with an explicit rollback copy.
# No service restart is performed here: restart dio_manager yourself afterwards.
#   deploy_scene_hook.sh [root@host] install [surround]   # backup, upload scene .so, verify, set AUDIO_STEREO_SURROUND
#   deploy_scene_hook.sh [root@host] rollback             # restore .so and startup script from the backups
#   deploy_scene_hook.sh [root@host] status               # which build is live, env line, backups present
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEV=${1:-root@10.173.189.1}
ACTION=${2:-status}
SURROUND=${3:-0.40}
H=/mnt/app/root/hooks
SCENE=$ROOT/build/libcarplay_hook.scene.so
STARTUP=$H/carplay_startup.sh
case "$DEV" in ''|-*|*[^a-zA-Z0-9_.@:-]*) echo 'Invalid SSH destination'; exit 2 ;; esac
authenticated() { if [ -n "${SSHPASS:-}" ]; then sshpass -e "$@"; else "$@"; fi; }
OPTS=(-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oConnectTimeout=10)
remote() { authenticated ssh "${OPTS[@]}" "$DEV" "$@"; }
upload() { authenticated ssh "${OPTS[@]}" "$DEV" "cat > '$2'" < "$1"; }   # no scp needed on the unit
verify() { remote "cat '$2'" | cmp -s - "$1" && echo "verified $2" || { echo "READ-BACK MISMATCH $2"; exit 1; }; }

case "$ACTION" in
status)
    remote "ls -la $H/libcarplay_hook.so* 2>/dev/null; grep -n 'AUDIO_STEREO_SURROUND\|AUDIO_STEREO_UPMIX' $STARTUP"
    echo "local scene build: $(shasum -a 256 "$SCENE" | cut -c1-16)…  production: $(shasum -a 256 "$ROOT/build/libcarplay_hook.so" | cut -c1-16)…"
    ;;
install)
    [ -f "$SCENE" ] || { echo "missing $SCENE (build with UPMIX_SCENE=1 ./scripts/build_hook.sh)"; exit 1; }
    case "$SURROUND" in 0.[0-9]*) ;; *) echo 'surround must look like 0.NN'; exit 2 ;; esac
    remote "[ -f $H/libcarplay_hook.so.pre-scene ] || cp $H/libcarplay_hook.so $H/libcarplay_hook.so.pre-scene; [ -f $STARTUP.pre-scene ] || cp $STARTUP $STARTUP.pre-scene; echo backups ok"
    upload "$SCENE" "$H/libcarplay_hook.so.scene"
    verify "$SCENE" "$H/libcarplay_hook.so.scene"
    remote "cp $H/libcarplay_hook.so.scene $H/libcarplay_hook.so && sed -i 's/^export AUDIO_STEREO_SURROUND=.*/export AUDIO_STEREO_SURROUND=$SURROUND/' $STARTUP && grep -n AUDIO_STEREO_SURROUND $STARTUP"
    verify "$SCENE" "$H/libcarplay_hook.so"
    echo "Installed scene build with surround $SURROUND. Now restart dio_manager (not done here). Rollback: $0 $DEV rollback"
    ;;
rollback)
    remote "cp $H/libcarplay_hook.so.pre-scene $H/libcarplay_hook.so && cp $STARTUP.pre-scene $STARTUP && grep -n AUDIO_STEREO_SURROUND $STARTUP && echo restored"
    echo "Rolled back. Restart dio_manager to apply."
    ;;
*) echo 'Expected install, rollback or status'; exit 2 ;;
esac
