#!/bin/sh
# Read-only snapshot. Run over SSH and save stdout locally after the failed boot.
# No mount, writes, kills, USB stack changes or service/reboot commands.
echo '=== HU / SI / DIO processes ==='
uname -a
pidin ar
echo '=== SI arguments and mapped objects ==='
pidin -p smartphone_integrator arguments
pidin -p smartphone_integrator mem
echo '=== SI / USB files ==='
ls -l /mnt/app/eso/bin/apps/smartphone_integrator
ls -l /mnt/app/root/si-stage1-20261020
ls -l /dev/io-usb
ls -l /tmp/si-stage1.pid /tmp/si-stage1.log /tmp/si-startup.log
echo '=== CarPlay interface ==='
ifconfig carplay0
echo '=== SI startup diagnostics ==='
for log in /tmp/si-stage1.log /tmp/si-startup.log /tmp/carplay_wrapper.log; do
    if [ -f "$log" ]; then
        echo "=== $log ==="
        cat "$log"
    fi
done
echo '=== Native SI / USB / DIO slog ==='
sloginfo | grep -E 'smartphone_integrator|CSIState|CDeviceDetection|MediaConnector|CDIOManager|TIMEOUT_|usblauncher|io-usb|[sS]i-startup|si-adapter'
echo '=== DIAGNOSTICS COMPLETE ==='
