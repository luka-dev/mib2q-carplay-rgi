#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
export RGD_CONTRACT_FRAMES=${RGD_CONTRACT_FRAMES:-${JAVA_OUTPUT:-$ROOT/build}/rgd-native-contract}
RGD_CONTRACT_STAGE=native RGD_CONTRACT_OUT="$RGD_CONTRACT_FRAMES" \
    python3 "$ROOT/tests/test_rgd_native_contract.py"
exec bash "$ROOT/scripts/java/docker.sh" test all "$@"
