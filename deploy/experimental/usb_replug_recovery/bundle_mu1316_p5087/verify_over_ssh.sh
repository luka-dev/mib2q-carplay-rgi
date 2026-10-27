#!/bin/sh

# Read-only verifier for persistent and post-boot runtime state.

set -eu

BUNDLE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SSH_HELPER=${AUDI_SSH_HELPER:-${AUDI_WORKSPACE:-$HOME/Desktop/AUDI_2}/audi_ssh.sh}
EXPECTED=${1:-}

case "${EXPECTED}" in
    stock|ncm|si|all) ;;
    *)
        echo "usage: $0 stock|ncm|si|all" >&2
        exit 2
        ;;
esac

[ -x "${SSH_HELPER}" ] || {
    echo "REFUSE: SSH helper is not executable: ${SSH_HELPER}" >&2
    exit 1
}

"${BUNDLE_DIR}/verify_bundle.sh"

if "${SSH_HELPER}" exec \
    '[ -f /tmp/usb_replug_bundle_reboot_required ]' >/dev/null 2>&1; then
    echo "FAIL: install/rollback is staged but the HU has not completed a full reboot" >&2
    exit 1
fi

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mhi2-usb-verify.XXXXXX")
trap 'rm -rf "${WORK_DIR}"' 0 1 2 3 15

expected_state()
{
    ARTIFACT=$1
    case "${EXPECTED}:${ARTIFACT}" in
        stock:*) echo stock ;;
        ncm:startncm.sh|ncm:stopncm.sh|ncm:usblauncher_otg.lua) echo payload ;;
        ncm:smartphone_integrator) echo stock ;;
        si:smartphone_integrator) echo payload ;;
        si:*) echo stock ;;
        all:*) echo payload ;;
        *) return 1 ;;
    esac
}

verify_remote()
{
    LABEL=$1
    ARTIFACT=$2
    REMOTE=$3
    CAPTURE=${WORK_DIR}/${LABEL}.${ARTIFACT}
    STATE=$(expected_state "${ARTIFACT}")
    "${SSH_HELPER}" exec "cat ${REMOTE}" > "${CAPTURE}"
    if ! cmp -s "${BUNDLE_DIR}/${STATE}/${ARTIFACT}" "${CAPTURE}"; then
        echo "FAIL: ${LABEL} ${REMOTE} is not expected ${STATE}" >&2
        return 1
    fi
    echo "PASS: ${LABEL} ${ARTIFACT}=${STATE}"
}

verify_remote persistent startncm.sh /mnt/system/etc/scripts/startncm.sh
verify_remote persistent stopncm.sh /mnt/system/etc/scripts/stopncm.sh
verify_remote persistent usblauncher_otg.lua /mnt/system/etc/usblauncher_otg.lua
verify_remote persistent smartphone_integrator /mnt/app/eso/bin/apps/smartphone_integrator

# These three checks intentionally fail before reboot if /etc still contains
# the previous RAM copy. That prevents a pre-reboot state from being accepted.
verify_remote runtime startncm.sh /etc/scripts/startncm.sh
verify_remote runtime stopncm.sh /etc/scripts/stopncm.sh
verify_remote runtime usblauncher_otg.lua /etc/usblauncher_otg.lua

echo "HU state verification: PASS expected=${EXPECTED}"
