#!/bin/sh
# logging_MoreIncredibleBash/mod/mibsi_stock_restore.sh on a fake unit (/eso -> mnt/app/eso,
# as on the MMX) with stand-in stock and MIBSI files: every refusal changes nothing, a run
# restores stock + deletes MIBSI's paths + keeps a verified backup on the card (outside the
# log folder), a rerun has nothing to do, a failed restore keeps MIBSI's libraries.  Every
# POSIX/ksh shell here.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SCRIPT=$ROOT/logging_MoreIncredibleBash/mod/mibsi_stock_restore.sh
T=$(mktemp -d); trap 'chmod -R u+w "$T" 2>/dev/null; rm -rf "$T"' EXIT
fail() { echo "FAIL ($1): $2"; exit 1; }
ck() { set -- $(cksum < "$1"); echo "$1 $2"; }
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/dash /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done

R_PATHS="mnt/app/eso/hmi/lsd/lsd.sh mnt/app/eso/bin/apps/gal mnt/app/eso/bin/apps/dio_manager mnt/system/etc/eso/production/dio_manager.json"
D_FILES="mnt/app/eso/lib/libmhi2qcarplaycluster.so mnt/app/eso/hmi/lsd/jars/002_mhi2q_aa_navigation_proxy.jar mnt/app/eso/hmi/engdefs/mibsi-main.esd"
setup() {   # fresh card + unit in MIBSI state; writes the sums table
    chmod -R u+w "$T" 2>/dev/null; rm -rf "$T/card" "$T/unit" "$T/out"; mkdir -p "$T/card/mod/stock_p5092" "$T/out" "$T/unit/mnt/app/root/mibsi/history"
    ln -s mnt/app/eso "$T/unit/eso"
    : > "$T/sums"
    for p in $R_PATHS; do
        n=${p##*/}; part=app; case $p in mnt/system/*) part=system ;; esac
        mkdir -p "$T/card/mod/stock_p5092/$part" "$T/unit/${p%/*}"
        echo "stock $n" > "$T/card/mod/stock_p5092/$part/$n"
        echo "MIBSI $n" > "$T/unit/$p"
        echo "R /$p $part/$n 0755 $(ck "$T/card/mod/stock_p5092/$part/$n") $(ck "$T/unit/$p")" >> "$T/sums"
    done
    for p in $D_FILES; do mkdir -p "$T/unit/${p%/*}"; echo "MIBSI $p" > "$T/unit/$p"; echo "D /$p" >> "$T/sums"; done
    mkdir -p "$T/unit/mnt/app/eso/hmi/engdefs/scripts/mibsi/v1"; echo s > "$T/unit/mnt/app/eso/hmi/engdefs/scripts/mibsi/v1/status.sh"
    echo r > "$T/unit/mnt/app/root/mibsi/version"; echo h > "$T/unit/mnt/app/root/mibsi/history/0"
    echo "D /mnt/app/eso/hmi/engdefs/scripts/mibsi" >> "$T/sums"; echo "D /mnt/app/root/mibsi" >> "$T/sums"
    echo "D /mnt/app/eso/hmi/engdefs/mibsi-main.esd.new" >> "$T/sums"   # absent: skipped
    printf "Current train = 'MHI2Q_ER_AUG22_P5092'\nMainUnit version = '1329'\n" > "$T/version.txt"
    ( cd "$T/unit" && find . | sort | while read f; do [ -f "$f" ] && echo "$f $(ck "$f")" || echo "$f"; done ) > "$T/before.lst"
}
unchanged() { ( cd "$T/unit" && find . | sort | while read f; do [ -f "$f" ] && echo "$f $(ck "$f")" || echo "$f"; done ) | cmp -s - "$T/before.lst"; }
runit() {   # runit <shell>: exit code of the script into $rc
    rc=0
    CP_UNIT_ROOT=$T/unit CP_VERSION_FILE=$T/version.txt CP_RESTORE_MOUNT= CP_RESTORE_SUMS=$T/sums \
        "$1" "$SCRIPT" "$T/card" "$T/out" > "$T/out/log.txt" 2>&1 || rc=$?
}
for sh in $shells; do
    # refusals: nothing on the unit changes
    setup; echo "Current train = 'MHI2Q_ER_AUG22_P5152'" > "$T/version.txt"; runit "$sh"
    [ "$rc" = 1 ] && unchanged && grep -q 'not MHI2Q_ER_AUG22_P5092' "$T/out/log.txt" || fail "$sh" "wrong firmware not refused ($rc)"
    setup; echo corrupt > "$T/card/mod/stock_p5092/app/gal"; runit "$sh"
    [ "$rc" = 1 ] && unchanged && grep -q 'card stock file .*gal' "$T/out/log.txt" || fail "$sh" "bad card stock file not refused ($rc)"
    setup; echo other > "$T/unit/mnt/app/eso/hmi/lsd/lsd.sh"; ( cd "$T/unit" && find . | sort | while read f; do [ -f "$f" ] && echo "$f $(ck "$f")" || echo "$f"; done ) > "$T/before.lst"; runit "$sh"
    [ "$rc" = 1 ] && unchanged && grep -q 'UNKNOWN version .*lsd.sh' "$T/out/log.txt" || fail "$sh" "unknown file version not refused ($rc)"
    ! grep -q 'uninstaller first' "$T/out/log.txt" || fail "$sh" "uninstaller hint without a .carplay-stock"
    # a dio_manager.json the CarPlay patch's installer edited (backup next to it): refused, with the way out
    setup; J=$T/unit/mnt/system/etc/eso/production/dio_manager.json; cp "$J" "$J.carplay-stock"; echo edited >> "$J"
    ( cd "$T/unit" && find . | sort | while read f; do [ -f "$f" ] && echo "$f $(ck "$f")" || echo "$f"; done ) > "$T/before.lst"; runit "$sh"
    [ "$rc" = 1 ] && unchanged && grep -q 'UNKNOWN version .*dio_manager.json' "$T/out/log.txt" && grep -q "patch's uninstaller first" "$T/out/log.txt" ||
        fail "$sh" "installer-edited dio_manager.json not refused with the hint ($rc)"
    setup; rm "$T/unit/eso"; mkdir -p "$T/unit/eso/bin/apps"; echo x > "$T/unit/eso/bin/apps/dio_manager"
    ( cd "$T/unit" && find . | sort | while read f; do [ -f "$f" ] && echo "$f $(ck "$f")" || echo "$f"; done ) > "$T/before.lst"; runit "$sh"
    [ "$rc" = 1 ] && unchanged && grep -q '/eso/bin/apps/dio_manager differs' "$T/out/log.txt" || fail "$sh" "/eso not the same tree, not refused ($rc)"
    # the run: stock back, MIBSI paths gone, verified backup
    setup; runit "$sh"
    [ "$rc" = 0 ] && grep -q 'DONE: MIBSI removed' "$T/out/log.txt" || fail "$sh" "run failed ($rc): $(tail -3 "$T/out/log.txt")"
    for p in $R_PATHS; do
        n=${p##*/}; part=app; case $p in mnt/system/*) part=system ;; esac
        cmp -s "$T/unit/$p" "$T/card/mod/stock_p5092/$part/$n" || fail "$sh" "$p not stock"
        grep -q "MIBSI $n" "$T/card/mibsi_backup/out/$p" || fail "$sh" "$p not backed up"
        [ ! -e "$T/unit/$p.stock-new" ] || fail "$sh" "$p.stock-new left behind"
    done
    [ "$(ls -l "$T/unit/mnt/app/eso/bin/apps/gal" | cut -c1-10)" = "-rwxr-xr-x" ] || fail "$sh" "stock mode not set"
    for p in $D_FILES mnt/app/eso/hmi/engdefs/scripts/mibsi mnt/app/root/mibsi; do
        [ ! -e "$T/unit/$p" ] || fail "$sh" "$p not deleted"
    done
    [ -f "$T/card/mibsi_backup/out/mnt/app/root/mibsi/history/0" ] && [ -f "$T/card/mibsi_backup/out/mnt/app/eso/hmi/engdefs/scripts/mibsi/v1/status.sh" ] ||
        fail "$sh" "MIBSI directories not backed up"
    [ ! -e "$T/out/mibsi_backup" ] || fail "$sh" "backup inside the log folder"
    grep -q 'stock' "$T/unit/eso/bin/apps/dio_manager" || fail "$sh" "/eso view not stock"
    # rerun: nothing to do
    runit "$sh"
    [ "$rc" = 0 ] && grep -q 'nothing to do' "$T/out/log.txt" || fail "$sh" "rerun not a no-op ($rc)"
    # a restore that fails partway keeps MIBSI's libraries for its remaining programs
    setup; chmod 555 "$T/unit/mnt/app/eso/bin/apps"; runit "$sh"; chmod 755 "$T/unit/mnt/app/eso/bin/apps"
    [ "$rc" = 2 ] || fail "$sh" "failed restore: exit $rc, expected 2"
    cmp -s "$T/unit/mnt/app/eso/hmi/lsd/lsd.sh" "$T/card/mod/stock_p5092/app/lsd.sh" || fail "$sh" "restore before the failure not kept"
    grep -q MIBSI "$T/unit/mnt/app/eso/bin/apps/gal" && [ -f "$T/unit/mnt/app/eso/lib/libmhi2qcarplaycluster.so" ] && [ -d "$T/unit/mnt/app/root/mibsi" ] ||
        fail "$sh" "MIBSI files deleted after a failed restore"
    runit "$sh"
    [ "$rc" = 0 ] && [ ! -e "$T/unit/mnt/app/eso/lib/libmhi2qcarplaycluster.so" ] || fail "$sh" "rerun after a failure did not finish ($rc)"
done
echo "MIBSI stock restore: refusals change nothing, restore + delete + verified backup, rerun no-op, partial failure keeps MIBSI libs:$shells PASS"
