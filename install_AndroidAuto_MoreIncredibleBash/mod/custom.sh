#!/bin/sh
# M.I.B. custom-script installer for mib2q-android-auto-cluster.
# Default action is a read-only audit. Add mib2q_aa_install or mib2q_aa_uninstall
# to the SD-card root to authorize that operation.
set -u
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/usr/bin
export PATH
unset LD_PRELOAD

case $0 in */*) MOD=${0%/*} ;; *) MOD=. ;; esac
MOD=$(cd "$MOD" && pwd) || exit 1
TEST_ROOT=${MIB2Q_AA_TEST_ROOT:-}

if [ -z "$TEST_ROOT" ] && [ ! -d /mnt/app/eso/bin/apps ] && [ -d /net/mmx/mnt/app/eso/bin/apps ]; then
    exec on -f mmx /bin/sh "$MOD/custom.sh" "$@"
fi

rooted() { if [ -n "$TEST_ROOT" ]; then echo "$TEST_ROOT$1"; else echo "$1"; fi; }
SD=$(rooted /fs/sda0)
[ -d "$SD" ] || SD=$(rooted /net/mmx/fs/sda0)
RES=$MOD/android-auto
PROFILES=$RES/profiles.list
BASE=$(rooted /mnt/app/root/sq5_android_auto)
BACKUP=$BASE/backup
CFG=$(rooted /mnt/system/etc/eso/production/smartphone_integrator.json)
JAR=$(rooted /mnt/app/eso/hmi/lsd/jars/mib2q_android_auto_cluster.jar)
GAL=$(rooted /mnt/app/eso/bin/apps/gal)
RECEIVER=$(rooted /mnt/app/eso/lib/libautoreceiver.so)
LSD=$(rooted /ifs/lsd.jxe)
FILES="libsq5_cluster_live.so cluster-player maneuver_render flag_atlas.rgba gal_startup.sh
sq5_luka_monitor.sh sq5_luka_processes.sh sq5_luka_cleanup.sh"

size_of() { set -- $(ls -l "$1" 2>/dev/null); echo ${5:-0}; }
sum_of() { set -- $(cksum "$1" 2>/dev/null); echo "${1:-0} ${2:-0}"; }
mode_for() { case $1 in *.sh|cluster-player|maneuver_render|*.so) echo 755;; *) echo 644;; esac; }
fail() { echo "FAILED: $*"; exit 1; }

verify_payload() {
    [ -f "$RES/manifest.cksum" ] || fail "missing $RES/manifest.cksum"
    count=0
    core=0
    jars=0
    while read crc bytes name extra; do
        [ -z "${extra:-}" ] || fail "malformed manifest line for $name"
        case "$name" in *..*|/*|*\\*) fail "unsafe manifest entry $name";; esac
        case "$name" in
            libsq5_cluster_live.so|cluster-player|maneuver_render|flag_atlas.rgba|gal_startup.sh|\
            sq5_luka_monitor.sh|sq5_luka_processes.sh|sq5_luka_cleanup.sh|\
            config_tool|profiles.list) core=$((core+1));;
            profiles/*/mib2q_android_auto_cluster.jar) jars=$((jars+1));;
            *) fail "unexpected manifest entry $name" ;;
        esac
        [ "$(sum_of "$RES/$name")" = "$crc $bytes" ] || fail "payload checksum $name"
        count=$((count+1))
    done < "$RES/manifest.cksum"
    [ "$core" = 10 ] || fail "manifest has $core common/profile-index files, expected 10"
    [ "$jars" -ge 1 ] || fail "manifest has no firmware Java payload"
    [ "$count" = $((core+jars)) ] || fail "manifest entry accounting failed"
}

