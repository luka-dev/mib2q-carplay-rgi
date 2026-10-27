#!/bin/bash
# Offline checks only; the Python runner owns atomic, run-scoped reporting.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
exec python3 "$ROOT/scripts/altscreen_validation.py" "$@"
