#!/usr/bin/env python3
"""Check local Markdown links/anchors in docs and the root README; no network needed.

External URLs are deliberately not validated. Fenced and inline code are examples,
not links. Run from any directory. Also used on the exported public snapshot.
"""
from pathlib import Path
import argparse
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
LINK = re.compile(r'!?\[[^\]\n]*\]\((<[^>\n]+>|(?:[^()\s]|\([^()\n]*\))+)(?:\s+["\'][^\n]*?["\'])?\)')

def prose(text):
    lines, fence = [], None
    for line in text.splitlines(keepends=True):
        m = re.match(r'^\s{0,3}(`{3,}|~{3,})', line)
        if fence:
            if m and m[1][0] == fence[0] and len(m[1]) >= len(fence):
                fence = None
            lines.append('\n')
        elif m:
            fence = m[1]
            lines.append('\n')
        else:
            # Retain labels/targets such as [`name`](file), but hide code examples.
            lines.append(re.sub(r'(`+)(.*?)\1', lambda x: ' ' * len(x[0]), line))
    return ''.join(lines)

def links(text):
    clean = prose(text)
    for m in LINK.finditer(clean):
        yield clean.count('\n', 0, m.start()) + 1, m[1].strip('<>')
    # Reference definitions and HTML image/link attributes are also local links.
    for pattern in (r'^\s{0,3}\[(?!\^)[^\]\n]+\]:\s*<?([^\s>]+)', r'<(?:a|img)\b[^>]*?\b(?:href|src)=["\']([^"\']+)["\']'):
        for m in re.finditer(pattern, clean, re.M | re.I):
            yield clean.count('\n', 0, m.start()) + 1, m[1]

def anchors(text):
    # Inline code contributes text to a heading's rendered slug.
    unfenced = []
    fence = None
    for line in text.splitlines():
        m = re.match(r'^\s{0,3}(`{3,}|~{3,})', line)
        if m:
            if fence is None: fence = m[1]
            elif m[1][0] == fence[0] and len(m[1]) >= len(fence): fence = None
            continue
        if fence is None: unfenced.append(line)
    clean = '\n'.join(unfenced)
    result = set(re.findall(r'\b(?:id|name)=["\']([^"\']+)["\']', clean))
    used = set()
    for m in re.finditer(r'^ {0,3}#{1,6}\s+(.+?)(?:\s+#+)?\s*$', clean, re.M):
        title = re.sub(r'!?\[([^\]]+)\]\([^)]*\)', r'\1', m[1])
        title = re.sub(r'<[^>]+>', '', title).replace('`', '')
        slug = re.sub(r'[^\w\- ]', '', title.lower()).replace(' ', '-')
        candidate, i = slug, 0
        while candidate in used:
            i += 1
            candidate = f'{slug}-{i}'
        used.add(candidate)
        result.add(candidate)
    return result

def check(paths):
    errors, cache = [], {}
    for path in paths:
        for line, url in links(path.read_text(encoding='utf-8')):
            split = urlsplit(url)
            if split.scheme or split.netloc: continue
            target = (path.parent / unquote(split.path)).resolve() if split.path else path.resolve()
            label = f'{path.relative_to(ROOT)}:{line}: {url}'
            if not target.exists():
                errors.append(label + ' (missing target)')
            elif split.fragment and target.suffix.lower() == '.md':
                if target not in cache: cache[target] = anchors(target.read_text(encoding='utf-8'))
                if unquote(split.fragment) not in cache[target]:
                    errors.append(label + ' (missing anchor)')
    return errors

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', nargs='*', help='optional Markdown files or folders, relative to repo')
    args = parser.parse_args()
    paths = []
    for item in args.paths or ['README.md', 'docs']:
        p = ROOT / item
        paths.extend(sorted(p.rglob('*.md')) if p.is_dir() else [p])
    errors = check(paths)
    for error in errors: print(error, file=sys.stderr)
    print(f'Checked {len(paths)} Markdown files; {len(errors)} broken local links/anchors.')
    return bool(errors)

if __name__ == '__main__':
    sys.exit(main())
