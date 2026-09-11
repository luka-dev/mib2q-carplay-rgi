# Hook examples

These examples illustrate mechanisms already present in the public framework.
They are standalone educational adaptations, not extracted private entry points.

`interpose_example.c` exports the fictional `demo_video_submit` API. On a host
configured for dynamic symbol interposition, its first call resolves the next
provider through `dlsym(RTLD_NEXT, ...)`. A `pthread_once` publishes the function
pointer to concurrent callers. There is no load-time constructor.

Every successful lookup leads to exactly one original call, with unchanged
arguments. Afterwards the wrapper counts completed successes/failures while
preserving both return value and `errno`. It never logs or stores payloads.
The mutex protects only counters and is released before any provider call.

The name and ABI are deliberately fictional. The example needs a provider
using that declaration and a host loader configured for interposition. It is
not a generic replacement for arbitrary exports. Lookup failure is latched
for the lifetime of the library; loading a provider later will not retry it.
Resolver re-entry into this same API and unloading a provider while in use
are outside the example's contract. Missing provider returns -1 with ENOSYS.

`process_scope.c` demonstrates restricting optional side effects to an intended
process. It distinguishes MATCH, OTHER and UNKNOWN; it matches a basename
exactly. Keep the original API callable even when optional hook work is skipped.
The example interposer does not automatically apply this independent filter.
The `/proc` probe is best-effort and its UNKNOWN policy belongs to the caller.

These hook examples are independent of the video helpers and receiver. They
do not supply the receiver with a stream. See the top-level public-source note
for the reference revision.
