#!/usr/bin/env python3
"""Prepare and verify Wi-Fi-only resources; SDIO and PF QoS are not selected."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def wifi_helper():
    spec = importlib.util.spec_from_file_location('reviewed_wifi_runtime', ROOT / 'deploy/binpatch/wifi_runtime.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def regular_files(directory: Path):
    result = {}
    for path in directory.rglob('*'):
        if path.is_symlink():
            raise ValueError('symlink is not a staged resource: ' + str(path))
        if path.is_file():
            result[str(path.relative_to(directory))] = sha(path)
    return result

def selected_files():
    return {'wifi/wifi_runtime.py', *('wifi/runtime/' + name for name in
            ('driver.so', 'stock.so', 'manifest.json', 'build-receipt.json'))}

def status():
    helper = wifi_helper()
    helper.resources(str(ROOT / 'deploy/wifi/runtime'))
    return {
        'status': 'wifi-only-resources-verified',
        'wifi': {'sha256': helper.TARGET, 'stock_sha256': helper.STOCK,
                 'supported_train': 'MHI2Q_US_AUG22_P5087',
                 'io_pkt_sha256_allowlist': list(helper.STACKS),
                 'installation': 'M.I.B. verified disk replacement and stock rollback'},
        'sdio': {'selected': False, 'mode': 'stock; no replacement'},
        'pf': {'qos_selected': False, 'active_session_updater': False,
               'required_carplay_ports': '6030:6032 retained by M.I.B. installer'},
        'io_pkt': 'stock binary; no replacement',
        'activation': 'No HU write, PF load, service restart or reboot.'}

def verify_stage(out: Path):
    out = out.resolve()
    manifest = json.loads((out / 'network-manifest.json').read_text())
    if (manifest.get('schema_version') != 2 or
            manifest.get('status') != 'wifi-only-staged-no-activation' or
            manifest.get('components') != ['wifi']):
        raise ValueError('unsupported stage: SDIO/PF stages are no longer selected')
    actual = regular_files(out)
    actual.pop('network-manifest.json', None)
    if set(actual) != selected_files():
        raise ValueError('stage must contain only selected Wi-Fi files')
    if actual != manifest.get('files'):
        raise ValueError('network stage files changed, missing, or added')
    helper = wifi_helper()
    helper.resources(str(out / 'wifi/runtime'))
    if sha(out / 'wifi/wifi_runtime.py') != sha(ROOT / 'deploy/binpatch/wifi_runtime.py'):
        raise ValueError('unrecognized staged Wi-Fi installer')
    return {'status': 'verified', 'directory': str(out), 'files': len(actual),
            'components': ['wifi'], 'manifest_sha256': sha(out / 'network-manifest.json'),
            'activation': 'none'}

def stage(out: Path, *, stack: Path | None = None, recipe=None,
          pf_input=None, phones=(), pf_mode=None, link_mbps=None,
          background_mbps=None, control_evidence=None, dynamic_phone=False):
    # Reject old API callers before creating output; never silently ignore their
    # requested SDIO or PF profile and package something different.
    if (recipe is not None or pf_input is not None or phones or pf_mode is not None
            or link_mbps is not None or background_mbps is not None
            or control_evidence is not None or dynamic_phone):
        raise ValueError('SDIO and PF QoS staging is disabled; Wi-Fi only')
    reviewed = status()
    stack_evidence = None
    if stack is not None:
        value = sha(stack)
        if value not in wifi_helper().STACKS:
            raise ValueError('unsupported io-pkt input; nothing staged')
        stack_evidence = {'path': str(stack.resolve()), 'sha256': value}
    out = out.resolve()
    for protected in (ROOT / n for n in ('deploy', 'wireless', 'scripts', 'tests',
                      'install_MoreIncredibleBash', 'uninstall_MoreIncredibleBash')):
        if out == protected or out.is_relative_to(protected) or protected.is_relative_to(out):
            raise ValueError('output overlaps project sources')
    out.mkdir(parents=True, exist_ok=False)
    try:
        shutil.copytree(ROOT / 'deploy/wifi/runtime', out / 'wifi/runtime')
        shutil.copyfile(ROOT / 'deploy/binpatch/wifi_runtime.py', out / 'wifi/wifi_runtime.py')
        manifest = {'schema_version': 2, 'status': 'wifi-only-staged-no-activation',
                    'components': ['wifi'], 'reviewed_resources': reviewed,
                    'io_pkt_evidence': stack_evidence, 'files': regular_files(out)}
        (out / 'network-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        return verify_stage(out)
    except BaseException:
        shutil.rmtree(out)
        raise

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('status')
    verify = commands.add_parser('verify')
    verify.add_argument('directory', type=Path)
    prepare = commands.add_parser('stage')
    prepare.add_argument('--out', type=Path, required=True)
    prepare.add_argument('--io-pkt', type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == 'status':
            result = status()
        elif args.command == 'verify':
            result = verify_stage(args.directory)
        else:
            result = stage(args.out, stack=args.io_pkt)
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, ValueError, KeyError) as error:
        print('Network patcher refused: ' + str(error), file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
