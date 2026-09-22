#!/bin/sh

# Read-only dead-session capture. Output stays on the Mac.

set -eu

SSH_HELPER=${AUDI_SSH_HELPER:-/Users/luka/Desktop/AUDI_2/audi_ssh.sh}
OUTPUT=${1:-./hu_usb_state.txt}

[ -x "${SSH_HELPER}" ] || {
    echo "REFUSE: SSH helper is not executable: ${SSH_HELPER}" >&2
    exit 1
}

"${SSH_HELPER}" exec '
  echo "USB nodes"
  ls -l /dev/io-usb/otg_dev /dev/io-usb/otg /dev/io-usb/io-usb
  echo "processes"
  pidin ar | grep -E "[i]o-usb-dcd|[i]o-otg|[u]sblauncher|[d]io_manager|[s]martphone_integrator"
  echo "NCM"
  ifconfig carplay0
  ifconfig -a | grep carplay
  echo "SI command"
  cat /ramdisk/pps/device/usb_ctrl1
  echo "PPS topology"
  ls /ramdisk/pps/device
  echo "quiet log"
  grep -E "NCM_FENCE|TIMEOUT_STARTUP|start_stack" /tmp/carplay_hook.log
' > "${OUTPUT}"

echo "captured read-only HU state: ${OUTPUT}"
