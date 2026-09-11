"""LOCAL ONLY: explicit source selection; this file is never shipped."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[3]
DEST = ROOT / 'output/share/altscreen-hook-excerpts'
PRIVATE = Path(__file__).resolve().parent


def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    pos = brace + 1
    # These selected functions have no braces in comments/string literals.
    while depth:
        if source[pos] == '{':
            depth += 1
        elif source[pos] == '}':
            depth -= 1
        pos += 1
    return source[start:pos]


NOTICE = '/* Copyright (c) 2026, original project author. */\n'
sources = {
    'video': ROOT / 'hook/altscreen/altscreen_av.c',
    'frame': ROOT / 'altscreen_render/frame_io.c',
    'public_framework': ROOT / 'output/share-review/public-rgi-baseline/hook/framework/hook_framework.c',
}
originals = {key: p.read_text() for key, p in sources.items()}
files = {}

avcc = function(originals['video'], 'static uint32_t avcc_to_annexb(')
avcc = avcc.replace('static uint32_t avcc_to_annexb(', 'uint32_t avcc_to_annexb(', 1)
avcc = avcc.replace('/* never happens on the write pass */', '/* output capacity exhausted */')
files['altscreen/avcc.c'] = NOTICE + '#include "avcc.h"\n#include <string.h>\n\n' + avcc + '\n'
files['altscreen/avcc.h'] = NOTICE + '''#ifndef EXCERPT_AVCC_H
#define EXCERPT_AVCC_H
#include <stdint.h>

/* Convert length-prefixed H.264 NAL units to Annex-B start codes.
 * Input must already be accessible as plain bytes. length_size is 1..4;
 * other values use 4. No container or session parsing is performed here.
 * With out == NULL, returns the required output size for the valid prefix.
 * With out != NULL, writes complete NAL units up to out_capacity.
 * A malformed/truncated suffix stops conversion; the valid prefix is kept.
 * in must address in_length bytes; out must address out_capacity bytes.
 * Input and output must not overlap. The result is a byte count, not a
 * validation status. The caller decides whether partial conversion is useful.
 */
uint32_t avcc_to_annexb(const uint8_t *in, uint32_t in_length,
                       uint8_t length_size, uint8_t *out,
                       uint32_t out_capacity);
#endif
'''

reader = function(originals['frame'], 'int av_read_exact(')
frame = function(originals['frame'], 'int av_read_frame(')
reader = reader.replace('av_read_exact', 'frame_read_exact')
frame = frame.replace('av_read_frame', 'frame_read').replace('av_read_exact', 'frame_read_exact')
frame = frame.replace('uint32_t *out_len)', 'uint32_t *out_len, uint32_t max_payload)')
frame = frame.replace('ALTAV_MAX_MSG', 'max_payload')
files['altscreen/frame_io.c'] = NOTICE + '''#include "frame_io.h"
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

''' + reader + '\n\n' + frame + '\n'
files['altscreen/frame_io.h'] = NOTICE + '''#ifndef EXCERPT_FRAME_IO_H
#define EXCERPT_FRAME_IO_H
#include <stdint.h>
#include <stddef.h>

/* Blocking POSIX descriptor reader: 0 success, -1 EOF/error.
 * The caller supplies descriptor lifetime and cancellation policy.
 * A descriptor must not be reused concurrently for a different connection.
 */
int frame_read_exact(int fd, void *buf, size_t length);

/* Read [u32 big-endian length][payload]. Start with *buf=NULL, *capacity=0.
 * Reuses/grows malloc storage; the caller eventually frees *buf.
 * All pointer arguments must be valid. max_payload is a caller policy.
 * Returns 0 success, -1 EOF/I/O error, -2 size/allocation error.
 * A zero-length record succeeds with *out_length=0.
 * On error, out_length is unspecified; discard any partially read record.
 */
int frame_read(int fd, uint8_t **buf, uint32_t *capacity,
               uint32_t *out_length, uint32_t max_payload);
#endif
'''

# Hook examples are explicit templates adapted from the pinned public framework.
for name in ('interpose_example.c','interpose_example.h','process_scope.c','process_scope.h','NOTES.md'):
    files['hook/' + name] = (PRIVATE/'templates/hook'/name).read_text()

baseline = json.loads((ROOT/'output/share-review/public-rgi-baseline/baseline.json').read_text())
commit = baseline['commit']
files['PUBLIC_SOURCE.md'] = f"""# Public reference

