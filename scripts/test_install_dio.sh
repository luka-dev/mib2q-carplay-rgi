#!/bin/sh
# patch_dio() from the M.I.B. installer on a stock-shaped dio_manager.json:
# registers the five route-guidance IDs, keeps "##" comment lines, backs up the
# stock file once, is a no-op on re-run, and leaves an unexpected layout untouched.
# Runs under every POSIX/ksh shell available here (QNX 6.5 /bin/sh is pdksh).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/install_MoreIncredibleBash/mod/custom.sh
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT

{ echo 'set -u'
  sed -n '/^backup_once()/p' "$SRC"
  sed -n '/^add_ids()/,/^}/p;/^patch_dio()/,/^}/p' "$SRC"
  echo 'DIO=$1; patch_dio'
} > "$T/h.sh"

cat > "$T/stock" <<'EOF'
        ## 0x5000/* StartNowPlayingUpdates */
        "MessagesSentByAccessory":["0x5000", "0x5002", "0x4C05"],
        ## 0x5001/* NowPlayingUpdate */
        "MessagesReceivedFromDevice":["0x4E09", "0x5001", "0x4C04"],
EOF
cat > "$T/want" <<'EOF'
        ## 0x5000/* StartNowPlayingUpdates */
        "MessagesSentByAccessory":["0x5000", "0x5002", "0x4C05", "0x5200", "0x5203"],
        ## 0x5001/* NowPlayingUpdate */
        "MessagesReceivedFromDevice":["0x4E09", "0x5001", "0x4C04", "0x5201", "0x5202", "0x5204"],
EOF

fail() { echo "FAIL ($1): $2"; exit 1; }
shells=
for s in /bin/ksh /bin/mksh /bin/dash /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    cp "$T/stock" "$T/d"; rm -f "$T/d.carplay-stock"
    "$sh" "$T/h.sh" "$T/d" > /dev/null || fail "$sh" "patch returned non-zero"
    cmp -s "$T/d" "$T/want" || fail "$sh" "wrong result: $(diff "$T/want" "$T/d" | head -3)"
    cmp -s "$T/d.carplay-stock" "$T/stock" || fail "$sh" "backup is not the stock file"
    out=$("$sh" "$T/h.sh" "$T/d") || fail "$sh" "re-run returned non-zero"
    case $out in *'already registered'*) ;; *) fail "$sh" "re-run was not a no-op: $out" ;; esac
    cmp -s "$T/d" "$T/want" || fail "$sh" "re-run changed the file"

    { cat "$T/stock"; grep MessagesSentByAccessory "$T/stock"; } > "$T/bad"; cp "$T/bad" "$T/bad0"
    "$sh" "$T/h.sh" "$T/bad" > /dev/null && fail "$sh" "duplicate list accepted"
    cmp -s "$T/bad" "$T/bad0" || fail "$sh" "unexpected layout was modified"
    [ ! -e "$T/bad.carplay-stock" ] || fail "$sh" "backup made for an untouched file"
done
ls "$T" | grep -q carplay-new && fail all "temporary file left behind"
echo "dio_manager.json route-guidance IDs:$shells PASS"
