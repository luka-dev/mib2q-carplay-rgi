#!/bin/sh

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
START_NCM="${ROOT}/deploy/usb_replug_recovery/startncm.sh"
STOP_NCM="${ROOT}/deploy/usb_replug_recovery/stopncm.sh"
IFCONFIG_STUB="${ROOT}/tests/ncm_ifconfig_stub.sh"

TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/ncm-fence-test.XXXXXX")
trap 'rm -rf "${TEST_DIR}"' EXIT HUP INT TERM

GEN_DIR="${TEST_DIR}/generation"
LOG_FILE="${TEST_DIR}/carplay_hook.log"
COMMAND_LOG="${TEST_DIR}/ifconfig.log"
STACK_PATH="${TEST_DIR}/otg_dev"
mkdir -p "${GEN_DIR}"
touch "${STACK_PATH}" "${LOG_FILE}" "${COMMAND_LOG}"

run_start()
{
    NCM_GENERATION_DIR="${GEN_DIR}" \
    NCM_LOG_FILE="${LOG_FILE}" \
    NCM_MOUNT_BIN=true \
    NCM_IF_UP_BIN=true \
    NCM_IFCONFIG_BIN="${IFCONFIG_STUB}" \
    NCM_TEST_CALLER=start \
    NCM_TEST_COMMAND_LOG="${COMMAND_LOG}" \
    "${START_NCM}" "${STACK_PATH}" carplay io-pkt "$1" "$2" "$3"
}

run_stop()
{
    NCM_GENERATION_DIR="${GEN_DIR}" \
    NCM_LOG_FILE="${LOG_FILE}" \
    NCM_IFCONFIG_BIN="${IFCONFIG_STUB}" \
    NCM_TEST_CALLER=stop \
    NCM_TEST_COMMAND_LOG="${COMMAND_LOG}" \
    NCM_STOP_DELAY="${4:-0}" \
    "${STOP_NCM}" carplay0 "$1" "$2" "$3"
}

assert_destroy_count()
{
    EXPECTED=$1
    ACTUAL=$(grep -c '^stop carplay0 destroy$' "${COMMAND_LOG}" || true)
    if [ "${ACTUAL}" -ne "${EXPECTED}" ]; then
        echo "expected ${EXPECTED} destroys, saw ${ACTUAL}" >&2
        exit 1
    fi
}

# A normal removal owns the current generation and destroys it.
run_start 1 2 3
run_stop 1 2 3 0
assert_destroy_count 1
if [ -r "${GEN_DIR}/ncm_generation_carplay0" ]; then
    echo "normal removal left generation token behind" >&2
    exit 1
fi

# A removal for an older USB address is stale even if it starts after the new
# generation is already active.
run_start 1 4 3
run_stop 1 2 3 0
assert_destroy_count 1

# The important stock race: stop snapshots generation A, sleeps, and generation
# B starts before the destructive action.  The delayed stop must stand down.
run_start 1 5 3
run_stop 1 5 3 1 &
STOP_PID=$!
sleep 0.2
run_start 1 6 3
wait "${STOP_PID}"
assert_destroy_count 2

ACTIVE_GENERATION=$(sed -n '1p' "${GEN_DIR}/ncm_generation_carplay0")
case "${ACTIVE_GENERATION}" in
    1:6:3:*) ;;
    *)
        echo "unexpected active generation: ${ACTIVE_GENERATION}" >&2
        exit 1
        ;;
esac

# A duplicate event for the same enumeration must reuse the ready interface,
# not mount a second global-name instance or replace the ownership token.
run_start 1 6 3
DUPLICATE_GENERATION=$(sed -n '1p' "${GEN_DIR}/ncm_generation_carplay0")
if [ "${DUPLICATE_GENERATION}" != "${ACTIVE_GENERATION}" ]; then
    echo "duplicate start replaced generation token" >&2
    exit 1
fi

# A lock whose recorded process is gone must not wedge all future sessions.
mkdir "${GEN_DIR}/ncm_lock_carplay0"
echo 2147483647 > "${GEN_DIR}/ncm_lock_carplay0/pid"
run_start 1 8 3

# A failed mount must remove only its own token.
if NCM_GENERATION_DIR="${GEN_DIR}" \
    NCM_LOG_FILE="${LOG_FILE}" \
    NCM_MOUNT_BIN=false \
    NCM_IF_UP_BIN=true \
    NCM_IFCONFIG_BIN="${IFCONFIG_STUB}" \
    NCM_TEST_CALLER=start \
    NCM_TEST_COMMAND_LOG="${COMMAND_LOG}" \
    "${START_NCM}" "${STACK_PATH}" carplay io-pkt 1 7 3; then
    echo "mount failure unexpectedly succeeded" >&2
    exit 1
fi
if [ -r "${GEN_DIR}/ncm_generation_carplay0" ]; then
    echo "mount failure left generation token behind" >&2
    exit 1
fi

grep -q 'skip stale_before_sleep' "${LOG_FILE}"
grep -q 'wait_lock' "${LOG_FILE}"
grep -q 'skip duplicate_ready' "${LOG_FILE}"
grep -q 'reclaim_lock owner=2147483647' "${LOG_FILE}"
grep -q 'fail mount_rc=1' "${LOG_FILE}"

echo "USB replug recovery shell tests: PASS"
