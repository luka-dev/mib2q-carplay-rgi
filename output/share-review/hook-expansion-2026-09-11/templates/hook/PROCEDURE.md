# Hook procedure and host demonstration

This guide explains the wrapper in this folder and a general procedure for
extending it. The demo uses an ordinary C export in a synthetic provider. Its
name and signature are not production integration points.

## Run the complete example

From the package root:

```sh
make -C hook demo
```

This builds a provider, an interposing library, a caller and a view-area demo
under `hook/build`. It runs only those local programs. Expected results are
256 forwarded calls with exact counters, followed by a synthetic rectangle and
successful view-state checks. `make -C hook clean` removes those four outputs.
An alternative output directory can be supplied with `OUT=/your/chosen/path`.
The Makefile uses ordinary make path handling; choose a build path without
spaces or commas. It does not install anything or change global loader settings.

The macOS recipe uses a flat-namespace demo caller and an explicit provider
dependency so the next-symbol lookup finds the intended provider after the
wrapper. Apple's documentation describes the `RTLD_NEXT` search order.
[Apple dlsym documentation](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/dlsym.3.html)

The Linux recipe preloads the wrapper only for the demo command. The loader's
preload behavior and restrictions are described in the Linux manual.
[Linux loader documentation](https://man7.org/linux/man-pages/man8/ld.so.8.html)

The macOS recipe was executed for this selection; the Linux recipe is provided
but was not executed here. These recipes are host demonstrations, not deployment
instructions for another process or platform. The driver intentionally looks up
the unique diagnostic export with `RTLD_DEFAULT`; the wrapper resolves its
original provider with `RTLD_NEXT`. They serve different purposes.

## How to build up a hook

1. **Write down the call contract.** Identify the actual calling convention,
   argument types, return convention, object ownership and calling thread before
   writing a wrapper. A matching symbol name alone does not establish an ABI.
   In this demo the caller, provider and wrapper share `interpose_example.h`.

2. **Establish transparent forwarding.** Resolve the original once at runtime,
   after a real call arrives. Keep the resolver small: no provider calls, worker
   startup or module dispatch inside it. The wrapper restores entry `errno`
   before calling the provider, then preserves its result and resulting `errno`.
   Do not call the same interposed name again to reach the original. Here an
   absent provider returns the explicitly documented -1/ENOSYS result.

3. **Separate eligibility from forwarding.** `process_scope.c` checks an exact
   basename and distinguishes MATCH, OTHER and UNKNOWN. Use that result to decide
   whether optional behavior runs; ordinary calls still need their original
   provider. The process filter is an operational choice, not authentication.
   It is independent of the supplied wrapper rather than silently enabled.

4. **Choose explicit observation points.** A useful order is pre-call inspection,
   one original call, then post-call inspection. Treat input objects as borrowed.
   Post-call logic checks status and output presence before reading a response.
   If an extension needs to change an output object, build its candidate changes
   separately and publish only after the whole candidate is valid; an allocation
   failure should not expose a half-built object. Such output editing is not
   implemented by the counter-only demo.

5. **Give modules a small lifecycle.** A module table can hold init, pre-call,
   post-call and shutdown callbacks. Initialize once at the first suitable
   runtime boundary; shut down in reverse initialization order. These patterns
   already appear in the [public framework reference](../PUBLIC_SOURCE.md).
   Keep heavy startup out of library constructors. Module callbacks must not
   re-enter a once-only initializer that is still running.

6. **Move deferred work onto an owner.** Copy the minimum immutable data needed,
   attach a session generation, and enqueue it with a finite capacity. Do not
   retain borrowed response pointers or hold a hook mutex while calling a
   provider. A callback from another thread posts a copied completion token to
   the owner. `view_switch.h` supplies a small serial-owner example; the renderer
   has a separate worker queue. The examples are not automatically connected.

7. **Close the lifecycle deliberately.** Stop accepting optional work, invalidate
   the active session, stop its producers, and drain callbacks before freeing
   referenced state. An old completion cannot revive a closed session. Keep
   waits bounded and define who owns recovery when they expire. The isolated
   renderer's process-exit policy is not a generic hook-library shutdown policy.

## Check the behavior, not just whether it loads

The demo provider checks incoming arguments and `errno`; half its calls succeed
and half return a controlled error. Four concurrent callers exercise first-use
initialization and the protected counter snapshot. The hook holds no statistics
mutex across a provider call. It retains neither payloads nor caller pointers.

For a real adapter, also exercise failed initialization, unavailable outputs,
reconnects, queue overload, delayed completions and shutdown. The supplied view
helper covers local state transitions only. It does not supply a registration
message, command sender, callback ABI, device identity or output binding.