select_profile() {
    PROFILE_ID= PROFILE_LABEL= PROFILE_JAR= PROFILE_STATE=
    CANDIDATE_LABEL= CANDIDATE_STATE=
    [ -f "$PROFILES" ] || return 1
    set -- $(sum_of "$GAL"); actual_gal_crc=$1; actual_gal_size=$2
    set -- $(sum_of "$RECEIVER"); actual_receiver_crc=$1; actual_receiver_size=$2
    set -- $(sum_of "$LSD"); actual_lsd_crc=$1; actual_lsd_size=$2
    while IFS='|' read pid label state gal_crc gal_size receiver_crc receiver_size lsd_crc lsd_size jar_path extra; do
        [ -n "$pid" ] || continue
        [ -z "${extra:-}" ] || continue
        if [ -n "$lsd_crc" ] && [ "$actual_lsd_crc $actual_lsd_size" = "$lsd_crc $lsd_size" ]; then
            if [ -z "$gal_size" ] || [ "$actual_gal_size" = "$gal_size" ]; then
                CANDIDATE_LABEL=$label
                CANDIDATE_STATE=$state
            fi
        fi
        [ "$state" = installable ] || continue
        [ -n "$gal_crc" ] && [ -n "$receiver_crc" ] && [ -n "$lsd_crc" ] && [ -n "$jar_path" ] || continue
        [ "$actual_gal_crc $actual_gal_size" = "$gal_crc $gal_size" ] || continue
        [ "$actual_receiver_crc $actual_receiver_size" = "$receiver_crc $receiver_size" ] || continue
        [ "$actual_lsd_crc $actual_lsd_size" = "$lsd_crc $lsd_size" ] || continue
        [ -f "$RES/$jar_path" ] || continue
        PROFILE_ID=$pid
        PROFILE_LABEL=$label
        PROFILE_STATE=$state
        PROFILE_JAR=$jar_path
    done < "$PROFILES"
    [ -n "$PROFILE_ID" ]
}

remount_rw() { [ -n "$TEST_ROOT" ] || mount -uw "$1"; }
remount_ro() { [ -n "$TEST_ROOT" ] || mount -ur "$1" 2>/dev/null; }
sync_all() { [ -n "$TEST_ROOT" ] || sync; }

launch_value() {
    "$RES/config_tool" get "$CFG" "$1" 2>/dev/null || echo unreadable
}

audit() {
    echo "MIB2Q Android Auto cluster - read-only audit"
    echo "  gal:                 $(sum_of "$GAL")"
    echo "  libautoreceiver.so:  $(sum_of "$RECEIVER")"
    echo "  lsd.jxe:             $(sum_of "$LSD")"
    if select_profile; then
        echo "  firmware profile:    $PROFILE_LABEL"
        echo "  compatibility:       EXACT INSTALLABLE PROFILE"
        echo "  selected Java:       $PROFILE_JAR"
    elif [ -n "$CANDIDATE_LABEL" ]; then
        echo "  firmware profile:    $CANDIDATE_LABEL"
        echo "  compatibility:       BLOCKED ($CANDIDATE_STATE)"
    else
        echo "  firmware profile:    unknown"
        echo "  compatibility:       NOT VALIDATED - installer will refuse"
    fi
    echo "  current GAL exec:    $(launch_value exec)"
    echo "  current GAL path:    $(launch_value path)"
    echo "  installed JAR:       $(size_of "$JAR") bytes"
    echo ""
    echo "Install plan:"
    echo "  + $BASE/{hook,player,renderer,startup scripts}"
    echo "  + $JAR"
    echo "  ~ $CFG: children.gal.exec/path only"
    echo "  = gal, libautoreceiver.so, lsd.jxe and gal.json remain byte-identical"
    echo "  = uninstall restores the saved configuration and removes only the added files"
}

put() {
    src=$1; dst=$2; mode=$3; tmp=$dst.aa-new.$$
    cp -p "$src" "$tmp" || return 1
    chmod "$mode" "$tmp" || { rm -f "$tmp"; return 1; }
    [ "$(sum_of "$src")" = "$(sum_of "$tmp")" ] || { rm -f "$tmp"; return 1; }
    mv -f "$tmp" "$dst"
}

