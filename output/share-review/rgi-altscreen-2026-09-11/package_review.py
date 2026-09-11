"""LOCAL ONLY: enforce a file allowlist and create a metadata-clean archive."""
from pathlib import Path
import hashlib
import json
import re
import stat
import zipfile

ROOT = Path(__file__).resolve().parents[3]
PRIVATE = Path(__file__).resolve().parent
DEST = ROOT / 'output/share/altscreen-hook-excerpts'
ARCHIVE = DEST.with_suffix('.zip')
selection = json.loads((PRIVATE/'selection.json').read_text())
allowlist = selection['allowlist']
actual = sorted(p.relative_to(DEST).as_posix() for p in DEST.rglob('*') if p.is_file())
assert actual == allowlist, (actual, allowlist)
assert not any(p.is_symlink() for p in DEST.rglob('*')), 'Symlink in share directory'
assert not any(part.startswith('.') for name in actual for part in Path(name).parts), 'Hidden file'
assert all(Path(name).suffix in ('.c','.h','.md','.sha256') for name in actual)

public_url = 'https://github.com/luka-dev/mib2q-carplay-rgi/blob/' + selection['public_baseline']['commit'] + '/hook/framework/hook_framework.c'
patterns = {
    'excluded_rgi': r'\brgi\b|\brgd\b|maneuver|routeguidance|rpath_|ICON_',
    'personal_location': r'/Users/|/home/|file://|Desktop/|Downloads/|AUDI_2|luka|LuKa_dev|com\.luka',
    'identity_or_endpoint': r'https?://|[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}|\b(?:\d{1,3}\.){3}\d{1,3}\b|\b[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}\b',
    'vehicle_identity': r'\b(?:WAU|WVW|WA1)[A-HJ-NPR-Z0-9]{14}\b',
    'firmware_integration': r'AirPlay|iAP2|Cinemo|\bQNX\b|\bOMX\b|\bMOST\b|\bBAP\b|\bDSI\b|\bASL\b|MHI[23]|MST2|\bAudi\b|\bVAG\b|LD_PRELOAD|Xbootclasspath|TEARDOWN|\bsub_[0-9a-f]+\b|\bunk_[0-9a-f]+\b',
    'implementation_details': r'\b(?:19831|19832|19810|6031|6032|7001|1440|542|1010|376)\b|0x(?:520[0-4]|1c8|1b8)\b|streamConnectionID|enabledFeatures|displayPanels|ForceKeyFrame|updateViewArea|switchContext|setUpdateRate|DIODIR|/ramdisk/|/mnt/|/dev/shmem/',
    'credentials': r'-----BEGIN [^-]*PRIVATE KEY-----|\b(?:ghp_|github_pat_|xox[baprs]-|AKIA)[A-Za-z0-9_-]+|(?:password|passwd|access_token|api_key)\s*[:=]',
}
hits = []
files = []
for name in actual:
    p = DEST/name
    data = p.read_bytes()
    assert b'\0' not in data, name
    text = data.decode('utf-8')
    scan_text = text.replace(public_url, '[VERIFIED_PUBLIC_REFERENCE]')
    for kind, pattern in patterns.items():
        if re.search(pattern,scan_text,re.I):
            hits.append({'file':name,'category':kind})
    files.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
assert not hits, hits
lookup_source = (DEST/'hook/interpose_example.c').read_text()
assert re.findall(r'dlsym\(RTLD_NEXT,\s*"([^"]+)"\)', lookup_source) == ['demo_video_submit']
assert 'RTLD_DEFAULT' not in lookup_source
tests=json.loads((PRIVATE/'test_results.json').read_text())
assert tests['helper_c99_asan_ubsan']=='passed' and tests['dynamic_interposition']=='passed' and tests['missing_provider']=='passed'

checksum_lines=(DEST/'CONTENTS.sha256').read_text().splitlines()
assert len(checksum_lines)==len(actual)-1
for line in checksum_lines:
    digest, name=line.split('  ',1)
    assert name in allowlist and name != 'CONTENTS.sha256'
    assert hashlib.sha256((DEST/name).read_bytes()).hexdigest()==digest

# Re-check original source files against the hashes taken before extraction.
for source in selection['sources'].values():
    assert hashlib.sha256((ROOT/source['path']).read_bytes()).hexdigest()==source['sha256'], source['path']

assert not ARCHIVE.exists(), 'Refusing to overwrite an existing archive'
with zipfile.ZipFile(ARCHIVE,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name in allowlist:
        info=zipfile.ZipInfo(DEST.name+'/'+name,date_time=(2026,9,11,0,0,0))
        info.create_system=3
        info.external_attr=(stat.S_IFREG | 0o644)<<16
        info.compress_type=zipfile.ZIP_DEFLATED
        info.extra=b''
        info.comment=b''
        z.writestr(info,(DEST/name).read_bytes())

with zipfile.ZipFile(ARCHIVE) as z:
    assert z.testzip() is None
    assert z.comment==b''
    assert z.namelist()==[DEST.name+'/'+name for name in allowlist]
    for info in z.infolist():
        assert not info.extra and not info.comment
        rel=info.filename[len(DEST.name)+1:]
        assert z.read(info.filename)==(DEST/rel).read_bytes()

result = {
    'checks':{'exact_allowlist':True,'text_only':True,'no_hidden_files_or_symlinks':True,
              'pattern_scan_findings':hits,'checksums_match':True,'original_sources_unchanged':True,
              'archive_verified_against_folder':True,'zip_metadata_normalized':True,
              'synthetic_host_checks':tests, 'no_rgi_content':True, 'only_fictional_hook_target':True, 'public_baseline':selection['public_baseline']},
    'archive':{'path':str(ARCHIVE.relative_to(ROOT)),'bytes':ARCHIVE.stat().st_size,
               'sha256':hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()},
    'files':files,
}
(PRIVATE/'review.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'files':len(files),'scan_findings':hits,'archive_bytes':result['archive']['bytes'],'archive_sha256':result['archive']['sha256']}))
