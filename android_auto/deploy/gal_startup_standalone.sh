#!/bin/sh
# Standalone Android Auto GAL launcher. smartphone_integrator supervises this PID.
set -u
BASE=/mnt/app/root/sq5_android_auto
GAL=/mnt/app/eso/bin/apps/gal
LOG=/tmp/mib2q_aa_startup.log
OWNER=/tmp/sq5_luka_supervisor.owner

sd_flag() {
    for root in /fs/sda0 /net/mmx/fs/sda0; do
        [ -f "$root/$1" ] && return 0
    done
    return 1
}

fallback() {
    reason=$1
    shift
    echo "stock GAL: $reason" >> "$LOG"
    unset LD_PRELOAD SQ5_CLUSTER_PROBE
    exec "$GAL" "$@"
}

echo "launch pid=$$ args=$*" >> "$LOG"
for flag in mib2q_aa_off sq5_hook_off; do
    sd_flag "$flag" && fallback "disabled by $flag" "$@"
done

for file in libsq5_cluster_live.so cluster-player maneuver_render \
            sq5_luka_monitor.sh sq5_luka_processes.sh; do
    [ -r "$BASE/$file" ] || fallback "missing $file" "$@"
done

# Keep LuKa's tested route-arrow renderer lifecycle, without the old main-screen mirror.
echo $$ > "$OWNER.$$" && mv "$OWNER.$$" "$OWNER"
H=$BASE WLOG=/tmp/sq5_luka_wrapper.log OWNER_FILE=$OWNER LD_PRELOAD= \
    "$BASE/sq5_luka_monitor.sh" "$$" </dev/null >>/tmp/sq5_luka_wrapper.log 2>&1 &

# The player owns window 99 and exits when this GAL generation disappears.
LD_PRELOAD= SQ5_CLUSTER_PROBE= \
LD_LIBRARY_PATH=/mnt/app/root/lib-target:/eso/lib:/mnt/app/usr/lib:/mnt/app/armle/lib:/mnt/app/armle/lib/dll:/mnt/app/armle/usr/lib \
    "$BASE/cluster-player" "$$" </dev/null >/tmp/sq5_cluster_player.log 2>&1 &

export SQ5_CLUSTER_PROBE=1
export LD_PRELOAD=$BASE/libsq5_cluster_live.so
echo "Android Auto cluster enabled; player=$! preload=$LD_PRELOAD" >> "$LOG"
exec "$GAL" "$@"
