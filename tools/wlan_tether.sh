#!/bin/sh
# Give the head unit internet through the iPhone's Personal Hotspot, on mlan0.
# Uses the stock stack only: the Marvell driver is already loaded drv_mode=3 (STA+AP)
# and `wpa_supplicant -Bi mlan0` is already running from /etc/boot/startup.sh.
# Runs alongside wired CarPlay (that link is USB carplay0, a different stack entirely).
#
#   scp tools/wlan_tether.sh root@10.173.189.1:/tmp/
#   sh /tmp/wlan_tether.sh status
#   sh /tmp/wlan_tether.sh join "iPhone" "hotspot-password"
#   sh /tmp/wlan_tether.sh off
#
# Nothing here persists across a reboot — deliberate, so a bad join can't brick access.
# ponytail: no coding changes, no HMI, no pf reload; see notes at the bottom.

set -u
IF=mlan0
CTRL=/var/run/wpa_supplicant

for p in /mnt/app/armle/usr/sbin /mnt/app/armle/sbin /usr/sbin /sbin /bin /usr/bin; do
  PATH="$PATH:$p"
done
export PATH

WPA="wpa_cli -i $IF -p $CTRL"

status() {
  echo "== $IF link"
  ifconfig $IF 2>&1 | grep -E 'flags|inet|status|ssid'
  echo "== supplicant"
  $WPA status 2>&1 | grep -E 'wpa_state|ssid|ip_address|bssid' || echo "wpa_cli failed (supplicant down?)"
  echo "== routes"
  netstat -rn 2>/dev/null | grep -E 'default|Destination'
  echo "== dhcp.client running?"
  pidin -pdhcp.client ar 2>/dev/null | grep -c "$IF"
  echo "== uap0 (the unit's own hotspot -- your SSH path)"
  ifconfig uap0 2>&1 | grep -E 'inet |status'
}

join() {
  SSID="$1"; PSK="$2"
  # The boot config sets ap_scan=2 (driver-driven selection); a normal PSK hotspot
  # needs ap_scan=1, which is what connectionmanager uses too.
  $WPA ap_scan 1 >/dev/null

  ID=$($WPA add_network | tail -1 | tr -d '\r')
  case "$ID" in ''|*[!0-9]*) echo "add_network failed: '$ID'"; exit 1;; esac
  echo "network id=$ID"

  $WPA set_network "$ID" ssid "\"$SSID\"" | tail -1
  $WPA set_network "$ID" psk "\"$PSK\"" | tail -1
  $WPA set_network "$ID" scan_ssid 1 | tail -1
  $WPA select_network "$ID" | tail -1

  echo "waiting for association..."
  i=0
  while [ $i -lt 30 ]; do
    S=$($WPA status 2>/dev/null | sed -n 's/^wpa_state=//p')
    [ "$S" = "COMPLETED" ] && break
    sleep 1
    i=$((i + 1))
  done
  echo "wpa_state=${S:-unknown}"
  [ "$S" = "COMPLETED" ] || { echo "not associated -- check SSID/password, and that the iPhone hotspot screen is open"; exit 1; }

  # Same invocation the stock /etc/wpa_action.sh uses on CONNECTED.
  if ! pidin -pdhcp.client ar 2>/dev/null | grep -q "$IF"; then
    dhcp.client -t20 -A1 -n -H -k -i $IF &
  fi

  echo "waiting for a lease..."
  i=0
  while [ $i -lt 20 ]; do
    ifconfig $IF | grep -q 'inet ' && break
    sleep 1
    i=$((i + 1))
  done

  status
  echo "== reachability"
  ping -c 3 1.1.1.1 2>&1 | tail -3
}

off() {
  for id in $($WPA list_networks 2>/dev/null | sed -n 's/^\([0-9]\+\)\t.*/\1/p'); do
    $WPA remove_network "$id" >/dev/null
  done
  $WPA disconnect >/dev/null 2>&1
  for pid in $(pidin -pdhcp.client ar 2>/dev/null | sed -n "s: /usr/sbin/dhcp.client.*$IF\$::p"); do
    slay -fs SIGTERM "$pid"
  done
  ifconfig $IF -alias 2>/dev/null
  echo "detached from hotspot"
}

case "${1:-status}" in
  status) status ;;
  join)   [ $# -eq 3 ] || { echo "usage: $0 join <SSID> <password>"; exit 2; }; join "$2" "$3" ;;
  off)    off ;;
  *)      echo "usage: $0 {status|join <SSID> <password>|off}"; exit 2 ;;
esac

# Notes / gotchas, learned from the firmware:
# * One radio, two roles. uap0 (10.173.189.1, your SSH path) and mlan0 share the
#   Marvell 88W8787. Joining a hotspot on another channel can move or drop uap0.
#   Turn ON "Maximize Compatibility" on the iPhone to force the hotspot to 2.4 GHz,
#   or keep the en0 debug-ethernet as a fallback way in.
# * Do NOT `pfctl -f /etc/pf.mlan0.conf`. It blocks inbound on uap0 except a few
#   ports and would likely cut your SSH session. Default pf.conf leaves mlan0 alone.
# * DNS: the system resolver points at local dnsmasq (setconf RESOLVE 127.0.0.1),
#   which merges /tmp/dhcp.resolv.conf written by /etc/dhcp/dhcp-up. If name
#   resolution stays broken, test with literal IPs first (ping 1.1.1.1).
# * The stock route to the same result is the HMI one: run the on-unit engineering
#   script /mnt/app/eso/hmi/engdefs/scripts/SCALE/WLAN_activate.sh (sets RCC coding
#   WLAN_Module / WLAN_ONOFF / WLAN_Client_mode), reboot, then pick the hotspot in
#   the data-connection menu; connectionmanager then does all of the above itself,
#   persistently. It refuses if a SIM is inserted or rSAP is connected
#   ("SIM inserted/SAP connected => WLAN tethering not supported!").
