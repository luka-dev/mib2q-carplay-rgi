#!/bin/bash
# The original transport group includes lifecycle and parking regressions.
set -euo pipefail
exec bash "$(dirname "$0")/java/docker.sh" test transports "$@"
