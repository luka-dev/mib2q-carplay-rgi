#!/bin/bash
# Same upstream command/default inputs; Java 8 is supplied by Docker.
set -euo pipefail
[ "$#" -eq 0 ] || { echo 'usage: ./scripts/build_java.sh' >&2; exit 2; }
exec bash "$(dirname "$0")/java/docker.sh" build
