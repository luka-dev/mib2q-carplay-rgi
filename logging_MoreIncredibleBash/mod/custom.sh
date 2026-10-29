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
# The marker lives in RAM and is gone after a reboot.  Nothing is deleted on the unit.
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
run foreign_find.txt find /mnt/app /mnt/system -name 'mibr-*' -o -name '*altscreen*' -o -name '*carplaycluster*'

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
echo "DONE: $OUT"
echo "Reconnect the phone, use CarPlay, then run this again to save the verbose session."
