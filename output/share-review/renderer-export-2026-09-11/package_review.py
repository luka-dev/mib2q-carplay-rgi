"""PRIVATE: disclosure allowlist, byte verification and clean ZIP metadata."""
from pathlib import Path
import ast
import hashlib
import json
import re
import stat
import zipfile

P=Path(__file__).resolve().parent
ROOT=P.parents[2]
DEST=ROOT/'output/share/altscreen-renderer-share'
ARCHIVE=DEST.with_suffix('.zip')
PREVIOUS=ROOT/'output/share-review/rgi-altscreen-2026-09-11'
selection=json.loads((P/'selection.json').read_text())
# Finder may recreate this known directory-view metadata while the user browses.
# It is never an archive input; all other unexpected files fail the review.
for metadata in DEST.rglob('.DS_Store'):
    assert metadata.is_file() and not metadata.is_symlink()
    metadata.unlink()
names=sorted(p.relative_to(DEST).as_posix() for p in DEST.rglob('*') if p.is_file())
assert names==selection['allowlist']
assert not any(p.is_symlink() for p in DEST.rglob('*'))
assert not any(part.startswith('.') for name in names for part in Path(name).parts)
assert all(Path(n).suffix in ('.c','.h','.md','.sha256') or n=='renderer/Makefile' for n in names)
assert not ARCHIVE.exists(), 'Refusing to overwrite an archive'

# Read the earlier deny patterns as data, without executing its packager.
tree=ast.parse((PREVIOUS/'package_review.py').read_text())
patterns=next(ast.literal_eval(n.value) for n in tree.body
              if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='patterns' for t in n.targets))
# OpenMAX and generic teardown are deliberately included by this new profile.
patterns['firmware_integration']=patterns['firmware_integration'].replace(r'|\bOMX\b','').replace('|TEARDOWN','')
patterns['private_renderer_binding']=r'QCOM|qualcomm|"OMX\.[A-Za-z]|screen_egl|eglSwap|eglCreate|SCREEN_PROPERTY|screen_(?:create|set|get|post)|7f000004|native-32m|APQ8064|MU1316|\bdio\b|plane99|mib2q-carplay-rgi-next'
public_url='https://github.com/luka-dev/mib2q-carplay-rgi/blob/'+selection['public_baseline']['commit']+'/hook/framework/hook_framework.c'
hits=[]; files=[]
for name in names:
    data=(DEST/name).read_bytes()
    assert b'\0' not in data
    text=data.decode('utf-8').replace(public_url,'[VERIFIED_PUBLIC_REFERENCE]')
    for category,pattern in patterns.items():
        if re.search(pattern,text,re.I): hits.append({'file':name,'category':category})
    files.append({'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
assert not hits, hits
hook=(DEST/'hook/interpose_example.c').read_text()
assert re.findall(r'dlsym\(RTLD_NEXT,\s*"([^"]+)"\)',hook)==['demo_video_submit']
assert 'RTLD_DEFAULT' not in hook

# The shared decoder calls only the reviewed standard component API surface.
decoder=(DEST/'renderer/sink_omx.c').read_text()
omx_calls=sorted(set(re.findall(r'\b(OMX_\w+)\s*\(',decoder)))
assert omx_calls==sorted(['OMX_Init','OMX_Deinit','OMX_GetHandle','OMX_FreeHandle',
                        'OMX_GetParameter','OMX_SetParameter','OMX_SendCommand',
                        'OMX_AllocateBuffer','OMX_FreeBuffer','OMX_UseBuffer',
                        'OMX_EmptyThisBuffer','OMX_FillThisBuffer']),omx_calls
assert 'output_backend.h' in decoder
assert not any('output_backend.c' in n for n in names)

checksum_names=[]
for line in (DEST/'CONTENTS.sha256').read_text().splitlines():
    digest,name=line.split('  ',1); checksum_names.append(name)
    assert hashlib.sha256((DEST/name).read_bytes()).hexdigest()==digest
assert sorted(checksum_names)==[n for n in names if n!='CONTENTS.sha256']
for src in selection['sources']:
    assert hashlib.sha256((ROOT/src['path']).read_bytes()).hexdigest()==src['sha256']

tests=json.loads((P/'test_results.json').read_text())
for key in ('strict_host_build','host_asan_ubsan','framed_receive_fragmentation_empty_records',
            'two_sessions_capture_exact_bytes','oversized_header_partial_body_rejected',
            'shutdown_with_idle_connected_client','decoder_mock_asan_ubsan'):
    assert tests[key]=='passed'
old_tests=json.loads((PREVIOUS/'test_results.json').read_text())
assert all(old_tests[k]=='passed' for k in ('helper_c99_asan_ubsan','dynamic_interposition','missing_provider'))
# Preserved helper and example bytes are exactly those tested earlier.
for f in json.loads((PREVIOUS/'review.json').read_text())['files']:
    if Path(f['path']).suffix in ('.c','.h'):
        assert hashlib.sha256((DEST/f['path']).read_bytes()).hexdigest()==f['sha256']

with zipfile.ZipFile(ARCHIVE,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for name in names:
        info=zipfile.ZipInfo(DEST.name+'/'+name,date_time=(2026,9,11,0,0,0))
        info.create_system=3; info.external_attr=(stat.S_IFREG|0o644)<<16
        info.compress_type=zipfile.ZIP_DEFLATED; info.extra=b''; info.comment=b''
        z.writestr(info,(DEST/name).read_bytes())
with zipfile.ZipFile(ARCHIVE) as z:
    assert z.testzip() is None and not z.comment
    assert z.namelist()==[DEST.name+'/'+n for n in names]
    for item in z.infolist():
        assert not item.extra and not item.comment
        assert z.read(item.filename)==(DEST/item.filename.split('/',1)[1]).read_bytes()

result={'checks':{'exact_allowlist':True,'text_only':True,'no_hidden_files_or_symlinks':True,
                  'pattern_scan_findings':hits,'checksums_match':True,'original_sources_unchanged':True,
                  'archive_matches_folder':True,'zip_metadata_normalized':True,
                  'standard_omx_calls':omx_calls,'hardware_output_backend_included':False,
                  'rgi_source_included':False,'tests':tests,'preserved_hook_tests':old_tests},
        'archive':{'path':str(ARCHIVE.relative_to(ROOT)),'bytes':ARCHIVE.stat().st_size,
                   'sha256':hashlib.sha256(ARCHIVE.read_bytes()).hexdigest()},'files':files,
        'code_lines':selection['lines_c_h']}
(P/'review.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'files':len(files),'code_lines':result['code_lines'],'scan_findings':hits,'archive':result['archive']}))
