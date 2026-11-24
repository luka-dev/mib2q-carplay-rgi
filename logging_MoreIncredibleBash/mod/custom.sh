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
# Opt-in diagnostics (Waze cluster build): an empty trigger file at the top of the card arms
# its RAM marker, and a missing one removes it (disarm without a reboot).  The hook reads the
# URL marker at the next SETUP or cluster setting change and the advert marker at the next
# phone connect; the renderer reads its two at the next stream.  info.txt records the armed
# set before and after the run.
#   alt_url_diag.txt  -> /tmp/carplay_alt_url_diag       advert_legacy.txt -> /tmp/carplay_alt_advert_legacy
#   luma_probe.txt    -> /tmp/carplay_luma_probe         capture111.txt    -> /tmp/carplay_cap111
#   card_display.txt  -> /tmp/carplay_alt_card_display  (hook, next phone connect: a third,
#                        card-only cluster display; its stream goes to /tmp/carplay_cap111b.h264.
#                        The hook holds the marker while that session runs (.active) and puts it
#                        back only after a good one (20 s or more, map keyframe); otherwise it
#                        stays removed and the reason is in carplay_alt_card_display.disarmed.
#                        Then this script does not re-arm: it renames the trigger to
#                        card_display.disarmed - rename it back to card_display.txt to arm again)
# Apart from those markers and a finished capture (capture111 / card_display) this run already
# saved, nothing is changed on the unit.
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

# <card trigger file without .txt>:<marker in $SRC>, see the header
DIAG="alt_url_diag:carplay_alt_url_diag advert_legacy:carplay_alt_advert_legacy"
DIAG="$DIAG luma_probe:carplay_luma_probe capture111:carplay_cap111"
DIAG="$DIAG card_display:carplay_alt_card_display"
diag_state() {   # the armed set as the hook and the renderers will see it: name=1|0 ...
    ds=
    for dp in $DIAG; do
        if [ -e "$SRC/${dp#*:}" ]; then ds="$ds ${dp%%:*}=1"; else ds="$ds ${dp%%:*}=0"; fi
    done
    echo "${ds# }"
}

