#!/bin/sh
# Offline candidate. Operates only on the root selected by MHI2Q_TARGET_ROOT;
# the normal empty prefix would address the HU and is not used by offline tests.
# No process is started, stopped or rebooted by this script.
set -eu
PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/usr/bin:${PATH:-}
export PATH

ACTION=${1:-}
STAGE=${MHI2Q_STAGE:-}
TARGET_ROOT=${MHI2Q_TARGET_ROOT:-}
[ -n "$TARGET_ROOT" ] || [ "${MHI2Q_ENABLE_HU_INSTALL:-0}" = 1 ] || {
    echo "live filesystem is disabled for this review candidate" >&2; exit 2;
}
[ -n "$STAGE" ] && [ -f "$STAGE/checksums.txt" ] || {
    echo "missing extracted review bundle in MHI2Q_STAGE" >&2; exit 2;
}
case $ACTION in install|rollback|preflight) ;; *)
    echo "usage: MHI2Q_STAGE=... $0 preflight|install|rollback" >&2; exit 2 ;;
esac

destination() { printf '%s%s' "$TARGET_ROOT" "$1"; }
fail() { echo "wireless candidate: $*" >&2; return 1; }

verify_bundle() {
    while read -r expected relative; do
        [ -n "$expected" ] || continue
        case $relative in ''|/*|..|../*|*/../*|*/..)
            fail "unsafe bundle path: $relative" || return 1 ;;
        esac
        [ -f "$STAGE/$relative" ] || { fail "missing $relative"; return 1; }
        result=$(openssl dgst -sha256 "$STAGE/$relative") || return 1
        actual=${result##* }
        [ "$actual" = "$expected" ] || { fail "checksum mismatch: $relative"; return 1; }
    done < "$STAGE/checksums.txt"
}

for_replaced() {
    callback=$1
    while read -r payload path reference mode; do
        [ -n "$payload" ] || continue
        "$callback" "$payload" "$path" "$reference" "$mode" || return 1
    done <<'MAP'
bin/btstack /mnt/app/eso/bin/apps/btstack reference/btstack.stock 755
lib/libairplay.so /mnt/app/eso/lib/libairplay.so reference/libairplay.stock.so 755
pf/pf.conf /mnt/system/etc/pf.conf reference/pf.conf.wired 644
pf/pf.ecm0.conf /mnt/system/etc/pf.ecm0.conf reference/pf.ecm0.conf.wired 644
pf/pf.mlan0.conf /mnt/system/etc/pf.mlan0.conf reference/pf.mlan0.conf.wired 644
pf/pf.ppp0.conf /mnt/system/etc/pf.ppp0.conf reference/pf.ppp0.conf.wired 644
integration/carplay_startup.sh /mnt/app/root/hooks/carplay_startup.sh reference/carplay_startup.wired.sh 755
integration/rc.local.wireless.candidate.sh /mnt/app/root/rc.local reference/rc.local.before.sh 755
MAP
}

for_owned() {
    callback=$1
    while read -r payload path mode; do
        [ -n "$payload" ] || continue
        "$callback" "$payload" "$path" "$mode" || return 1
    done <<'MAP'
bin/mhi2q-wireless-daemon /mnt/app/root/hooks/mhi2q-wireless-daemon 755
bin/dio_owner_exec /mnt/app/root/hooks/dio_owner_exec 755
bin/wireless_dio_supervisor_bin /mnt/app/root/hooks/wireless_dio_supervisor_bin 755
lib/libdio_receiver_bridge.so /mnt/app/root/hooks/libdio_receiver_bridge.so 755
integration/wireless_dio_supervisor.sh /mnt/app/root/hooks/wireless_dio_supervisor.sh 755
integration/wireless_carplay.env /mnt/app/root/hooks/wireless_carplay.env 600
MAP
}

