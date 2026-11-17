#!/bin/sh
# M.I.B. -> Advanced Settings -> Run Custom Script: save every CarPlay log to the SD card.
# QNX 6.5 /bin/sh is ksh (pdksh) - this script stays inside its portable subset.
#
# Each run writes <card>/carplay_logs/NNN/ (001, 002, ... - numbered, because the unit's
# clock is often wrong; info.txt records what the clock said) and then creates
# /tmp/carplay_verbose, which turns on the full hook and renderer diagnostics.
#
#   1. run it once        -> saves what is there now, switches verbose on
#   2. reconnect the phone and use CarPlay (the hook reads the marker per session)
#   3. run it again       -> saves the verbose session into the next folder
#
# The marker lives in RAM and is gone after a reboot.
#
# Opt-in MIBSI removal, by an empty trigger file at the top of the card (one of the two):
#   mibsi_uninstall.txt -> runs MIBSI's own uninstaller from its uninstall card, once (mibsi_uninstall)
#   mibsi_stock_restore.txt -> removes MIBSI with the firmware's stock files (mibsi_stock_restore.sh)
# Apart from the marker and what MIBSI's uninstaller or mibsi_stock_restore.sh change when
# their trigger asks for it, nothing is changed on the unit.
# Copyright (c) 2026 LuKa (@LuKa_dev)
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/sbin
export PATH
unset LD_PRELOAD

case $0 in */*) D=${0%/*} ;; *) D=. ;; esac
D=$(cd "$D" && pwd) || exit 1

# The logs live in the MMX's /tmp.  Forward a manual RCC launch.
if [ ! -d /mnt/app/eso/hmi/lsd ] && [ -d /net/mmx/mnt/app/eso/hmi/lsd ]; then
    exec on -f mmx /bin/sh "$D/custom.sh" "$@"
fi

SRC=${CP_LOG_SRC:-/tmp}                  # override only for a host test
CARD=${D%/mod}                           # this script sits in <card>/mod/

# The card may be mounted read-only: remount the mount point that holds it.
can_write() { : > "$1/.carplay_write_test" 2>/dev/null && rm -f "$1/.carplay_write_test"; }
if ! can_write "$CARD"; then
    for m in "$CARD" /fs/sda0 /fs/sdb0 /fs/usb0_0 /net/mmx/fs/sda0 /net/mmx/fs/sdb0 /net/mmx/fs/usb0_0; do
        [ -d "$m/mod" ] || continue
        mount -uw "$m" 2>/dev/null
        can_write "$m" && { CARD=$m; break; }
    done
fi
can_write "$CARD" || { echo "FAILED: card at $CARD is not writable"; exit 1; }

BASE=$CARD/carplay_logs
mkdir -p "$BASE" || { echo "FAILED mkdir $BASE"; exit 1; }
# Next number = highest existing save + 1 (a deleted older folder is never reused).
last=0
for d in "$BASE"/*; do
    v=${d##*/}
    case $v in ''|*[!0-9]*) continue ;; esac
    while :; do case $v in 0?*) v=${v#0} ;; *) break ;; esac; done   # 008 is octal to ksh
    [ "$v" -gt "$last" ] && last=$v
done
n=$((last+1))
case $n in ?) NN=00$n ;; ??) NN=0$n ;; *) NN=$n ;; esac
OUT=$BASE/$NN
mkdir "$OUT" || { echo "FAILED mkdir $OUT"; exit 1; }
echo "custom.sh: saving CarPlay logs to $OUT"

run() {   # run <file> <command...>: best effort, output and errors into <file>
    f=$OUT/$1; shift
    "$@" > "$f" 2>&1 || echo "(exit $?)" >> "$f"
}

# ---- what the unit looks like right now ----
{
    echo "save        $NN"
    echo "date        $(date 2>&1)"
    echo "verbose     $( [ -e /mnt/app/carplay_verbose ] || [ -e "$SRC/carplay_verbose" ] && echo on || echo off) (before this run)"
    echo "card        $CARD"
    echo "script      $D/custom.sh"
} > "$OUT/info.txt"
run uname.txt uname -a
run pidin_ar.txt pidin ar
run pidin_info.txt pidin info
run mount.txt mount
run df.txt df -k
run ifconfig.txt ifconfig -a
run netstat_rn.txt netstat -rn
run pf_rules.txt pfctl -sr
run sloginfo_mmx.txt sloginfo
run sloginfo_rcc.txt on -f rcc sloginfo
run hooks_ls.txt ls -la /mnt/app/root/hooks /mnt/app/eso/hmi/lsd/jars /mnt/app/eso/bin/apps/smartphone_integrator /mnt/app/armle/lib/dll/deva-ctrl-qc.so
run carplay_stock_ls.txt find /mnt/app /mnt/system -name '*.carplay-stock'
run tmp_ls.txt ls -la "$SRC/"     # /tmp is a symlink to /dev/shmem: list what it points at
run cores_ls.txt ls -la /mnt/ota/system/core
run e2p_train.txt on -f rcc /usr/apps/modifyE2P r 3A0 19     # firmware train, ASCII bytes
run dio_env.txt pidin -p dio_manager env                       # the LD_PRELOAD dio really runs with
# leftovers of other CarPlay mods: they preload their own libraries into dio
run foreign_ls.txt ls -la /mnt/app /mnt/app/root
run foreign_find.txt find /mnt/app /mnt/system -name 'mibr-*' -o -name '*altscreen*' -o -name '*carplaycluster*' \
    -o -name 'mhi2q*' -o -name '*mibsi*' -o -name '*carplay_loader*'
