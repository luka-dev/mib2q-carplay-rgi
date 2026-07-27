#!/bin/sh
# build_replay.sh -- compile libcarplay_replay.so on the QNX VM, pull it back.
# Usage: sh build_replay.sh   (from c_hook_trace/)
# TRAP: keep the remote $SSH block apostrophe-free (a ' in a comment inside
#       $SSH '...' silently breaks the remote sh).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
VM=192.168.64.16
SSH="sshpass -p root ssh -oHostKeyAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oPreferredAuthentications=password -oUserKnownHostsFile=/dev/null -oLogLevel=ERROR -oNumberOfPasswordPrompts=1 root@$VM"
SCP="sshpass -p root scp -oHostKeyAlgorithms=+ssh-rsa -oStrictHostKeyChecking=no -oPreferredAuthentications=password -oUserKnownHostsFile=/dev/null -oLogLevel=ERROR root@$VM"

echo ">> upload"
$SSH 'rm -rf /tmp/cpreplay && mkdir -p /tmp/cpreplay'
tar czf - -C "$HERE" replay.c | $SSH 'cd /tmp/cpreplay && tar xzf -'

echo ">> compile"
$SSH 'export QNX_HOST=/usr/qnx650/host/qnx6/x86 QNX_TARGET=/usr/qnx650/target/qnx6
GCC=$QNX_HOST/usr/bin/ntoarmv7-gcc
cd /tmp/cpreplay
set -e
$GCC -shared -fPIC -O2 -std=gnu99 -Wall -o libcarplay_replay.so replay.c
echo -- DT_NEEDED --
$QNX_HOST/usr/bin/ntoarmv7-objdump -p libcarplay_replay.so | grep NEEDED
ls -l libcarplay_replay.so'

echo ">> pull -> $HERE"
$SCP:/tmp/cpreplay/libcarplay_replay.so "$HERE/libcarplay_replay.so"
echo ">> done: $HERE/libcarplay_replay.so"
