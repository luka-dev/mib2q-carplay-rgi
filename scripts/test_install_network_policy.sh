#!/bin/bash
# Exercise the actual installer against stale SDIO/PF payloads using the E2E firmware fixture.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
export MOD=${MOD:-$ROOT/build/install_unprotected/install_MoreIncredibleBash/mod}
CHECK=$(mktemp); trap 'rm -f "$CHECK"' EXIT
python3 - "$ROOT" "$CHECK" <<'PY'
from pathlib import Path
import shlex,sys
root=Path(sys.argv[1]);s=(root/'scripts/test_install_e2e.sh').read_text()
s=s[:s.index('unit; run "cat $T/us"')]
s=s.replace('ROOT=$(cd "$(dirname "$0")/.." && pwd)', 'ROOT='+shlex.quote(str(root)))
s+='''
unit
mkdir -p "$U/mnt/app/armle/sbin"
printf 'original stock SDIO\\n' > "$U/mnt/app/armle/sbin/io-sdiorm-mib2-qc"
cp "$U/mnt/app/armle/sbin/io-sdiorm-mib2-qc" "$T/expected-sdio"
cp "$U/mnt/system/etc/pf.conf" "$T/original-pf"
PAYLOAD=$T/mod/carplay/root
mkdir -p "$PAYLOAD/mnt/app/armle/sbin" "$PAYLOAD/mnt/system/etc" "$PAYLOAD/mnt/app/root/hooks"
printf 'retired SDIO replacement\\n' > "$PAYLOAD/mnt/app/armle/sbin/io-sdiorm-mib2-qc"
printf 'pass all # retired QoS replacement\\n' > "$PAYLOAD/mnt/system/etc/pf.conf"
printf '1\\n' > "$PAYLOAD/mnt/app/root/hooks/carplay_pf_dynamic.enabled"
printf '#!/bin/sh\\nexit 0\\n' > "$PAYLOAD/mnt/app/root/hooks/carplay_pf_watch"
printf '%s\\n' './mnt/app/armle/sbin/io-sdiorm-mib2-qc' './mnt/system/etc/pf.conf' './mnt/app/root/hooks/carplay_pf_dynamic.enabled' './mnt/app/root/hooks/carplay_pf_watch' >> "$T/mod/carplay/manifest.txt"
run "cat $T/us" || fail network-policy "old-card install failed"
cmp -s "$U/mnt/app/armle/sbin/io-sdiorm-mib2-qc" "$T/expected-sdio" || fail network-policy "stock SDIO overwritten"
[ ! -e "$U/mnt/app/root/hooks/carplay_pf_dynamic.enabled" ] && [ ! -e "$U/mnt/app/root/hooks/carplay_pf_watch" ] || fail network-policy "retired QoS installed"
python3 - "$T/original-pf" "$U/mnt/system/etc/pf.conf" <<'PFEXPECTED'
from pathlib import Path
import sys
old,new=(Path(p).read_bytes() for p in sys.argv[1:])
# Only the independently specified required-port difference is allowed.
a=old.splitlines();b=new.splitlines();assert len(a)==len(b)
changes=[(x,y) for x,y in zip(a,b) if x!=y]
assert len(changes)==1,changes
x,y=changes[0];assert b'pass in quick on carplay0 proto tcp' in x
assert b' 6030,' in x and y==x.replace(b' 6030,',b' 6030:6032,',1)
PFEXPECTED
for profile in pf.conf pf.ecm0.conf pf.mlan0.conf pf.ppp0.conf; do
 grep -q ' 6030:6032,' "$U/mnt/system/etc/$profile" || fail network-policy "required ports missing"
done
echo 'Installer network policy: stock SDIO preserved, whole PF replacement rejected, stale QoS excluded, required ports kept PASS'
'''
Path(sys.argv[2]).write_text(s)
PY
bash "$CHECK"