# MIBSI leftovers: its renderers, libraries and HMI jars, a dio_manager that loads its library
# as DT_NEEDED (dio_env.txt cannot show that, dio_mem.txt does), lsd.sh and dio_manager.json edits
MIBSI_FILES="/mnt/app/eso/bin/apps/mhi2q-aa-cluster-display /mnt/app/eso/bin/apps/mhi2q-aa-turn-card-renderer
    /mnt/app/eso/lib/libmhi2qcarplaycluster.so /mnt/app/eso/lib/libmhi2qaacluster.so
    /mnt/app/eso/hmi/lsd/jars/001_mhi2q_aa_observer.jar /mnt/app/eso/hmi/lsd/jars/002_mhi2q_aa_navigation_proxy.jar
    /mnt/app/root/mibsi /eso/bin/apps/dio_manager /eso/bin/apps/gal /eso/lib/libmhi2qaacluster.so
    /mnt/system/etc/eso/production/dio_manager.json /mnt/app/eso/bin/apps/dio_manager"
run mibsi_ls.txt ls -la $MIBSI_FILES
run lsd_mibsi.txt grep -n MIBSI /mnt/app/eso/hmi/lsd/lsd.sh
run dio_mem.txt pidin -p dio_manager mem
dio_sums() {   # sizes always, a sum when a checksum tool is on PATH (cksum was, on P5092 runs)
    ck=
    for t in cksum md5 md5sum sha1sum sha256sum; do
        command -v "$t" > /dev/null 2>&1 && { ck=$t; break; }
    done
    echo "checksum tool: ${ck:-none, sizes only}"
    for x in /mnt/app/eso/bin/apps/dio_manager /eso/bin/apps/dio_manager /mnt/system/etc/eso/production/dio_manager.json*; do
        [ -f "$x" ] || { echo "$x: missing"; continue; }
        echo "$x: $(wc -c < "$x") bytes"
        # No text search works inside a binary on the unit (grep -c and tr both reported 0
        # for the MIBSI loader, which links libmhi2qcarplaycluster.so): match its checksum.
        case $x in *.json*) ;; *)
            [ "$ck" = cksum ] && case $(cksum < "$x" 2>/dev/null) in
                2388818448*695420*) echo "$x: MIBSI P5092 CarPlay loader (known cksum; loads libmhi2qcarplaycluster.so)" ;;
            esac ;;
        esac
        [ -z "$ck" ] || "$ck" "$x"
    done
    :
}
run dio_cksum.txt dio_sums
# Red-menu software update (SWDL) state, kept by RCC in efs-persist (the path M.I.B. itself
# backs up and cleans): FileCopyInfo/<name>.info records installed components, update.txt.*
# the last aborted/cancelled update with each step's status, Log/<user|service>/<n>/*.z (zlib)
# every session's steps and error codes.  Saved only on a unit with MIBSI or with a MIBSI
# trigger on the card: the red menu is how MIBSI is installed and removed, and a CarPlay bug
# report does not need these logs.
SWDL_DIR=${CP_SWDL_DIR:-/net/rcc/mnt/efs-persist/SWDL}   # override only for a host test
MIBSI_DIR=${CP_MIBSI_DIR:-/mnt/app/root/mibsi}           # override only for a host test
swdl_state() {
    echo "--- $SWDL_DIR/FileCopyInfo"
    ls -la "$SWDL_DIR/FileCopyInfo"
    for x in "$SWDL_DIR"/FileCopyInfo/*.info "$SWDL_DIR"/update.txt*; do
        [ -f "$x" ] || continue
        echo "--- $x"; cat "$x"; echo
    done
    for x in /mnt/app/eso/hmi/engdefs/mibsi-main.esd.new.fileinfo /mnt/app/eso/hmi/engdefs/mibsi-main.esd.new.checksum; do
        [ -f "$x" ] || continue
        echo "--- $x"; cat "$x"; echo
    done
    echo "--- /mnt/app/root/mibsi"
    ls -laR /mnt/app/root/mibsi
    for x in version installed.release origin; do
        [ -f "/mnt/app/root/mibsi/$x" ] || continue
        echo "--- /mnt/app/root/mibsi/$x"; cat "/mnt/app/root/mibsi/$x"; echo
    done
    :
}
if [ -d "$MIBSI_DIR" ] || [ -f "$CARD/mibsi_uninstall.txt" ] || [ -f "$CARD/mibsi_stock_restore.txt" ]; then
    run swdl_state.txt swdl_state
    # The whole SWDL folder (about 0.5 MB): Log/user/<n>/ holds every red-menu session, failed
    # ones included, and survives a reboot.
    mkdir "$OUT/swdl"
    run swdl_copy.txt cp -R "$SWDL_DIR/FileCopyInfo" "$SWDL_DIR/Log" "$OUT/swdl/"
fi

# Opt-in (mibsi_uninstall.txt at the top of the card, next to MIBSI's uninstall card files):
# run MIBSI's own uninstaller, Bootstrap/final/product/install_product.sh, from here.  The red
# menu runs it as its final script with a 30 s limit (FinalScriptMaxTime) and keeps no output;
# on this unit every red-menu try copied the card, ran the final script ({Final}=DONE in
# update.txt.Abort.old), then logged error 113 for "MIBSI bootstrap", and MIBSI-Logs never
# reached the card.  Here it runs unchanged, without a time limit, with its output saved.  It
# checks the firmware, its dependencies and its payload first and refuses before any write.
# One dependency is the stock libairplay.so: while this CarPlay patch has it patched, MIBSI's
# uninstaller refuses here too, until this patch's uninstaller has put it back.
# The trigger is renamed to mibsi_uninstall.done first, so it runs once per card.
mibsi_uninstall() {
    p=$CARD/Bootstrap/final/product/install_product.sh
    if [ ! -d "$MIBSI_DIR" ]; then
        echo "mibsi uninstall: $MIBSI_DIR absent (MIBSI not installed); nothing run"; return 0
    fi
    if [ ! -f "$p" ] || [ ! -f "$CARD/MIBSI-UNINSTALL" ]; then
        echo "mibsi uninstall: MIBSI's uninstall card files missing ($p, $CARD/MIBSI-UNINSTALL); nothing run"; return 0
    fi
    mv -f "$CARD/mibsi_uninstall.txt" "$CARD/mibsi_uninstall.done" ||
        { echo "mibsi uninstall: could not rename the trigger; nothing run"; return 0; }
    echo "mibsi uninstall: sh $p $CARD"
    sh "$p" "$CARD" > "$OUT/mibsi_uninstall.txt" 2>&1
    rc=$?
    MIBSI_UN_RC=$rc   # for the screen summary (a function with a redirection runs in this shell)
    echo "mibsi uninstall: exit $rc (0 = uninstalled; then restart the MMI) - output in mibsi_uninstall.txt"
    sync
    # the same checks as above, after the uninstall
    run mibsi_ls_after.txt ls -la $MIBSI_FILES
    run lsd_mibsi_after.txt grep -n MIBSI /mnt/app/eso/hmi/lsd/lsd.sh
    run dio_cksum_after.txt dio_sums
}
# Opt-in (mibsi_stock_restore.txt, with the stock files in mod/stock_p5092/): remove MIBSI by
# restoring the four stock files it replaced and deleting what it added; the steps and checks
# are in mibsi_stock_restore.sh.  Output: mibsi_stock_restore.txt; backup: <card>/mibsi_backup/NNN/
# (not in carplay_logs/).  The trigger becomes .done only when it finished; a refusal changes
# nothing and keeps it.
mibsi_stock_restore() {
    echo "mibsi stock restore: sh $D/mibsi_stock_restore.sh $CARD $OUT"
    sh "$D/mibsi_stock_restore.sh" "$CARD" "$OUT" > "$OUT/mibsi_stock_restore.txt" 2>&1
    rc=$?
    MIBSI_RC=$rc      # for the screen summary
    tail -1 "$OUT/mibsi_stock_restore.txt"
    echo "mibsi stock restore: exit $rc (0 done, 1 refused - nothing changed, 2 stopped partway - rerun)"
    [ "$rc" = 0 ] && mv -f "$CARD/mibsi_stock_restore.txt" "$CARD/mibsi_stock_restore.done"
    run mibsi_ls_after.txt ls -la $MIBSI_FILES
    run lsd_mibsi_after.txt grep -n MIBSI /mnt/app/eso/hmi/lsd/lsd.sh
    run dio_cksum_after.txt dio_sums
}
if [ -f "$CARD/mibsi_uninstall.txt" ] && [ -f "$CARD/mibsi_stock_restore.txt" ]; then
    m="mibsi: both mibsi_uninstall.txt and mibsi_stock_restore.txt on the card; neither run (keep one)"
    echo "$m"; echo "$m" >> "$OUT/info.txt"
elif [ -f "$CARD/mibsi_uninstall.txt" ]; then
    mibsi_uninstall >> "$OUT/info.txt" 2>&1
elif [ -f "$CARD/mibsi_stock_restore.txt" ]; then
    mibsi_stock_restore >> "$OUT/info.txt" 2>&1
fi

# ---- logs and captures from /tmp (never the shared-memory objects also living there) ----
mkdir "$OUT/tmp"
for f in "$SRC"/*.log "$SRC"/*.log.* "$SRC"/carplay_* "$SRC"/spatial_* "$SRC"/*.pid; do
    [ -f "$f" ] || continue
    cp "$f" "$OUT/tmp/" 2>/dev/null || echo "copy failed: $f" >> "$OUT/info.txt"
done

# ---- configs we patch ----
mkdir "$OUT/config"
for f in /mnt/system/etc/eso/production/dio_manager.json \
         /mnt/system/etc/eso/production/smartphone_integrator.json \
         /mnt/system/etc/pf.conf /mnt/system/etc/pf.ecm0.conf \
         /mnt/system/etc/usblauncher_otg.lua* \
         /mnt/system/etc/scripts/startncm.sh* /mnt/system/etc/scripts/stopncm.sh*; do
    [ -f "$f" ] && cp "$f" "$OUT/config/" 2>/dev/null
done

# ---- SI's own dumps at a CarPlay failure (sloginfo of that moment): the newest five ----
mkdir "$OUT/si_errors"
i=0
for f in $(ls -t "$SRC"/smartphone_integrator_error_*.tar.gz 2>/dev/null); do
    [ $i -lt 5 ] || break
    cp "$f" "$OUT/si_errors/" 2>/dev/null && i=$((i+1))
done

# ---- every crash dump and freeze log (the dumper writes them to /mnt/ota/system/core) ----
# A whole-MMI freeze needs j9/gemib/pc too, not only ours.  The dir survives reboots and
# fills up to ~16 MB, so a file the previous save already holds (same name and size) is
# skipped: two saves still fit one GitHub attachment.  The unit has no cmp.
CORE=${CP_CORE_SRC:-/mnt/ota/system/core}
case $last in ?) PREV=$BASE/00$last ;; ??) PREV=$BASE/0$last ;; *) PREV=$BASE/$last ;; esac
mkdir "$OUT/core"
for f in "$CORE"/*; do
    [ -f "$f" ] && [ -s "$f" ] || continue          # also skips the empty EBNavFreezeLog.N slots
    p=$PREV/core/${f##*/}
    [ -f "$p" ] && [ "$(wc -c < "$p")" = "$(wc -c < "$f")" ] && continue
    cp "$f" "$OUT/core/" 2>/dev/null
