#!/bin/bash
# Write the public snapshot of this tree to <out dir>: every tracked file (working-tree
# state) except the protected (VIN-lock) build, copyrighted music and the decompiled Apple
# APAC reference.  Text marked ">>> protected" ... "<<< protected" (a line range, or inline
# in markdown) or "protected-only" (one line) is cut, CARPLAY_PROT code is removed with
# unifdef, and the generated doc blocks are rebuilt.  Fails if anything private survives.
#
#   scripts/export_public.sh /tmp/carplay-public
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:?usage: scripts/export_public.sh <out dir>}
EXCLUDE="protector scripts/build_hook_protected.sh scripts/export_public.sh
         docs/build-and-deploy/VIN_PROTECTION.md tests/fixtures/private-audio
         apac_decoder/reference/decomp"

rm -rf "$OUT"; mkdir -p "$OUT"
( cd "$ROOT" && git ls-files -z | tar --null -T - -cf - ) | tar -xf - -C "$OUT"
for p in $EXCLUDE; do rm -rf "${OUT:?}/$p"; done

python3 - "$OUT" <<'PY'
import os, re, sys
root = sys.argv[1]
inline = re.compile(r'<!-- >>> protected -->.*?<!-- <<< protected -->')
for d, _, files in os.walk(root):
    if '/.git' in d:
        continue
    for name in files:
        p = os.path.join(d, name)
        try:
            text = open(p, encoding='utf-8').read()
        except (UnicodeDecodeError, OSError):
            continue
        if 'protected' not in text:
            continue
        lines, out, skip = inline.sub('', text).split('\n'), [], False
        for line in lines:
            if '>>> protected' in line: skip = True; continue
            if '<<< protected' in line: skip = False; continue
            if skip or 'protected-only' in line: continue
            out.append(line)
        assert not skip, f'unclosed protected block in {p}'
        new = '\n'.join(out)
        if new != text:
            open(p, 'w', encoding='utf-8').write(new)
PY

grep -rl --include='*.c' --include='*.cpp' --include='*.h' CARPLAY_PROT "$OUT" | while read -r f; do
    unifdef -UCARPLAY_PROT -o "$f" "$f" || [ $? -eq 1 ]   # 1 = changed
done
python3 "$OUT/scripts/gen_docs_map.py" > /dev/null

# Nothing private may survive the cut.
LEAK='[Vv][Ii][Nn][ -]?([Pp]rotect|[Ll]ock)|REPOSITORY_COMPARISON|mhi2q-evo|protector/|build_hook_protected|VIN_PROTECTION|PROT_SECRET|CARPLAY_PROT|carplay_prot|protected_<VIN>|>>> protected|<<< protected|protected-only|/Users/luka|WAUZZZ|WA19NAF|leandr1999|100\.127\.|100\.85\.'
if grep -rIl -E "$LEAK" "$OUT"; then echo "FAILED: private text left in the files above"; exit 1; fi

# Relative links in the markdown still resolve.
python3 - "$OUT" <<'PY'
import os, re, sys
root, bad = sys.argv[1], []
for d, _, files in os.walk(root):
    for name in files:
        if not name.endswith('.md') or '/docs/sources' in d:
            continue
        p = os.path.join(d, name)
        for m in re.finditer(r'\]\(<?([^)#>\s]+)', open(p, encoding='utf-8', errors='replace').read()):
            u = m.group(1)
            if '://' in u or u.startswith('mailto:'):
                continue
            if not os.path.exists(os.path.normpath(os.path.join(d, u))):
                bad.append(f'{os.path.relpath(p, root)} -> {u}')
if bad:
    print('broken links (%d):' % len(bad)); print('\n'.join(bad[:40]))
PY
echo "exported $(cd "$OUT" && find . -type f | wc -l | tr -d ' ') files to $OUT"
