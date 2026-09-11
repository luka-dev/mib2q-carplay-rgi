"""PRIVATE: explicit, reproducible allowlist; does not copy any source tree."""
from pathlib import Path
import hashlib
import json
import renderer_export

P=Path(__file__).resolve().parent
ROOT=P.parents[2]
OLD=P/'previous-excerpts'
PREVIOUS=ROOT/'output/share-review/rgi-altscreen-2026-09-11'
DEST=ROOT/'output/share/altscreen-renderer-share'
assert not DEST.exists(), 'Refusing to overwrite a package'

previous_review=json.loads((PREVIOUS/'review.json').read_text())
previous_selection=json.loads((PREVIOUS/'selection.json').read_text())
base={}
for f in previous_review['files']:
    data=(OLD/f['path']).read_bytes()
    assert hashlib.sha256(data).hexdigest()==f['sha256'], f['path']
    base[f['path']]=data.decode('utf-8')

files={**base,**renderer_export.export()}
files['README.md']='''# altScreen renderer source subset

Selected real receiver and decoder sources, extracted and adapted for sharing.
The host receiver builds and captures incoming data. The OpenMAX core is included
as source; display integration is an explicit, unimplemented backend boundary.

| Area | Included behavior |
|---|---|
| `renderer/main.c` | Framed TCP receiver, bounded worker queue, generation changes and session cleanup |
| `renderer/sink_omx.c` | Configuration cache, decoder states, input recycling, output callbacks and teardown |
| `renderer/output_backend.h` | Contract for component configuration, native buffers and decoded output |
| `renderer/sink_capture.c`, `sink.h`, `Makefile` | Working host receiver/capture build and sink interface |
| `altscreen/avcc.c`, `.h` | Length-prefixed H.264 NAL units to Annex-B conversion |
| `altscreen/frame_io.c`, `.h` | Caller-bounded records over a POSIX descriptor |
| `hook/interpose_example.c`, `.h` | Lazy original-symbol lookup, forwarding, result counters and errno preservation |
| `hook/process_scope.c`, `.h` | Exact process-name matching with an explicit unknown result |

Build with `make -C renderer`; run `./renderer/video_receiver PORT` with a port
you choose. It listens on loopback and logs session counters. Optional
`VIDEO_CAPTURE_FILE` creates/truncates a raw capture. The caller must provide
already-plain Annex-B records; the receiver does not discover a device or obtain
a stream by itself. See [renderer contracts](renderer/README.md) before use.

The platform output backend, graphics presentation, display registration,
phone-session setup, production hook entry points, control channel, SDK headers,
startup assets and deployment tooling are outside this source selection.
`sink_omx.c` is not linked into the host build. Integrating that core requires
your own implementation of `output_backend.h` and component-specific adaptation.

The receiver/decoder and video helpers are adapted local implementation excerpts.
The hook examples are small educational adaptations of public mechanisms, using
a fictional API. They are independent examples and are not wired into the
receiver. [Provenance](PUBLIC_SOURCE.md) distinguishes these two sources.

## Validation of this selection

Host C99 builds use strict compiler warnings. Address/undefined-behavior sanitizer
checks exercised fragmented input, empty records, reconnects, exact capture
bytes, oversized/truncated records and shutdown with an idle connected client.
A synthetic OpenMAX component exercised configuration isolation across two
sessions, input/output recycling, rejected and oversized input, output-change
error handling and resource-release order. This does not validate a codec,
real asynchronous hardware behavior or display output.

The helper checks and fictional interposer checks passed, including 256 calls
across four threads, original argument/result/errno preservation and absent
provider behavior. Test fixtures, SDK files and test binaries are not included.
`CONTENTS.sha256` covers every other file in this source-only selection.
'''
files['PUBLIC_SOURCE.md']='''# Source provenance

The hook examples illustrate mechanisms already present in this public revision:

[Public framework](https://github.com/luka-dev/mib2q-carplay-rgi/blob/f36790d450392516ed9cbfab9cc06aa13a08bf31/hook/framework/hook_framework.c)

Reference commit: `f36790d450392516ed9cbfab9cc06aa13a08bf31`.

That public framework already includes next-symbol lookup, lazy initialization,
process filtering and module dispatch. `hook/` contains educational adaptations,
not a verbatim framework export. The example API is fictional. Process matching
uses an exact basename and reports an explicit unknown state.

`renderer/` and `altscreen/` contain selected local implementation excerpts and
supporting adaptations; they are not claimed to come from that public revision.
The receiver keeps the actual queue/worker design with a smaller receive loop.
The decoder keeps the actual component/buffer lifecycle; platform output code
is replaced by the declared contract in `output_backend.h`. The capture sink
is an adapted host implementation. The backend interface and standalone build
glue were prepared for this selection.

Original development paths, private component selection and display bindings
are not included. This document identifies provenance; it does not add a new
license or broaden any existing license grant.
'''
files['hook/NOTES.md']=base['hook/NOTES.md'].replace(
    'The video helpers are independent of these examples; no complete receiving or\ndisplay pipeline is connected here. See the top-level public-source note for\nthe reference revision.',
    'These hook examples are independent of the video helpers and receiver. They\ndo not supply the receiver with a stream. See the top-level public-source note\nfor the reference revision.')
files.pop('CONTENTS.sha256')
files['CONTENTS.sha256']=''.join(hashlib.sha256(text.encode()).hexdigest()+'  '+name+'\n'
                                for name,text in sorted(files.items()))

for name,text in files.items():
    path=DEST/name
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(text)

sources=json.loads((P/'renderer_sources.json').read_text())['sources']
sources+=list(previous_selection['sources'].values())
for source in sources:
    assert hashlib.sha256((ROOT/source['path']).read_bytes()).hexdigest()==source['sha256']
selection={'profile':'substantial real renderer/queue/OMX core; output backend withheld; no RGI',
           'allowlist':sorted(files),'sources':sources,
           'public_baseline':previous_selection['public_baseline'],
           'previous_export':previous_review['archive'],
           'lines_c_h':sum(len(t.splitlines()) for n,t in files.items() if Path(n).suffix in ('.c','.h'))}
(P/'selection.json').write_text(json.dumps(selection,indent=2)+'\n')
print(json.dumps({'destination':str(DEST),'files':len(files),'code_lines':selection['lines_c_h']}))
