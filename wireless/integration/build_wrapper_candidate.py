#!/usr/bin/env python3
"""Prepare a review-only wrapper with dio ownership and receiver bridge."""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import argparse
import subprocess


REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "deploy/smartphone_integrator/carplay_startup.sh"
OUT = Path(__file__).resolve().parent / "out/carplay_startup.wireless.offline.sh"
HANDOFF_OUT = Path(__file__).resolve().parent / "out/carplay_startup.handoff.offline.sh"
EXPECTED_SHA = "90d9e65bc97c8a835c4aa85f785271702bfe7f09d292ab45001ea453b78f87a2"
CHECK = '''[ -r "$H/libcarplay_hook.so" ] || {
    echo "[startup] missing $H/libcarplay_hook.so" >> "$WLOG"
    exit 127
}
'''
BRIDGE_CHECK = '''[ -r "$H/libdio_receiver_bridge.so" ] || {
    echo "[startup] missing $H/libdio_receiver_bridge.so" >> "$WLOG"
    exit 127
}
'''
OLD_PRELOAD = 'export LD_PRELOAD="$H/libcarplay_hook.so"'
NEW_PRELOAD = 'export LD_PRELOAD="$H/libcarplay_hook.so:$H/libdio_receiver_bridge.so"'
OWNER_ANCHOR = 'WLOG=${WLOG:-/tmp/carplay_wrapper.log}\n'
QUIESCE_EXPORT = 'export MHI2Q_REQUIRE_BRIDGE_QUIESCE=1\n'
STOCK210_EXPORT = '''# Native owner publication needs filesystem rename; QNX RAM /tmp lacks it.
export OWNER_FILE=${OWNER_FILE:-/mnt/persist/.mhi2q_dio_generation}
export MHI2Q_CP_AIRPLAY_PORT=5000
'''
OWNER_TRAMPOLINE = '''# Claim dio ownership before publishing the generation or forking its monitor.
# The helper keeps both lock descriptors across this script and exec dio_manager.
if [ "${MHI2Q_DIO_OWNER_CLAIMED:-0}" != 1 ]; then
    [ -x "$H/dio_owner_exec" ] || {
        echo "[startup] missing $H/dio_owner_exec" >> "$WLOG"
        exit 127
    }
    export MHI2Q_DIO_PUBLISH_OWNER=1
    exec "$H/dio_owner_exec" wired "$H/carplay_startup.sh" "$@"
fi
'''
OWNER_SOURCE = '''OWNER_STAGE=${OWNER_FILE}.${DIO_PID}
echo "$DIO_PID" > "$OWNER_STAGE" || startup_fail
mv "$OWNER_STAGE" "$OWNER_FILE" || startup_fail
'''
OWNER_PUBLISHED = '''if [ "${MHI2Q_DIO_GENERATION_PUBLISHED:-0}" != 1 ]; then
    OWNER_STAGE=${OWNER_FILE}.${DIO_PID}
    echo "$DIO_PID" > "$OWNER_STAGE" || startup_fail
    mv "$OWNER_STAGE" "$OWNER_FILE" || startup_fail
fi
'''
NCM_SOURCE = '''NCM_GEN=
[ -r /tmp/ncm_generation_carplay0 ] && read NCM_GEN < /tmp/ncm_generation_carplay0
if [ -n "$NCM_GEN" ]; then
    echo "$NCM_GEN" > /tmp/ncm_session_generation.$DIO_PID && mv -f /tmp/ncm_session_generation.$DIO_PID /tmp/ncm_session_generation
else
    rm -f /tmp/ncm_session_generation
fi
'''
NCM_WIRELESS_GUARD = '''NCM_GEN=
if [ "${MHI2Q_DIO_OWNER_MODE:-}" != wireless ]; then
    [ -r /tmp/ncm_generation_carplay0 ] && read NCM_GEN < /tmp/ncm_generation_carplay0
    if [ -n "$NCM_GEN" ]; then
        echo "$NCM_GEN" > /tmp/ncm_session_generation.$DIO_PID && mv -f /tmp/ncm_session_generation.$DIO_PID /tmp/ncm_session_generation
    else
        rm -f /tmp/ncm_session_generation
    fi
fi
'''
MONITOR_SOURCE = '''LD_PRELOAD= "$H/carplay_monitor.sh" "$DIO_PID" </dev/null >>"$WLOG" 2>&1 &
MONITOR_PID=$!
'''
MONITOR_WIRELESS_GUARD = '''if [ "${MHI2Q_DIO_OWNER_MODE:-}" != wireless ]; then
    LD_PRELOAD= "$H/carplay_monitor.sh" "$DIO_PID" </dev/null >>"$WLOG" 2>&1 &
    MONITOR_PID=$!
fi
'''
EXEC_DIO = 'exec "$DIODIR/dio_manager" "$@"'
EXEC_DIO_WITH_AIRPLAY = '''# Resolve dio_manager's libairplay.so from the installed MU1316 app image.
export LD_LIBRARY_PATH=/mnt/app/eso/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
exec "$DIODIR/dio_manager" "$@"'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--property-handoff", action="store_true")
    parser.add_argument("--stock210", action="store_true")
    args = parser.parse_args()
    if args.stock210 and not args.property_handoff:
        parser.error("--stock210 requires --property-handoff")
    raw = SOURCE.read_bytes()
    if sha256(raw).hexdigest() != EXPECTED_SHA:
        raise RuntimeError("current wired CarPlay wrapper differs from reviewed source")
    source = raw.decode()
    if (source.count(CHECK) != 1 or source.count(OLD_PRELOAD) != 1 or
            source.count(OWNER_ANCHOR) != 1 or source.count(OWNER_SOURCE) != 1 or
            source.count(NCM_SOURCE) != 1 or source.count(MONITOR_SOURCE) != 1 or
            source.count(EXEC_DIO) != 1):
        raise RuntimeError("wrapper preload or file guard is not unique")
    candidate = (source.replace(OWNER_ANCHOR, OWNER_ANCHOR + OWNER_TRAMPOLINE)
                       .replace(CHECK, CHECK + BRIDGE_CHECK)
                       .replace(OWNER_SOURCE, OWNER_PUBLISHED)
                       .replace(NCM_SOURCE, NCM_WIRELESS_GUARD)
                       .replace(MONITOR_SOURCE, MONITOR_WIRELESS_GUARD)
                       .replace(OLD_PRELOAD, NEW_PRELOAD)
                       .replace(EXEC_DIO, EXEC_DIO_WITH_AIRPLAY))
    if args.property_handoff:
        candidate = candidate.replace(OWNER_ANCHOR, OWNER_ANCHOR + QUIESCE_EXPORT)
    if args.stock210:
        candidate = candidate.replace(OWNER_ANCHOR, OWNER_ANCHOR + STOCK210_EXPORT)
    if (candidate.replace(OWNER_TRAMPOLINE, "")
                 .replace(QUIESCE_EXPORT, "")
                 .replace(STOCK210_EXPORT, "")
                 .replace(BRIDGE_CHECK, "")
                 .replace(OWNER_PUBLISHED, OWNER_SOURCE)
                 .replace(NCM_WIRELESS_GUARD, NCM_SOURCE)
                 .replace(MONITOR_WIRELESS_GUARD, MONITOR_SOURCE)
                 .replace(NEW_PRELOAD, OLD_PRELOAD)
                 .replace(EXEC_DIO_WITH_AIRPLAY, EXEC_DIO)) != source:
        raise RuntimeError("candidate has changes outside the bridge guard/preload")
    output = HANDOFF_OUT if args.property_handoff else OUT
    if args.stock210:
        output = OUT.parent / "carplay_startup.stock210.offline.sh"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(candidate)
    output.chmod(0o644)
    subprocess.run(["sh", "-n", str(output)], check=True)
    print(f"{output} SHA-256 {sha256(output.read_bytes()).hexdigest()}")
    print("review-only wrapper candidate; wired child claims dio before startup")


if __name__ == "__main__":
    main()
