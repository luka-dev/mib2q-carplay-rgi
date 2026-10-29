"""Freeze current project plus reviewed sibling branches into an isolated candidate.

Never changes the live checkout/index or deploys to a head unit. Historical
branch deltas are applied once to a source snapshot, not stacked binary patches.
This freezes the local checkout, not the published production branch. Published
fixes must be audited separately before the result is a release candidate.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve_wifi_overlay(project, out):
    """Merge the known independent MCD/Wi-Fi additions at shared anchors."""
    rejected = {str(p.relative_to(project)) for p in project.rglob('*.rej')}
    if rejected != {'install_MoreIncredibleBash/mod/custom.sh.rej', 'scripts/test_binpatch.sh.rej'}:
        raise ValueError(f'Unexpected overlay conflicts: {rejected}')
    path = project / 'install_MoreIncredibleBash/mod/custom.sh'
    text = path.read_text()
    anchor = '    patch_si || exit 1\n'
    restore = '    [ -e "$SI_BIN.carplay-stock" ] && mv -f "$SI_BIN.carplay-stock" "$SI_BIN"\n'
    if text.count(anchor) != 1 or text.count(restore) != 1 or 'patch_wifi() {' not in text:
        raise ValueError('Installer merge anchors drifted')
    text = text.replace(anchor, anchor + '    patch_wifi || exit 1\n')
    text = text.replace(restore, restore + '    [ -e "$WIFI_SO.carplay-stock" ] && mv -f "$WIFI_SO.carplay-stock" "$WIFI_SO"\n')
    path.write_text(text)
    path = project / 'scripts/test_binpatch.sh'
    text = path.read_text()
    anchor = 'P=$ROOT/deploy/binpatch/binpatch.py\n'
    if text.count(anchor) != 1:
        raise ValueError('Test merge anchor drifted')
    text = text.replace(anchor, 'WIFI=qnx6/MMX2QC/app/10/default/app/armle/lib/dll/devnp-mrvl_wlan-sdiorm.so\n' + anchor)
    anchor = '        "$py" "$P" mcd "$U/$fw/$MCD" "$T/mout" > /dev/null\n'
    checks = '''        "$py" "$P" wifi "$U/$fw/$WIFI" "$T/wout" > /dev/null
        [ "$(cmp -l "$U/$fw/$WIFI" "$T/wout" | wc -l | tr -d ' ')" = 40 ] || exit 1
        rc=0; "$py" "$P" wifi "$T/wout" "$T/wagain" > /dev/null || rc=$?
        [ "$rc" = 3 ] && [ ! -e "$T/wagain" ] || exit 1
'''
    if text.count(anchor) != 1:
        raise ValueError('MCD test anchor drifted')
    text = text.replace(anchor, checks + anchor)
    path.write_text(text)
    for suffix in ('*.rej', '*.orig'):
        for path in project.rglob(suffix):
            target = out / 'merge-records' / path.relative_to(project)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(path, target)


def snapshot(out):
    out = out.resolve()
    if out.exists():
        raise ValueError('Choose a fresh output; snapshots are immutable inputs')
    project = out / 'project'
    project.mkdir(parents=True)
    names = sorted(set(git('ls-files', '-z', '--cached', '--others', '--exclude-standard').decode().split('\0')) - {''})
    sources = {}
    for name in names:
        source, dest = ROOT / name, project / name
        if not source.is_file():
            continue
        if source.is_symlink():
            raise ValueError(f'Source symlink requires explicit inventory: {name}')
        before = sha(source)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)
        if before != sha(source) or before != sha(dest):
            raise ValueError(f'Source changed while freezing: {name}')
        sources[name] = before
    branches = []
    for branch in ('feat/mcd-coalesce', 'feat/wifi-driver-patch'):
        head = git('rev-parse', branch).decode().strip()
        base = git('merge-base', 'HEAD', branch).decode().strip()
        delta = git('diff', '--binary', base, head)
        patch_path = out / (branch.split('/')[-1] + '.patch')
        patch_path.write_bytes(delta)
        # patch(1) applies text hunks; binary additions are copied directly from
        # the branch object after text application, never from another worktree.
        text = git('diff', base, head, '--', '*.sh', '*.py', '*.md', '*.conf', '*.custom')
        result = subprocess.run(['patch', '-p1', '--batch', '--forward'], cwd=project,
                                input=text, capture_output=True)
        (out / (branch.split('/')[-1] + '.log')).write_bytes(result.stdout + result.stderr)
        if result.returncode:
            if branch == 'feat/wifi-driver-patch':
                resolve_wifi_overlay(project, out)
            else:
                raise ValueError(f'Branch conflicts in snapshot: {branch}; inspect preserved .rej and log')
        changes = git('diff', '--name-only', '-z', base, head).decode().split('\0')
        for name in filter(None, changes):
            if name not in sources and (not (project / name).exists() or (project / name).suffix not in ('.sh', '.py', '.md', '.conf', '.custom')):
                data = git('show', head + ':' + name)
                dest = project / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(data)
        branches.append({'branch': branch, 'head': head, 'base': base,
                         'patch_sha256': sha(patch_path), 'changed_paths': list(filter(None, changes))})
    manifest = {'status': 'source-consolidated-builds-pending',
                'head': git('rev-parse', 'HEAD').decode().strip(),
                'branch': git('branch', '--show-current').decode().strip(),
                'input_sources': sources, 'branch_overlays': branches,
                'frozen_sources': {str(p.relative_to(project)): sha(p) for p in project.rglob('*') if p.is_file()},
                'limits': ['No live checkout/index changes, commit, HU writes or activation.',
                           'Local source snapshot is not the published production baseline; audit upstream/main before packaging.',
                           'Experimental local source is captured but is not automatically enabled.',
                           'SDIO production boot image and PF identities require separate activation.']}
    published = subprocess.run(['git', '-C', str(ROOT), 'rev-parse', '--verify',
                                'upstream/main'], capture_output=True, text=True)
    if published.returncode == 0:
        manifest['published_reference'] = {
            'ref': 'upstream/main', 'commit': published.stdout.strip(),
            'status': 'Reference only: published fixes are not automatically merged.',
            'branch_patch_equivalence': {
                b['branch']: git('cherry', 'upstream/main', b['head'], b['base']).decode().splitlines()
                for b in branches
            },
        }
    (out / 'source_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'project': str(project), 'source_files': len(sources),
                      'branches': [x['head'] for x in branches]}, indent=2))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, default=ROOT / 'build/unified-all-20261023')
    snapshot(p.parse_args().out)


if __name__ == '__main__':
    main()
