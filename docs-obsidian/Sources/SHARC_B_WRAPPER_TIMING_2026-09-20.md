# First-B wrapper: conditional two-bank timing — 2026-09-20

Static/offline only. This closes steady buffer algebra, **not** interrupt or
callback readiness/transition/error timing, and does not assert current car state.

## Recovered dataflow

Process handler124DF3 receives five512-frame input lanes and returns eight
output lanes. At descriptor+7 is the bank selector; +6 is the offset within
that bank. For selector s and offset o:

```text
input  lane j: B95EA + 2560*s + 512*j + o
output lane k: BA9EA + 4096*s + 512*k + o
```

The handler copies the current input sub-block, **then reads existing output
from that same selector/offset**, and only afterward checks bank completion.
At512 frames it publishes the completed input/output bases to BC9EA/BC9EB,
toggles selector `(s+1)&1`, resets offset, and follows callback scheduling.

With explicit zero initial output banks and successful callbacks completing
on time, input bank0 is processed after it fills but is not read back until
bank0 is reused after bank1. The additional wrapper delay is therefore
**1024 samples =21.333333ms at48kHz**, not512 samples.

This is separate from the spectral library's one-hop512-sample overlap delay.
A neutral spectral path under the same successful assumptions therefore has
1536 samples =32ms before any additional library/common-input/output delays.
Filter phase/group delay is not represented by this integer-delay sum.

## Evidence

- 124E0C/124E0E/124E15/124E18: input stride, selector, base and offset.
- 124E60: input-lane512 stride.
- 124E63/124E69/124EA5: output base, selector<<12 and lane512 stride.
- 124EBC..124ED6: completed-bank count, published pointers, selector toggle.
- 124EA8 follows output-copy loop: parameter/event work happens after copy-out.
- 124F14..124F25 and callback124F56: scheduling/control path, deliberately not
  assigned a real-time deadline or assumed active by the reference.

Shortword range124DF3..124ED8 SHA256:
`0f527209f2f8f16396f935f00f56fd425c43da5dc1cc1437724de4ef8c8a56f4`.
Derived loader SHA256:
`9169c24f0337c6bfd3646e2538a567bca839b24aa0db5f67b6984943c2286ca7`.
Original CodeFlash SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
Program addresses are SHARC shortwords; buffer addresses are32-bit DM words.
`amp_b_wrapper.wrapper_evidence()` checks loader/range hashes and15 decoded
instruction pins before reporting these constants.

## Executable reference and tests

`BWrapperDoubleBufferReference` requires both initial input/output banks,
selector, sub-block offset and an explicit on-time-successful-callback
assumption. Its supplied callback processes a complete512×5 bank and returns
512×8 outputs. It is not a made-up B algorithm. Blocks crossing a bank boundary
are rejected; callers must split as the scheduler does. Callback input is copied
so alias writes cannot accidentally mutate retained bank storage.

Nine tests cover both initial selectors, partition invariance, explicit old
output history, callback ordering, rejection boundaries and additive library
delay. An independent flat-DM-address simulator gives identical audio. A
synthetic mode5 reference processed through actual64-frame wrapper copies
matches AmpChain's existing mode5 rendering plus explicit1024-frame delay.

```sh
python3 -m unittest discover -s tools/soundamp-virtual-cabin \
  -p test_amp_b_wrapper.py
```

For this **specific conditional steady scenario**, an AmpChain first-B
configuration may use `wrapper_delay_samples:1024`, while retaining an explicit
label documenting on-time callbacks and zero initial wrapper output banks.
No default delay, B activation, production hook or vehicle setting was changed.
Existing scenarios using0 or another explicit delay remain hypotheses; they
are not silently rewritten into a supposedly measured configuration.

## Remaining timing limits

Late/missing callbacks, initialization or library failure, B909B transition
clearing, BC9EC inhibition, active handler changes and shared-pointer races
cannot be represented by a universal fixed delay. The actual firmware event
schedule is still an external-state/timing question. Nonzero initial output
banks produce their stored samples before the normal delayed response; this
is why histories are required, not silently assumed away.
