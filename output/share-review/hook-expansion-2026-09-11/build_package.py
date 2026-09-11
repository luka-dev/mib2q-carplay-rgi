"""PRIVATE recipe: extend only the reviewed share folder; never modify production."""
from pathlib import Path
import hashlib
import json

P=Path(__file__).resolve().parent
ROOT=P.parents[2]
DEST=ROOT/'output/share/altscreen-renderer-share'
previous=json.loads((P/'previous-review.json').read_text())
selection=json.loads((P/'previous-selection.json').read_text())
files={}
for entry in previous['files']:
    name=entry['path']; data=(P/'base'/name).read_bytes()
    assert hashlib.sha256(data).hexdigest()==entry['sha256']
    assert (DEST/name).read_bytes()==data, 'Share content changed: '+name
    files[name]=data.decode()
additions=['hook/Makefile','hook/PROCEDURE.md','hook/VIEW_AREAS.md',
           'hook/view_area.c','hook/view_area.h','hook/view_switch.c','hook/view_switch.h',
           'hook/demo/driver.c','hook/demo/provider.c','hook/demo/view_demo.c']
for name in additions:
    assert not (DEST/name).exists()
    files[name]=(P/'templates'/name).read_text()
files['README.md']=files['README.md'].replace(
    '| `hook/process_scope.c`, `.h` | Exact process-name matching with an explicit unknown result |',
    '| `hook/process_scope.c`, `.h` | Exact process-name matching with an explicit unknown result |\n'
    '| `hook/view_area.c`, `.h` | Visible/safe rectangles, inset validation and diagnostic JSON |\n'
    '| `hook/view_switch.c`, `.h` | Desired/confirmed areas, request tokens, coalescing and stale-completion rejection |\n'
    '| `hook/demo/`, `Makefile` | Runnable synthetic provider, interposer caller and view examples |')
files['README.md']=files['README.md'].replace(
    'Build with `make -C renderer`;',
    'Start with [the hook procedure](hook/PROCEDURE.md) and [view-area notes](hook/VIEW_AREAS.md).\n'
    '`make -C hook demo` runs the included forwarding and geometry/state demonstrations;\n'
    'the macOS recipe was verified. The view JSON is diagnostic output, not a complete\n'
    'display declaration or command. All example geometry is synthetic.\n\n'
    'Build with `make -C renderer`;')
files['README.md']=files['README.md'].replace(
    'The hook examples are small educational adaptations of public mechanisms, using\n'
    'a fictional API. They are independent examples and are not wired into the\n'
    'receiver. [Provenance](PUBLIC_SOURCE.md) distinguishes these two sources.',
    'The forwarding examples adapt public mechanisms using a fictional API. The view\n'
    'geometry and completion examples generalize selected local concepts. They are\n'
    'independent helpers and are not wired into the receiver or a control transport.\n'
    '[Provenance](PUBLIC_SOURCE.md) identifies excerpts and newly written examples.')
files['README.md']=files['README.md'].replace(
    'provider behavior. Test fixtures, SDK files and test binaries are not included.\n',
    'provider behavior. The shipped host demonstration passed; additional ASan/UBSan\n'
    'checks covered rectangle containment/overflow, JSON buffer sizing, request\n'
    'coalescing, failures, duplicate/stale completions and counter exhaustion.\n'
    'Small synthetic demo sources are included; private test fixtures, SDK files\n'
    'and all compiled binaries are excluded. The Linux demo recipe was not run.\n')
files['PUBLIC_SOURCE.md']=files['PUBLIC_SOURCE.md'].replace(
    'process filtering and module dispatch. `hook/` contains educational adaptations,\n',
    'process filtering and module dispatch. The interposer and process-filter examples\n'
    'in `hook/` are educational adaptations,\n')
files['PUBLIC_SOURCE.md']=files['PUBLIC_SOURCE.md'].replace(
    'Original development paths, private component selection and display bindings\n',
    '`hook/view_area.*` adapts the selected local visible/safe rectangle builder,\n'
    'retaining its rectangle property names. Portable validation, inset conversion\n'
    'and diagnostic JSON formatting were added for this selection.\n'
    '`hook/view_switch.*` is a newly written owner-thread model inspired by local\n'
    'generation/completion guards; it is not the production command sender. The\n'
    'demo provider/caller and view demo use synthetic input. The procedure notes\n'
    'distinguish this local model from actual loader behavior and link primary\n'
    'loader documentation. No complete view-registration or command payload is\n'
    'included, and the examples do not claim protocol compatibility.\n\n'
    'Original development paths, private component selection and display bindings\n')
files['hook/NOTES.md']=files['hook/NOTES.md'].replace(
    '# Hook examples\n',
    '# Hook examples\n\n'
    'Read [PROCEDURE.md](PROCEDURE.md) for the call flow and runnable host demo.\n'
    '[VIEW_AREAS.md](VIEW_AREAS.md) explains the additional geometry and completion\n'
    'helpers. Those helpers adapt local concepts; the public-reference discussion\n'
    'below applies to the interposer and process filter.\n')
files.pop('CONTENTS.sha256')
files['CONTENTS.sha256']=''.join(hashlib.sha256(t.encode()).hexdigest()+'  '+n+'\n' for n,t in sorted(files.items()))
sources=selection['sources']+json.loads((P/'new_sources.json').read_text())
for source in sources:
    assert hashlib.sha256((ROOT/source['path']).read_bytes()).hexdigest()==source['sha256']
for name,text in files.items():
    target=DEST/name; target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists() or target.read_text()!=text: target.write_text(text)
selection.update(profile='renderer subset plus host hook procedure/demo and generalized view/safe geometry and completion state; no RGI or hardware binding',
                 allowlist=sorted(files),sources=sources,
                 lines_c_h=sum(len(t.splitlines()) for n,t in files.items() if Path(n).suffix in ('.c','.h')),
                 previous_export=previous['archive'])
(P/'selection.json').write_text(json.dumps(selection,indent=2)+'\n')
print(json.dumps({'files':len(files),'code_lines':selection['lines_c_h']}))
