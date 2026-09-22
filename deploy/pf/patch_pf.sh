#!/bin/bash
# Recipe: stock PF profiles -> patched, opening the CarPlay control range.
#
# The only change is the carplay0 inbound TCP rule: port 6030 becomes the range
# 6030:6032. This is the exact edit install_MoreIncredibleBash/mod/custom.sh
# makes in-shell on the unit; patched/ is the reference result for A/B checks
# (the installer regenerates it live, it does not copy these files).
#
#   ./patch_pf.sh            # stock/ -> patched/
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
STOCK="$HERE/stock"
OUT="$HERE/patched"
mkdir -p "$OUT"

for f in pf.conf pf.ecm0.conf pf.mlan0.conf pf.ppp0.conf; do
  python3 - "$STOCK/$f" "$OUT/$f" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
hits = 0
out = []
for line in open(src, encoding="utf-8").read().splitlines(keepends=True):
    if line.startswith("pass in quick on carplay0 proto tcp from any to any port {") and " 6030," in line:
        line = line.replace(" 6030,", " 6030:6032,")
        hits += 1
    out.append(line)
if hits != 1:
    sys.exit(f"{src}: expected exactly 1 carplay0 rule to widen, got {hits}")
open(dst, "w", encoding="utf-8").write("".join(out))
print(f"  {src.split('/')[-1]}: 6030 -> 6030:6032")
PY
done
echo "patched profiles in $OUT/"