install_it() {
    verify_payload
    if ! select_profile; then
        if [ -n "$CANDIDATE_LABEL" ]; then
            fail "$CANDIDATE_LABEL is recognized but blocked ($CANDIDATE_STATE); exact native hashes/ABI validation are still required"
        fi
        fail "firmware does not match an installable profile; run scripts/check_firmware.py and port first"
    fi
    ex=$(launch_value exec); pa=$(launch_value path)
    if [ "$pa/$ex" = "$BASE/gal_startup.sh" ]; then
        [ -f "$BACKUP/smartphone_integrator.json" ] || fail "installed launch has no backup"
    elif [ "$pa/$ex" = "/mnt/app/eso/bin/apps/gal" ]; then
        :
    else
        fail "GAL is already launched through $pa/$ex; uninstall that integration or merge it explicitly"
    fi

    remount_rw /mnt/app || fail "remount /mnt/app"
    remount_rw /mnt/system || fail "remount /mnt/system"
    mkdir -p "$BASE" "$BACKUP" || fail "create install directory"

    if [ ! -f "$BACKUP/smartphone_integrator.json" ]; then
        cp -p "$CFG" "$BACKUP/smartphone_integrator.json" || fail "backup smartphone_integrator.json"
    fi
    "$RES/config_tool" patch "$BACKUP/smartphone_integrator.json" > "$BASE/installed.json.new" \
        || fail "prepare smartphone_integrator.json"

    for name in $FILES; do
        put "$RES/$name" "$BASE/$name" "$(mode_for "$name")" || fail "copy $name"
    done
    put "$RES/$PROFILE_JAR" "$JAR" 644 || fail "copy Java overlay for $PROFILE_LABEL"
    echo "$PROFILE_ID" > "$BASE/profile.id.new" || fail "record firmware profile"
    mv -f "$BASE/profile.id.new" "$BASE/profile.id" || fail "activate firmware profile record"
    put "$BASE/installed.json.new" "$BASE/installed.json" 644 || fail "record installed configuration"
    put "$BASE/installed.json" "$CFG" 644 || fail "activate GAL launcher"
    rm -f "$BASE/installed.json.new"
    sync_all
    remount_ro /mnt/system || true
    remount_ro /mnt/app || true
    echo "INSTALLED $PROFILE_LABEL. Remove mib2q_aa_install from the SD card, then reboot the MMI."
}

uninstall_it() {
    [ -f "$BACKUP/smartphone_integrator.json" ] || fail "no installer backup found"
    if [ -f "$BASE/installed.json" ]; then
        cmp -s "$CFG" "$BASE/installed.json" || cmp -s "$CFG" "$BACKUP/smartphone_integrator.json" \
            || fail "smartphone_integrator.json changed after install; refusing to overwrite it"
    fi
    remount_rw /mnt/app || fail "remount /mnt/app"
    remount_rw /mnt/system || fail "remount /mnt/system"
    H=$BASE WLOG=/tmp/sq5_luka_wrapper.log "$BASE/sq5_luka_cleanup.sh" >/dev/null 2>&1 || true
    put "$BACKUP/smartphone_integrator.json" "$CFG" 644 || fail "restore smartphone_integrator.json"
    rm -f "$JAR"
    for name in $FILES installed.json installed.json.new profile.id profile.id.new; do rm -f "$BASE/$name"; done
    rm -rf "$BACKUP"
    rmdir "$BASE" 2>/dev/null || true
    sync_all
    remount_ro /mnt/system || true
    remount_ro /mnt/app || true
    echo "UNINSTALLED. Remove mib2q_aa_uninstall from the SD card, then reboot the MMI."
}

audit
if [ -f "$SD/mib2q_aa_install" ] && [ -f "$SD/mib2q_aa_uninstall" ]; then
    fail "both action markers are present; keep only one"
elif [ -f "$SD/mib2q_aa_install" ]; then
    install_it
elif [ -f "$SD/mib2q_aa_uninstall" ]; then
    uninstall_it
else
    echo "AUDIT ONLY: no action marker is present; nothing was modified."
fi
