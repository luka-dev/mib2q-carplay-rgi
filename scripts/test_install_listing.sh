#!/bin/sh
# list_payload() from the M.I.B. installer.  With manifest.txt: every listed file is
# checked on its own, a missing one fails by name, macOS junk lines are skipped.
# Without one, against a stub find: the QNX fs-dos "./dir/..: Filename too long" quirk
# (exit 1, full list) passes; any other error or an empty list fails.
# Every shell found here (QNX 6.5 /bin/sh is pdksh).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/install_MoreIncredibleBash/mod/custom.sh
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
{ echo 'set -u'; echo 'RES=$3'; sed -n '/^list_payload()/,/^}/p' "$SRC"; echo 'list_payload "$1" "$2"'; } > "$T/h.sh"
mkdir -p "$T/bin" "$T/card/a" "$T/res" "$T/nores"
: > "$T/card/a/one"; : > "$T/card/a/two"
stub() {   # stub <stderr line or empty> <print files 0|1>
    { echo '#!/bin/sh'
      [ "$2" = 1 ] && echo 'echo ./a/one; echo ./a/two'
      [ -n "$1" ] && echo "echo '$1' >&2; exit 1"
      echo 'exit 0'; } > "$T/bin/find"
    chmod +x "$T/bin/find"
}
fail() { echo "FAIL ($1): $2"; exit 1; }
shells=
for s in /bin/ksh /bin/mksh "$(command -v mksh 2>/dev/null)" /bin/dash /bin/sh; do [ -n "$s" ] && [ -x "$s" ] && shells="$shells $s"; done
for sh in $shells; do
    stub "find: ./a/..: Filename too long" 1
    PATH=$T/bin:$PATH "$sh" "$T/h.sh" "$T/card" "$T/list" "$T/nores" > /dev/null || fail "$sh" "fs-dos quirk rejected"
    [ "$(wc -l < "$T/list")" -eq 2 ] && [ ! -e "$T/list.err" ] || fail "$sh" "list incomplete or .err left"
    stub "find: ./a: Permission denied" 1
    ! PATH=$T/bin:$PATH "$sh" "$T/h.sh" "$T/card" "$T/list" "$T/nores" > /dev/null || fail "$sh" "real error accepted"
    [ ! -e "$T/list" ] || fail "$sh" "list left after failure"
    stub "" 0
    ! PATH=$T/bin:$PATH "$sh" "$T/h.sh" "$T/card" "$T/list" "$T/nores" > /dev/null || fail "$sh" "empty list accepted"
    stub "" 1
    PATH=$T/bin:$PATH "$sh" "$T/h.sh" "$T/card" "$T/list" "$T/nores" > /dev/null || fail "$sh" "clean list rejected"
    # manifest: per-file check, no find at all (stub find would fail)
    stub "find: must not run" 0
    printf './a/one\n./a/._one\n./a/two\n' > "$T/res/manifest.txt"
    PATH=$T/bin:$PATH "$sh" "$T/h.sh" "$T/card" "$T/list" "$T/res" > /dev/null || fail "$sh" "manifest rejected"
    [ "$(wc -l < "$T/list")" -eq 2 ] || fail "$sh" "manifest list wrong"
    printf './a/one\n./a/three\n' > "$T/res/manifest.txt"
    out=$(PATH=$T/bin:$PATH "$sh" "$T/h.sh" "$T/card" "$T/list" "$T/res") && fail "$sh" "missing file accepted"
    case $out in *'missing payload file ./a/three'*) ;; *) fail "$sh" "missing file not named: $out" ;; esac
done
echo "install payload listing: fs-dos '..' quirk tolerated, real errors fail, manifest per-file check:$shells PASS"
