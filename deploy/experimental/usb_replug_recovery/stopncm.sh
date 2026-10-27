#!/bin/sh

# Generation-aware replacement for the stock MU1316 stopncm.sh.
# With arguments, teardown is permitted only for the USB enumeration that
# currently owns the interface.  With no arguments, stock CarLife-compatible
# behavior is retained, unless NCM_EXPECT_GENERATION names the exact generation
# the ended CarPlay session ran on (smartphone_integrator's otgStackCleanupScript
# via carplay_ncm_cleanup.sh): then only that generation is destroyed and a
# newer one, from a phone replugged in the meantime, is left alone.

REQUESTED_INTERFACE=${1:-}
USB_BUSNO=${2:-}
USB_DEVNO=${3:-}
USB_IFACE=${4:-}

CARLIFE_FLAG=/tmp/carlife_active
NCM_GENERATION_DIR=${NCM_GENERATION_DIR:-/tmp}
# The lock is a directory: /tmp is /dev/shmem on the HU and mkdir there fails
# (ENOSYS).  /ramdisk is a RAM qnx4 disk, cleared at boot.  The unit has no
# rmdir binary, so the lock is removed with rm -rf.
NCM_LOCK_BASE=${NCM_LOCK_BASE:-/ramdisk}
NCM_LOG_FILE=${NCM_LOG_FILE:-/tmp/carplay_hook.log}
NCM_IFCONFIG_BIN=${NCM_IFCONFIG_BIN:-ifconfig}
NCM_STOP_DELAY=${NCM_STOP_DELAY:-1}
LOCK_HELD=0

log_ncm()
{
    echo "NCM_FENCE stop $$ $*" >> "${NCM_LOG_FILE}"
}

release_lock()
{
    if [ "${LOCK_HELD}" = 1 ]; then
        rm -f "${LOCK_DIR}/pid"
        rm -rf "${LOCK_DIR}" >/dev/null 2>&1
        LOCK_HELD=0
    fi
}

acquire_lock()
{
    LOCK_TRIES=0
    while ! mkdir "${LOCK_DIR}" >/dev/null 2>&1; do
        LOCK_OWNER=
        if [ -r "${LOCK_DIR}/pid" ]; then
            read LOCK_OWNER < "${LOCK_DIR}/pid"
        fi

        case "${LOCK_OWNER}" in
            ''|*[!0-9]*)
                ;;
            *)
                if ! kill -0 "${LOCK_OWNER}" >/dev/null 2>&1; then
                    rm -f "${LOCK_DIR}/pid"
                    rm -rf "${LOCK_DIR}" >/dev/null 2>&1
                    log_ncm "reclaim_lock owner=${LOCK_OWNER} interface=${INTERFACE}"
                    # Bounded like a wait: a lock that cannot be removed must
                    # not spin forever.
                    LOCK_TRIES=$((LOCK_TRIES + 1))
                    [ "${LOCK_TRIES}" -lt 8 ] && continue
                    log_ncm "fail lock_reclaim owner=${LOCK_OWNER} interface=${INTERFACE}"
                    return 1
                fi
                ;;
        esac

        LOCK_TRIES=$((LOCK_TRIES + 1))
        if [ "${LOCK_TRIES}" -ge 8 ]; then
            log_ncm "fail lock_timeout owner=${LOCK_OWNER} interface=${INTERFACE}"
            return 1
        fi
        log_ncm "wait_lock owner=${LOCK_OWNER} interface=${INTERFACE}"
        sleep 1
    done

    if ! echo "$$" > "${LOCK_DIR}/pid"; then
        rm -rf "${LOCK_DIR}" >/dev/null 2>&1
        log_ncm "fail lock_owner_write interface=${INTERFACE}"
        return 1
    fi
    LOCK_HELD=1
    return 0
}

read_generation()
{
    NCM_READ_GENERATION=__missing__
    if [ -r "${GENERATION_FILE}" ]; then
        read NCM_READ_GENERATION < "${GENERATION_FILE}"
    fi
}

if [ -n "${REQUESTED_INTERFACE}" ]; then
    INTERFACE=${REQUESTED_INTERFACE}
    EXPLICIT_GENERATION=1
