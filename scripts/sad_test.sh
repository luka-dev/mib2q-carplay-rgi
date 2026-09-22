#!/bin/bash
# One command per step for the SAD test on the car.
#
#   ./scripts/sad_test.sh <host> deploy    push the hook + codec, restart dio
#   ./scripts/sad_test.sh <host> check     pull the logs and show what was announced
#   ./scripts/sad_test.sh <host> revert    put AUDIO_SPATIAL back to 0
#
# Test through CarPlay, not qsa_play: the player sets no SAD metadata and would only show
# whatever the last session left behind (docs/audio/BUFFERED_AUDIO_PATH.md).
set -e
DEV="root@${1:?usage: sad_test.sh <host> deploy|check|revert}"; MODE="${2:-check}"
PASS="harman_f"
P="$(cd "$(dirname "$0")/.." && pwd)"
T=/tmp/sad_test.$$
CM="-oControlMaster=auto -oControlPath=/tmp/audi-sad-sock -oControlPersist=900"
OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oConnectTimeout=60"
S="ssh $OPTS $CM"
sshpass -p "$PASS" $S "$DEV" true    # open the multiplexed master once

case "$MODE" in
deploy)
    $S "$DEV" 'mount -uw /mnt/app'
    for f in libcarplay_hook.so libcarplay_apac.so; do
        sshpass -p "$PASS" scp $OPTS $CM "$P/build/$f" "$DEV:/mnt/app/root/hooks/$f.new" >/dev/null
        $S "$DEV" "chmod 755 /mnt/app/root/hooks/$f.new && mv /mnt/app/root/hooks/$f.new /mnt/app/root/hooks/$f"
        $S "$DEV" "cat /mnt/app/root/hooks/$f" > "$T.$f"
        cmp -s "$T.$f" "$P/build/$f" && echo "OK  $f" || { echo "FAIL $f"; exit 1; }
    done
    $S "$DEV" 'grep "^export AUDIO_SPATIAL=" /mnt/app/root/hooks/carplay_startup.sh'
    $S "$DEV" '> /tmp/carplay_hook.log; for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in *dio_manager*) kill ${p#/proc/} 2>/dev/null;; esac; done 2>/dev/null; echo "dio restarted, log cleared"'
    rm -f "$T".*
    echo
    echo "Now: reconnect the iPhone, play the channel-ID track, let it run ~30 s."
    echo "Then: $0 ${1} check"
    ;;
check)
    $S "$DEV" 'cat /tmp/carplay_hook.log' > "$T.hook" 2>/dev/null || true
    $S "$DEV" 'sloginfo' > "$T.slog" 2>/dev/null || true
    echo "=== what the hook announced ==="
    grep -E "createMetadata" "$T.hook" | tail -5 || echo "  (no createMetadata line -- the seam was not reached)"
    echo "=== what the driver received ==="
    grep -E "convert_sad_header_to_csd: media_type" "$T.slog" | tail -5 || echo "  (no SAD line in slog)"
    echo "=== the sink, for context ==="
    grep -E "device reports voices|audio sink: writes=" "$T.hook" | tail -3
    echo
    echo "asad=0xfc sample_width=0x18 -> announced six channels."
    echo "asad=0xc0 sample_width=0x10 -> not announced; another caller builds the header."
    rm -f "$T".*
    ;;
revert)
    sed 's/^export AUDIO_SPATIAL=1$/export AUDIO_SPATIAL=0/' \
        "$P/deploy/smartphone_integrator/carplay_startup.sh" > "$T.startup"
    $S "$DEV" 'mount -uw /mnt/app'
    for f in carplay_monitor.sh carplay_processes.sh; do
        sshpass -p "$PASS" scp $OPTS $CM "$P/deploy/smartphone_integrator/$f" \
            "$DEV:/mnt/app/root/hooks/$f.new" >/dev/null
    done
    sshpass -p "$PASS" scp $OPTS $CM "$T.startup" "$DEV:/mnt/app/root/hooks/carplay_startup.sh.new" >/dev/null
    $S "$DEV" 'chmod 755 /mnt/app/root/hooks/carplay_startup.sh.new /mnt/app/root/hooks/carplay_monitor.sh.new /mnt/app/root/hooks/carplay_processes.sh.new && mv /mnt/app/root/hooks/carplay_monitor.sh.new /mnt/app/root/hooks/carplay_monitor.sh && mv /mnt/app/root/hooks/carplay_processes.sh.new /mnt/app/root/hooks/carplay_processes.sh && mv /mnt/app/root/hooks/carplay_startup.sh.new /mnt/app/root/hooks/carplay_startup.sh && sync'
    $S "$DEV" 'for p in /proc/[0-9]*; do c=$(tr "\0" " " < $p/cmdline 2>/dev/null); case "$c" in *dio_manager*) kill ${p#/proc/} 2>/dev/null;; esac; done 2>/dev/null; grep "^export AUDIO_SPATIAL=" /mnt/app/root/hooks/carplay_startup.sh'
    rm -f "$T".*
    ;;
*) echo "unknown mode: $MODE"; exit 1 ;;
esac
