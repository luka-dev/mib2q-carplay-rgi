#!/bin/bash
# Operate the compact AUDIO_STATE_TRACE=1 capture on the MHI2Q unit.
set -euo pipefail

DEV="${CARPLAY_TRACE_DEV:-root@10.173.189.1}"
PASS="${CARPLAY_TRACE_PASS:-harman_f}"
OPTS=(-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa
      -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oConnectTimeout=10)
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REMOTE_BASE=/tmp/carplay_state_trace.log

remote() {
    sshpass -p "$PASS" ssh "${OPTS[@]}" "$DEV" "$@"
}

usage() {
    echo "usage: $0 clear | tail | pull [output-directory]" >&2
    exit 2
}

case "${1:-}" in
    clear)
        # Preserve the current inode: dio_manager may already have it open.
        # Unlinking here would make the writer continue into an unreachable
        # file until its next restart/rotation, yielding an empty pull.
        remote ': > /tmp/carplay_state_trace.log; rm -f /tmp/carplay_state_trace.log.1 /tmp/carplay_state_trace.log.2 /tmp/carplay_state_trace.log.3'
        echo "Cleared compact state trace on $DEV"
        ;;
    tail)
        remote 'touch /tmp/carplay_state_trace.log; tail -f /tmp/carplay_state_trace.log'
        ;;
    pull)
        stamp=$(date +%Y%m%d-%H%M%S)
        out="${2:-$PROJECT_DIR/captures/state-trace/$stamp}"
        mkdir -p "$out"
        pulled=0
        # Oldest rotation first makes multi-file analysis chronological even
        # before the analyzer's monotonic sort.
        for suffix in .3 .2 .1 ''; do
            if remote "test -f $REMOTE_BASE$suffix"; then
                sshpass -p "$PASS" scp "${OPTS[@]}" \
                    "$DEV:$REMOTE_BASE$suffix" "$out/"
                pulled=1
            fi
        done
        [ "$pulled" = 1 ] || { echo "No state trace found on $DEV" >&2; exit 1; }
        echo "Pulled trace to $out"
        python3 "$PROJECT_DIR/tools/analyze_carplay_state_trace.py" \
            "$out"/carplay_state_trace.log* | tee "$out/summary.txt"
        ;;
    *) usage ;;
esac
