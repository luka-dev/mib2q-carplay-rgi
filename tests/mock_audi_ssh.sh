#!/bin/sh

set -eu

[ "${1:-}" = exec ] || {
    echo "mock_audi_ssh supports exec only" >&2
    exit 2
}
shift

[ -n "${MOCK_HU_ROOT:-}" ] || {
    echo "MOCK_HU_ROOT is required" >&2
    exit 2
}

COMMAND=$*
TRANSLATED=$(printf '%s\n' "${COMMAND}" | sed \
    -e 's#mount -uw /mnt/system#:#g' \
    -e 's#mount -uw /mnt/app#:#g' \
    -e 's#sync#:#g' \
    -e "s# /tmp/# ${MOCK_HU_ROOT}/tmp/#g" \
    -e "s# /etc/# ${MOCK_HU_ROOT}/etc/#g" \
    -e "s#/mnt/system#${MOCK_HU_ROOT}/mnt/system#g" \
    -e "s#/mnt/app#${MOCK_HU_ROOT}/mnt/app#g")

exec /bin/sh -c "${TRANSLATED}"
