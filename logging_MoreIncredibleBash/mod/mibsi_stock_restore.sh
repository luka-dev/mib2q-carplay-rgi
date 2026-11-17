#!/bin/sh
# Remove MIBSI (the release checksummed below) from an MHI2Q_ER_AUG22_P5092 MU1329 unit by
# putting back the four stock files it replaced and deleting what it added.  Called by
# custom.sh when mibsi_stock_restore.txt is on the card:  sh mibsi_stock_restore.sh <card> <out>
#
# The stock files come from the unit's own firmware (MMX2QC app.img / efs-sys system.img of
# MHI2Q_ER_AUG22_P5092 MU1329), copied to <card>/mod/stock_p5092/; they are never in the repo.
# The list of MIBSI paths is the one its mibsi-install-tool manages, plus the red-menu staging
# copies of its menu.  QNX 6.5 /bin/sh is pdksh: portable subset only.
#
#   1. check (no writes): firmware, card stock files, every file it will replace must be
#      MIBSI's known version or already stock, /eso/bin/apps/dio_manager must match
#      /mnt/app/eso/bin/apps/dio_manager (/eso is the same tree)
#   2. back up every path it will replace or delete to <card>/mibsi_backup/<NNN of out>/ and
#      verify the file copies.  Outside carplay_logs/: MIBSI's and the firmware's files do not
#      belong in a bug report
#   3. replace each file through <file>.stock-new + mv, verify it; delete MIBSI's paths only
#      after all four replacements are verified (its loaders need its libraries until then)
#   4. check again
# Exit 0 = done or nothing to do, 1 = refused before any change, 2 = stopped partway (rerun;
# the backup is on the card).  Restart the MMI afterwards.
set -u
CARD=$1 OUT=$2
U=${CP_UNIT_ROOT:-}                                         # host test only: fake unit root
VERSION_FILE=${CP_VERSION_FILE:-/net/rcc/dev/shmem/version.txt}
MOUNT=${CP_RESTORE_MOUNT-1}                                 # empty: no remount (host test)
STOCK=$CARD/mod/stock_p5092
TABLE=$OUT/restore_table.txt
BK=$CARD/mibsi_backup/${OUT##*/}

# R <path> <stock file> <mode> <stock cksum> <size> <MIBSI cksum> <size>  (replaced, restore)
# D <path>                                     (MIBSI only, absent in stock: back up, delete)
table() {
    if [ -n "${CP_RESTORE_SUMS:-}" ]; then cat "$CP_RESTORE_SUMS"; return; fi
    cat <<'EOF'
R /mnt/app/eso/hmi/lsd/lsd.sh app/lsd.sh 0777 1896769701 15055 143679406 15279
R /mnt/app/eso/bin/apps/gal app/gal 0777 2839327272 1405891 2379396259 1405891
R /mnt/app/eso/bin/apps/dio_manager app/dio_manager 0777 1446556070 691203 2388818448 695420
R /mnt/system/etc/eso/production/dio_manager.json system/dio_manager.json 0755 3241944262 15838 2451958873 6473
D /mnt/app/eso/lib/libmhi2qaacluster.so
D /mnt/app/eso/lib/libmhi2qcarplaycluster.so
D /mnt/app/eso/bin/apps/mhi2q-aa-turn-card-renderer
D /mnt/app/eso/bin/apps/mhi2q-aa-cluster-display
D /mnt/app/eso/hmi/lsd/jars/001_mhi2q_aa_observer.jar
D /mnt/app/eso/hmi/lsd/jars/002_mhi2q_aa_navigation_proxy.jar
D /mnt/app/eso/hmi/engdefs/mibsi-main.esd
D /mnt/app/eso/hmi/engdefs/mibsi-main.esd.new
D /mnt/app/eso/hmi/engdefs/mibsi-main.esd.new.checksum
D /mnt/app/eso/hmi/engdefs/mibsi-main.esd.new.fileinfo
D /mnt/app/eso/hmi/engdefs/scripts/mibsi
D /mnt/app/root/mibsi
EOF
}

ck() { set -- $(cksum < "$1" 2>/dev/null); echo "${1:-none} ${2:-none}"; }
say() { echo "restore: $*"; }
refuse() { say "REFUSED, nothing changed: $*"; exit 1; }

# ---- 1. check --------------------------------------------------------------------------
command -v cksum > /dev/null 2>&1 || refuse "no cksum tool on this unit"
[ -f "$VERSION_FILE" ] || refuse "no $VERSION_FILE"
grep "Current train = 'MHI2Q_ER_AUG22_P5092'" "$VERSION_FILE" > /dev/null 2>&1 &&
    grep "MainUnit version = '1329'" "$VERSION_FILE" > /dev/null 2>&1 ||
    refuse "firmware is not MHI2Q_ER_AUG22_P5092 MU1329 (the stock files are from that build)"
table > "$TABLE" || refuse "cannot write $TABLE"
todo=0 bad=0
while read kind p rel mode ssum ssize msum msize; do
    case $kind in
    R)
        got=$(ck "$STOCK/$rel")
        [ "$got" = "$ssum $ssize" ] || { say "card stock file $STOCK/$rel is $got, expected $ssum $ssize"; bad=1; }
        now=$(ck "$U$p")
        if [ "$now" = "$ssum $ssize" ]; then say "stock already: $p"
        elif [ "$now" = "$msum $msize" ]; then say "MIBSI version, will restore: $p"; todo=1
        else say "UNKNOWN version $now (neither stock nor MIBSI): $p"; bad=1
            [ ! -e "$U$p.carplay-stock" ] ||
                say "  the CarPlay patch's installer changed it: run that patch's uninstaller first"
        fi ;;
    D)
        if [ -e "$U$p" ]; then say "MIBSI path, will delete: $p"; todo=1; fi ;;
    esac