else
    [ -f "${CARLIFE_FLAG}" ] && IS_CARLIFE=1 || IS_CARLIFE=0
    [ "${IS_CARLIFE}" = 1 ] && INTERFACE=carlife0 || INTERFACE=carplay0
    EXPLICIT_GENERATION=0
fi

case "${INTERFACE}" in
    *[!A-Za-z0-9_]*)
        log_ncm "reject invalid interface=${INTERFACE}"
        exit 2
        ;;
esac

GENERATION_FILE="${NCM_GENERATION_DIR}/ncm_generation_${INTERFACE}"
LOCK_DIR="${NCM_LOCK_BASE}/ncm_lock_${INTERFACE}"

trap 'release_lock' 0
trap 'release_lock; exit 1' 1 2 3 15

if ! acquire_lock; then
    exit 1
fi

echo "<STOPPING THE NCM DRIVER INTERFACE=\"${INTERFACE}\" BUSNO=\"${USB_BUSNO}\" DEVNO=\"${USB_DEVNO}\" IFACE=\"${USB_IFACE}\">"

if [ "${EXPLICIT_GENERATION}" = 1 ]; then
    if [ -z "${USB_BUSNO}" ] || [ -z "${USB_DEVNO}" ] || [ -z "${USB_IFACE}" ]; then
        log_ncm "reject incomplete generation interface=${INTERFACE}"
        exit 2
    fi

    REQUESTED_KEY="${USB_BUSNO}:${USB_DEVNO}:${USB_IFACE}"
    read_generation
    OBSERVED_GENERATION=${NCM_READ_GENERATION}

    case "${OBSERVED_GENERATION}" in
        "${REQUESTED_KEY}":*)
            ;;
        *)
            log_ncm "skip stale_before_sleep requested=${REQUESTED_KEY} active=${OBSERVED_GENERATION} interface=${INTERFACE}"
            exit 0
            ;;
    esac

    log_ncm "armed requested=${REQUESTED_KEY} observed=${OBSERVED_GENERATION} interface=${INTERFACE}"
elif [ -n "${NCM_EXPECT_GENERATION:-}" ]; then
    read_generation
    if [ "${NCM_READ_GENERATION}" != "${NCM_EXPECT_GENERATION}" ]; then
        log_ncm "skip session_mismatch expected=${NCM_EXPECT_GENERATION} active=${NCM_READ_GENERATION} interface=${INTERFACE}"
        exit 0
    fi
    OBSERVED_GENERATION=${NCM_READ_GENERATION}
    log_ncm "armed session=${OBSERVED_GENERATION} interface=${INTERFACE}"
else
    OBSERVED_GENERATION=legacy
    log_ncm "legacy_no_identity interface=${INTERFACE}"
fi

# Preserve Audi's one-second removal/unmount settling interval while holding
# the shared transaction lock.  A new start either finishes before this stop
# validates ownership or waits until this teardown has completed.
sleep "${NCM_STOP_DELAY}"

if [ "${OBSERVED_GENERATION}" != legacy ]; then
    read_generation
    if [ "${NCM_READ_GENERATION}" != "${OBSERVED_GENERATION}" ]; then
        log_ncm "skip replaced_after_sleep observed=${OBSERVED_GENERATION} active=${NCM_READ_GENERATION} interface=${INTERFACE}"
        exit 0
    fi
fi

"${NCM_IFCONFIG_BIN}" "${INTERFACE}" destroy
IFCONFIG_RC=$?

if [ "${OBSERVED_GENERATION}" != legacy ] && [ "${IFCONFIG_RC}" -eq 0 ]; then
    read_generation
    if [ "${NCM_READ_GENERATION}" = "${OBSERVED_GENERATION}" ]; then
        rm -f "${GENERATION_FILE}"
    fi
fi

if [ "${INTERFACE}" = carlife0 ]; then
    echo "Deleting ${CARLIFE_FLAG}"
    rm -f "${CARLIFE_FLAG}"
fi

log_ncm "destroy rc=${IFCONFIG_RC} observed=${OBSERVED_GENERATION} interface=${INTERFACE}"
exit "${IFCONFIG_RC}"
