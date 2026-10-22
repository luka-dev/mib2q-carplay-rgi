#!/bin/sh
# layout_MoreIncredibleBash/mod/custom.sh on a fake card and a fake /mnt/app file:
# the shipped all-comment template removes the installed file, a valid file is installed
# (also with Windows CRLF endings), the previous one is saved on the card, and a file with an
# unknown key, a wrong field count, a non-number or an unknown word changes nothing.
# Every POSIX/ksh shell here (QNX 6.5 /bin/sh is pdksh).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL ($1): $2"; exit 1; }
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/dash /bin/sh; do [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    rm -rf "$T/card" "$T/app"; mkdir -p "$T/card" "$T/app"
    cp -R "$ROOT/layout_MoreIncredibleBash/mod" "$T/card/"
    M=$T/card/mod
    DST=$T/app/carplay_layout.cfg
    run() { CP_LAYOUT_DST=$DST "$sh" "$M/custom.sh" > "$T/out" 2>&1; }

    # shipped template: nothing active -> no file (and an old one is removed and kept on the card)
    echo "cards left" > "$DST"
    run || fail "$sh" "template run failed: $(cat "$T/out")"
    [ ! -e "$DST" ] || fail "$sh" "template did not remove the installed file"
    grep -q '^cards left$' "$M/carplay_layout.cfg.previous" || fail "$sh" "previous file not saved"
    grep -q 'Built-in layout' "$T/out" || fail "$sh" "no built-in message"

    # a valid file with comments, blank lines and CRLF endings is installed byte for byte
    printf '# Q7\r\nfull 360 87 720 297\r\n\r\nclassic 520 87 400 297 # centred\r\ncompact classic\r\nview_source auto\r\ncards center\r\neta no\r\ncar_profile off\r\nspeed_limit yes' > "$M/carplay_layout.cfg"
    run || fail "$sh" "valid run failed: $(cat "$T/out")"
    cmp -s "$M/carplay_layout.cfg" "$DST" || fail "$sh" "installed file differs"
    grep -q 'installed 8 setting' "$T/out" || fail "$sh" "wrong setting count: $(cat "$T/out")"
    grep -q '    full 360 87 720 297$' "$T/out" || fail "$sh" "CR left in the summary"

    # rejected files leave the installed one alone
    for bad in "zoom 3" "full 1 2 3" "sport 90 90 36O 300" "cards middle" "eta maybe" "compact *" "view_source Vc" "car_profile maybe"; do
        printf 'eta yes\n%s\n' "$bad" > "$M/carplay_layout.cfg"
        if run; then fail "$sh" "accepted: $bad"; fi
        grep -q 'line 2: not understood' "$T/out" || fail "$sh" "no line report for: $bad"
        grep -q 'speed_limit yes' "$DST" || fail "$sh" "installed file changed by: $bad"
    done

    # removing the card's file also means built-in
    rm -f "$M/carplay_layout.cfg"
    run || fail "$sh" "missing-file run failed"
    [ ! -e "$DST" ] || fail "$sh" "missing card file did not remove the installed one"
done
echo "layout M.I.B.: template = built-in, install (CRLF), previous kept, bad lines refused:$shells PASS"
