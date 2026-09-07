#!/bin/sh

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUNDLE_DIR=${PROJECT_DIR}/deploy/usb_replug_recovery/bundle_mu1316_p5087
MOCK_SSH=${PROJECT_DIR}/tests/mock_audi_ssh.sh
TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/mhi2-usb-bundle-test.XXXXXX")
HU_ROOT=${TEST_DIR}/hu
trap 'rm -rf "${TEST_DIR}"' 0 1 2 3 15

mkdir -p \
    "${HU_ROOT}/mnt/system/etc/scripts" \
    "${HU_ROOT}/mnt/app/eso/bin/apps" \
    "${HU_ROOT}/etc/scripts" \
    "${HU_ROOT}/tmp"

cp "${BUNDLE_DIR}/stock/startncm.sh" \
   "${HU_ROOT}/mnt/system/etc/scripts/startncm.sh"
cp "${BUNDLE_DIR}/stock/stopncm.sh" \
   "${HU_ROOT}/mnt/system/etc/scripts/stopncm.sh"
cp "${BUNDLE_DIR}/stock/usblauncher_otg.lua" \
   "${HU_ROOT}/mnt/system/etc/usblauncher_otg.lua"
cp "${BUNDLE_DIR}/stock/smartphone_integrator" \
   "${HU_ROOT}/mnt/app/eso/bin/apps/smartphone_integrator"

simulate_reboot()
{
    rm -f "${HU_ROOT}/tmp/usb_replug_bundle_reboot_required"
    cp "${HU_ROOT}/mnt/system/etc/scripts/startncm.sh" \
       "${HU_ROOT}/etc/scripts/startncm.sh"
    cp "${HU_ROOT}/mnt/system/etc/scripts/stopncm.sh" \
       "${HU_ROOT}/etc/scripts/stopncm.sh"
    cp "${HU_ROOT}/mnt/system/etc/usblauncher_otg.lua" \
       "${HU_ROOT}/etc/usblauncher_otg.lua"
}

run_bundle()
{
    MOCK_HU_ROOT=${HU_ROOT} \
    AUDI_SSH_HELPER=${MOCK_SSH} \
    "$@"
}

simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" stock

run_bundle "${BUNDLE_DIR}/install_over_ssh.sh" ncm
if run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" ncm >/dev/null 2>&1; then
    echo "pre-reboot runtime unexpectedly verified as new NCM" >&2
    exit 1
fi
simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" ncm
if run_bundle "${BUNDLE_DIR}/install_over_ssh.sh" si >/dev/null 2>&1; then
    echo "SI-only installer accepted an active NCM payload" >&2
    exit 1
fi
run_bundle "${BUNDLE_DIR}/rollback_over_ssh.sh" ncm
simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" stock

run_bundle "${BUNDLE_DIR}/install_over_ssh.sh" si
simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" si
if run_bundle "${BUNDLE_DIR}/install_over_ssh.sh" ncm >/dev/null 2>&1; then
    echo "NCM-only installer accepted an active SI payload" >&2
    exit 1
fi
run_bundle "${BUNDLE_DIR}/rollback_over_ssh.sh" si
simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" stock

run_bundle "${BUNDLE_DIR}/install_over_ssh.sh" all
simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" all
run_bundle "${BUNDLE_DIR}/rollback_over_ssh.sh" all
simulate_reboot
run_bundle "${BUNDLE_DIR}/verify_over_ssh.sh" stock

# An unknown target must be refused rather than overwritten.
echo unknown >> "${HU_ROOT}/mnt/system/etc/scripts/startncm.sh"
if run_bundle "${BUNDLE_DIR}/install_over_ssh.sh" ncm >/dev/null 2>&1; then
    echo "installer accepted an unknown target" >&2
    exit 1
fi

echo "USB replug bundle install/rollback mock: PASS"
