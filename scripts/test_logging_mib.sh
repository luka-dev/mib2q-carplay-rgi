#!/bin/sh
# logging_MoreIncredibleBash/mod/custom.sh on a fake card: numbered saves 001/002,
# /tmp logs and captures copied, shared-memory objects skipped, verbose marker made
# by run 1 and reported by run 2.  Every POSIX/ksh shell here (QNX 6.5 /bin/sh is pdksh).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL ($1): $2"; exit 1; }
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/dash /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    rm -rf "$T/card" "$T/src"; mkdir -p "$T/card" "$T/src"
    cp -R "$ROOT/logging_MoreIncredibleBash/mod" "$T/card/"
    echo h > "$T/src/carplay_hook.log"; echo a > "$T/src/spatial_apap_1.bin"; echo s > "$T/src/shmem_obj"
    rm -rf "$T/core"; mkdir "$T/core"; echo j > "$T/core/j9.core.gz"; : > "$T/core/EBNavFreezeLog.9"
    for k in 1 2 3 4 5 6 7; do echo e > "$T/src/smartphone_integrator_error_$k.tar.gz"; done
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "run 1"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "run 2"
    L=$T/card/carplay_logs
    [ -f "$L/001/tmp/carplay_hook.log" ] && [ -f "$L/002/tmp/spatial_apap_1.bin" ] || fail "$sh" "logs not copied"
    [ ! -e "$L/001/tmp/shmem_obj" ] || fail "$sh" "shared-memory object copied"
    [ "$(ls "$L/001/si_errors" | wc -l | tr -d ' ')" = 5 ] || fail "$sh" "SI error dumps: not the newest five"
    [ -f "$L/001/e2p_train.txt" ] && [ -f "$L/001/foreign_find.txt" ] && [ -f "$L/001/dio_env.txt" ] || fail "$sh" "train/foreign/env lists missing"
    [ -f "$L/001/core/j9.core.gz" ] && [ ! -e "$L/001/core/EBNavFreezeLog.9" ] || fail "$sh" "core dir not copied (or empty file copied)"
    [ ! -e "$L/002/core/j9.core.gz" ] || fail "$sh" "unchanged core copied again"
    [ -e "$T/src/carplay_verbose" ] || fail "$sh" "no verbose marker"
    grep -q 'verbose     off' "$L/001/info.txt" && grep -q 'verbose     on' "$L/002/info.txt" || fail "$sh" "verbose state not recorded"
    # numbering continues after the highest save, even with gaps and past 008/009
    rm -rf "$L/001"; mkdir "$L/009"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "run 3"
    [ -d "$L/010" ] && [ ! -e "$L/001" ] || fail "$sh" "next save is not highest+1: $(ls "$L" | tr '\n' ' ')"
done
echo "logging M.I.B.: highest+1 numbering, /tmp logs, SI dumps, cores once, verbose marker:$shells PASS"
