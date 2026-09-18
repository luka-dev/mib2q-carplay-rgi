#!/bin/bash
# Upload the complete release and run the same installer as M.I.B. No restarts.
# Usage: deploy_over_ssh.sh [root@host] [install|check|uninstall]
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEV=${1:-root@10.173.189.1}
ACTION=${2:-install}
BUILD=${CARPLAY_BUILD_DIR:-$ROOT/build}
PACKAGE=$ROOT/install_MoreIncredibleBash
case "$ACTION" in install|check|uninstall) ;; *) echo 'Expected install, check or uninstall'; exit 2 ;; esac
case "$DEV" in ''|-*|*[^a-zA-Z0-9_.@:-]*) echo 'Invalid SSH destination'; exit 2 ;; esac
if [ "$ACTION" = uninstall ]; then
    PACKAGE=$ROOT/uninstall_MoreIncredibleBash
    python3 "$ROOT/scripts/package_mib.py" --uninstall
else
    python3 "$ROOT/scripts/package_mib.py" --build-dir "$BUILD" --check
fi

# Existing SSH keys/agent/known_hosts are respected. SSHPASS is optional;
# sshpass reads it from the environment instead of a password in argv/source.
authenticated() {
    if [ -n "${SSHPASS:-}" ]; then sshpass -e "$@"; else "$@"; fi
}
OPTS=(-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oConnectTimeout=10)
remote() { authenticated ssh "${OPTS[@]}" "$DEV" "$@"; }
NAME=carplay-install-$(date +%Y%m%d%H%M%S)-$$
STAGE=$(remote "if [ -d /mnt/app/eso/hmi/lsd ]; then echo /tmp/$NAME; elif [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then echo /net/mmx/tmp/$NAME; else exit 1; fi")
case "$STAGE" in "/tmp/$NAME"|"/net/mmx/tmp/$NAME") ;; *) echo 'MMX staging path could not be determined'; exit 1 ;; esac
remote "mkdir '$STAGE'"
cleanup() { remote "rm -rf '$STAGE'" >/dev/null 2>&1 || :; }
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
authenticated scp -O "${OPTS[@]}" -r "$PACKAGE/mod" "$DEV:$STAGE/"
if [ "$ACTION" != uninstall ]; then
    # Upload the explicit release file list, never the development build tree.
    FILES=()
    while read -r digest name; do
        case "$name" in ../mod/custom.sh) ;; *) FILES+=("$BUILD/$name") ;; esac
    done < "$BUILD/SHA256SUMS"
    FILES+=("$BUILD/CKSUMS" "$BUILD/SHA256SUMS")
    remote "mkdir '$STAGE/build'"
    authenticated scp -O "${OPTS[@]}" "${FILES[@]}" "$DEV:$STAGE/build/"
fi
remote "/bin/sh '$STAGE/mod/custom.sh' '$ACTION'"
if [ "$ACTION" = check ]; then
    echo 'Preflight completed; no installed files changed.'
else
    echo 'Completed. Reboot the HU manually when ready.'
fi