done < "$TABLE"
[ "$bad" = 0 ] || refuse "see the lines above"
# MIBSI writes /eso/...; this script writes /mnt/app/eso/...: they must be one tree
if [ -e "$U/eso/bin/apps/dio_manager" ] &&
   [ "$(ck "$U/eso/bin/apps/dio_manager")" != "$(ck "$U/mnt/app/eso/bin/apps/dio_manager")" ]; then
    refuse "/eso/bin/apps/dio_manager differs from /mnt/app/eso/bin/apps/dio_manager"
fi
[ "$todo" = 1 ] || { say "nothing to do: all four files are stock and no MIBSI path is left"; exit 0; }

# ---- 2. back up -------------------------------------------------------------------------
mkdir -p "$BK" || refuse "cannot create $BK"
while read kind p rest; do
    [ -e "$U$p" ] || continue
    mkdir -p "$BK${p%/*}" && cp -R "$U$p" "$BK${p%/*}/" || refuse "backup of $p failed"
    if [ -f "$U$p" ] && [ "$(ck "$U$p")" != "$(ck "$BK$p")" ]; then refuse "backup of $p does not match"; fi
    ls -ld "$U$p" >> "$BK/modes.txt" 2>&1
done < "$TABLE"
sync
say "backup: $BK"

# ---- 3. restore -------------------------------------------------------------------------
if [ -n "$MOUNT" ]; then
    mount -uw /mnt/app || refuse "cannot remount /mnt/app read-write"
    mount -uw /mnt/system || { mount -ur /mnt/app; refuse "cannot remount /mnt/system read-write"; }
fi
remount_ro() { [ -z "$MOUNT" ] || { mount -ur /mnt/system; mount -ur /mnt/app; }; }
failed=0
while read kind p rel mode ssum ssize msum msize; do
    [ "$kind" = R ] || continue
    [ "$(ck "$U$p")" = "$ssum $ssize" ] && continue
    n=$U$p.stock-new
    if cp "$STOCK/$rel" "$n" && chmod "$mode" "$n" && [ "$(ck "$n")" = "$ssum $ssize" ] &&
       mv -f "$n" "$U$p" && [ "$(ck "$U$p")" = "$ssum $ssize" ]; then
        chown 0:0 "$U$p" 2>/dev/null
        say "restored stock: $p"
    else
        rm -f "$n"; say "FAILED to restore $p (now $(ck "$U$p"))"; failed=1; break
    fi
done < "$TABLE"
sync
if [ "$failed" = 1 ]; then
    remount_ro; say "STOPPED: MIBSI's files are kept so its programs still find them; rerun"; exit 2
fi
while read kind p rest; do
    [ "$kind" = D ] && [ -e "$U$p" ] || continue
    if rm -rf "$U$p" && [ ! -e "$U$p" ]; then say "deleted: $p"; else say "FAILED to delete $p"; failed=1; fi
done < "$TABLE"
sync
remount_ro

# ---- 4. check again ---------------------------------------------------------------------
while read kind p rel mode ssum ssize msum msize; do
    case $kind in
    R) [ "$(ck "$U$p")" = "$ssum $ssize" ] || { say "NOT stock after restore: $p"; failed=1; } ;;
    D) [ ! -e "$U$p" ] || { say "still present: $p"; failed=1; } ;;
    esac
done < "$TABLE"
if [ -e "$U/eso/bin/apps/dio_manager" ] &&
   [ "$(ck "$U/eso/bin/apps/dio_manager")" != "$(ck "$U/mnt/app/eso/bin/apps/dio_manager")" ]; then
    say "/eso/bin/apps/dio_manager differs from the restored /mnt/app/eso one"; failed=1
fi
[ "$failed" = 0 ] || { say "STOPPED: see the lines above; rerun (backup: $BK)"; exit 2; }
say "DONE: MIBSI removed, the four files are stock; restart the MMI"
exit 0
