#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "${SCRIPT_DIR}/c_render"

echo "=== Cluster Renderer Build (macOS) ==="
make clean
make

echo ""
echo "Built: c_render + test_harness"
ls -lh c_render test_harness