# ---- what the unit looks like right now ----
{
    echo "save        $NN"
    echo "date        $(date 2>&1)"
    echo "verbose     $( [ -e /mnt/app/carplay_verbose ] || [ -e "$SRC/carplay_verbose" ] && echo on || echo off) (before this run)"
    echo "diag        $(diag_state) (before this run)"
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

# ---- opt-in diagnostics for the next CarPlay session: card trigger file -> RAM marker ----
# Present on the card: create the marker.  Gone from the card: remove it.  The renderer
# removes /tmp/carplay_cap111 when it takes its single capture, so every run re-arms it
# while capture111.txt stays on the card.  The re-arm also drops the RAM copy of the last
# capture, but only once tmp/ above holds all of it and the renderer has closed it, so a
# new capture starts from empty.  A capture still being written stays: the renderer keeps
# appending until its 12 MB cap or the end of that hook connection, and a later run saves
# the rest.
cap111_closed() {   # the saved renderer log shows 'cap111 wrote' after its last 'cap111 armed'
    rl=$OUT/tmp/altscreen_render.log
    [ -f "$rl" ] || return 1
    ev=$(grep -e 'cap111 armed' -e 'cap111 wrote' "$rl" 2>/dev/null)
    case ${ev##*cap111?armed} in
        *cap111?wrote*) return 0 ;;
    esac
    # No capture event: closed when the log is whole since this renderer started (an
    # earlier process wrote it and its exit closed it); unknown after a log truncation.
    [ -z "$ev" ] && grep 'altscreen_render: process start' "$rl" > /dev/null 2>&1
}
cap=$SRC/carplay_cap111.h264
if [ -e "$CARD/capture111.txt" ] && [ -f "$cap" ] && [ -f "$OUT/tmp/${cap##*/}" ]; then
    if ! cap111_closed; then
        echo "diag: $cap not closed per the renderer log; kept (a later run saves the rest)" >> "$OUT/info.txt"
    elif [ "$(wc -c < "$cap")" = "$(wc -c < "$OUT/tmp/${cap##*/}")" ]; then
        rm -f "$cap" && echo "diag: $cap saved to tmp/ and removed for the re-arm" >> "$OUT/info.txt"
    fi
fi
# The card-display capture is written by the hook (appended across sessions, 12 MB cap).
# Drop the RAM copy once tmp/ holds all of it and the saved hook log shows it closed.
cap111b_closed() {   # 'cap111b wrote' after the last 'cap111b armed' (rotated log first)
    ev=$(cat "$OUT/tmp/carplay_hook.log.1" "$OUT/tmp/carplay_hook.log" 2>/dev/null |
         grep -e 'cap111b armed' -e 'cap111b wrote')
    case ${ev##*cap111b?armed} in
        *cap111b?wrote*) return 0 ;;
    esac
    return 1
}
capb=$SRC/carplay_cap111b.h264
if [ -e "$CARD/card_display.txt" ] && [ -f "$capb" ] && [ -f "$OUT/tmp/${capb##*/}" ]; then
    if ! cap111b_closed; then
        echo "diag: $capb not closed per the hook log; kept (a later run saves the rest)" >> "$OUT/info.txt"
    elif [ "$(wc -c < "$capb")" = "$(wc -c < "$OUT/tmp/${capb##*/}")" ]; then
        rm -f "$capb" && echo "diag: $capb saved to tmp/ and removed" >> "$OUT/info.txt"
    fi
fi
# The hook's rollback wins over the trigger: after a self-disarm (its reason file was saved
# to tmp/ above) this run does not re-arm card_display, and the trigger is renamed so the
# next run does not either.  The reason file goes once the trigger is renamed or gone.
CARD_OFF=
cdis=$SRC/carplay_alt_card_display.disarmed
if [ -f "$cdis" ]; then
    CARD_OFF=$(cat "$cdis" 2>/dev/null)
    [ -n "$CARD_OFF" ] || CARD_OFF="no reason recorded"
    echo "CARD DISPLAY self-disarmed by the hook: $CARD_OFF"
    echo "diag: card_display self-disarmed by the hook: $CARD_OFF" >> "$OUT/info.txt"
    if [ ! -e "$CARD/card_display.txt" ]; then
        rm -f "$cdis"
    elif mv -f "$CARD/card_display.txt" "$CARD/card_display.disarmed"; then
        rm -f "$cdis"
        m="diag: card_display.txt renamed to card_display.disarmed (rename it back to card_display.txt to arm again)"
        echo "$m"; echo "$m" >> "$OUT/info.txt"
    else
        echo "diag: could not rename card_display.txt; card_display stays disarmed" >> "$OUT/info.txt"
    fi
fi
[ -e "$SRC/carplay_alt_card_display.active" ] &&
    echo "diag: card_display session running (the hook holds the marker until it ends)" >> "$OUT/info.txt"
# ( : > ) in a subshell: a failed redirection on ':' ends ksh93/dash, not only the command.
for dp in $DIAG; do
    mk=$SRC/${dp#*:}
    trig=$CARD/${dp%%:*}.txt
    case ${dp%%:*}:$CARD_OFF in card_display:?*) trig= ;; esac   # self-disarmed: never re-armed here
    if [ -n "$trig" ] && [ -e "$trig" ]; then
        ( : > "$mk" ) 2>/dev/null || { echo "WARN could not create $mk"; echo "diag: could not create $mk" >> "$OUT/info.txt"; }
    else
        rm -f "$mk" 2>/dev/null || { echo "WARN could not remove $mk"; echo "diag: could not remove $mk" >> "$OUT/info.txt"; }
    fi
done
# No card_display trigger: a card session still running must not put the marker back.
[ -e "$CARD/card_display.txt" ] || rm -f "$SRC/carplay_alt_card_display.active"
DIAG_NOW=$(diag_state)
echo "diag markers: $DIAG_NOW (after this run)" >> "$OUT/info.txt"
echo "diag markers: $DIAG_NOW"

sync

# ---- switch the full diagnostics on for the next CarPlay session ----
if : > "$SRC/carplay_verbose" 2>/dev/null; then
    echo "verbose on: $SRC/carplay_verbose (hook: next phone connect; cluster renderer: now)"
else
    echo "WARN could not create $SRC/carplay_verbose"
fi
echo "DONE: $OUT"
echo "Reconnect the phone, use CarPlay, then run this again to save the verbose session."
