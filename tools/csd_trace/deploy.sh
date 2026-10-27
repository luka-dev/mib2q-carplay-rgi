#!/bin/bash
# Install the pass-through CSD tracer on the unit.  Keeps the stock library beside it as
# libcsdIpcClient_real.so, which the tracer dlopens.  io-audio maps the library once at
# start-up, so this takes effect at the next reboot -- and so does reverting it.
#
#   ./deploy.sh <host>          install
#   ./deploy.sh <host> revert   put the stock library back
set -e
DEV="root@${1:?usage: deploy.sh <host> [revert]}"; PASS="harman_f"
MODE="${2:-install}"
P="$(cd "$(dirname "$0")/../.." && pwd)"
CM="-oControlMaster=auto -oControlPath=/tmp/audi-csd-sock -oControlPersist=900"
OPTS="-oHostKeyAlgorithms=+ssh-rsa -oPubkeyAcceptedAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oUserKnownHostsFile=/dev/null -oConnectTimeout=60"
S="ssh $OPTS $CM"
L=/mnt/app/armle/lib/libcsdIpcClient.so

sshpass -p "$PASS" $S "$DEV" "mount -uw /mnt/app"
if [ "$MODE" = revert ]; then
    $S "$DEV" "[ -f ${L}_real ] && mv ${L}_real $L && sync && echo reverted || echo 'no stock copy found'"
    $S "$DEV" "ls -la $L"
    echo "reboot for it to take effect"
    exit 0
fi
# keep the stock library under the name the tracer dlopens, once
$S "$DEV" "[ -f ${L}_real ] || cp $L ${L}_real; ls -la ${L}_real"
sshpass -p "$PASS" scp $OPTS $CM "$P/build/libcsdIpcClient_trace.so" "$DEV:$L.new"
$S "$DEV" "chmod 755 $L.new && mv $L.new $L && sync"
$S "$DEV" "cat $L" > /tmp/csd_rb.so 2>/dev/null
cmp -s /tmp/csd_rb.so "$P/build/libcsdIpcClient_trace.so" \
  && echo "OK  tracer verified byte for byte" || { echo "FAIL"; exit 1; }
echo "reboot, then reproduce; the trace lands in /tmp/csd_trace.log"
