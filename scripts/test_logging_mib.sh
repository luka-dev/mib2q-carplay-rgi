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
    # card display: trigger -> marker + info.txt state; capture saved; RAM copy dropped only when closed
    : > "$T/card/card_display.txt"; echo c > "$T/src/carplay_cap111b.h264"
    echo 'x [WRN] [altscreen] card display: cap111b armed gen=2 existing=0 bytes' > "$T/src/carplay_hook.log"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "card run 1"
    [ -e "$T/src/carplay_alt_card_display" ] || fail "$sh" "card_display marker not created"
    grep -q 'card_display=0 (before this run)' "$L/011/info.txt" && grep -q 'card_display=1 (after this run)' "$L/011/info.txt" || fail "$sh" "card_display state not in info.txt"
    [ -f "$L/011/tmp/carplay_cap111b.h264" ] || fail "$sh" "cap111b not saved"
    [ -f "$T/src/carplay_cap111b.h264" ] && grep -q 'cap111b.h264 not closed' "$L/011/info.txt" || fail "$sh" "open cap111b removed"
    echo 'x [WRN] [altscreen] card display: cap111b wrote 2 bytes frames=1' >> "$T/src/carplay_hook.log"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "card run 2"
    [ ! -e "$T/src/carplay_cap111b.h264" ] && grep -q 'cap111b.h264 saved to tmp/ and removed' "$L/012/info.txt" || fail "$sh" "closed cap111b not dropped"
    # the marker held by a running card session (.active, no reason file): re-armed, state noted
    rm -f "$T/src/carplay_alt_card_display"; : > "$T/src/carplay_alt_card_display.active"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "card run 3"
    grep -q 'card_display=0 (before this run)' "$L/013/info.txt" && [ -e "$T/src/carplay_alt_card_display" ] || fail "$sh" "re-arm while a session holds the marker"
    grep -q 'card_display session running' "$L/013/info.txt" || fail "$sh" "running card session not noted"
    rm -f "$T/src/carplay_alt_card_display.active"
    # a hook self-disarm: reported, not re-armed, trigger renamed, reason file consumed
    rm -f "$T/src/carplay_alt_card_display"
    echo 'session ended after 3000 ms (<20 s, end=teardown)' > "$T/src/carplay_alt_card_display.disarmed"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > "$T/screen" || fail "$sh" "card run 4"
    [ ! -e "$T/src/carplay_alt_card_display" ] && [ ! -e "$T/src/carplay_alt_card_display.disarmed" ] || fail "$sh" "re-armed after a self-disarm"
    [ -f "$T/card/card_display.disarmed" ] && [ ! -e "$T/card/card_display.txt" ] || fail "$sh" "trigger not renamed after a self-disarm"
    grep -q 'self-disarmed by the hook: session ended after 3000 ms' "$L/014/info.txt" && grep -q 'card_display=0 (after this run)' "$L/014/info.txt" ||
        fail "$sh" "self-disarm not in info.txt"
    [ -f "$L/014/tmp/carplay_alt_card_display.disarmed" ] || fail "$sh" "self-disarm reason not saved"
    grep -q 'CARD DISPLAY self-disarmed by the hook' "$T/screen" && grep -q 'rename it back' "$T/screen" || fail "$sh" "self-disarm not on screen"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "card run 5"
    [ ! -e "$T/src/carplay_alt_card_display" ] || fail "$sh" "re-armed with the renamed trigger"
    mv "$T/card/card_display.disarmed" "$T/card/card_display.txt"     # the user arms again
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "card run 6"
    [ -e "$T/src/carplay_alt_card_display" ] && grep -q 'card_display=1 (after this run)' "$L/016/info.txt" || fail "$sh" "not re-armed after the rename-back"
    rm -f "$T/card/card_display.txt"; : > "$T/src/carplay_alt_card_display.active"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "card run 7"
    [ ! -e "$T/src/carplay_alt_card_display" ] && grep -q 'card_display=0 (after this run)' "$L/017/info.txt" || fail "$sh" "card_display not disarmed"
    [ ! -e "$T/src/carplay_alt_card_display.active" ] || fail "$sh" "running card session's marker loan not revoked"
done
echo "logging M.I.B.: highest+1 numbering, /tmp logs, SI dumps, cores once, verbose marker, card_display marker + self-disarm + cap111b save/cleanup:$shells PASS"
