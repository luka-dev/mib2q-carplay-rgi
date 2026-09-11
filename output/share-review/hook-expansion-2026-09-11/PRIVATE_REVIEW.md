# PRIVATE — hook/view expansion, 2026-09-11

Current deliverable: `output/share/altscreen-renderer-share` and matching ZIP.
This revision supersedes the 19-file/1,200-line renderer subset. The user asked
for more general hook information/procedure and possibly safe/view areas.
The existing no-RGI/no-hardware-integration boundary remains in effect.

Added ten source/document/build files: hook procedure, view-area notes, a host
Makefile, three synthetic demo sources, portable visible/safe rectangle helpers,
and an owner-thread desired/confirmed view-state model. The rectangle property
names come from the local build_rect/build_viewarea representation and are now
intentionally disclosed. No calibrated dimensions, full declaration catalog,
session command payload, identifiers, platform object API or sender are shared.
The state model is newly written, inspired by local generation/ACK checks; it
does not claim to be verbatim production code or solve remote command ordering.

The hook guide covers exact ABI/ownership, transparent forwarding, lazy init,
process scope, pre/post-call inspection, atomic candidate output changes,
module lifecycle, deferred work and shutdown. Its host-loader statements cite
Apple's dlsym manual and the Linux man-pages loader documentation. The public
framework reference remains pinned to f36790d450392516ed9cbfab9cc06aa13a08bf31.

The shipped macOS Makefile demo passed: 256 interposed calls across four threads,
argument/result/errno preservation, exact counters and the synthetic view demo.
Separate strict-C99 ASan/UBSan checks passed for rectangle containment/overflow,
diagnostic buffer sizing, coalescing, failures, duplicate/old-session completion
tokens and counter exhaustion. Linux recipe unexecuted; no hardware/protocol
adapter provided or tested. Prior renderer/helper sources and renderer Makefile
are byte-identical to the previously tested package; those checks were preserved.

Review enforced 29 explicit text files, no hidden files/symlinks, content checksums,
private identity/binding scans and standard decoder-call allowlist. Only the
fictional hook target is resolved through RTLD_NEXT. The demo driver's separate
RTLD_DEFAULT lookup is limited to the diagnostic demo_hook_get_stats export.
The one Linux preload assignment is allowed only in the host-demo Makefile.
Three exact URLs are allowed: pinned public framework and the two primary manuals.

The new ZIP was verified against the folder and atomically replaced the previous
ZIP after verifying the previous hash. ZIP metadata is normalized; Finder metadata
and all built binaries remain outside its allowlist. Earlier verified sources and
ZIP are retained privately for provenance/reproduction. Production source hashes
still match; unrelated audio/document work was not modified.

Final package: 29 files, 1,471 C/header lines, 35,740 ZIP bytes.
SHA-256: bd3d0c74a878d13f554bb4e69729e607dbe26f9a9874c02119a4719fb9efe720.
No external transfer/publication was performed.
