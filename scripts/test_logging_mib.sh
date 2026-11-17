#!/bin/sh
# logging_MoreIncredibleBash/mod/custom.sh on a fake card: numbered saves 001/002,
# /tmp logs and captures copied, shared-memory objects skipped, verbose marker made
# by run 1 and reported by run 2.  Every POSIX/ksh shell here (QNX 6.5 /bin/sh is pdksh).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT
fail() { echo "FAIL ($1): $2"; exit 1; }
ck() { set -- $(cksum < "$1"); echo "$1 $2"; }
restore_run() {   # the card run with mibsi_stock_restore.sh on the fake unit $T/unit; screen in $T/screen
    CP_UNIT_ROOT=$T/unit CP_VERSION_FILE=$T/version.txt CP_RESTORE_MOUNT= CP_RESTORE_SUMS=$T/sums \
        CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > "$T/screen"
}
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/dash /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    rm -rf "$T/card" "$T/src" "$T/swdl" "$T/mibsi" "$T/ran"; mkdir -p "$T/card" "$T/src" "$T/swdl/FileCopyInfo" "$T/swdl/Log/user/3" "$T/mibsi"
    printf '[Bootstrap\\GEM\\0\\default\\File]=/net/mmx/mnt/app/eso/hmi/engdefs/mibsi-main.esd.new\nCRC=1\n' > "$T/swdl/FileCopyInfo/Bootstrap.info"
    printf '[Launcher\\version\\0\\default\\File]=/HBpersistence/SWDL/Log/service/MIB\n' > "$T/swdl/FileCopyInfo/Launcher.info"
    echo g > "$T/swdl/Log/user/3/General.z"
    export CP_SWDL_DIR="$T/swdl" CP_MIBSI_DIR="$T/mibsi"
    cp -R "$ROOT/logging_MoreIncredibleBash/mod" "$T/card/"
    # MIBSI's uninstall card: a stand-in for its install_product.sh that records its argument
    mkdir -p "$T/card/Bootstrap/final/product"; : > "$T/card/MIBSI-UNINSTALL"
    printf 'echo "card=$1" >> "%s/ran"; echo "MIBSI: uninstall termine et relu"; exit 0\n' "$T" > "$T/card/Bootstrap/final/product/install_product.sh"
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
    # SWDL state is copied every run; MIBSI's uninstaller runs only with the trigger, once
    [ -f "$L/001/swdl/Log/user/3/General.z" ] && grep -q mibsi-main "$L/001/swdl_state.txt" || fail "$sh" "SWDL state not saved"
    [ -f "$T/swdl/FileCopyInfo/Bootstrap.info" ] || fail "$sh" "an SWDL record was touched"
    [ ! -e "$T/ran" ] || fail "$sh" "MIBSI uninstaller ran without the trigger"
    : > "$T/card/mibsi_uninstall.txt"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > "$T/screen" || fail "$sh" "uninstall run"
    grep -q 'uninstalled by its own uninstaller. Restart the MMI' "$T/screen" && ! grep -q 'use CarPlay, then' "$T/screen" ||
        fail "$sh" "uninstall result / restart step not on screen"
    [ "$(cat "$T/ran")" = "card=$T/card" ] || fail "$sh" "MIBSI uninstaller not run once with the card path: $(cat "$T/ran" 2>/dev/null)"
    grep -q 'uninstall termine' "$L/003/mibsi_uninstall.txt" && grep -q 'mibsi uninstall: exit 0' "$L/003/info.txt" || fail "$sh" "uninstall output/exit not saved"
    [ -f "$L/003/mibsi_ls_after.txt" ] && [ -f "$L/003/dio_cksum_after.txt" ] || fail "$sh" "after-uninstall state not saved"
    [ -f "$T/card/mibsi_uninstall.done" ] && [ ! -e "$T/card/mibsi_uninstall.txt" ] || fail "$sh" "trigger not renamed to .done"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "run after uninstall"
    [ "$(wc -l < "$T/ran" | tr -d ' ')" = 1 ] || fail "$sh" "MIBSI uninstaller ran a second time"
    rmdir "$T/mibsi"; : > "$T/card/mibsi_uninstall.txt"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "run without MIBSI"
    [ "$(wc -l < "$T/ran" | tr -d ' ')" = 1 ] && grep -q 'MIBSI not installed' "$L/005/info.txt" || fail "$sh" "uninstaller ran with MIBSI absent"
    rm -f "$T/card/mibsi_uninstall.txt"
    # stock restore: with both triggers neither runs; a refusal (no version file) keeps its trigger
    mkdir "$T/mibsi"; : > "$T/card/mibsi_uninstall.txt"; : > "$T/card/mibsi_stock_restore.txt"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > "$T/screen" || fail "$sh" "both-triggers run"
    grep -q 'neither run' "$T/screen" || fail "$sh" "both-triggers message not on screen"
    [ "$(wc -l < "$T/ran" | tr -d ' ')" = 1 ] && grep -q 'neither run' "$L/006/info.txt" && [ ! -e "$L/006/mibsi_stock_restore.txt" ] ||
        fail "$sh" "a MIBSI removal ran with both triggers"
    rm -f "$T/card/mibsi_uninstall.txt"
    CP_VERSION_FILE=$T/no_version CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > "$T/screen" || fail "$sh" "stock restore run"
    grep -q 'MIBSI NOT removed (refused, nothing changed)' "$T/screen" || fail "$sh" "stock restore refusal not on screen"
    grep -q 'mibsi stock restore: exit 1' "$L/007/info.txt" && grep -q REFUSED "$L/007/mibsi_stock_restore.txt" && [ -f "$T/card/mibsi_stock_restore.txt" ] ||
        fail "$sh" "stock restore refusal not reported, or its trigger dropped"
    rm -f "$T/card/mibsi_stock_restore.txt"
    # stock restore on a fake unit: stopped partway (exit 2) keeps the trigger and asks for a rerun
    # before CarPlay; the rerun finishes, renames the trigger and asks for the restart
    U=$T/unit; rm -rf "$U"; mkdir -p "$U/mnt/app/eso/bin/apps" "$U/mnt/app/eso/lib" "$T/card/mod/stock_p5092/app"
    echo "stock gal" > "$T/card/mod/stock_p5092/app/gal"; echo "MIBSI gal" > "$U/mnt/app/eso/bin/apps/gal"; echo l > "$U/mnt/app/eso/lib/libmhi2qcarplaycluster.so"
    printf "Current train = 'MHI2Q_ER_AUG22_P5092'\nMainUnit version = '1329'\n" > "$T/version.txt"
    printf 'R /mnt/app/eso/bin/apps/gal app/gal 0755 %s %s\nD /mnt/app/eso/lib/libmhi2qcarplaycluster.so\n' \
        "$(ck "$T/card/mod/stock_p5092/app/gal")" "$(ck "$U/mnt/app/eso/bin/apps/gal")" > "$T/sums"
    : > "$T/card/mibsi_stock_restore.txt"; chmod 555 "$U/mnt/app/eso/bin/apps"
    restore_run || fail "$sh" "stopped stock restore run"; chmod 755 "$U/mnt/app/eso/bin/apps"
    grep -q 'STOPPED partway' "$T/screen" && grep -q 'finish the MIBSI removal (do not use CarPlay yet)' "$T/screen" && ! grep -q 'use CarPlay, then' "$T/screen" ||
        fail "$sh" "stopped stock restore: rerun step not on screen"
    grep -q 'mibsi stock restore: exit 2' "$L/008/info.txt" && [ -f "$T/card/mibsi_stock_restore.txt" ] && [ -f "$U/mnt/app/eso/lib/libmhi2qcarplaycluster.so" ] ||
        fail "$sh" "stopped stock restore: exit not saved, trigger dropped or MIBSI library deleted"
    restore_run || fail "$sh" "stock restore run"
    grep -q 'MIBSI REMOVED' "$T/screen" && grep -q 'run its installer again' "$T/screen" && grep -q 'Restart the MMI, then run this again' "$T/screen" ||
        fail "$sh" "stock restore result / restart step not on screen"
    grep -q 'mibsi stock restore: exit 0' "$L/009/info.txt" && [ -f "$T/card/mibsi_stock_restore.done" ] && [ ! -e "$T/card/mibsi_stock_restore.txt" ] ||
        fail "$sh" "stock restore: exit not saved, or trigger not renamed to .done"
    grep -q 'stock gal' "$U/mnt/app/eso/bin/apps/gal" && [ ! -e "$U/mnt/app/eso/lib/libmhi2qcarplaycluster.so" ] || fail "$sh" "stock restore did not restore/delete"
    grep -q 'MIBSI gal' "$T/card/mibsi_backup/009/mnt/app/eso/bin/apps/gal" && [ ! -e "$L/009/mibsi_backup" ] || fail "$sh" "backup not in <card>/mibsi_backup/009"
    rm -f "$T/card/mibsi_stock_restore.done"
    # numbering continues after the highest save, even with gaps and past 008/009; no MIBSI and
    # no trigger: no SWDL state
    rmdir "$T/mibsi"; rm -rf "$L/001"; mkdir "$L/011"
    CP_LOG_SRC=$T/src CP_CORE_SRC=$T/core "$sh" "$T/card/mod/custom.sh" > /dev/null || fail "$sh" "run 3"
    [ -d "$L/012" ] && [ ! -e "$L/001" ] || fail "$sh" "next save is not highest+1: $(ls "$L" | tr '\n' ' ')"
    [ ! -e "$L/012/swdl" ] && [ ! -e "$L/012/swdl_state.txt" ] || fail "$sh" "SWDL state saved without MIBSI or a trigger"
done
echo "logging M.I.B.: highest+1 numbering, /tmp logs, SI dumps, cores once, verbose marker, SWDL state (MIBSI only) + opt-in MIBSI uninstall (once) / stock restore (refused, stopped, done):$shells PASS"