The hook examples illustrate mechanisms already present in this public revision:

[Public framework](https://github.com/luka-dev/mib2q-carplay-rgi/blob/{commit}/hook/framework/hook_framework.c)

Reference commit: `{commit}`.

The public framework already contains next-symbol lookup, lazy initialization,
process filtering and a module-dispatch mechanism. The examples here are small
educational adaptations, not verbatim copies or a complete framework export.
The process-name comparison is exact and returns an explicit unknown state;
the example API is fictional and its observations are limited to counters.

The two video helper implementations are selected local excerpts. They are not
claimed to be files from the public revision. Original development paths and
private integration details are not included in this selection.
"""
files['README.md'] = """# altScreen / hook source excerpts

A limited selection of portable video helpers and hook examples for discussion.
This is not a complete receiving/display implementation or a deployment package.

| File | Included behavior |
|---|---|
| `altscreen/avcc.c`, `avcc.h` | Length-prefixed H.264 NAL units to Annex-B conversion |
| `altscreen/frame_io.c`, `frame_io.h` | Caller-bounded records over a POSIX descriptor |
| `hook/interpose_example.c`, `.h` | Lazy original-symbol lookup, forwarding and result counters |
| `hook/process_scope.c`, `.h` | Process-name filter with an explicit unknown result |
| `hook/NOTES.md` | Contracts and limits of the hook examples |

The video functions are real source excerpts with standalone declarations.
The hook examples are educational adaptations of mechanisms already described
in the [public reference](PUBLIC_SOURCE.md). Their API name/signature are fictional;
no private production entry point is exposed by those examples.

The helpers use C99/POSIX; the hook example uses pthreads and the host dynamic
loader. Function contracts are in the headers. Host checks use synthetic data
and a fictional provider, not vehicle firmware. `CONTENTS.sha256` covers every
other included file. No platform SDK, build/deployment instructions or binaries
are included.
"""

if DEST.exists():
    raise SystemExit('Refusing to overwrite an existing share directory')
DEST.mkdir(parents=True)
for rel, text in sorted(files.items()):
    p = DEST / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)

checksums = ''.join(hashlib.sha256((DEST/rel).read_bytes()).hexdigest() + '  ' + rel + '\n' for rel in sorted(files))
(DEST/'CONTENTS.sha256').write_text(checksums)

private_manifest = {
    'profile': 'user revision: remove all RGI; add limited hook usefulness based on public baseline',
    'public_baseline': {'url': baseline['repository'], 'commit': commit},
    'sources': {key: {'path':str(path.relative_to(ROOT)), 'sha256':hashlib.sha256(path.read_bytes()).hexdigest()} for key,path in sources.items()},
    'selection': {
        'altscreen/avcc.c': 'avcc_to_annexb only; static removed; one comment generalized',
        'altscreen/frame_io.c': 'av_read_exact + av_read_frame; renamed; max size passed by caller',
        'hook/interpose_example.c': 'educational adaptation of public next-symbol/lazy-init pattern; fictional API, original status and errno preserved',
        'hook/process_scope.c': 'adapted public process filter; exact basename and explicit UNKNOWN rather than substring/fail-open',
        'hook/*.h': 'standalone example declarations, no production ABI',
        'notices': 'author name and public handle anonymized in the share copy only; year and copyright retained',
    },
    'allowlist': sorted([*files, 'CONTENTS.sha256']),
    'original_files_unchanged': all(path.read_text()==originals[key] for key,path in sources.items()),
}
(PRIVATE/'selection.json').write_text(json.dumps(private_manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'folder':str(DEST),'files':len(files)+1,'source_lines':sum(len(t.splitlines()) for n,t in files.items() if n.endswith(('.c','.h'))),'original_files_unchanged':private_manifest['original_files_unchanged']}))
