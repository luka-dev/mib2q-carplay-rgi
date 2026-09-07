#!/bin/sh

set -eu

BUNDLE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

command -v shasum >/dev/null 2>&1 || {
    echo "REFUSE: host shasum is required" >&2
    exit 1
}

(
    cd "${BUNDLE_DIR}"
    shasum -a 256 -c MANIFEST.sha256
)

sh -n "${BUNDLE_DIR}/payload/startncm.sh"
sh -n "${BUNDLE_DIR}/payload/stopncm.sh"
sh -n "${BUNDLE_DIR}/stock/startncm.sh"
sh -n "${BUNDLE_DIR}/stock/stopncm.sh"

[ -x "${BUNDLE_DIR}/payload/startncm.sh" ]
[ -x "${BUNDLE_DIR}/payload/stopncm.sh" ]
[ -x "${BUNDLE_DIR}/payload/smartphone_integrator" ]
[ -x "${BUNDLE_DIR}/install_over_ssh.sh" ]
[ -x "${BUNDLE_DIR}/rollback_over_ssh.sh" ]
[ -x "${BUNDLE_DIR}/verify_over_ssh.sh" ]
[ -x "${BUNDLE_DIR}/capture_state_over_ssh.sh" ]

STOCK_SIZE=$(wc -c < "${BUNDLE_DIR}/stock/smartphone_integrator" | tr -d ' ')
PATCHED_SIZE=$(wc -c < "${BUNDLE_DIR}/payload/smartphone_integrator" | tr -d ' ')
[ "${STOCK_SIZE}" = 802798 ] || {
    echo "REFUSE: unexpected stock SI size=${STOCK_SIZE}" >&2
    exit 1
}
[ "${PATCHED_SIZE}" = 802798 ] || {
    echo "REFUSE: unexpected patched SI size=${PATCHED_SIZE}" >&2
    exit 1
}

echo "bundle verification: PASS"