done

sync

# ---- switch the full diagnostics on for the next CarPlay session ----
if : > "$SRC/carplay_verbose" 2>/dev/null; then
    echo "verbose on: $SRC/carplay_verbose (hook: next phone connect; cluster renderer: now)"
else
    echo "WARN could not create $SRC/carplay_verbose"
fi
# A MIBSI removal's result goes on screen as well (its details are in info.txt): the old
# processes keep running until the restart, so after a removal the next step is the restart.
case ${MIBSI_RC-} in
0) echo "MIBSI REMOVED: the four files are stock. Restart the MMI now, before using CarPlay."
   echo "If this CarPlay patch is installed, run its installer again afterwards (dio_manager.json is stock)." ;;
1) echo "MIBSI NOT removed (refused, nothing changed): see $OUT/mibsi_stock_restore.txt" ;;
2) echo "MIBSI removal STOPPED partway: see $OUT/mibsi_stock_restore.txt, then run this again" ;;
esac
case ${MIBSI_UN_RC-} in
'') ;;
0) echo "MIBSI uninstalled by its own uninstaller. Restart the MMI now, before using CarPlay." ;;
*) echo "MIBSI's uninstaller failed (exit $MIBSI_UN_RC): see $OUT/mibsi_uninstall.txt" ;;
esac
echo "DONE: $OUT"
if [ "${MIBSI_RC-}" = 2 ]; then
    echo "Run this again now to finish the MIBSI removal (do not use CarPlay yet)."
elif [ "${MIBSI_RC-}" = 0 ] || [ "${MIBSI_UN_RC-}" = 0 ]; then
    echo "Restart the MMI, then run this again to save the state after the restart."
else
    echo "Reconnect the phone, use CarPlay, then run this again to save the verbose session."
fi