check_replaced() {
    path=$(destination "$2")
    [ -f "$path" ] && [ ! -e "$path.wireless-prev" ] ||
        { fail "missing source or existing backup: $path"; return 1; }
    cmp -s "$path" "$STAGE/$3" || { fail "unexpected current bytes: $path"; return 1; }
}
check_owned() {
    path=$(destination "$2")
    [ ! -e "$path" ] || { fail "owned path already exists: $path"; return 1; }
    [ -d "${path%/*}" ] || { fail "missing parent directory: $path"; return 1; }
    [ -f "$STAGE/$1" ] || { fail "missing payload: $1"; return 1; }
}
backup_one() {
    path=$(destination "$2")
    cp -p "$path" "$path.wireless-prev" &&
        cmp -s "$path.wireless-prev" "$STAGE/$3" &&
        cmp -s "$path" "$STAGE/$3"
}
drop_backup() {
    path=$(destination "$2")
    [ ! -e "$path.wireless-prev" ] || rm -f "$path.wireless-prev"
}
apply_replaced() {
    path=$(destination "$2")
    if [ -n "$TARGET_ROOT" ] && [ "${MHI2Q_TEST_FAIL_PATH:-}" = "$2" ]; then
        return 1
    fi
    temporary=$path.wireless-new.$$
    cp "$STAGE/$1" "$temporary" && chmod "$4" "$temporary" &&
        mv -f "$temporary" "$path" && return 0
    rm -f "$temporary"
    return 1
}
apply_owned() {
    path=$(destination "$2")
    temporary=$path.wireless-new.$$
    cp "$STAGE/$1" "$temporary" && chmod "$3" "$temporary" &&
        mv -f "$temporary" "$path" && return 0
    rm -f "$temporary"
    return 1
}
check_rollback_replaced() {
    path=$(destination "$2")
    if [ -f "$path.wireless-prev" ]; then
        cmp -s "$path.wireless-prev" "$STAGE/$3" ||
            { fail "backup differs from reviewed source: $path"; return 1; }
    else
        cmp -s "$path" "$STAGE/$3" ||
            { fail "missing backup for modified file: $path"; return 1; }
    fi
    cmp -s "$path" "$STAGE/$1" || cmp -s "$path" "$STAGE/$3" ||
        { fail "file changed since installation: $path"; return 1; }
}
restore_one() {
    path=$(destination "$2")
    [ ! -f "$path.wireless-prev" ] || mv -f "$path.wireless-prev" "$path"
}
remove_owned() {
    path=$(destination "$2")
    [ -e "$path" ] || return 0
    if cmp -s "$path" "$STAGE/$1"; then
        rm -f "$path"
    else
        echo "wireless candidate: preserved changed owned file: $path" >&2
    fi
}
rollback_internal() {
    for_replaced check_rollback_replaced || return 1
    for_replaced restore_one && for_owned remove_owned
}
check_config() {
    config=$STAGE/integration/wireless_carplay.env
    [ -r "$config" ] || { fail "per-unit wireless_carplay.env is missing"; return 1; }
    # The daemon validates identity, AP settings, MFi address, and IE index
    # without opening hardware or installing the IE.
    (
        set -a
        . "$config"
        "$STAGE/bin/mhi2q-wireless-daemon" --check-config
    ) || { fail "daemon rejected per-unit configuration"; return 1; }
}
prepare_writable() {
    [ -n "$TARGET_ROOT" ] && return 0
    mount -uw /mnt/app && mount -uw /mnt/system
}
check_lock_filesystem() {
    if [ -n "$TARGET_ROOT" ]; then
        MHI2Q_DIO_OWNER_LOCK=$(destination /mnt/persist/.mhi2q_dio_owner.lock)
        MHI2Q_DIO_WIRED_PRIORITY_LOCK=$(destination /mnt/persist/.mhi2q_dio_wired_priority.lock)
        export MHI2Q_DIO_OWNER_LOCK MHI2Q_DIO_WIRED_PRIORITY_LOCK
    fi
    "$STAGE/bin/dio_owner_exec" --check-locks ||
        { fail "persistent filesystem does not support owner locks"; return 1; }
}

verify_bundle
if [ "$ACTION" = rollback ]; then
    for_replaced check_rollback_replaced || exit 1
    prepare_writable || { fail "could not remount app/system writable"; exit 1; }
    rollback_internal || exit 1
    echo "wireless candidate: original wired and stock files restored; no processes touched"
    exit 0
fi
for_replaced check_replaced
for_owned check_owned
check_config
if [ "$ACTION" = preflight ]; then
    echo "wireless candidate: preflight passed; no files changed"
    exit 0
fi
check_lock_filesystem || exit 1
prepare_writable || { fail "could not remount app/system writable"; exit 1; }
if ! for_replaced backup_one; then
    for_replaced drop_backup || :
    fail "backup failed; no target was replaced" || exit 1
fi
if ! for_owned apply_owned || ! for_replaced apply_replaced; then
    rollback_internal || fail "automatic rollback incomplete; inspect backups"
    fail "installation failed and was rolled back" || exit 1
fi
echo "wireless candidate: files staged; no process restarted or rebooted"
