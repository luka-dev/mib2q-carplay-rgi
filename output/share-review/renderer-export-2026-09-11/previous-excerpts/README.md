# altScreen / hook source excerpts

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
