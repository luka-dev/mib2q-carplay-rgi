# HUD ramp investigation, 2026-09-11

> Follow-up: the sharp EXIT bug is now fixed in the Java mapper (base EXIT direction00); five ordinary-turn types also retain finer angles. See [current 54-type selection](../../../docs/reference/CARPLAY_HUD_ICON_SELECTION.md). The analysis below records the pre-selection investigation.

Primary write-up: [HUD_RAMP_3D_CHAIN.md](../../../docs/reference/HUD_RAMP_3D_CHAIN.md).
The work is offline. No application behavior or vehicle state was changed.

## Reproduce the useful result

The NSATask text bytes are FD61[G1952][0xe2000:0xf9ce4], linked at 0x10000.
Header at text+0x64 contains `{0x10000,0,0x28000}`. Data is 180 bytes at file
0xf9ce4, linked at 0x28000. `NSATask_linked.json` records the wrapper metadata.
The wrapper's BSS allowance is NOT a recovered memory map. Actual globals also
exist above that allowance; unmapped `MEMORY[...]` in IDA is expected.

Original source SHA256:

- FD61 G1952: `0146bfb0ac3584f3f6fe19f2d38a97c65f3bbd4bbed63d2d43c51db24bf4dc63`
- FD62 G1952: `259409a247a297417d6b398bb42a9f51897514604f320c0b184ccb92b53c35ee`
- FD51 G1952: `138351c2f12cd8e45d9f50fd4833775371ec46401b6838f04c24a9a4baaabae2`
- FD51 M2325: `6e59b62c982873a778bf72b749bf6dc113fe3fe14ee4bb3ae3855d0c624f6fe5`

G/M FD51 images differ at only 9 bytes: linked 0x20006,0x2000a..b,0x2000f,
0x20011..13,0x20017 and 0x79918. The navigation tables and executed path agree.
The differing byte at 0x79918 belongs to another parameter accessor.

Do not mistake the old combined IDB's NSA placement at 0x80700000 for the
task's link addresses: most application code was not analyzed there. Correct
link addresses recovered many previously missed application functions.

Verified page adapter: linked 0x138fc. The standalone
`tools/verify_hud_nsa_navigation.py` maps the original bytes directly and does
not require the synthetic ELF or an IDB. 768 cases preserve the complete
108-byte selector block. The serialized input's byte 23 becomes GDC packet
byte 22. The 17 road selectors and 2D selector are copied as well.

Saved IDA pseudocode is evidence, not production C. Locals and external library
types may be imperfect. `NSA_0x16908.c` is an explicitly incomplete export of
a very large initializer (tool truncation); do not treat it as the full function.

## Recovered producer and verified mapping

The producer is FD51 V850. The complete descriptor path is:

```
5ec3c: BAP cache (LSG0x32, FctID0x17) -> RAM fedfadd4
8ab76: reset8acb0 + update8adac
8b32e: main -> class; 8b3b2: class/direction -> model enum
8b486: z marker; 8b536: default/exit/side-road enums and sorting
974e8 -> 97b96: display model fedfc600
86534: 19 bytes [main selector, marker, 17 roads]
85286: page3 payload[19..37]; 8508e -> payload fedfc1e4
8a984 -> 81b9e -> 81dea -> 827d0: DLE framed serial packet
82a8e -> 83154: transmit through MCU peripheral/DMA
NSA138fc: frame[23] -> GDCpacket[22], road fields copied alongside
GDC main node8da20 -> FD62 surface records/resources
```

The `kernel-spi-channel.json` export records ARM board initialization:
0x80028d18 -> 0x8003f458(0x3f040000,160,2000000), channel SpiUsartDev4,
matching NSATask's opened device. The MCU sender's framing matches the NSA parser.
This establishes the software chain; physical wiring was not probed.

`tools/ghidra/HudManeuverChainAudit.java` executes the original V850 importer,
converter, model transfer and selector packer, including original memory helpers.
Only the BAP cache getter is substituted, asserting LSG0x32/FctID0x17. Active
3D mode3 is forced, RAM starts cleared, secondary descriptors are empty.
829 cases per firmware, 7,207,899 original instructions per firmware;
The logged 7,208,728 steps include 829 substituted cache calls.
G1952 and M2325 CSVs are identical. This covers all 169 distinct descriptors
from 7,890 actual Java mapper runs plus the additional ramp/grid/edge inputs.
The full DLE/hardware sender was inspected statically, not executed by this probe.

`tools/hud_mcu_decoder.py` independently implements the recovered table algorithm
and matches both original V850 runs on all 829 cases, including roads and markers.
Its JSON exports retain tables and firmware fingerprints.

Correction: EXIT_RIGHT/E0 -> model36 -> selector35/r0305, NOT selector39.
Selector39/r0309 is SERVICE_ROAD_RIGHT17. EXIT_RIGHT/A0 and EXIT_LEFT/60 map to
model0, with cleared 3D arrow and roads after the normal reset. The shipping
Java ramp mapper emits these unsupported combinations at +/-135 degrees.
TURN/E0 -> selector32; CHANGE_LANE/C0 -> selector25. ARRIVED/00 -> selector120.
FOLLOW_STREET/00/z0/empty roads has a special importer branch clearing slot1;
its isolated empty 3D result must not be generalized to the whole HUD.

The `mcu-verified/` exports have corrected GP and include 33 selected functions.
The ignored `mcu-linked/` directory is an obsolete exploratory cache with wrong
GP-relative RAM labels. Do not use it as current evidence. Original firmware and
original IDA/Ghidra databases were closed without saving analysis changes.

## MCU findings and false leads to avoid repeating

FD51 uses V850, link base **0x20000**, GP **0xFEDFF8B4**, TP **0x28018**.
Temporary Ghidra rebasing and register context are required for meaningful
decompilation. Startup loads 0xFEDF78B4 then adds 0x4000 twice (410ca/410ce).
See `mcu-verified/startup-registers.asm`. CTBP=0xB59C0. The earlier GP value
was 0x8000 too low. Old base 0/GP-less exports often hide references.

- File 0x4d10 and 0x9a80 contain inverse FctID/index tables, not proof of two
  separate BAP endpoints. Their roles are not established as NAVSD.
- The 44-pointer table at file 0x4dd8 is the CAN receive-handler table, with
  absolute pointers such as 0x66c9a. It is not a ManeuverDescriptor dispatch.
- Its handlers DO unpack bitfields once decoded at the correct addresses;
  earlier claims of zero bit unpacking were false negatives.
- `0x6273c`, `0x63f68`, `0x64dbc` cover generic property/list handling. No
  navigation geometry mapping was established there.
- `0x931d2` controls mirror/position state; it is not the navigation icon mapper.
- `0x7f0e2` reads parameter records, checks a checksum and copies chunks into
  a 0x2f9-byte buffer. Its existence does not establish it as a navigation page
  builder; do not treat old comments calling it a screen builder as evidence.

The user has explicitly parked distance/bargraph. Concurrent upmix work in the
same repository is unrelated and must not be included in maneuver changes.
