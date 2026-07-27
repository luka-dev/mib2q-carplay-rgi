#!/bin/sh
# build.sh -- compile libcarplay_trace.so on the QNX VM, pull it back here.
# Usage: sh build.sh   (from c_hook_trace/)
# TRAP: keep the remote $SSH block apostrophe-free (a ' in a comment inside
#       $SSH '...' silently breaks the remote sh).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VM=192.168.64.16
SSH="sshpass -p root ssh -oHostKeyAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oPreferredAuthentications=password -oUserKnownHostsFile=/dev/null -oLogLevel=ERROR -oNumberOfPasswordPrompts=1 root@$VM"
SCP="sshpass -p root scp -oHostKeyAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oPreferredAuthentications=password -oUserKnownHostsFile=/dev/null -oLogLevel=ERROR root@$VM"

echo ">> upload"
$SSH 'rm -rf /tmp/cptrace && mkdir -p /tmp/cptrace'
tar czf - -C "$HERE" carplay_trace.c | $SSH 'cd /tmp/cptrace && tar xzf -'

echo ">> compile"
$SSH 'export QNX_HOST=/usr/qnx650/host/qnx6/x86 QNX_TARGET=/usr/qnx650/target/qnx6
GCC=$QNX_HOST/usr/bin/ntoarmv7-gcc
cd /tmp/cptrace
set -e
$GCC -shared -fPIC -O2 -std=gnu99 -Wall -o libcarplay_trace.so carplay_trace.c
echo -- DT_NEEDED --
$QNX_HOST/usr/bin/ntoarmv7-objdump -p libcarplay_trace.so | grep NEEDED
ls -l libcarplay_trace.so'

echo ">> pull -> $HERE"
$SCP:/tmp/cptrace/libcarplay_trace.so "$HERE/libcarplay_trace.so"
echo ">> done: $HERE/libcarplay_trace.so"
