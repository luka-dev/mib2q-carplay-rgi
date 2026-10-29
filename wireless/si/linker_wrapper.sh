#!/bin/bash
{ echo "=== LINKER CALL ==="; for a in "$@"; do echo "  ARG: $a"; done; } >> /tmp/linker.log
cmd="$1"; shift
"$cmd" "$@" 2>> /tmp/linker.log
rc=$?
echo "=== EXIT $rc ===" >> /tmp/linker.log
exit $rc
