# V850 outer request binding and API18 source — 2026-09-20

> Follow-up: [coefficient paths and matrix orientation](SHARC_MATRIX_CONTROL_2026-09-20.md)
> are now bound. This found and fixed an extra transpose in AmpChain v5/v6.
> Current403 tests,168 SHARC ranges,43 V850 listings; later “next” notes below are historical.

Static only: local CodeFlash, bounded r2 V850/32 LE/base0 disassembly and offline
tests. No car access, requests, deployment or settings changes. Addresses below
are V850 byte/file offsets unless explicitly called SHARC word addresses.

## Result and correction

The generic route to extended B commands is now bound through function **0xA21**,
not merely two unclassified handler pointers. It forwards request-supplied raw
command/value words, so it can reach the previously decoded indexed writes and
source-RAM store. This proves a code path, **not its use during playback**.

**Correction:** API18 does have a dedicated V850 sender at AA886, reached through
events22/368. The previous “no dedicated API18 writer bound” statement is superseded.
Its source is a signed coordinate clamped to −9..+9, not an implicit +9.5dB.
No UI identity (Bass, LFE or MMI percent) is assigned to that coordinate.

## External dispatch to 0xA21

Main dispatcher99774 uses block table401F8, stride12. Record4021C contains
block IDF5, instance pointer03FF29F7 and function directory965E4. It checks
block/instance, including broadcast cases, then scans function records(stride8)
and operation records(stride12). Input is an already prepared descriptor:
payload pointer+0, length+4, function+8, block+10, instance+11, operation+12.
This is **not a complete wire packet specification** or authentication proof.

| Directory/function | Operation | Flags | Handler | Behavior |
|---|---:|---:|---|---|
| 965E4 / A21, record966CC→96458 | 2 | D0 | D40C8 | two-word write, four-byte response from staging |
| same | 1 | 02 | D4180 | one-word command, separate result wait/read |
| 96734 / A21, record96814→965A8 | 0 | D0 | D4130 | two-word write, return0 |
| same | 1 | 02 | D42DC | one-word command, separate result wait/read |

FlagsD0 select function pointer+4 with(response,request); low nibble2 selects
pointer+8 with the same arguments, provided the first stage returned0.
Main switch table99BD2..99BE0 is data and excluded from linear code listings.

Alternate dispatcherD57C8 is called atA29C8/A29E2 fromA2874. That caller:

- Rejects when gp−17868==1; requires BE16(message+6)>=14.
- Selects the alternate dispatcher for BE16(message+20)==A018.
- Reads inner function from BE16(message+22), payload beginsmessage+24.
- Uses message[19]&7F:1→inner operation0;3→inner operation1;2 does not call it.

The internal offsets do not establish outermost ingress or who sends the message.
D57C8 does not initialize the input descriptor's length, and neither A21 write
handler checks it. Do not invent a validated eight-byte guard on these paths.

## Staging, events and responses

D40C8/D4130 call DD814(3), then decode two BE32 words via99D98 into
gp−15936(command) and gp−15932(value). There is no command whitelist or float
conversion here. Both call C3990(369). Event369 goes toA782E, which reads the
shared staging words and callsAC648; it then callsDD830(3).

DD814/DD830 wrap EAADC/EAB44: decrement/block and increment/wake behavior is
visible, consistent with counting-semaphore synchronization. The scheduler,
initial semaphore values and complete concurrent lifecycle are not emulated.
C3990 calls B5CFC, a deduplicating u16 event ring of376 entries, then signals(1,10).
It does **not** make a private command/value copy for each queued event.

AC648 forwards the two raw words to descriptorC4AE1, the known SHARC API bridge.
D40C8 returns BE32 from the staging-value slot, response length4, return12.
There is no dedicated DSP-result wait in this write handler; a write response
must not be interpreted as a verified resulting DSP coefficient.

The separate operation1 handlers decode only the command, set completion selector
gp−15711 to2(main) or7(alternate), and queue370. AC6CC sends that command with
**raw payload0**, reads one word at SHARC DM B95E8 into gp−15708, and signals
(2,12) or(7,16). D4180 waits via DCFB8(12,70); D42DC via DD0A4(16,70).
Both encode result BE32 and do not branch on the wait's return. Units and full
timeout semantics are not asserted. Since the command word is unrestricted,
this handler is not guaranteed side-effect-free for arbitrary command IDs.

## Sender census

An aligned direct-JARL-lp candidate scan over byte range80000..E0000 finds17
references toAC648, all inspected in bounded listings. It is not a proof about
indirect calls, other link registers or alternative writers to the transport.

Most candidates are the previously bound ordinary API senders. Three additional
sites matter: AA886 fixedAPI18; AC6DE request-command/raw0; AC756 fixed0800AFFE/raw0
followed by bulk result reads(event371). No fixed sender for14000000,12000000,
10000000 or20000003 was found in this census. Those extended writes remain
reachable through request-supplied A21 words, not proven ordinary playback events.

## Dedicated API18 source

AA644 is reached atA77CA(event22) andA77D6(event368). Its tail AA86A..AA886 reads
s8(gp−15840), converts it to float viaEBCA4 and sends API18. B6DD0 initializes
the source byte0. Direct byte-store/gp-relative census80000..E0000 finds only
B6DD0 and CA732/CA73E/CA744; this does not exclude indirect writers.

Request handlerCA4B4 is bound in block22's directory95A24, functionD28 at95B9C,
operation table959C4. Operation0 callsCA4B4; operation2 calls it and, on success,
thenCA8BC; operation1 usesCA8BC alone.

- Selector5: length3, signed coordinatepayload[2].
- Selector0: length8, same coordinatepayload[6] after earlier fields succeed.
- CA6F4/CA6F8 accept −12..+12; other values take an error path.
- CA728 stores the raw coordinate atgp−14973.
- CA732/CA73E/CA744 store clamp(raw,−9,+9) atgp−15840.
- With mode bytesgp−17771/−17770/−16783 unchanged, CA834 queues event22.
  Changed-mode paths have their own conditional event/signal behavior.

This is a multi-field sequential writer, not an atomic validated transaction:
prior fields can already be changed before a later field fails validation.
The isolated helper `api18_request_coordinate` models only the accepted signed
field's clamp, not full packet validation, events, DSP apply or defer release.

**The byte is not used only by API18.** AA644 first sends20 coefficient words
to SHARC addressE0584; AA7F2 uses the same byte in a conditional correction lookup.
Event22 additionally callsA903C, which reads the byte atA90B6/A9106 while preparing
another coefficient block. Those table values, conditions and DSP consumers are
the next bounded static task. Do not replace their effects with one scalar gain.

Zero maps to API18 float0 on this path, but zero does not mean flat whole-amp
response: common level-dependent EQ, calibration bases and other slots remain.
The user's Bass0/LFE0 and current vehicleB state remain unbound.

## Reproduction and validation

`tools/audit_amp_b_outer_control.py` exports image-checked table records, ranges,
sender census, raw/control source slots and boundaries to `b_outer_control.json`
through the existing `audit_amp_level_control.py --loader` command. The V850
listing set grows25→41; all41 are checked byte-for-byte and decode without
invalid/truncated instructions. SHARC remains166 bounded ranges, UNKNOWN0.

392 tests pass:117 extraction +275 model. New tests include five event routes
executed from decoded branch instructions and **all256 byte inputs** through
the decoded API18 domain/clamp slice, with explicit old raw coordinate0.
They are bounded interpreters, not whole-firmware or bit-exact audio emulation.
CodeFlash SHA256:
`8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`.
AmpChain defaults and production hooks are unchanged.
