#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
mkdir -p "$BUILD_DIR"

cd "${SCRIPT_DIR}/c_render"

echo "=== Cluster Renderer Build (macOS) ==="
make clean
make

echo ""
echo "Built: c_render + test_harness"
ls -lh "$BUILD_DIR/c_render" "$BUILD_DIR/test_harness"
