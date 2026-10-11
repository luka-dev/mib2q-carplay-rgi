#!/bin/bash
# Offline export; no sockets, no HU access. Stock JXE reconstruction has known
# corrupt string literals: its complete wire output is evidence, not a CAN capture.
set -euo pipefail
PROJECT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_DIR=${1:-"$PROJECT_DIR/output/maneuver-audit"}
mkdir -p "$OUT_DIR"
bash "$PROJECT_DIR/scripts/java/docker.sh" test maneuvers
for name in java_mapping.csv ramp_mapping.csv selection_examples.csv; do
    source_dir=$(cd "${JAVA_OUTPUT:-$PROJECT_DIR/build}" && pwd)
    target_dir=$(cd "$OUT_DIR" && pwd)
    if [ "$source_dir" != "$target_dir" ]; then cp "$source_dir/$name" "$target_dir/$name"; fi
done
