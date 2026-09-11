# PRIVATE — renderer export review, 2026-09-11

Do not send this directory. The deliverable is `output/share/altscreen-renderer-share.zip`
or its matching source folder. The previous `altscreen-hook-excerpts` folder/archive
was removed after comparison against its recorded hashes. Its reviewed text inputs
remain here in `previous-excerpts` so the export recipe can be reproduced.

The user rejected the small helper-only package and asked for an honestly useful
renderer subset, suggesting removal of EGL presentation. The working interpretation
was stated as real `altscreen_render` receiver/decoder sources without native output
integration; the user then said to continue. RGI remains excluded. This deliberately
shares more private implementation than the superseded 269-line helper selection:
19 files, 1,200 C/header lines in total, of which 932 are renderer C/header lines.

Selected real logic includes the receive queue, generations, worker/sink lifecycle,
input configuration caching, OpenMAX state transitions, input recycling, output
callbacks, startup retry limits and callback draining. The capture backend builds
and runs on the host. The OpenMAX core needs an external output backend and is not
part of that host build. The source adaptations and limits are disclosed in the
public README and provenance document.

Withheld: actual native display/presentation code; EGL/Screen calls; vendor component
name and configuration; proprietary output format/layout; allocator and display
binding; target geometry/endpoints; Java/control-channel ownership; phone-facing
session and stream setup; production hook targets/ABI; graphics recovery/loader
implementation; SDK, binaries and deployment materials. All RGI code and comparison
or reverse-engineering artifacts are outside the allowlist. The public repository
URL in PUBLIC_SOURCE.md is the one explicit identity/reference exception.

Public baseline is commit f36790d450392516ed9cbfab9cc06aa13a08bf31. Previously fetched
framework inputs were verified against GitHub Git blob hashes. Generic hook mechanics
already occur there; the renderer excerpts are explicitly identified as local code.
Only that current public tree was inspected, not every historical commit.

Validation: final exported receiver and decoder sources compiled with strict C99
warnings and ASan/UBSan. Host socket tests verified fragmented records, empty records,
two-session byte-exact capture, oversized/truncated rejection and shutdown with an
idle connected client. A synchronous fake OpenMAX component verified two-session
configuration isolation, normal recycling, rejected/oversized input, latched output
change errors and release ordering. These are not real codec, asynchronous hardware,
fault-exhaustion or display tests. The shipped Makefile also built successfully in a
private copy. Previously tested video/hook helper bytes are unchanged; those tests
covered 256 calls across four threads and absent-provider behavior.

Known excerpt contracts: one isolated decoder process; complete configuration in one
record; new session for parameter changes; fixed output configuration; zero timestamps;
pending queue discarded on reconnect/shutdown. Hardware-specific integration and
recovery remain recipient work. The adapter header has declarations, not fake stubs.

Packaging review used an exact allowlist, text-only and symlink checks, targeted
identity/private-binding scans, a standard OpenMAX call allowlist, content checksums,
and unchanged original source hashes. ZIP entries have normalized metadata and were
read back and compared to the folder. Finder can recreate `.DS_Store` in a browsed
folder; archive inputs are explicit, so such metadata cannot enter the ZIP.

Archive: 22,994 bytes.
SHA-256: 84463120b462240df2fbcac8a5819d1c3c467d9ee4cc68802a5b67b5996a309d.

No publication or external transfer was performed. Production sources were unchanged;
unrelated pre-existing audio and document changes were left intact.
