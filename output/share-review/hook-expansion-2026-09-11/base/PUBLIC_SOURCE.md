# Source provenance

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
